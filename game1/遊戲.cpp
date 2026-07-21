#include <iostream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <fstream>
#include <vector>
#include <map>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace std;

// ===== 🎨 顏色與視覺工具 =====
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
// 啟用 ANSI 色碼（Windows Terminal / 新版主控台支援）
void enableAnsiColors() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}
const char* C_RESET = "\033[0m";
const char* C_RED    = "\033[91m";  // 傷害
const char* C_GREEN  = "\033[92m";  // 治療/健康
const char* C_YELLOW = "\033[93m";  // 暴擊/金幣
const char* C_BLUE   = "\033[94m";  // 資訊
const char* C_MAGENTA= "\033[95m";  // 絕招
const char* C_CYAN   = "\033[96m";  // 標題
const char* C_GRAY   = "\033[90m";  // 次要
const char* C_BOLD   = "\033[1m";

// 產生視覺化血條：████████░░░░（依血量比例變色：綠→黃→紅）
string makeBar(int cur, int mx, int width = 18) {
    if (mx <= 0) mx = 1;
    if (cur < 0) cur = 0;
    if (cur > mx) cur = mx;
    double ratio = (double)cur / mx;
    int filled = (int)(ratio * width + 0.5);
    const char* col = ratio > 0.5 ? C_GREEN : (ratio > 0.25 ? C_YELLOW : C_RED);
    string bar = col;
    for (int i = 0; i < width; i++) bar += (i < filled) ? "█" : "░";
    bar += C_RESET;
    return bar;
}

// ===== 📊 戰績統計（通關/陣亡結算用）=====
long long gTotalDamage = 0; // 累計造成傷害
int gMaxHit = 0;            // 最大單次傷害
int gKills = 0;             // 擊殺數
int gUltUsed = 0;           // 絕招使用次數
int gTurns = 0;             // 總回合數

// === 基礎結構 ===
struct Equipment {
    string name; string type; int value; int subValue;
    string element; // 武器元素：火/冰/雷/毒/無
    int elemValue;  // 元素效果強度（如灼燒/中毒每回合傷害）
    string wclass;  // 武器類型：劍/槍/匕首/法杖/弓/巨劍/鐮/萬能…（防具留空）
};

struct Skill {
    string name;
    int reqLevel;
    int mpCost;
    double damageMult;
    string effect;
    string attribute;
    int attrValue;
    string reqWeapon; // 施展所需武器類型（"" 或 "任意" = 不限；絕招通常需對應武器）
    string flavor;    // 施展時的詳細描述（絕招專用，讓演出更豐富）
};

struct JobTemplate {
    string name; string rarity;
    int hp; int atk; int mp; int regen;
    int agiBonus; int lukBonus; string desc;
    string weaponClass; // 此職業的專屬武器類型（空 = 舊職業，尚未套用新系統）
};

// === 職業資料庫 (15 舊職 + 3 新職試作 = 18 種) ===
JobTemplate jobDB[18] = {
    {"劍士", "N", 130, 25, 30, 6, 0, 0, "攻守平衡的基礎近戰職", "劍"},
    {"魔法師", "N", 80, 15, 100, 15, 0, 0, "擅長高爆發的脆弱法系", "法杖"},
    {"刺客", "N", 95, 28, 40, 8, 10, 10, "基礎閃避與暴擊特化的暗殺者", "匕首"},
    {"狂戰士", "R", 200, 18, 0, 0, 0, 0, "捨棄魔力，以鮮血換取力量", "巨劍"},
    {"弓箭手", "N", 100, 22, 40, 8, 5, 5, "穩定的遠程物理輸出", "弓"},
    {"吟遊詩人", "N", 110, 20, 60, 12, 5, 0, "魔力充沛且平衡的輔助型戰士", "法杖"},
    {"武僧", "R", 140, 26, 30, 5, 15, 0, "閃避極高的肉搏大師", "拳"},
    {"槍手", "R", 90, 30, 50, 10, 0, 15, "極致暴擊率的火槍使用者", "槍"},
    {"聖騎士", "SR", 180, 20, 50, 8, 0, 0, "擁有神聖庇護的重裝防禦者", "劍"},
    {"死靈法師", "SR", 90, 18, 120, 18, 0, 0, "精通黑暗魔法與吸血的法師", "法杖"},
    {"幻術師", "SR", 85, 20, 110, 15, 20, 0, "難以捉摸，極高閃避的法系", "法杖"},
    {"黑暗祭司", "SR", 120, 24, 80, 10, 0, 10, "操縱詛咒的異端神職者", "法杖"},
    {"龍騎士", "SSR", 250, 35, 60, 10, 5, 5, "完美數值！攻防一體的傳說存在", "槍"},
    {"魔劍士", "SSR", 160, 32, 90, 15, 10, 10, "物理與魔法雙修的無敵劍客", "劍"},
    {"創造神", "EX", 9999, 999, 999, 99, 50, 50, "【上帝模式專屬】無視一切規則的造物主", "萬能"},
    // === 新職業試作（8 技能 + 2 絕招，絕招需對應武器）===
    {"天劍聖", "SSR", 190, 34, 70, 10, 5, 5, "【新】掌握十道劍技的近戰劍聖，絕招需持【劍】", "劍"},
    {"破軍槍神", "SSR", 150, 33, 55, 9, 5, 20, "【新】極致暴擊的槍術宗師，絕招需持【槍】", "槍"},
    {"影襲刺客", "SR", 120, 30, 50, 8, 20, 15, "【新】高閃避高暴擊的暗影殺手，絕招需持【匕首】", "匕首"}
};
const int JOB_COUNT = 18;
// 越稀有技能越多：N=4, R=5, SR=6, SSR=8, EX(創造神)=3
int getSkillCount(int jIdx) {
    string r = jobDB[jIdx].rarity;
    if (r == "EX") return 3;
    if (r == "N")  return 4;
    if (r == "R")  return 5;
    if (r == "SR") return 6;
    return 8; // SSR
}

int getJobIndex(string jName) {
    for (int i = 0; i < JOB_COUNT; i++) {
        if (jobDB[i].name == jName) return i;
    }
    return 0;
}

// === 🌑 魔法階位（暗黑世界觀）===
// 表面是修煉突破，實則是「太陽之核」寄生越來越深的五個階段
int getRankIndex(int lv) {
    // 門檻壓低，讓一般玩家在前中期就能看完整條剝洋蔥揭密
    if (lv <= 1) return 0;      // 微光者 (Lv.1)
    if (lv <= 3) return 1;      // 銘刻者 (Lv.2-3)
    if (lv <= 6) return 2;      // 結晶者 (Lv.4-6)
    if (lv <= 9) return 3;      // 共鳴者 (Lv.7-9)
    return 4;                   // 受冕者 (Lv.10+)
}
string getRankName(int lv) {
    const char* names[5] = {"微光者", "銘刻者", "結晶者", "共鳴者", "受冕者"};
    return names[getRankIndex(lv)];
}
// 寄生神經同步率（升越高，被太陽之核控制越深）
int getSyncRate(int lv) {
    int s = 8 + lv * 6;
    if (s > 99) s = 99;
    return s;
}

// === 角色類別 ===
class Character {
public:
    string name; string job;
    int level; int exp; int maxExp; int statPoints;
    int baseHp; int maxHp; int hp;
    int baseAtk; int atk;
    int mp; int maxMp; int mpRegen;
    int strength; int constitution; int agility; int lucky;
    int dodgeChance; int critChance;
    int skillLevel[10]; // 目前職業各技能的等級（Lv.1 起，每級傷害倍率 +0.3；新職最多 10 個）
    Equipment weapon; Equipment armor;

    Character(string n, string j, int h, int a, int m, int r) {
        name = n; job = j; level = 1; exp = 0; maxExp = 100; statPoints = 0;
        baseHp = h; baseAtk = a; strength = 0; constitution = 0; agility = 0; lucky = 0;
        for (int i = 0; i < 10; i++) skillLevel[i] = 1;
        weapon = { "新手舊劍", "Weapon", 0, 5, "無", 0, "劍" };
        armor = { "破舊布衣", "Armor", 0, 5, "無", 0, "" };
        updateStats();
        hp = maxHp; mp = m; maxMp = m; mpRegen = r;
    }

    void updateStats() {
        maxHp = baseHp + (constitution * 15) + armor.value;
        atk = baseAtk + (strength * 2) + weapon.value;
        dodgeChance = 5 + agility + armor.subValue;
        critChance = 5 + lucky + weapon.subValue;
        if (dodgeChance > 75) dodgeChance = 75;
        if (critChance > 100) critChance = 100;
    }

    bool isAlive() { return hp > 0; }

    bool takeDamage(int damage) {
        if ((rand() % 100) < dodgeChance) {
            cout << "💨 【閃避！】" << name << " 輕巧地閃過了攻擊！\n";
            return false;
        }
        hp -= damage; if (hp < 0) hp = 0;
        cout << "💥 " << name << " 受到了 " << damage << " 點傷害！ (剩餘 HP: " << hp << "/" << maxHp << ")\n";
        return true;
    }

    void gainExp(int amount) {
        exp += amount;
        cout << "✨ 汲取了 " << amount << " 縷魔力粉塵…… (" << exp << "/" << maxExp << ")\n";
        while (exp >= maxExp) {
            int oldRank = getRankIndex(level);
            exp -= maxExp; level++; maxExp = 100 + (level * 50); statPoints += 4;
            cout << "\n🎉🎉🎉 【突破！】 🎉🎉🎉\n 恭喜突破至 Lv." << level << "！魔力上限提升，獲得 4 點屬性點！\n";
            updateStats();
            hp = min(maxHp, hp + 30);
            if (job != "狂戰士") mp = min(maxMp, mp + 15);
            // 🩸 底部紅字：表面榮耀，實則寄生加深
            cout << C_GRAY << "  [系統警告] 寄生神經對接宿主大腦，同步率 " << getSyncRate(level) << "%\n" << C_RESET;
            // 🌑 跨越魔法階位時，掀開一層真相
            int newRank = getRankIndex(level);
            if (newRank > oldRank) revealRank(newRank);
        }
    }

    // 晉升魔法階位時的「表面 + 真相」演出
    void revealRank(int r) {
        const char* title[5] = {"微光者", "銘刻者", "結晶者", "共鳴者", "受冕者"};
        const char* glory[5] = {
            "你覺醒了魔力感應，教會稱你為「受神眷顧的雛鳥」。",
            "你在靈魂表面刻下魔力迴路，成為正式法師，施法更快更強。",
            "你的心臟凝聚出硬幣大小的『魔導原核』，壽命延長，魔力近乎無限。",
            "你能召喚古代英靈與自身疊加，施展毀滅級的禁咒。",
            "你的肉身轉化為半神光體，被尊為『神的地上代理人』。"};
        const char* truth[5] = {
            "…你吸入肺腑的魔力，其實是太陽之核碾碎歷史人類靈魂的精神粉塵。",
            "…那不是導體，是太陽之核植入你體內、監視思想的『生物神經汲取網』。",
            "…原核是『熟透的結晶化靈魂』；你，已被列為收割的候補燃料。",
            "…英靈不是認可你，是慘死勇者的殘魂被強行縫進你的靈魂，替你承受反噬。",
            "…你已失去作為人的主權，成為太陽之核的實體接收終端。"};
        // 🗣️ 主角內心獨白：隨著被寄生越深，從熱血逐漸崩壞
        const char* voice[5] = {
            "只要刻苦練習魔法，總有一天，我也能成為照亮世界的太陽！",
            "迴路發動時好燙……但這點痛算什麼？為了保護大家，我會更強。",
            "奇怪……我已經想不起故鄉的臉了，心臟好冷。可是力量確實變強了……這樣就好，對吧？",
            "安靜點！你們這些在我腦子裡尖叫的死人！我不管你們是誰——把力量借給我，不然我們一起被煉成廢渣！",
            "太陽？那不是什麼光明的天體……那是懸在所有人頭頂的屠宰場。今天，我要親手把這顆虛偽的火球，從天上拽下來！"};
        cout << "\n" << C_CYAN << "═══════ 魔法階位晉升：【" << title[r] << "】 ═══════\n" << C_RESET;
        cout << C_YELLOW << " " << glory[r] << "\n" << C_RESET;
        cout << C_RED << " " << truth[r] << "\n" << C_RESET;
        cout << C_GRAY << " ── " << name << " 喃喃道 ──\n" << C_RESET;
        cout << C_BOLD << " 「" << voice[r] << "」\n" << C_RESET;
        cout << C_CYAN << "════════════════════════════════════\n" << C_RESET;
    }
};

void clearScreen() {
    #ifdef _WIN32
    system("cls");
    #else
    system("clear");
    #endif
}
void waitPlayer() { cout << "\n請按 Enter 鍵繼續..."; cin.get(); }

// 🌟 戰鬥速度：0=手動(按Enter) 1=快速(自動~0.7s) 2=極速(自動~0.15s)
int gBattleSpeed = 1;
const char* battleSpeedName() { return gBattleSpeed == 0 ? "手動" : (gBattleSpeed == 1 ? "快速" : "極速"); }
void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#endif
}
// 戰鬥中的停頓：手動模式等玩家按 Enter，自動模式短暫延遲後續播
void battlePause() {
    if (gBattleSpeed == 0) { waitPlayer(); }
    else { cout.flush(); sleepMs(gBattleSpeed == 1 ? 700 : 150); }
}

string getSaveFileName(int slot) { return "rpg_save_slot" + to_string(slot) + ".txt"; }

void peekSaveSlot(int slot) {
    ifstream in(getSaveFileName(slot));
    if (in) {
        string name, job; int level = 0;
        getline(in, name); in >> job >> level; // 名字可能含空格，需用 getline
        cout << " [" << slot << "] 檔號 " << slot << " : " << name << " (" << job << ") Lv." << level << "\n";
    } else {
        cout << " [" << slot << "] 檔號 " << slot << " : --- 空白存檔 ---\n";
    }
}

void saveGame(Character& p, int gold, int potions, int wave, int slot) {
    string filename = getSaveFileName(slot);
    ofstream outFile(filename);
    if (!outFile) { cout << "❌ 存檔失敗，無法寫入檔案！\n"; waitPlayer(); return; }
    outFile << p.name << "\n" << p.job << "\n" << p.level << "\n" << p.exp << "\n" << p.maxExp << "\n"
            << p.statPoints << "\n" << p.baseHp << "\n" << p.hp << "\n" << p.baseAtk << "\n"
            << p.mp << "\n" << p.maxMp << "\n" << p.mpRegen << "\n"
            << p.strength << "\n" << p.constitution << "\n" << p.agility << "\n" << p.lucky << "\n"
            << gold << "\n" << potions << "\n" << wave << "\n"
            << p.weapon.name << "\n" << p.weapon.value << "\n" << p.weapon.subValue << "\n"
            << p.armor.name << "\n" << p.armor.value << "\n" << p.armor.subValue << "\n"
            << p.weapon.element << "\n" << p.weapon.elemValue << "\n"
            << p.armor.element << "\n" << p.armor.elemValue << "\n";
    for (int i = 0; i < 10; i++) outFile << p.skillLevel[i] << "\n";
    outFile << (p.weapon.wclass.empty() ? "無" : p.weapon.wclass) << "\n";
    outFile.close();
    cout << "💾 【系統提示】進度已成功儲存至 檔號 " << slot << "！\n";
    waitPlayer();
}

// 商店商品通用結構：裝備本體、售價、最低登場章節
struct ShopItem { Equipment eq; int price; int minCh; };

// 全域資料池（皆有內建預設，可由對應的 JSON 檔覆蓋）
vector<ShopItem> gWeaponPool;   // 武器商品池 ← weapons.json
vector<ShopItem> gArmorPool;    // 防具商品池 ← armors.json
vector<string>   gMonsters[3];  // 三章一般魔物名 ← monsters.json

// 依章節從商品池隨機挑出本次販售的品項（每次進店都不一樣）
vector<int> rollStock(vector<ShopItem>& pool, int chapter, int maxOffer) {
    vector<int> eligible;
    for (int i = 0; i < (int)pool.size(); i++) if (pool[i].minCh <= chapter) eligible.push_back(i);
    // Fisher-Yates 洗牌
    for (int i = (int)eligible.size() - 1; i > 0; i--) { int j = rand() % (i + 1); swap(eligible[i], eligible[j]); }
    int n = min((int)eligible.size(), maxOffer);
    return vector<int>(eligible.begin(), eligible.begin() + n);
}

// 🌟 武器商店：販售帶有元素屬性的武器（隨章節變強、每次進貨隨機）
void weaponShop(Character& p, int& gold, int chapter) {
    vector<ShopItem>& pool = gWeaponPool; // 使用全域武器池（可由 weapons.json 覆蓋）
    vector<int> offer = rollStock(pool, chapter, 4); // 進店時決定本次庫存
    int offerCount = (int)offer.size();

    while (true) {
        clearScreen();
        cout << "=================================\n         🗡️ 武器鍛造鋪         \n=================================\n";
        cout << " 金幣: ✨ " << gold << "  (第 " << chapter << " 章精選 " << offerCount << " 件)\n";
        cout << " 目前武器: 【" << p.weapon.name << "】(類型:" << (p.weapon.wclass.empty() ? "無" : p.weapon.wclass) << ") 攻+" << p.weapon.value
             << " 暴+" << p.weapon.subValue << " 屬性:" << p.weapon.element << "\n";
        cout << "---------------------------------\n";
        for (int i = 0; i < offerCount; i++) {
            Equipment& e = pool[offer[i]].eq;
            cout << " [" << (i + 1) << "] " << e.name << " ｜ [" << e.wclass << "] 攻+" << e.value << " 暴+" << e.subValue;
            if (e.element != "無") {
                cout << " ｜ " << e.element << "屬性";
                if (e.elemValue > 0) cout << "(每回合 " << e.elemValue << ")";
            }
            cout << " ｜ 💰" << pool[offer[i]].price << "\n";
        }
        cout << " [0] 🏃 離開武器店\n---------------------------------\n";
        cout << " 說明：武器類型決定能否施展職業絕招；火/毒→持續傷害，雷/冰→機率麻痺\n";
        cout << "請選擇 (0-" << offerCount << "): ";
        int c; if (!(cin >> c)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');
        if (c == 0) break;
        if (c >= 1 && c <= offerCount) {
            ShopItem& wi = pool[offer[c - 1]];
            if (gold >= wi.price) {
                gold -= wi.price; p.weapon = wi.eq; p.updateStats();
                cout << "🛒 購買並裝備了【" << p.weapon.name << "】！攻擊力提升至 " << p.atk << "！\n";
            } else cout << "❌ 金幣不足！還差 " << (wi.price - gold) << " 金幣。\n";
            waitPlayer();
        }
    }
}

// 🌟 防具商店：販售帶有防禦特性的防具（隨章節變強、每次進貨隨機）
// 防具 element：無 / 反傷(反彈%傷害) / 減傷(減免%傷害)，elemValue 為百分比
void armorShop(Character& p, int& gold, int chapter) {
    vector<ShopItem>& pool = gArmorPool; // 使用全域防具池（可由 armors.json 覆蓋）
    vector<int> offer = rollStock(pool, chapter, 4);
    int offerCount = (int)offer.size();

    while (true) {
        clearScreen();
        cout << "=================================\n         🛡️ 防具鎧甲鋪         \n=================================\n";
        cout << " 金幣: ✨ " << gold << "  (第 " << chapter << " 章精選 " << offerCount << " 件)\n";
        cout << " 目前防具: 【" << p.armor.name << "】 防+" << p.armor.value
             << " 閃+" << p.armor.subValue << " 特性:" << p.armor.element
             << (p.armor.element != "無" ? " " + to_string(p.armor.elemValue) + "%" : "") << "\n";
        cout << "---------------------------------\n";
        for (int i = 0; i < offerCount; i++) {
            Equipment& e = pool[offer[i]].eq;
            cout << " [" << (i + 1) << "] " << e.name << " ｜ 防+" << e.value << " 閃+" << e.subValue;
            if (e.element != "無") cout << " ｜ " << e.element << " " << e.elemValue << "%";
            cout << " ｜ 💰" << pool[offer[i]].price << "\n";
        }
        cout << " [0] 🏃 離開防具店\n---------------------------------\n";
        cout << " 說明：減傷→受擊時減少傷害，反傷→反彈部分傷害給敵人\n";
        cout << "請選擇 (0-" << offerCount << "): ";
        int c; if (!(cin >> c)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');
        if (c == 0) break;
        if (c >= 1 && c <= offerCount) {
            ShopItem& ai = pool[offer[c - 1]];
            if (gold >= ai.price) {
                gold -= ai.price; p.armor = ai.eq; p.updateStats();
                cout << "🛒 購買並穿上了【" << p.armor.name << "】！最大生命提升至 " << p.maxHp << "！\n";
            } else cout << "❌ 金幣不足！還差 " << (ai.price - gold) << " 金幣。\n";
            waitPlayer();
        }
    }
}

// 🌟 開發者控制台函式
// 🌟 開發者控制台函式 (已擴充等級與職業修改)
void adminPanel(Character& p, int& gold, int& potions, int& wave, int& mHp, int& mAtk, string mName, Skill skillDB[][10]) {
    while (true) {
        clearScreen();
        int jIdx = getJobIndex(p.job); // 隨時對應目前職業的技能
        cout << "=========================================\n       ⚙️ 開發者控制模式 (Admin) ⚙️       \n=========================================\n";
        cout << " [1] 修改最大生命 (目前: " << p.maxHp << ")\n";
        cout << " [2] 修改基礎攻擊 (目前: " << p.baseAtk << ")\n";
        cout << " [3] 修改金幣數量 (目前: " << gold << ")\n";
        cout << " [4] 修改藥水數量 (目前: " << potions << ")\n";
        cout << " [5] 修改目前波數 (目前: " << wave << ")\n";
        if (mHp != -1) {
            cout << " [6] 修改怪物血量 (目標: " << mName << " HP:" << mHp << ")\n";
            cout << " [7] 修改怪物攻擊 (目標: " << mName << " ATK:" << mAtk << ")\n";
        }
        cout << " [8] 修改角色等級 (目前: Lv." << p.level << ")\n";
        cout << " [9] 修改角色職業 (目前: " << p.job << ")\n";
        cout << " [10] 升級技能 (共 " << getSkillCount(jIdx) << " 個技能可強化)\n";
        cout << " [0] ↩️ 返回遊戲\n=========================================\n請選擇要修改的項目 (0-10): ";

        int choice; if (!(cin >> choice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');
        if (choice == 0) break;

        // 如果選擇修改職業，印出職業參照表
        if (choice == 9) {
            cout << "👉 職業參照表：\n  0:劍士, 1:魔法師, 2:刺客, 3:狂戰士, 4:弓箭手\n  5:吟遊詩人, 6:武僧, 7:槍手, 8:聖騎士, 9:死靈法師\n  10:幻術師, 11:黑暗祭司, 12:龍騎士, 13:魔劍士, 14:創造神\n  15:天劍聖(劍), 16:破軍槍神(槍), 17:影襲刺客(匕首) ← 新職\n";
            cout << "👉 請輸入新的職業編號 (0-17): ";
        }
        else if (choice == 10) {
            int sc = getSkillCount(jIdx);
            cout << "👉 目前【" << p.job << "】的技能：\n";
            for (int i = 0; i < sc; i++)
                cout << "  [" << (i + 1) << "] " << ((!skillDB[jIdx][i].reqWeapon.empty() && skillDB[jIdx][i].reqWeapon != "任意") ? "🌟" : "") << skillDB[jIdx][i].name
                     << " (Lv." << p.skillLevel[i] << "，倍率 "
                     << (skillDB[jIdx][i].damageMult + (p.skillLevel[i] - 1) * 0.3) << ")\n";
            cout << "👉 請輸入要升級的技能編號 (1-" << sc << "): ";
        }
        else cout << "👉 請輸入新的數值: ";

        int newVal; if (!(cin >> newVal)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');

        if (choice == 1) { p.baseHp = newVal; p.updateStats(); p.hp = p.maxHp; cout << "✅ 修改成功！\n"; }
        else if (choice == 2) { p.baseAtk = newVal; p.updateStats(); cout << "✅ 修改成功！\n"; }
        else if (choice == 3) { gold = newVal; cout << "✅ 修改成功！\n"; }
        else if (choice == 4) { potions = newVal; cout << "✅ 修改成功！\n"; }
        else if (choice == 5) { wave = newVal; cout << "✅ 修改成功！(下一波生效)\n"; }
        else if (choice == 6 && mHp != -1) { mHp = newVal; cout << "✅ 修改成功！\n"; }
        else if (choice == 7 && mHp != -1) { mAtk = newVal; cout << "✅ 修改成功！\n"; }
        else if (choice == 8) {
            p.level = newVal;
            p.maxExp = 100 + (p.level * 50); // 確保升級所需經驗值同步更新
            cout << "✅ 修改成功！目前等級已設定為 Lv." << p.level << "\n";
        }
        else if (choice == 9) {
            if (newVal >= 0 && newVal <= 17) {
                p.job = jobDB[newVal].name;
                for (int i = 0; i < 10; i++) p.skillLevel[i] = 1; // 新職業技能重置
                if (!jobDB[newVal].weaponClass.empty()) p.weapon.wclass = jobDB[newVal].weaponClass; // 對應武器類型，方便施展絕招
                cout << "✅ 修改成功！職業已切換為【" << p.job << "】，技能已對應更新。\n";
            } else {
                cout << "❌ 無效的職業編號！\n";
            }
        }
        else if (choice == 10) {
            if (newVal >= 1 && newVal <= getSkillCount(jIdx)) {
                p.skillLevel[newVal - 1]++;
                cout << "✅ 技能【" << skillDB[jIdx][newVal - 1].name << "】提升至 Lv."
                     << p.skillLevel[newVal - 1] << "！\n";
            } else {
                cout << "❌ 無效的技能編號！\n";
            }
        }
        waitPlayer();
    }
}

// 🌟 技能資料庫初始化（越稀有技能越多；SR/SSR 最後 1~2 個為絕招，需對應武器）
void initAllSkills(Skill db[][10]) {
    // --- 劍士 (N, 劍, 4) ---
    db[0][0] = {"強擊斬", 1, 10, 1.5, "造成 1.5 倍傷害", "NONE", 0};
    db[0][1] = {"聖光重擊", 3, 18, 2.0, "傷害並提升本次戰鬥防禦", "BUFF_DEF", 10};
    db[0][2] = {"諸神防壁斬", 5, 25, 3.5, "3.5倍爆發，大幅提升防禦", "BUFF_DEF", 25};
    db[0][3] = {"破空劍擊", 7, 30, 3.0, "凝聚劍氣的強力一擊", "NONE", 0};

    // --- 魔法師 (N, 法杖, 4) ---
    db[1][0] = {"火球術", 1, 15, 1.8, "附加點燃，每回合扣血", "BURN", 15};
    db[1][1] = {"雷霆召喚", 3, 28, 2.5, "造成傷害並麻痺敵人1回合", "STUN", 1};
    db[1][2] = {"終焉隕石術", 5, 45, 5.0, "毀滅性傷害並重度點燃", "BURN", 30};
    db[1][3] = {"寒冰新星", 7, 30, 2.8, "冰霜爆發使敵凍結", "STUN", 1};

    // --- 刺客 (N, 匕首, 4) ---
    db[2][0] = {"毒刃伏擊", 1, 12, 1.5, "潛伏攻擊，附加中毒流血", "BURN", 10};
    db[2][1] = {"幻影瞬殺", 3, 20, 2.8, "必定暴擊的殘影攻擊", "NONE", 0};
    db[2][2] = {"萬箭穿心殺", 5, 30, 4.2, "必定暴擊的絕殺", "NONE", 0};
    db[2][3] = {"影襲連刺", 7, 26, 3.0, "疾速的連續刺擊", "NONE", 0};

    // --- 狂戰士 (R, 巨劍, 5) ---
    db[3][0] = {"血性打擊", 1, 15, 2.2, "耗血15，2.2 倍傷害", "NONE", 0};
    db[3][1] = {"怒火重擊", 3, 25, 3.5, "耗血25，3.5 倍傷害", "NONE", 0};
    db[3][2] = {"嗜血毀滅殺", 5, 40, 4.5, "耗血40，傷害並吸取大量生命", "HEAL", 60};
    db[3][3] = {"狂亂斬", 7, 20, 3.2, "失控的連續劈砍", "NONE", 0};
    db[3][4] = {"不死怒吼", 9, 25, 2.0, "戰吼大幅提升攻擊", "BUFF_ATK", 20};

    // --- 弓箭手 (N, 弓, 4) ---
    db[4][0] = {"二連矢", 1, 12, 1.6, "連續射擊", "NONE", 0};
    db[4][1] = {"封喉箭", 3, 22, 2.0, "造成傷害並封鎖行動", "STUN", 1};
    db[4][2] = {"星辰爆裂箭", 5, 35, 4.0, "星光爆發", "NONE", 0};
    db[4][3] = {"貫穿箭", 7, 26, 3.0, "穿透一切的強箭", "NONE", 0};

    // --- 吟遊詩人 (N, 法杖, 4) ---
    db[5][0] = {"激昂和弦", 1, 15, 1.2, "音波攻擊，提升攻擊力", "BUFF_ATK", 8};
    db[5][1] = {"催眠曲", 3, 25, 1.5, "造成傷害並使魔物沉睡", "STUN", 1};
    db[5][2] = {"英雄戰歌", 5, 40, 3.0, "大合唱，大幅提升攻擊力", "BUFF_ATK", 25};
    db[5][3] = {"治癒詩篇", 7, 28, 1.5, "溫柔的樂音治癒自身", "HEAL", 50};

    // --- 武僧 (R, 拳, 5) ---
    db[6][0] = {"寸勁", 1, 10, 1.7, "內力爆發", "NONE", 0};
    db[6][1] = {"昇龍拳", 3, 20, 2.2, "強勢擊飛敵人使其暈眩", "STUN", 1};
    db[6][2] = {"阿修羅霸凰拳", 5, 35, 4.8, "極限爆發傷害", "NONE", 0};
    db[6][3] = {"鐵山靠", 7, 22, 2.5, "以身軀猛烈撞擊", "NONE", 0};
    db[6][4] = {"氣功護體", 9, 24, 1.2, "聚氣護體提升防禦", "BUFF_DEF", 20};

    // --- 槍手 (R, 槍, 5) ---
    db[7][0] = {"速射", 1, 12, 1.8, "快速拔槍", "NONE", 0};
    db[7][1] = {"燃燒彈", 3, 25, 2.5, "爆炸火焰附加持續燃燒", "BURN", 20};
    db[7][2] = {"死亡彈幕", 5, 40, 4.5, "無差別掃射", "NONE", 0};
    db[7][3] = {"狙擊", 7, 28, 3.5, "瞄準要害的致命一擊", "NONE", 0};
    db[7][4] = {"麻痺彈", 9, 26, 2.2, "電擊彈使敵麻痺", "STUN", 1};

    // --- 聖騎士 (SR, 劍, 6：最後 1 絕招) ---
    db[8][0] = {"制裁之錘", 1, 15, 1.5, "聖光打擊使其暈眩", "STUN", 1};
    db[8][1] = {"神聖風暴", 3, 25, 2.0, "聖能爆發並回復生命", "HEAL", 30};
    db[8][2] = {"天降聖劍", 5, 45, 4.2, "召喚神劍，大幅提升防禦", "BUFF_DEF", 40};
    db[8][3] = {"聖盾格擋", 7, 22, 1.5, "舉起聖盾大幅提升防禦", "BUFF_DEF", 25};
    db[8][4] = {"審判之光", 9, 30, 3.2, "聖光審判敵人", "NONE", 0};
    db[8][5] = {"【絕】神聖裁決", 12, 40, 5.2, "降下神罰並治癒自身", "HEAL", 60, "劍",
        "你高舉聖劍，向天祈求神明的裁決。\n萬丈金光自雲端傾瀉而下，籠罩整個戰場——\n【神聖裁決】洗淨敵人的罪孽，同時治癒你的傷勢！"};

    // --- 死靈法師 (SR, 法杖, 6) ---
    db[9][0] = {"骨矛", 1, 14, 1.8, "射出骨矛", "NONE", 0};
    db[9][1] = {"靈魂抽取", 3, 26, 2.5, "抽取靈魂並治癒自身", "HEAL", 40};
    db[9][2] = {"亡靈大軍", 5, 50, 4.0, "召喚亡靈附加致命劇毒", "BURN", 35};
    db[9][3] = {"腐蝕詛咒", 7, 28, 2.4, "腐蝕血肉使其中毒", "BURN", 25};
    db[9][4] = {"死亡纏繞", 9, 32, 3.0, "死亡之力吸取生命", "HEAL", 50};
    db[9][5] = {"【絕】亡者君臨", 12, 48, 5.0, "召喚亡者大軍並劇毒", "BURN", 40, "法杖",
        "你詠唱禁忌的咒文，撕開了生與死的界線。\n無數亡者的枯手自地底伸出，纏住敵人——\n【亡者君臨】將對手拖入永無止盡的死亡深淵！"};

    // --- 幻術師 (SR, 法杖, 6) ---
    db[10][0] = {"心靈震爆", 1, 16, 1.6, "精神打擊", "NONE", 0};
    db[10][1] = {"鏡像破碎", 3, 28, 2.0, "引爆幻象使其混亂(麻痺)", "STUN", 1};
    db[10][2] = {"無限月讀", 5, 45, 4.6, "精神毀滅", "NONE", 0};
    db[10][3] = {"幻影迷宮", 7, 26, 2.2, "困惑敵人使其麻痺", "STUN", 1};
    db[10][4] = {"精神鞭笞", 9, 30, 3.0, "無形的精神折磨", "NONE", 0};
    db[10][5] = {"【絕】千幻現世", 12, 46, 5.0, "無數幻影同時襲擊並使敵混亂", "STUN", 1, "法杖",
        "你彈指之間，現實與幻象的界線徹底崩解。\n千百個你同時浮現，虛實難辨、真假交錯——\n【千幻現世】讓敵人在無盡的幻影中迷失、崩潰！"};

    // --- 黑暗祭司 (SR, 法杖, 6) ---
    db[11][0] = {"暗影箭", 1, 15, 1.9, "黑暗侵蝕", "NONE", 0};
    db[11][1] = {"痛苦詛咒", 3, 25, 2.0, "折磨靈魂造成強烈流血", "BURN", 25};
    db[11][2] = {"深淵降臨", 5, 45, 4.0, "開啟虛空，吸收生命", "HEAL", 80};
    db[11][3] = {"血祭", 7, 28, 2.6, "以鮮血獻祭換取力量", "NONE", 0};
    db[11][4] = {"黑暗結界", 9, 30, 1.5, "詛咒結界大幅提升防禦", "BUFF_DEF", 25};
    db[11][5] = {"【絕】墮天審判", 12, 48, 5.2, "墮落之力並吸取大量生命", "HEAL", 70, "法杖",
        "你獻上自己的血，喚醒了沉睡的墮天使。\n漆黑的羽翼緩緩展開，遮蔽了最後的光明——\n【墮天審判】吞噬敵人的生命，化為你的力量！"};

    // --- 龍騎士 (SSR, 槍, 8：最後 2 絕招) ---
    db[12][0] = {"龍之牙", 1, 20, 2.0, "巨龍撕咬", "NONE", 0};
    db[12][1] = {"真龍吐息", 3, 35, 3.0, "烈焰噴發，燒盡一切", "BURN", 40};
    db[12][2] = {"龍星群", 5, 60, 6.0, "龍王奧義，滅世傷害", "NONE", 0};
    db[12][3] = {"躍龍刺", 7, 28, 3.0, "高空俯衝的貫穿突刺", "NONE", 0};
    db[12][4] = {"龍鱗護甲", 9, 30, 1.5, "龍鱗覆體大幅提升防禦", "BUFF_DEF", 30};
    db[12][5] = {"龍威震懾", 11, 34, 3.2, "龍威震懾使敵麻痺", "STUN", 1};
    db[12][6] = {"【絕】天龍破", 14, 50, 6.0, "化身巨龍俯衝貫穿", "NONE", 0, "槍",
        "你縱身躍向高空，槍尖凝聚著龍族的怒火。\n下墜之際，你化作一頭咆哮的巨龍俯衝而下——\n【天龍破】以撕裂蒼穹之勢，將敵人一槍貫穿！"};
    db[12][7] = {"【絕】創世龍神烈波", 13, 66, 7.5, "龍神奧義，滅世烈焰", "BURN", 45, "槍",
        "你血脈中沉睡的龍神，終於甦醒了。\n天地間所有的龍焰匯聚於你的槍尖之上——\n【創世龍神烈波】化為一道毀滅世界的烈焰洪流，\n將敵人連同腳下的大地，一同焚燒殆盡！"};

    // --- 魔劍士 (SSR, 劍, 8) ---
    db[13][0] = {"魔力附魔斬", 1, 18, 1.8, "魔武雙擊，提升攻擊力", "BUFF_ATK", 10};
    db[13][1] = {"幻影劍舞", 3, 30, 3.0, "無數劍氣，提升防禦力", "BUFF_DEF", 20};
    db[13][2] = {"超究武神霸斬", 5, 50, 5.5, "極限爆發", "NONE", 0};
    db[13][3] = {"魔炎斬", 7, 28, 3.0, "附魔火焰的斬擊", "BURN", 25};
    db[13][4] = {"魔力爆發", 9, 30, 1.5, "解放魔力大幅提升攻擊", "BUFF_ATK", 22};
    db[13][5] = {"次元斬", 11, 36, 3.8, "切裂空間的一擊", "NONE", 0};
    db[13][6] = {"【絕】魔劍·真空斷", 14, 50, 6.0, "凝聚魔力的真空斬", "NONE", 0, "劍",
        "你將體內的魔力全數灌注入劍身之中。\n揮劍的軌跡撕裂了空氣本身，形成真空之刃——\n【真空斷】以肉眼不可見的鋒芒，將敵人無聲斬斷！"};
    db[13][7] = {"【絕】終焉魔神劍", 13, 64, 7.5, "魔神之力，滅世斬擊", "BURN", 40, "劍",
        "你解放了封印在劍中的魔神之力。\n漆黑的魔炎纏繞劍身，燒穿了現實的帷幕——\n【終焉魔神劍】以魔神的怒火化為滅世斬擊，\n將一切存在拖入終焉的火海之中！"};

    // --- 創造神 (EX, 3：神級特殊) ---
    db[14][0] = {"神之指", 1, 0, 10.0, "輕輕一指，灰飛煙滅", "STUN", 1};
    db[14][1] = {"神之恩典", 1, 0, 5.0, "造成傷害並完全治癒自身", "HEAL", 9999};
    db[14][2] = {"世界覆寫", 1, 0, 99.0, "直接抹除魔物的存在", "NONE", 0};

    // === 天劍聖 (SSR, 劍, 8) ===
    db[15][0] = {"斬鐵", 1, 8, 1.4, "基礎劍技", "NONE", 0, "任意", ""};
    db[15][1] = {"破甲刺", 2, 12, 1.7, "穿刺並使敵暈眩", "STUN", 1, "任意", ""};
    db[15][2] = {"烈風連斬", 3, 16, 2.0, "多段劍風", "NONE", 0, "任意", ""};
    db[15][3] = {"聖光斬", 4, 20, 2.3, "聖光傷害並提升防禦", "BUFF_DEF", 12, "任意", ""};
    db[15][4] = {"劍氣衝擊", 5, 24, 2.6, "遠程劍氣使敵暈眩", "STUN", 1, "任意", ""};
    db[15][5] = {"連環劍舞", 7, 28, 3.0, "劍舞連擊提升攻擊", "BUFF_ATK", 15, "任意", ""};
    db[15][6] = {"【絕】天照一閃", 12, 45, 5.5, "凝聚全身劍意的一閃", "NONE", 0, "劍",
        "你闔眼凝神，將畢生劍意灌注於刃尖——\n睜眼的剎那，一道縱貫天地的白光已然斬出，\n【天照一閃】撕裂空間，萬物在光中歸於寂靜！"};
    db[15][7] = {"【絕】諸神黃昏劍", 13, 60, 7.0, "召喚幻影劍雨的終極奧義", "BURN", 30, "劍",
        "天空崩裂，無數發光的幻影巨劍自虛空降臨，\n每一柄都承載著隕落諸神的怒火。\n你揮下手中之劍——【諸神黃昏】傾瀉而下，\n以毀天滅地之勢將敵人埋葬於劍雨之中！"};

    // === 破軍槍神 (SSR, 槍, 8) ===
    db[16][0] = {"刺擊", 1, 8, 1.5, "快速突刺", "NONE", 0, "任意", ""};
    db[16][1] = {"迴旋槍", 2, 12, 1.8, "橫掃一圈", "NONE", 0, "任意", ""};
    db[16][2] = {"貫穿刺", 3, 16, 2.2, "無視部分防禦的直刺", "NONE", 0, "任意", ""};
    db[16][3] = {"風雷突", 4, 20, 2.4, "雷光突刺使敵麻痺", "STUN", 1, "任意", ""};
    db[16][4] = {"亂舞連刺", 5, 24, 2.7, "疾風般的連續突刺", "NONE", 0, "任意", ""};
    db[16][5] = {"戰意昂揚", 7, 26, 1.5, "鼓舞自身大幅提升攻擊", "BUFF_ATK", 18, "任意", ""};
    db[16][6] = {"【絕】流星破軍", 12, 48, 5.8, "化作流星的貫穿突刺", "STUN", 1, "槍",
        "你俯身壓低槍尖，全身氣勢如即將墜落的流星。\n大地在腳下崩裂，你化作一道赤紅光芒暴衝而出——\n【流星破軍】洞穿一切阻擋，餘勢所及，天地為之震顫！"};
    db[16][7] = {"【絕】萬槍天穹", 13, 62, 7.2, "召喚槍陣覆蓋戰場", "BURN", 35, "槍",
        "你將長槍高舉向天，向沉睡的戰神祈願。\n剎那間，成千上萬柄星光鑄成的長槍浮現於天穹，\n隨你手臂揮落——【萬槍天穹】如暴雨傾盆而下，\n將敵人連同其立足之地一併貫穿、焚盡！"};

    // === 影襲刺客 (SR, 匕首, 6：最後 1 絕招) ===
    db[17][0] = {"背刺", 1, 8, 1.6, "從陰影突襲", "NONE", 0, "任意", ""};
    db[17][1] = {"淬毒割喉", 2, 12, 1.7, "附加中毒流血", "BURN", 10, "任意", ""};
    db[17][2] = {"影分身", 3, 16, 2.0, "殘影擾敵使其麻痺", "STUN", 1, "任意", ""};
    db[17][3] = {"致命連刺", 4, 20, 2.4, "快速連刺", "NONE", 0, "任意", ""};
    db[17][4] = {"暗殺步法", 5, 22, 1.8, "進入殺意狀態提升攻擊", "BUFF_ATK", 20, "任意", ""};
    db[17][5] = {"【絕】影殺·歿", 12, 44, 5.6, "瞬間消失後的必殺一擊", "NONE", 0, "匕首",
        "你的身影驟然融入陰影，氣息徹底消失。\n敵人驚惶四顧的瞬間，冰冷的匕首已抵住其要害——\n【影殺·歿】無聲無息，等對手察覺時，\n致命的一刀早已沒入心臟，鮮血染紅了黑夜。"};
}

// ===== 技能資料驅動化：JSON 讀寫（可編輯 skills.json 而不需改程式碼）=====
static string jsonEscape(const string& s) {
    string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c == '\r') { /* 略過 */ }
        else o += c;
    }
    return o;
}
// 解析 JSON 字串（處理 \n \t \" \\ 轉義）；i 指向開頭的 " ，結束後 i 指向結尾 " 的下一個
static string parseJsonString(const string& s, size_t& i) {
    string out; i++; // 跳過開頭 "
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n == 'n') out += '\n'; else if (n == 't') out += '\t';
            else if (n == '"') out += '"'; else if (n == '\\') out += '\\';
            else if (n == '/') out += '/'; else out += n;
            i += 2;
        } else { out += s[i]; i++; }
    }
    if (i < s.size()) i++; // 跳過結尾 "
    return out;
}
// 解析「扁平物件的陣列」，每個物件轉成 map<鍵,值字串>
static vector<map<string,string>> parseJsonArray(const string& s) {
    vector<map<string,string>> result;
    size_t i = 0;
    while (i < s.size() && s[i] != '[') i++;
    if (i < s.size()) i++;
    while (i < s.size()) {
        while (i < s.size() && (s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t'||s[i]==',')) i++;
        if (i >= s.size() || s[i] == ']') break;
        if (s[i] != '{') { i++; continue; }
        i++;
        map<string,string> obj;
        while (i < s.size() && s[i] != '}') {
            while (i < s.size() && (s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t'||s[i]==',')) i++;
            if (i >= s.size() || s[i] == '}') break;
            if (s[i] != '"') { i++; continue; }
            string key = parseJsonString(s, i);
            while (i < s.size() && (s[i]==' '||s[i]==':'||s[i]=='\n'||s[i]=='\r'||s[i]=='\t')) i++;
            string val;
            if (i < s.size() && s[i] == '"') val = parseJsonString(s, i);
            else {
                size_t st = i;
                while (i < s.size() && s[i] != ',' && s[i] != '}') i++;
                val = s.substr(st, i - st);
                while (!val.empty() && (val.back()==' '||val.back()=='\n'||val.back()=='\r'||val.back()=='\t')) val.pop_back();
            }
            obj[key] = val;
        }
        if (i < s.size()) i++;
        result.push_back(obj);
    }
    return result;
}
static int jInt(map<string,string>& o, const string& k, int def) {
    auto it = o.find(k); if (it == o.end() || it->second.empty()) return def;
    try { return stoi(it->second); } catch (...) { return def; }
}
static double jDbl(map<string,string>& o, const string& k, double def) {
    auto it = o.find(k); if (it == o.end() || it->second.empty()) return def;
    try { return stod(it->second); } catch (...) { return def; }
}
static string jStr(map<string,string>& o, const string& k, const string& def) {
    auto it = o.find(k); return it == o.end() ? def : it->second;
}
// 把目前技能資料匯出成 skills.json（首次執行用，產生可編輯的檔）
void writeSkillsToJson(const string& path, Skill db[][10]) {
    ofstream f(path);
    if (!f) return;
    f << "[\n";
    bool first = true;
    for (int j = 0; j < 18; j++)
        for (int s = 0; s < 10; s++) {
            if (db[j][s].name.empty()) continue;
            Skill& k = db[j][s];
            if (!first) f << ",\n"; first = false;
            f << "{\"job\":" << j << ",\"slot\":" << s
              << ",\"name\":\"" << jsonEscape(k.name) << "\""
              << ",\"reqLevel\":" << k.reqLevel << ",\"mpCost\":" << k.mpCost
              << ",\"mult\":" << k.damageMult
              << ",\"effect\":\"" << jsonEscape(k.effect) << "\""
              << ",\"attribute\":\"" << jsonEscape(k.attribute) << "\""
              << ",\"attrValue\":" << k.attrValue
              << ",\"reqWeapon\":\"" << jsonEscape(k.reqWeapon) << "\""
              << ",\"flavor\":\"" << jsonEscape(k.flavor) << "\"}";
        }
    f << "\n]\n";
}
// 從 skills.json 讀取並覆蓋技能資料；成功回傳 true
bool loadSkillsFromJson(const string& path, Skill db[][10]) {
    ifstream f(path);
    if (!f) return false;
    stringstream ss; ss << f.rdbuf();
    string content = ss.str();
    vector<map<string,string>> arr = parseJsonArray(content);
    if (arr.empty()) return false;
    for (auto& o : arr) {
        int j = jInt(o, "job", -1), s = jInt(o, "slot", -1);
        if (j < 0 || j >= 18 || s < 0 || s >= 10) continue;
        db[j][s].name      = jStr(o, "name", "");
        db[j][s].reqLevel  = jInt(o, "reqLevel", 1);
        db[j][s].mpCost    = jInt(o, "mpCost", 0);
        db[j][s].damageMult= jDbl(o, "mult", 1.0);
        db[j][s].effect    = jStr(o, "effect", "");
        db[j][s].attribute = jStr(o, "attribute", "NONE");
        db[j][s].attrValue = jInt(o, "attrValue", 0);
        db[j][s].reqWeapon = jStr(o, "reqWeapon", "");
        db[j][s].flavor    = jStr(o, "flavor", "");
    }
    return true;
}

// ===== 內建預設資料（安全網；首次執行會匯出成 JSON 供編輯）=====
void initDefaultWeapons() {
    gWeaponPool = {
        { {"精鋼劍",   "Weapon", 15, 3,  "無", 0,  "劍" },   80,   1 },
        { {"淬毒匕首", "Weapon", 25, 8,  "毒", 12, "匕首" }, 250,  1 },
        { {"烈焰長劍", "Weapon", 30, 5,  "火", 15, "劍" },   320,  1 },
        { {"寒冰之刃", "Weapon", 45, 6,  "冰", 0,  "劍" },   420,  2 },
        { {"雷神之槍", "Weapon", 55, 7,  "雷", 0,  "槍" },   520,  2 },
        { {"獄炎巨劍", "Weapon", 60, 5,  "火", 30, "巨劍" }, 560,  2 },
        { {"秘銀聖劍", "Weapon", 85, 10, "無", 0,  "劍" },   720,  3 },
        { {"死亡鐮刀", "Weapon", 95, 8,  "毒", 40, "鐮" },   850,  3 },
        { {"雷霆審判", "Weapon", 110,10, "雷", 0,  "槍" },   1050, 3 },
        { {"疾影短刀", "Weapon", 40, 12, "無", 0,  "匕首" }, 380,  2 },
        { {"破軍龍槍", "Weapon", 100,9,  "雷", 0,  "槍" },   980,  3 },
        { {"星光法杖", "Weapon", 35, 4,  "無", 0,  "法杖" }, 350,  1 },
        { {"烈焰法杖", "Weapon", 55, 5,  "火", 20, "法杖" }, 560,  2 },
        { {"死靈法杖", "Weapon", 95, 8,  "毒", 30, "法杖" }, 980,  3 },
        { {"精靈之弓", "Weapon", 50, 10, "無", 0,  "弓" },   480,  2 }
    };
}
void initDefaultArmors() {
    gArmorPool = {
        { {"鐵甲",       "Armor", 20, 3,  "無",   0, "" },  90,   1 },
        { {"荊棘背心",   "Armor", 15, 2,  "反傷", 25,"" }, 280,  1 },
        { {"厚重板甲",   "Armor", 40, 0,  "減傷", 15,"" }, 340,  1 },
        { {"精鋼鎧",     "Armor", 60, 4,  "無",   0, "" }, 450,  2 },
        { {"復仇之盾甲", "Armor", 45, 3,  "反傷", 40,"" }, 600,  2 },
        { {"龍鱗甲",     "Armor", 80, 2,  "減傷", 25,"" }, 700,  2 },
        { {"聖光鎧",     "Armor", 120,6,  "無",   0, "" }, 900,  3 },
        { {"荊棘魔鎧",   "Armor", 90, 4,  "反傷", 60,"" }, 1100, 3 },
        { {"不滅神甲",   "Armor", 150,3,  "減傷", 35,"" }, 1300, 3 }
    };
}
void initDefaultMonsters() {
    gMonsters[0] = {"綠色史萊姆", "森林毒蛛", "幼小妖狐", "狂暴野豬", "劇毒花妖", "迷霧樹精", "哥布林斥候"};
    gMonsters[1] = {"幽靈骷髏", "怨恨殭屍", "詛咒石像鬼", "地底岩魔", "吸血蝙蝠群", "黑暗騎士"};
    gMonsters[2] = {"憤怒的半人馬", "墮落巨魔", "地獄雙頭犬", "深淵惡魔", "烈焰魅魔", "死亡使者"};
}

// ---- 裝備商品池 JSON ----
void writeShopItemsJson(const string& path, vector<ShopItem>& pool, const string& type) {
    ofstream f(path); if (!f) return;
    f << "[\n";
    for (size_t i = 0; i < pool.size(); i++) {
        Equipment& e = pool[i].eq;
        if (i) f << ",\n";
        f << "{\"name\":\"" << jsonEscape(e.name) << "\",\"type\":\"" << type << "\""
          << ",\"value\":" << e.value << ",\"subValue\":" << e.subValue
          << ",\"element\":\"" << jsonEscape(e.element) << "\",\"elemValue\":" << e.elemValue
          << ",\"wclass\":\"" << jsonEscape(e.wclass) << "\""
          << ",\"price\":" << pool[i].price << ",\"minCh\":" << pool[i].minCh << "}";
    }
    f << "\n]\n";
}
bool loadShopItemsJson(const string& path, vector<ShopItem>& pool, const string& type) {
    ifstream f(path); if (!f) return false;
    stringstream ss; ss << f.rdbuf();
    vector<map<string,string>> arr = parseJsonArray(ss.str());
    if (arr.empty()) return false;
    pool.clear();
    for (auto& o : arr) {
        ShopItem it;
        it.eq.name = jStr(o, "name", "?");
        it.eq.type = jStr(o, "type", type);
        it.eq.value = jInt(o, "value", 0);
        it.eq.subValue = jInt(o, "subValue", 0);
        it.eq.element = jStr(o, "element", "無");
        it.eq.elemValue = jInt(o, "elemValue", 0);
        it.eq.wclass = jStr(o, "wclass", "");
        it.price = jInt(o, "price", 100);
        it.minCh = jInt(o, "minCh", 1);
        pool.push_back(it);
    }
    return true;
}

// ---- 職業 JSON ----
void writeJobsJson(const string& path) {
    ofstream f(path); if (!f) return;
    f << "[\n";
    for (int i = 0; i < JOB_COUNT; i++) {
        JobTemplate& j = jobDB[i];
        if (i) f << ",\n";
        f << "{\"name\":\"" << jsonEscape(j.name) << "\",\"rarity\":\"" << jsonEscape(j.rarity) << "\""
          << ",\"hp\":" << j.hp << ",\"atk\":" << j.atk << ",\"mp\":" << j.mp << ",\"regen\":" << j.regen
          << ",\"agi\":" << j.agiBonus << ",\"luk\":" << j.lukBonus
          << ",\"desc\":\"" << jsonEscape(j.desc) << "\",\"weaponClass\":\"" << jsonEscape(j.weaponClass) << "\"}";
    }
    f << "\n]\n";
}
bool loadJobsJson(const string& path) {
    ifstream f(path); if (!f) return false;
    stringstream ss; ss << f.rdbuf();
    vector<map<string,string>> arr = parseJsonArray(ss.str());
    if (arr.empty()) return false;
    int n = min((int)arr.size(), JOB_COUNT);
    for (int i = 0; i < n; i++) {
        jobDB[i].name = jStr(arr[i], "name", jobDB[i].name);
        jobDB[i].rarity = jStr(arr[i], "rarity", jobDB[i].rarity);
        jobDB[i].hp = jInt(arr[i], "hp", jobDB[i].hp);
        jobDB[i].atk = jInt(arr[i], "atk", jobDB[i].atk);
        jobDB[i].mp = jInt(arr[i], "mp", jobDB[i].mp);
        jobDB[i].regen = jInt(arr[i], "regen", jobDB[i].regen);
        jobDB[i].agiBonus = jInt(arr[i], "agi", jobDB[i].agiBonus);
        jobDB[i].lukBonus = jInt(arr[i], "luk", jobDB[i].lukBonus);
        jobDB[i].desc = jStr(arr[i], "desc", jobDB[i].desc);
        jobDB[i].weaponClass = jStr(arr[i], "weaponClass", jobDB[i].weaponClass);
    }
    return true;
}

// ---- 魔物 JSON ----
void writeMonstersJson(const string& path) {
    ofstream f(path); if (!f) return;
    f << "[\n";
    bool first = true;
    for (int c = 0; c < 3; c++)
        for (size_t i = 0; i < gMonsters[c].size(); i++) {
            if (!first) f << ",\n"; first = false;
            f << "{\"chapter\":" << (c + 1) << ",\"name\":\"" << jsonEscape(gMonsters[c][i]) << "\"}";
        }
    f << "\n]\n";
}
bool loadMonstersJson(const string& path) {
    ifstream f(path); if (!f) return false;
    stringstream ss; ss << f.rdbuf();
    vector<map<string,string>> arr = parseJsonArray(ss.str());
    if (arr.empty()) return false;
    vector<string> tmp[3];
    for (auto& o : arr) {
        int c = jInt(o, "chapter", 1) - 1;
        if (c < 0 || c > 2) continue;
        string nm = jStr(o, "name", "");
        if (!nm.empty()) tmp[c].push_back(nm);
    }
    for (int c = 0; c < 3; c++) if (!tmp[c].empty()) gMonsters[c] = tmp[c]; // 非空才覆蓋，避免整章空掉
    return true;
}

// 統一載入所有資料：先套內建預設，再以存在的 JSON 覆蓋，不存在則匯出
void loadAllGameData(Skill skillDB[][10]) {
    initAllSkills(skillDB); initDefaultWeapons(); initDefaultArmors(); initDefaultMonsters();
    ifstream t1("skills.json");  if (t1.good()) { t1.close(); loadSkillsFromJson("skills.json", skillDB); } else { t1.close(); writeSkillsToJson("skills.json", skillDB); }
    ifstream t2("jobs.json");    if (t2.good()) { t2.close(); loadJobsJson("jobs.json"); }             else { t2.close(); writeJobsJson("jobs.json"); }
    ifstream t3("weapons.json"); if (t3.good()) { t3.close(); loadShopItemsJson("weapons.json", gWeaponPool, "Weapon"); } else { t3.close(); writeShopItemsJson("weapons.json", gWeaponPool, "Weapon"); }
    ifstream t4("armors.json");  if (t4.good()) { t4.close(); loadShopItemsJson("armors.json", gArmorPool, "Armor"); }   else { t4.close(); writeShopItemsJson("armors.json", gArmorPool, "Armor"); }
    ifstream t5("monsters.json");if (t5.good()) { t5.close(); loadMonstersJson("monsters.json"); }     else { t5.close(); writeMonstersJson("monsters.json"); }
}

// 🏁 結算畫面（通關或陣亡都用）：顯示戰績與評價
void showResultScreen(Character& p, int wave, int gold, bool won) {
    clearScreen();
    string rank;
    if (won) rank = (gTurns <= 40 ? "S" : "A");
    else if (wave >= 8) rank = "B";
    else if (wave >= 5) rank = "C";
    else rank = "D";
    const char* rc = (rank == "S") ? C_YELLOW : (rank == "A" ? C_MAGENTA : (rank == "B" ? C_CYAN : C_GRAY));

    cout << (won ? C_YELLOW : C_RED);
    cout << "╔═══════════════════════════════════════════════╗\n";
    if (won) cout << "║        🏆  通  關  成  功  ！  🏆             ║\n";
    else     cout << "║           💀  遊  戲  結  束  💀              ║\n";
    cout << "╚═══════════════════════════════════════════════╝" << C_RESET << "\n\n";

    if (won) cout << C_GREEN << " 你擊敗了遠古滅世巨龍，奪回聖物，黑夜終於散去。\n 大陸的人們將永遠傳頌你的名字——\n" << C_RESET << "\n";
    else     cout << C_GRAY << " 勇者 " << p.name << " 的冒險在此止步...\n 但傳說不會就此結束，讀取存檔再戰一次吧。\n" << C_RESET << "\n";

    cout << C_CYAN << "─────────────── 📊 最終戰績 ───────────────" << C_RESET << "\n";
    cout << "  勇者：" << C_BOLD << p.name << C_RESET << "  (" << p.job << ")  Lv." << p.level << "\n";
    cout << "  推進波數：" << (won ? 10 : wave) << " / 10\n";
    cout << "  擊殺魔物：" << gKills << " 隻\n";
    cout << "  累計傷害：" << C_RED << gTotalDamage << C_RESET << "\n";
    cout << "  最大單擊：" << C_RED << C_BOLD << gMaxHit << C_RESET << "\n";
    cout << "  絕招發動：" << C_MAGENTA << gUltUsed << C_RESET << " 次\n";
    cout << "  總回合數：" << gTurns << "\n";
    cout << "  持有金幣：" << C_YELLOW << gold << C_RESET << "\n";
    cout << "  最終裝備：" << p.weapon.name << " ／ " << p.armor.name << "\n";
    cout << C_CYAN << "───────────────────────────────────────────" << C_RESET << "\n";
    cout << "            評價： " << rc << C_BOLD << "【 " << rank << " 】" << C_RESET << "\n";
    cout << C_CYAN << "───────────────────────────────────────────" << C_RESET << "\n";
    waitPlayer();
}

int rollGachaJob() {
    int roll = rand() % 100;
    vector<int> pool;
    // 🩸 英靈縫合術：從地下深淵挖出歷代慘死勇者的殘魂，刺進你的脊椎
    cout << "\n🕯️ 教會將一枚枚泛黃的靈魂碎片壓入你的脊骨……\n";
    if (roll < 3) { pool = {12, 13, 15, 16}; cout << "🌟🌟🌟 金光炸裂！一縷『傳說級英靈』的殘魂與你縫合了！(SSR)\n"; }
    else if (roll < 18) { pool = {8, 9, 10, 11, 17}; cout << "✨ 紫燄纏繞！一名『稀有英靈』被強行嫁接上身。(SR)\n"; }
    else if (roll < 48) { pool = {3, 6, 7}; cout << "🔵 藍光一閃，一段陌生的死亡記憶湧入腦海。(R)\n"; }
    else { pool = {0, 1, 2, 4, 5}; cout << "⚪ 一縷無名亡魂沉入你的體內，隱隱傳來啜泣。(N)\n"; }
    cout << C_GRAY << "  [系統標記] 已將 1 具勇者殘魂縫合至宿主靈魂，精神污染度上升。\n" << C_RESET;
    return pool[rand() % pool.size()];
}

int main() {
    #ifdef _WIN32
    static bool initMenu = [](){ system("chcp 65001 > nul"); return true; }();
    enableAnsiColors(); // 🎨 啟用顏色
    #endif
    srand(time(0));

    Skill skillDatabase[18][10];
    // 🌟 資料驅動：載入內建預設，再以 skills/jobs/weapons/armors/monsters.json 覆蓋（不存在則自動匯出）
    loadAllGameData(skillDatabase);

    string playerName;
    Character player("TEMP", "劍士", 100, 20, 30, 5);
    int gold = 30, potions = 1, wave = 1;
    bool isLoaded = false;
    int currentSaveSlot = 1;

    while (true) {
        clearScreen();
        cout << "=========================================\n       ⚔️  勇者傳說：宿命之絆 ⚔️         \n=========================================\n";
        cout << "          [ 1 ] 🟢 建立新角色 (新冒險)\n";
        cout << "          [ 2 ] 💾 選擇存檔 (讀取進度)\n";
        cout << "          [ 3 ] 📜 觀看職業與魔物圖鑑\n";
        cout << "          [ 4 ] 📖 遊戲簡介 (故事與玩法)\n";
        cout << "          [ 6 ] 🩸 真相檔案 (完整劇情揭密)\n";
        cout << "          [ 5 ] ❌ 離開遊戲\n=========================================\n請選擇 (1-6): ";
        int menuChoice; if (!(cin >> menuChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');

        if (menuChoice == 5) return 0;
        else if (menuChoice == 6) {
            clearScreen();
            cout << C_RED << "═══════════════════════════════════════════════\n";
            cout << "     🩸 真 相 檔 案 ── 太陽之核的秘密 🩸\n";
            cout << "═══════════════════════════════════════════════\n" << C_RESET;
            cout << C_GRAY << "（警告：以下內容將完整揭露本作的黑暗真相。\n 若想在遊玩中親自體驗剝洋蔥的過程，請按 Enter 返回。）\n\n" << C_RESET;
            cout << "這個世界的魔法，並非自然的饋贈——\n而是懸在天空的『太陽之核』散播的寄生能量。\n所謂『成為勇者、突破階位、終成神明』的王道傳說，\n實則是一層層把活人養成高純度電池的養殖流程。\n\n";
            const char* rn[5] = {"微光者","銘刻者","結晶者","共鳴者","受冕者"};
            const char* rg[5] = {
                "覺醒魔力感應，被教會稱為『受神眷顧的雛鳥』。",
                "在靈魂表面刻下魔力迴路，成為正式法師。",
                "心臟凝聚出『魔導原核』，壽命延長、魔力近乎無限。",
                "能召喚古代英靈疊加自身，施展毀滅級禁咒。",
                "肉身化為半神光體，被尊為『神的地上代理人』。"};
            const char* rt[5] = {
                "吸入肺腑的魔力，是太陽之核碾碎人類靈魂的精神粉塵。",
                "迴路是植入體內、監視你思想的『生物神經汲取網』。",
                "原核是『熟透的結晶化靈魂』，你已被列為收割燃料。",
                "英靈是慘死勇者的殘魂，被縫進你的靈魂替你承受反噬。",
                "你已失去人的主權，成為太陽之核的實體接收終端。"};
            for (int i = 0; i < 5; i++) {
                cout << C_CYAN << "── 第 " << (i+1) << " 階：" << rn[i] << " ──\n" << C_RESET;
                cout << C_YELLOW << " 表面：" << rg[i] << "\n" << C_RESET;
                cout << C_RED << " 真相：" << rt[i] << "\n\n" << C_RESET;
            }
            cout << C_MAGENTA << "── 命運抽卡的真相 ──\n" << C_RESET;
            cout << " 那不是幸運，是『英靈縫合術』——把慘死勇者的殘魂刺進你的脊椎。\n\n";
            cout << C_MAGENTA << "── 靈魂溶劑的真相 ──\n" << C_RESET;
            cout << " 恢復藥水，是同胞的靈魂榨取物，用來壓制你腦中殘魂的尖叫。\n\n";
            cout << C_MAGENTA << "── 隱藏職【創造神】的真相 ──\n" << C_RESET;
            cout << " 這整個世界，是造物主設計的封閉沙盒。\n 而選擇創造神的你，就是那位俯瞰一切的造物主本身。\n";
            cout << C_RED << "═══════════════════════════════════════════════\n" << C_RESET;
            waitPlayer(); continue;
        }
        else if (menuChoice == 4) {
            clearScreen();
            cout << "=========================================\n";
            cout << "        📖 遊戲簡介 ── 勇者傳說：宿命之絆        \n";
            cout << "=========================================\n";
            cout << "【故事背景】\n";
            cout << " 守護大陸的聖物遭到竊取，黑夜隨之降臨，\n";
            cout << " 魔物自迷霧森林湧出，吞噬著人們的希望。\n";
            cout << " 身為被命運選中的勇者，你必須穿越三大章節，\n";
            cout << " 一路討伐魔物，最終擊敗【遠古滅世巨龍】，奪回聖物！\n\n";
            cout << "【玩法說明】\n";
            cout << " ⚔️ 波次戰鬥：以回合制迎戰一波波魔物，共 10 波。\n";
            cout << " 🌟 職業系統：17 種職業 + 隱藏職，可自選或命運抽卡 (含 SSR)；新職有 8 技能 + 2 絕招。\n";
            cout << " 💪 屬性配點：升級獲得點數，投資力量/體質/敏捷/幸運。\n";
            cout << " 🔮 技能連招：每個職業有 3 招專屬技能，隨等級解鎖。\n";
            cout << " 🧪 狀態效果：點燃、中毒、暈眩、吸血、攻防增益等。\n";
            cout << " 🏪 流浪商人：每逢偶數波可補給藥水並儲存進度。\n\n";
            cout << "【三大章節】\n";
            cout << " 第一章 🌲 迷霧森林   → BOSS 👑 哥布林酋長\n";
            cout << " 第二章 💀 地下墓穴   → BOSS 🔮 不死巫妖王\n";
            cout << " 第三章 🌋 終焉戰場   → BOSS 🐉 遠古滅世巨龍\n\n";
            cout << "【操作方式】\n";
            cout << " 依畫面提示輸入對應數字選擇行動，戰鬥中可\n";
            cout << " 1.普通攻擊  2.使用技能  3.喝藥水。祝你好運，勇者！\n";
            cout << "=========================================\n";
            waitPlayer(); continue;
        }
        else if (menuChoice == 3) {
            clearScreen();
            cout << "================= 📜 魔物情報與圖鑑 =================\n";
            cout << "【第一章：迷霧森林】(專屬特技：毒液飛濺，附加持續中毒)\n";
            cout << "一般魔物："; for(auto& m : gMonsters[0]) cout << m << " | "; cout << "\n鎮區守衛：👑 哥布林酋長\n\n";

            cout << "【第二章：地下墓穴】(專屬特技：生命汲取，傷害轉化回復)\n";
            cout << "一般魔物："; for(auto& m : gMonsters[1]) cout << m << " | "; cout << "\n鎮區守衛：🔮 不死巫妖王\n\n";

            cout << "【第三章：終焉戰場】(專屬特技：粉碎重擊，無視並破壞防禦狀態)\n";
            cout << "一般魔物："; for(auto& m : gMonsters[2]) cout << m << " | "; cout << "\n鎮區守衛：🐉 遠古滅世巨龍\n\n";

            cout << "-----------------------------------------------------\n";
            cout << "【職業圖鑑】(14 職 + 3 新職 + 1 隱藏職)\n";
            for(int i=0; i<14; i++) {
                cout << " [" << jobDB[i].rarity << "] " << jobDB[i].name << " : " << jobDB[i].desc << "\n";
            }
            cout << " [EX] ??? : 傳說中掌握世界代碼的造物主...\n";
            cout << " -- 🆕 新職業（每職 8 技能 + 2 絕招，絕招需對應武器）--\n";
            for(int i=15; i<18; i++) {
                cout << " [" << jobDB[i].rarity << "] " << jobDB[i].name << " (專武:" << jobDB[i].weaponClass << ") : " << jobDB[i].desc << "\n";
            }
            waitPlayer(); continue;
        }
        else if (menuChoice == 2) {
            clearScreen();
            cout << "================ 💾 讀取遊戲 =================\n";
            for (int i = 1; i <= 3; i++) peekSaveSlot(i);
            cout << " [4] ↩️ 返回主選單\n----------------------------------------------\n";
            cout << "請選擇要讀取的檔號 (1-4): ";
            int slotChoice;
            if (!(cin >> slotChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            cin.ignore(1000, '\n');
            if (slotChoice == 4) continue;

            if (slotChoice >= 1 && slotChoice <= 3) {
                ifstream inFile(getSaveFileName(slotChoice));
                if (!inFile) { cout << "❌ 該檔號沒有存檔紀錄！\n"; waitPlayer(); continue; }
                getline(inFile, player.name); // 名字可能含空格，需用 getline（職業名無空格用 >> 即可）
                inFile >> player.job >> player.level >> player.exp >> player.maxExp
                       >> player.statPoints >> player.baseHp >> player.hp >> player.baseAtk
                       >> player.mp >> player.maxMp >> player.mpRegen
                       >> player.strength >> player.constitution >> player.agility >> player.lucky
                       >> gold >> potions >> wave;
                inFile.ignore();
                getline(inFile, player.weapon.name); inFile >> player.weapon.value >> player.weapon.subValue; inFile.ignore();
                getline(inFile, player.armor.name); inFile >> player.armor.value >> player.armor.subValue;
                // 武器/防具元素、技能等級、武器類型（舊存檔沒有這些欄，讀取失敗時保持預設）
                player.weapon.element = "無"; player.weapon.elemValue = 0;
                player.armor.element = "無"; player.armor.elemValue = 0;
                for (int i = 0; i < 10; i++) player.skillLevel[i] = 1;
                // 依職業預設武器類型（舊檔沒存 wclass 時的合理值）
                player.weapon.wclass = jobDB[getJobIndex(player.job)].weaponClass;
                if (player.weapon.wclass.empty()) player.weapon.wclass = "劍";
                inFile.ignore();
                if (getline(inFile, player.weapon.element)) {
                    if (player.weapon.element.empty()) player.weapon.element = "無";
                    inFile >> player.weapon.elemValue; inFile.ignore();
                    if (getline(inFile, player.armor.element)) {
                        if (player.armor.element.empty()) player.armor.element = "無";
                        inFile >> player.armor.elemValue;
                        for (int i = 0; i < 10; i++) if (!(inFile >> player.skillLevel[i])) break;
                        inFile.ignore();
                        string wc; if (getline(inFile, wc) && !wc.empty() && wc != "無") player.weapon.wclass = wc;
                    }
                }
                inFile.close();
                player.updateStats();
                isLoaded = true; currentSaveSlot = slotChoice;
                cout << "\n🎉 歡迎回來，" << player.name << "！存檔載入成功，目前位於第 " << wave << " 波。\n";
                waitPlayer(); break;
            }
        }
        else if (menuChoice == 1) {
            clearScreen();
            cout << "================ 🟢 建立新存檔 =================\n";
            for (int i = 1; i <= 3; i++) peekSaveSlot(i);
            cout << " [4] ↩️ 返回主選單\n----------------------------------------------\n";
            cout << "請選擇要儲存新遊戲的檔號 (覆蓋舊檔) (1-4): ";
            int slotChoice; if (!(cin >> slotChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            cin.ignore(1000, '\n');
            if (slotChoice == 4) continue;
            if (slotChoice >= 1 && slotChoice <= 3) { currentSaveSlot = slotChoice; break; }
        }
    }

    if (!isLoaded) {
        clearScreen();
        cout << " 請輸入你的勇者名字 (輸入 !!!!! 觸發上帝模式，或 !!!!!!WW 呂 啟動控制台): ";
        getline(cin, playerName);

        int selectedJobIdx = 0;
        if (playerName == "!!!!!") {
            playerName = "✨上帝WW✨"; selectedJobIdx = 14; gold = 9999; potions = 99;
            cout << "\n⚠️ 系統警告：【上帝模式】已啟用。\n"; waitPlayer();
        } else if (playerName == "!!!!!!WW 呂") {
            playerName = "⚙️系統管理員 呂⚙️"; selectedJobIdx = 14; gold = 9999; potions = 99;
            cout << "\n⚠️ 系統警告：最高權限已驗證。啟動【開發者控制模式】。\n"; waitPlayer();
        } else {
            while (true) {
                clearScreen();
                cout << "=========================================\n        🌟 選擇或抽取你的傳奇職業 🌟         \n=========================================\n";
                cout << " [ 1 ] 🛡️ 劍士   (基礎)\n [ 2 ] 🔮 魔法師 (基礎)\n [ 3 ] 🏹 刺客   (基礎)\n";
                cout << " [ 4 ] 🎲 命運抽卡 (抽取隨機職業，含稀有 SSR 隱藏職業！)\n";
                cout << " -- 🆕 新職業試作（8 技能 + 2 絕招）--\n";
                cout << " [ 5 ] ⚔️ 天劍聖   (劍/SSR)\n [ 6 ] 🔱 破軍槍神 (槍/SSR)\n [ 7 ] 🗡️ 影襲刺客 (匕首/SR)\n";
                cout << "=========================================\n請選擇 (1-7): ";
                int jobChoice; if (!(cin >> jobChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
                cin.ignore(1000, '\n');
                if (jobChoice >= 1 && jobChoice <= 3) { selectedJobIdx = jobChoice - 1; break; }
                else if (jobChoice >= 5 && jobChoice <= 7) { selectedJobIdx = 15 + (jobChoice - 5); break; }
                else if (jobChoice == 4) {
                    selectedJobIdx = rollGachaJob();
                    cout << " 🎉 恭喜！你抽中了【" << jobDB[selectedJobIdx].rarity << "】級職業：「" << jobDB[selectedJobIdx].name << "」！\n";
                    cout << " 簡介：" << jobDB[selectedJobIdx].desc << "\n";
                    waitPlayer(); break;
                }
            }
        }

        JobTemplate jt = jobDB[selectedJobIdx];
        player = Character(playerName, jt.name, jt.hp, jt.atk, jt.mp, jt.regen);
        player.agility += jt.agiBonus; player.lucky += jt.lukBonus;
        // 新職業：起始武器改為對應類型，方便施展絕招
        if (!jt.weaponClass.empty() && selectedJobIdx != 14) {
            player.weapon.wclass = jt.weaponClass;
            player.weapon.name = "見習" + jt.weaponClass;
        }

        if (selectedJobIdx == 14) {
            player.weapon = {"造物主的指令", "Weapon", 500, 100, "雷", 999, "萬能"};
            player.armor = {"系統防火牆", "Armor", 5000, 100, "無", 0, ""};
        }
        player.updateStats(); player.hp = player.maxHp;

        clearScreen();
        if (selectedJobIdx == 14) cout << "📜 【序章：降維打擊】\n身為 " << player.job << " 的你，帶著破壞平衡的屬性降臨了迷霧森林...\n";
        else cout << "📜 【序章：命運的開端】\n「聖物遭竊，黑夜降臨。」身為 " << player.job << " 的你，踏入了迷霧森林...\n";
        waitPlayer();
    }

    int jobIndex = getJobIndex(player.job);

    while (player.isAlive()) {
        bool isControlMode = (player.name == "⚙️系統管理員 呂⚙️");

        while (player.statPoints > 0) {
            clearScreen();
            cout << C_CYAN << "=========================================\n        💪 英雄聖殿：配點分配 💪         \n=========================================" << C_RESET << "\n";
            cout << " 剩餘可用點數: " << C_YELLOW << C_BOLD << "✨ " << player.statPoints << " 點" << C_RESET << "\n-----------------------------------------\n";
            cout << " [ 1 ] ⚔️ 力量 (目前: " << player.strength << ") -> 增加總攻擊力\n [ 2 ] 🛡️ 體質 (目前: " << player.constitution << ") -> 增加最大生命值\n [ 3 ] ⚡ 敏捷 (目前: " << player.agility << ") -> 增加 1% 閃避率\n [ 4 ] 🍀 幸運 (目前: " << player.lucky << ") -> 增加 1% 暴擊率\n";
            cout << " [ 5 ] ⚡ 一鍵平均分配剩餘全部點數\n";
            cout << C_CYAN << "=========================================" << C_RESET << "\n請選擇 (1-5): ";
            int statChoice; if (!(cin >> statChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            cin.ignore(1000, '\n');
            if (statChoice == 5) { // 平均分配，餘數給力量
                int each = player.statPoints / 4, rem = player.statPoints % 4;
                player.strength += each + rem; player.constitution += each; player.agility += each; player.lucky += each;
                player.statPoints = 0; player.updateStats();
                cout << C_GREEN << "✅ 已平均分配完畢！" << C_RESET << "\n"; waitPlayer(); continue;
            }
            if (statChoice < 1 || statChoice > 4) { cout << C_RED << "❌ 請輸入 1~5！" << C_RESET << "\n"; waitPlayer(); continue; }
            cout << "👉 要投入幾點？(1~" << player.statPoints << "，直接按 Enter = 全部投入): ";
            string line; getline(cin, line);
            int n = player.statPoints; // 預設全投
            if (!line.empty()) { try { n = stoi(line); } catch (...) { n = 1; } }
            if (n < 1) n = 1; if (n > player.statPoints) n = player.statPoints; // 夾在合法範圍
            if (statChoice == 1) player.strength += n; else if (statChoice == 2) player.constitution += n;
            else if (statChoice == 3) player.agility += n; else if (statChoice == 4) player.lucky += n;
            player.statPoints -= n; player.updateStats();
        }

        if (wave > 1 && wave % 2 == 0) {
            bool inShop = true;
            while (inShop) {
                clearScreen();
                cout << "=================================\n         🏪 流浪商人營地         \n=================================\n";
                cout << " 金幣: ✨ " << gold << " | 藥水: 🧪 " << potions << " | 勇者等級: Lv." << player.level << "\n---------------------------------\n";
                int shopChapter = (wave <= 3) ? 1 : (wave <= 6 ? 2 : 3);
                cout << "1. 🧪 購買【靈魂過濾溶劑】(💰 20 金幣)\n2. 🗡️ 逛武器鍛造鋪\n3. 🛡️ 逛防具鎧甲鋪\n4. 💾 儲存目前遊戲進度\n5. 🏃 離開營地繼續前進\n";
                if (isControlMode) cout << "0. ⚙️ 開啟控制台 (修改數值)\n";
                cout << "請選擇: ";
                int shopChoice; if (!(cin >> shopChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
                cin.ignore(1000, '\n');

                if (shopChoice == 1) {
                    if (gold >= 20) { gold -= 20; potions++; cout << "🛒 交易成功。\n" << C_GRAY << "  （溶劑在瓶中緩緩蠕動，彷彿還記得自己曾是誰。）\n" << C_RESET; } else { cout << "❌ 金幣不足！\n"; } waitPlayer();
                }
                else if (shopChoice == 2) { weaponShop(player, gold, shopChapter); }
                else if (shopChoice == 3) { armorShop(player, gold, shopChapter); }
                else if (shopChoice == 4) { saveGame(player, gold, potions, wave, currentSaveSlot); }
                else if (shopChoice == 5) { inShop = false; }
                else if (isControlMode && shopChoice == 0) { int dummy = -1; adminPanel(player, gold, potions, wave, dummy, dummy, "", skillDatabase); jobIndex = getJobIndex(player.job); }
            }
        }

        string currentChapter = ""; string mName = ""; int mHp = 50, mAtk = 10;
        bool isFinalBoss = false; // 是否為第 10 波最終滅世巨龍（只有擊敗它才算通關）
        if (wave <= 3) {
            currentChapter = "第一章：🌲 迷霧森林";
            if (wave == 3) { mName = "👑 【章節BOSS】哥布林酋長"; mHp = 170; mAtk = 24; }
            else { mName = gMonsters[0][rand() % gMonsters[0].size()] + " (Lv." + to_string(wave) + ")"; mHp = 50 + (wave * 15); mAtk = 11 + (wave * 2); }
        }
        else if (wave <= 6) {
            currentChapter = "第二章：💀 地下墓穴";
            if (wave == 6) { mName = "🔮 【章節BOSS】不死巫妖王"; mHp = 380; mAtk = 38; }
            else { mName = gMonsters[1][rand() % gMonsters[1].size()] + " (Lv." + to_string(wave) + ")"; mHp = 110 + (wave * 22); mAtk = 18 + (wave * 3); }
        }
        else {
            currentChapter = "第三章：🌋 終焉戰場";
            if (wave >= 10) { mName = "🐉 【最終滅世BOSS】遠古滅世巨龍"; mHp = 900; mAtk = 68; isFinalBoss = true; } // 第10波(含以後)都是最終BOSS，確保遊戲會結束
            else { mName = gMonsters[2][rand() % gMonsters[2].size()] + " (Lv." + to_string(wave) + ")"; mHp = 220 + (wave * 26); mAtk = 28 + (wave * 4); }
        }

        int mHpMax = mHp; // 怪物血量上限（吸血回復不得超過）
        // 🌟 狀態變數 (包含玩家中毒)
        int mBurnDuration = 0, mBurnDamage = 0; bool mStunned = false;
        int pBurnDuration = 0, pBurnDamage = 0; // 玩家中毒/燃燒狀態
        int pAtkBuff = 0, pDefBuff = 0;

        while (player.isAlive() && mHp > 0) {
            clearScreen();
            cout << C_CYAN << "═════════════════════════════════════════════════" << C_RESET << "\n";
            cout << " 🗺️ " << C_CYAN << currentChapter << C_RESET << "  " << C_GRAY << "(第 " << wave << " 波)" << C_RESET << "\n";
            cout << C_CYAN << "═════════════════════════════════════════════════" << C_RESET << "\n";
            cout << " " << C_BOLD << "【" << player.name << "】" << C_RESET << C_GRAY << "(" << player.job << ") Lv." << player.level
                 << C_RESET << C_MAGENTA << " ﹝" << getRankName(player.level) << "﹞" << C_RESET
                 << C_GRAY << " 同步率" << getSyncRate(player.level) << "%" << C_RESET << "\n";
            cout << "  💖 " << makeBar(player.hp, player.maxHp) << " " << player.hp << "/" << player.maxHp << "\n";
            cout << "  ✨ MP " << player.mp << "/" << player.maxMp << "  ｜ 🧪 藥水 x" << potions << "  ｜ " << C_YELLOW << "💰 " << gold << C_RESET << "\n";
            cout << "  " << C_GRAY << "🗡️ " << player.weapon.name << (player.weapon.wclass.empty() ? "" : "[" + player.weapon.wclass + "]") << (player.weapon.element != "無" ? "(" + player.weapon.element + ")" : "")
                 << " ｜ 🛡️ " << player.armor.name << (player.armor.element != "無" ? "(" + player.armor.element + ")" : "") << C_RESET << "\n";
            if (pAtkBuff > 0) cout << "  " << C_YELLOW << "[狀態] ⚔️ 攻擊提升 +" << pAtkBuff << C_RESET << "\n";
            if (pDefBuff > 0) cout << "  " << C_BLUE << "[狀態] 🛡️ 防禦提升 +" << pDefBuff << C_RESET << "\n";
            if (pBurnDuration > 0) cout << "  " << C_RED << "[異常] 🤢 中毒流血中 (剩餘 " << pBurnDuration << " 回合)" << C_RESET << "\n";
            cout << C_GRAY << "-------------------------------------------------" << C_RESET << "\n";
            cout << " " << C_BOLD << "【" << mName << "】" << C_RESET << "\n";
            cout << "  👹 " << makeBar(mHp, mHpMax) << " " << (mHp < 0 ? 0 : mHp) << "/" << mHpMax << "\n";
            if (mBurnDuration > 0) cout << "  " << C_RED << "[異常] 🔥 燃燒中 (每回合 -" << mBurnDamage << ")" << C_RESET << "\n";
            if (mStunned) cout << "  " << C_YELLOW << "[異常] ⚡ 麻痺暈眩中" << C_RESET << "\n";
            cout << C_CYAN << "═════════════════════════════════════════════════" << C_RESET << "\n\n";

            // 🌟 結算持續傷害狀態
            if (pBurnDuration > 0) {
                cout << "🤢 中毒發作！你受到了 " << pBurnDamage << " 點毒性傷害！\n";
                player.hp -= pBurnDamage; pBurnDuration--;
                if (!player.isAlive()) { cout << "💀 你毒發身亡了...\n"; battlePause(); break; }
            }
            if (mBurnDuration > 0) {
                cout << "🔥 燃燒效果發作！" << mName << " 受到了 " << mBurnDamage << " 點灼燒傷害！\n";
                mHp -= mBurnDamage; mBurnDuration--;
                if (mHp <= 0) { gKills++; cout << "💀 魔物被燒死了！\n"; cout << "\n" << C_GREEN << C_BOLD << "🎉 勝利！" << C_RESET << C_YELLOW << " 💰+" << (25 + wave * 2) << C_RESET << "\n"; gold += 25 + (wave * 2); battlePause(); break; }
            }

            cout << "1. 🗡️ 普通攻擊 | 2. 🔮 使用技能 | 3. 🧪 喝藥水 | 4. ⚡ 戰鬥速度(目前:" << battleSpeedName() << ")\n";
            if (isControlMode) cout << "0. ⚙️ 開啟控制台\n";
            cout << "選擇行動: ";
            int choice; if (!(cin >> choice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            cin.ignore(1000, '\n'); // 清掉殘留換行，否則 waitPlayer 會被跳過

            int finalDamage = 0; bool tookAction = false;

            if (isControlMode && choice == 0) { adminPanel(player, gold, potions, wave, mHp, mAtk, mName, skillDatabase); jobIndex = getJobIndex(player.job); continue; }
            else if (choice == 4) { gBattleSpeed = (gBattleSpeed + 1) % 3; cout << "⚡ 戰鬥速度已切換為【" << battleSpeedName() << "】\n"; sleepMs(500); continue; }
            else if (choice == 1) {
                cout << "\n========== ⚔️ 你的回合 ==========\n";
                cout << "🗡️ 你揮舞【" << player.weapon.name << "】朝著 " << mName << " 攻擊！\n";
                finalDamage = player.atk + pAtkBuff + (rand() % 6 - 3);
                if (rand() % 100 < player.critChance) { finalDamage *= 2; cout << C_YELLOW << C_BOLD << "🔥 暴擊！威力翻倍！" << C_RESET << "\n"; }
                // 🌟 武器元素屬性觸發
                string el = player.weapon.element;
                if (el == "火") {
                    mBurnDamage = player.weapon.elemValue; mBurnDuration = 2;
                    cout << " ➥ 🔥【火焰附魔】點燃了 " << mName << "！(每回合 -" << mBurnDamage << ")\n";
                } else if (el == "毒") {
                    mBurnDamage = player.weapon.elemValue; mBurnDuration = 3;
                    cout << " ➥ 🐍【劇毒附魔】" << mName << " 陷入中毒！(每回合 -" << mBurnDamage << ")\n";
                } else if (el == "雷") {
                    if (rand() % 100 < 35) { mStunned = true; cout << " ➥ ⚡【雷電附魔】" << mName << " 被電得麻痺了！\n"; }
                } else if (el == "冰") {
                    if (rand() % 100 < 35) { mStunned = true; cout << " ➥ ❄️【寒冰附魔】" << mName << " 被凍結，無法行動！\n"; }
                }
                tookAction = true;
            } else if (choice == 2) {
                cout << "\n========== ⚔️ 你的回合 ==========\n";
                int skCount = getSkillCount(jobIndex);
                for (int i=0; i<skCount; i++) {
                    Skill sk = skillDatabase[jobIndex][i];
                    bool isUlt = (!sk.reqWeapon.empty() && sk.reqWeapon != "任意"); // 需特定武器者為絕招
                    if (player.job == "創造神" || player.level >= sk.reqLevel) {
                        cout << "[" << i+1 << "] " << (isUlt ? "🌟" : "") << sk.name
                             << " (Lv." << player.skillLevel[i] << " 耗魔:" << sk.mpCost << ") " << sk.effect;
                        if (isUlt && sk.reqWeapon != "任意" && !sk.reqWeapon.empty()) cout << " ⟪需" << sk.reqWeapon << "⟫";
                        cout << "\n";
                    } else cout << "[" << i+1 << "] ??? (Lv." << sk.reqLevel << " 解鎖)\n";
                }
                cout << "選擇技能 (1-" << skCount << "): "; int skChoice; if (!(cin >> skChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
                cin.ignore(1000, '\n'); // 清掉殘留換行
                if (skChoice >= 1 && skChoice <= skCount) {
                    Skill sk = skillDatabase[jobIndex][skChoice-1];
                    bool isUlt = (!sk.reqWeapon.empty() && sk.reqWeapon != "任意");
                    bool weaponOk = (sk.reqWeapon.empty() || sk.reqWeapon == "任意"
                                     || player.weapon.wclass == sk.reqWeapon || player.weapon.wclass == "萬能");
                    if (player.level < sk.reqLevel && player.job != "創造神") { cout << "❌ 等級不足，尚未領悟此技能！(需 Lv." << sk.reqLevel << ")\n"; waitPlayer(); continue; }
                    else if (!weaponOk) { cout << "❌ 絕招【" << sk.name << "】需裝備【" << sk.reqWeapon << "】類武器才能施展！(目前武器類型：" << (player.weapon.wclass.empty() ? "無" : player.weapon.wclass) << ")\n"; waitPlayer(); continue; }
                    else if (player.mp >= sk.mpCost) {
                        player.mp -= sk.mpCost;
                        double effMult = sk.damageMult + (player.skillLevel[skChoice-1] - 1) * 0.3; // 技能等級加成
                        finalDamage = (player.atk + pAtkBuff) * effMult;
                        if (player.job == "刺客" && skChoice >= 2) finalDamage *= 2;
                        if (isUlt && !sk.flavor.empty()) {
                            gUltUsed++;
                            cout << "\n" << C_MAGENTA << C_BOLD << "✨✨✨ 絕 招 發 動 ！ ✨✨✨" << C_RESET << "\n";
                            cout << C_MAGENTA << sk.flavor << C_RESET << "\n";
                            cout << C_MAGENTA << "──【" << sk.name << "】(絕招 Lv." << player.skillLevel[skChoice-1] << ")──" << C_RESET << "\n";
                        } else {
                            cout << "\n" << C_BLUE << "🌟 使出技能【" << sk.name << "】(Lv." << player.skillLevel[skChoice-1] << ")！" << C_RESET << "\n";
                        }
                        if (sk.attribute == "BURN") { mBurnDamage = sk.attrValue; mBurnDuration = 3; cout << " ➥ 附加：施加了烈焰/劇毒！\n"; }
                        else if (sk.attribute == "STUN") { mStunned = true; cout << " ➥ 附加：強大的衝擊讓魔物暈眩了！\n"; }
                        else if (sk.attribute == "HEAL") { player.hp = min(player.maxHp, player.hp + sk.attrValue); cout << " ➥ 附加：治癒了自身 " << sk.attrValue << " 點生命！\n"; }
                        else if (sk.attribute == "BUFF_ATK") { pAtkBuff += sk.attrValue; cout << " ➥ 附加：戰意高昂！攻擊力提升！\n"; }
                        else if (sk.attribute == "BUFF_DEF") { pDefBuff += sk.attrValue; cout << " ➥ 附加：堅如磐石！防禦力提升！\n"; }
                        tookAction = true;
                    } else { cout << "❌ 魔力不足！(需 " << sk.mpCost << " MP，目前 " << player.mp << ")\n"; waitPlayer(); continue; }
                } else { cout << "❌ 無效的技能編號！\n"; waitPlayer(); continue; }
            } else if (choice == 3) {
                if (potions > 0) { potions--; player.hp = min(player.maxHp, player.hp + 60); cout << "🧪 你飲下靈魂溶劑，回復 60 點生命；腦中的尖叫暫時被壓了下去…\n"; tookAction = true; }
                else { cout << "❌ 沒有溶劑了！殘魂的低語又響了起來…\n"; waitPlayer(); continue; }
            }

            if (tookAction) {
                gTurns++;
                if (finalDamage > 0) {
                    mHp -= finalDamage;
                    gTotalDamage += finalDamage; if (finalDamage > gMaxHit) gMaxHit = finalDamage; // 戰績統計
                    cout << " 👉 造成了 " << C_RED << C_BOLD << finalDamage << C_RESET << " 點傷害！\n";
                    cout << "    " << makeBar(mHp, mHpMax) << " " << (mHp < 0 ? 0 : mHp) << "/" << mHpMax << "\n";
                }
                player.mp = min(player.maxMp, player.mp + player.mpRegen);
            } else continue;

            if (mHp <= 0) { gKills++; cout << "\n" << C_GREEN << C_BOLD << "🎉 勝利！擊敗了 " << mName << "！" << C_RESET << C_YELLOW << " 💰+" << (25 + wave * 2) << C_RESET << "\n"; gold += 25 + (wave * 2); battlePause(); break; }

            battlePause(); // 先讓玩家看清自己這回合的攻擊結果，再進入敵方回合

            // 🌟 怪物反擊與施放專屬技能
            cout << "\n========== 👹 " << mName << " 的回合 ==========\n";
            if (mStunned) {
                cout << "⚡ " << mName << " 處於暈眩狀態，無法行動！\n";
                mStunned = false;
            } else {
                int rawMonsterDam = mAtk + (rand() % 4 - 2);
                bool useSkill = (rand() % 100 < 25); // 25% 機率放技能

                if (useSkill) {
                    if (wave <= 3) {
                        cout << "🍄 " << mName << " 使用了特技【毒液飛濺】！你中毒了！\n";
                        rawMonsterDam *= 1.2;
                        pBurnDuration = 3;
                        pBurnDamage = 5 + wave; // 每回合扣血量隨波數提升
                    } else if (wave <= 6) {
                        cout << "🧛 " << mName << " 使用了特技【生命汲取】！\n";
                        rawMonsterDam *= 1.3;
                        int healAmount = rawMonsterDam / 2;
                        if (mHp + healAmount > mHpMax) healAmount = mHpMax - mHp; // 不超過血量上限
                        if (healAmount < 0) healAmount = 0;
                        mHp += healAmount;
                        cout << " 🩸 " << mName << " 透過吸血回復了 " << healAmount << " 點 HP！\n";
                    } else {
                        cout << "🔥 " << mName << " 使用了特技【粉碎重擊】！無視並破壞了你的防禦狀態！\n";
                        rawMonsterDam *= 1.8;
                        pDefBuff = 0; // 破甲
                    }
                } else if (rand() % 100 < 10) {
                    rawMonsterDam *= 2; cout << "🚨 糟糕！魔物使出了暴擊！\n";
                }

                int monsterDamage = rawMonsterDam;
                if (pDefBuff > 0) {
                    int absorbed = min(pDefBuff, monsterDamage);
                    monsterDamage -= absorbed;
                    cout << "🛡️ 你的防禦抵消了 " << absorbed << " 點傷害！\n";
                }
                // 🌟 防具「減傷」特性
                if (player.armor.element == "減傷" && player.armor.elemValue > 0) {
                    int reduced = monsterDamage * player.armor.elemValue / 100;
                    monsterDamage -= reduced;
                    if (reduced > 0) cout << "🛡️【" << player.armor.name << "】減傷 " << player.armor.elemValue << "%，再減免了 " << reduced << " 點！\n";
                }
                if (monsterDamage < 1) monsterDamage = 1;

                bool hitLanded = player.takeDamage(monsterDamage);
                // 🌟 防具「反傷」特性
                if (hitLanded && player.armor.element == "反傷" && player.armor.elemValue > 0) {
                    int reflect = monsterDamage * player.armor.elemValue / 100;
                    if (reflect > 0) {
                        mHp -= reflect;
                        cout << "🌵【" << player.armor.name << "・反傷】" << mName << " 受到了 " << reflect << " 點反彈傷害！(剩餘 HP: " << (mHp < 0 ? 0 : mHp) << ")\n";
                        if (mHp <= 0 && player.isAlive()) {
                            gKills++;
                            cout << "\n" << C_GREEN << C_BOLD << "🎉 勝利！" << mName << " 被反傷擊倒了！" << C_RESET << C_YELLOW << " 💰+" << (25 + wave * 2) << C_RESET << "\n";
                            gold += 25 + (wave * 2); battlePause(); break;
                        }
                    }
                }
            }

            if (!player.isAlive()) { cout << "💀 你倒下了... 請重新讀檔再來吧！\n"; battlePause(); break; }
            battlePause();
        }
        if(player.isAlive()){
            // ✅ 只有「真的擊敗第 10 波最終滅世巨龍」才算通關（調波數不會誤判）
            if (isFinalBoss) {
                cout << "\n=================================================\n";
                cout << " 📈 【戰後結算】\n";
                cout << "=================================================\n";
                player.gainExp(wave * 130);
                waitPlayer();
                showResultScreen(player, wave, gold, true); // 🏆 通關結算
                break;
            }
            wave++;
            cout << "\n=================================================\n";
            cout << " 📈 【戰後結算】目前已推進至第 " << wave << " 波\n";
            cout << "=================================================\n";
            player.gainExp(wave * 130);
            waitPlayer();
        }
    }
    if (!player.isAlive()) {
        showResultScreen(player, wave, gold, false); // 💀 陣亡結算
    }
    return 0;
}
