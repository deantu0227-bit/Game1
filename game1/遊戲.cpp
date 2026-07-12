#include <iostream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <fstream>
#include <vector>

using namespace std;

// === 基礎結構 ===
struct Equipment {
    string name; string type; int value; int subValue;
};

struct Skill {
    
    string name;
    int reqLevel;
    int mpCost;
    double damageMult;
    string effect;
    string attribute;
    int attrValue;
};

struct JobTemplate {
    string name; string rarity;
    int hp; int atk; int mp; int regen;
    int agiBonus; int lukBonus; string desc;
};

// === 職業資料庫 (15 種) ===
JobTemplate jobDB[15] = {
    {"劍士", "N", 130, 25, 30, 6, 0, 0, "攻守平衡的基礎近戰職"},
    {"魔法師", "N", 80, 15, 100, 15, 0, 0, "擅長高爆發的脆弱法系"},
    {"刺客", "N", 95, 28, 40, 8, 10, 10, "基礎閃避與暴擊特化的暗殺者"},
    {"狂戰士", "R", 200, 18, 0, 0, 0, 0, "捨棄魔力，以鮮血換取力量"},
    {"弓箭手", "N", 100, 22, 40, 8, 5, 5, "穩定的遠程物理輸出"},
    {"吟遊詩人", "N", 110, 20, 60, 12, 5, 0, "魔力充沛且平衡的輔助型戰士"},
    {"武僧", "R", 140, 26, 30, 5, 15, 0, "閃避極高的肉搏大師"},
    {"槍手", "R", 90, 30, 50, 10, 0, 15, "極致暴擊率的火槍使用者"},
    {"聖騎士", "SR", 180, 20, 50, 8, 0, 0, "擁有神聖庇護的重裝防禦者"},
    {"死靈法師", "SR", 90, 18, 120, 18, 0, 0, "精通黑暗魔法與吸血的法師"},
    {"幻術師", "SR", 85, 20, 110, 15, 20, 0, "難以捉摸，極高閃避的法系"},
    {"黑暗祭司", "SR", 120, 24, 80, 10, 0, 10, "操縱詛咒的異端神職者"},
    {"龍騎士", "SSR", 250, 35, 60, 10, 5, 5, "完美數值！攻防一體的傳說存在"},
    {"魔劍士", "SSR", 160, 32, 90, 15, 10, 10, "物理與魔法雙修的無敵劍客"},
    {"創造神", "EX", 9999, 999, 999, 99, 50, 50, "【上帝模式專屬】無視一切規則的造物主"}
};

int getJobIndex(string jName) {
    for (int i = 0; i < 15; i++) {
        if (jobDB[i].name == jName) return i;
    }
    return 0;
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
    Equipment weapon; Equipment armor;

    Character(string n, string j, int h, int a, int m, int r) {
        name = n; job = j; level = 1; exp = 0; maxExp = 100; statPoints = 0;
        baseHp = h; baseAtk = a; strength = 0; constitution = 0; agility = 0; lucky = 0;
        weapon = { "新手舊劍", "Weapon", 0, 5 };
        armor = { "破舊布衣", "Armor", 0, 5 };
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
        cout << "✨ 獲得了 " << amount << " 點經驗值！ (" << exp << "/" << maxExp << ")\n";
        while (exp >= maxExp) {
            exp -= maxExp; level++; maxExp = 100 + (level * 50); statPoints += 4;
            cout << "\n🎉🎉🎉 【升級！】 🎉🎉🎉\n 恭喜升到了 Lv." << level << "！獲得 4 點屬性點！\n";
            updateStats();
            hp = min(maxHp, hp + 30);
            if (job != "狂戰士") mp = min(maxMp, mp + 15);
        }
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

string getSaveFileName(int slot) { return "rpg_save_slot" + to_string(slot) + ".txt"; }

void peekSaveSlot(int slot) {
    ifstream in(getSaveFileName(slot));
    if (in) {
        string name, job; int level;
        in >> name >> job >> level;
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
            << p.armor.name << "\n" << p.armor.value << "\n" << p.armor.subValue << "\n";
    outFile.close();
    cout << "💾 【系統提示】進度已成功儲存至 檔號 " << slot << "！\n";
    waitPlayer();
}

// 🌟 開發者控制台函式
// 🌟 開發者控制台函式 (已擴充等級與職業修改)
void adminPanel(Character& p, int& gold, int& potions, int& wave, int& mHp, int& mAtk, string mName) {
    while (true) {
        clearScreen();
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
        cout << " [0] ↩️ 返回遊戲\n=========================================\n請選擇要修改的項目 (0-9): ";

        int choice; if (!(cin >> choice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');
        if (choice == 0) break;

        // 如果選擇修改職業，印出職業參照表
        if (choice == 9) {
            cout << "👉 職業參照表：\n  0:劍士, 1:魔法師, 2:刺客, 3:狂戰士, 4:弓箭手\n  5:吟遊詩人, 6:武僧, 7:槍手, 8:聖騎士, 9:死靈法師\n  10:幻術師, 11:黑暗祭司, 12:龍騎士, 13:魔劍士, 14:創造神\n";
        }

        cout << "👉 請輸入新的數值: ";
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
            if (newVal >= 0 && newVal <= 14) {
                p.job = jobDB[newVal].name;
                cout << "✅ 修改成功！職業已切換為【" << p.job << "】\n";
            } else {
                cout << "❌ 無效的職業編號！\n";
            }
        }
        waitPlayer();
    }
}

// 🌟 技能資料庫初始化
void initAllSkills(Skill db[15][3]) {
    db[0][0] = {"強擊斬", 1, 10, 1.5, "造成 1.5 倍傷害", "NONE", 0};
    db[0][1] = {"聖光重擊", 3, 18, 2.0, "傷害並提升本次戰鬥防禦", "BUFF_DEF", 10};
    db[0][2] = {"諸神防壁斬", 5, 25, 3.5, "3.5倍爆發，大幅提升防禦", "BUFF_DEF", 25};

    db[1][0] = {"火球術", 1, 15, 1.8, "附加點燃，每回合扣血", "BURN", 15};
    db[1][1] = {"雷霆召喚", 3, 28, 2.5, "造成傷害並麻痺敵人1回合", "STUN", 1};
    db[1][2] = {"終焉隕石術", 5, 45, 5.0, "毀滅性傷害並重度點燃", "BURN", 30};

    db[2][0] = {"毒刃伏擊", 1, 12, 1.5, "潛伏攻擊，附加中毒流血", "BURN", 10};
    db[2][1] = {"幻影瞬殺", 3, 20, 2.8, "必定暴擊的殘影攻擊", "NONE", 0};
    db[2][2] = {"萬箭穿心殺", 5, 30, 4.2, "必定暴擊的絕殺", "NONE", 0};

    db[3][0] = {"血性打擊", 1, 15, 2.2, "耗血15，2.2 倍傷害", "NONE", 0};
    db[3][1] = {"怒火重擊", 3, 25, 3.5, "耗血25，3.5 倍傷害", "NONE", 0};
    db[3][2] = {"嗜血毀滅殺", 5, 40, 4.5, "耗血40，傷害並吸取大量生命", "HEAL", 60};

    db[4][0] = {"二連矢", 1, 12, 1.6, "連續射擊", "NONE", 0};
    db[4][1] = {"封喉箭", 3, 22, 2.0, "造成傷害並封鎖行動", "STUN", 1};
    db[4][2] = {"星辰爆裂箭", 5, 35, 4.0, "星光爆發", "NONE", 0};

    db[5][0] = {"激昂和弦", 1, 15, 1.2, "音波攻擊，提升攻擊力", "BUFF_ATK", 8};
    db[5][1] = {"催眠曲", 3, 25, 1.5, "造成傷害並使魔物沉睡", "STUN", 1};
    db[5][2] = {"英雄戰歌", 5, 40, 3.0, "大合唱，大幅提升攻擊力", "BUFF_ATK", 25};

    db[6][0] = {"寸勁", 1, 10, 1.7, "內力爆發", "NONE", 0};
    db[6][1] = {"昇龍拳", 3, 20, 2.2, "強勢擊飛敵人使其暈眩", "STUN", 1};
    db[6][2] = {"阿修羅霸凰拳", 5, 35, 4.8, "極限爆發傷害", "NONE", 0};

    db[7][0] = {"速射", 1, 12, 1.8, "快速拔槍", "NONE", 0};
    db[7][1] = {"燃燒彈", 3, 25, 2.5, "爆炸火焰附加持續燃燒", "BURN", 20};
    db[7][2] = {"死亡彈幕", 5, 40, 4.5, "無差別掃射", "NONE", 0};

    db[8][0] = {"制裁之錘", 1, 15, 1.5, "聖光打擊使其暈眩", "STUN", 1};
    db[8][1] = {"神聖風暴", 3, 25, 2.0, "聖能爆發並回復生命", "HEAL", 30};
    db[8][2] = {"天降聖劍", 5, 45, 4.2, "召喚神劍，大幅提升防禦", "BUFF_DEF", 40};

    db[9][0] = {"骨矛", 1, 14, 1.8, "射出骨矛", "NONE", 0};
    db[9][1] = {"靈魂抽取", 3, 26, 2.5, "抽取靈魂並治癒自身", "HEAL", 40};
    db[9][2] = {"亡靈大軍", 5, 50, 4.0, "召喚亡靈附加致命劇毒", "BURN", 35};

    db[10][0] = {"心靈震爆", 1, 16, 1.6, "精神打擊", "NONE", 0};
    db[10][1] = {"鏡像破碎", 3, 28, 2.0, "引爆幻象使其混亂(麻痺)", "STUN", 1};
    db[10][2] = {"無限月讀", 5, 45, 4.6, "精神毀滅", "NONE", 0};

    db[11][0] = {"暗影箭", 1, 15, 1.9, "黑暗侵蝕", "NONE", 0};
    db[11][1] = {"痛苦詛咒", 3, 25, 2.0, "折磨靈魂造成強烈流血", "BURN", 25};
    db[11][2] = {"深淵降臨", 5, 45, 4.0, "開啟虛空，吸收生命", "HEAL", 80};

    db[12][0] = {"龍之牙", 1, 20, 2.0, "巨龍撕咬", "NONE", 0};
    db[12][1] = {"真龍吐息", 3, 35, 3.0, "烈焰噴發，燒盡一切", "BURN", 40};
    db[12][2] = {"龍星群", 5, 60, 6.0, "龍王奧義，滅世傷害", "NONE", 0};

    db[13][0] = {"魔力附魔斬", 1, 18, 1.8, "魔武雙擊，提升攻擊力", "BUFF_ATK", 10};
    db[13][1] = {"幻影劍舞", 3, 30, 3.0, "無數劍氣，提升防禦力", "BUFF_DEF", 20};
    db[13][2] = {"超究武神霸斬", 5, 50, 5.5, "極限爆發", "NONE", 0};

    db[14][0] = {"神之指", 1, 0, 10.0, "輕輕一指，灰飛煙滅", "STUN", 1};
    db[14][1] = {"神之恩典", 1, 0, 5.0, "造成傷害並完全治癒自身", "HEAL", 9999};
    db[14][2] = {"世界覆寫", 1, 0, 99.0, "直接抹除魔物的存在", "NONE", 0};
}

int rollGachaJob() {
    int roll = rand() % 100;
    vector<int> pool;
    if (roll < 3) { pool = {12, 13}; cout << "\n🌟🌟🌟 金光閃爍！獲得 SSR 職業！\n"; }
    else if (roll < 18) { pool = {8, 9, 10, 11}; cout << "\n✨ 紫光綻放！獲得 SR 職業！\n"; }
    else if (roll < 48) { pool = {3, 6, 7}; cout << "\n🔵 藍光一閃！獲得 R 職業！\n"; }
    else { pool = {0, 1, 2, 4, 5}; cout << "\n⚪ 命運的輪盤停止，獲得 N 職業。\n"; }
    return pool[rand() % pool.size()];
}

int main() {
    #ifdef _WIN32
    static bool initMenu = [](){ system("chcp 65001 > nul"); return true; }();
    #endif
    srand(time(0));

    // 🌟 恢復完整 19 種魔物陣列
    string ch1Monsters[] = {"綠色史萊姆", "森林毒蛛", "幼小妖狐", "狂暴野豬", "劇毒花妖", "迷霧樹精", "哥布林斥候"};
    string ch2Monsters[] = {"幽靈骷髏", "怨恨殭屍", "詛咒石像鬼", "地底岩魔", "吸血蝙蝠群", "黑暗騎士"};
    string ch3Monsters[] = {"憤怒的半人馬", "墮落巨魔", "地獄雙頭犬", "深淵惡魔", "烈焰魅魔", "死亡使者"};

    Skill skillDatabase[15][3];
    initAllSkills(skillDatabase);

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
        cout << "          [ 5 ] ❌ 離開遊戲\n=========================================\n請選擇 (1-5): ";
        int menuChoice; if (!(cin >> menuChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
        cin.ignore(1000, '\n');

        if (menuChoice == 5) return 0;
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
            cout << " 🌟 職業系統：14 種職業 + 隱藏職，可自選或命運抽卡 (含 SSR)。\n";
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
            cout << "一般魔物："; for(auto m : ch1Monsters) cout << m << " | "; cout << "\n鎮區守衛：👑 哥布林酋長\n\n";

            cout << "【第二章：地下墓穴】(專屬特技：生命汲取，傷害轉化回復)\n";
            cout << "一般魔物："; for(auto m : ch2Monsters) cout << m << " | "; cout << "\n鎮區守衛：🔮 不死巫妖王\n\n";

            cout << "【第三章：終焉戰場】(專屬特技：粉碎重擊，無視並破壞防禦狀態)\n";
            cout << "一般魔物："; for(auto m : ch3Monsters) cout << m << " | "; cout << "\n鎮區守衛：🐉 遠古滅世巨龍\n\n";

            cout << "-----------------------------------------------------\n";
            cout << "【職業圖鑑】(全 14 職 + 1 隱藏職)\n";
            for(int i=0; i<14; i++) {
                cout << " [" << jobDB[i].rarity << "] " << jobDB[i].name << " : " << jobDB[i].desc << "\n";
            }
            cout << " [EX] ??? : 傳說中掌握世界代碼的造物主...\n";
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
                inFile >> player.name >> player.job >> player.level >> player.exp >> player.maxExp
                       >> player.statPoints >> player.baseHp >> player.hp >> player.baseAtk
                       >> player.mp >> player.maxMp >> player.mpRegen
                       >> player.strength >> player.constitution >> player.agility >> player.lucky
                       >> gold >> potions >> wave;
                inFile.ignore();
                getline(inFile, player.weapon.name); inFile >> player.weapon.value >> player.weapon.subValue; inFile.ignore();
                getline(inFile, player.armor.name); inFile >> player.armor.value >> player.armor.subValue;
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
                cout << " [ 4 ] 🎲 命運抽卡 (抽取隨機職業，含稀有 SSR 隱藏職業！)\n=========================================\n請選擇 (1-4): ";
                int jobChoice; if (!(cin >> jobChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
                cin.ignore(1000, '\n');
                if (jobChoice >= 1 && jobChoice <= 3) { selectedJobIdx = jobChoice - 1; break; }
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

        if (selectedJobIdx == 14) {
            player.weapon = {"造物主的指令", "Weapon", 500, 100};
            player.armor = {"系統防火牆", "Armor", 5000, 100};
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
            cout << "=========================================\n        💪 英雄聖殿：配點分配 💪         \n=========================================\n";
            cout << " 剩餘可用點數: ✨ " << player.statPoints << " 點\n----------------------------------------=\n";
            cout << " [ 1 ] ⚔️ 力量 (目前: " << player.strength << ") -> 增加總攻擊力\n [ 2 ] 🛡️ 體質 (目前: " << player.constitution << ") -> 增加最大生命值\n [ 3 ] ⚡ 敏捷 (目前: " << player.agility << ") -> 增加 1% 閃避率\n [ 4 ] 🍀 幸運 (目前: " << player.lucky << ") -> 增加 1% 暴擊率\n=========================================\n請選擇要投資的屬性 (1-4): ";
            int statChoice; if (!(cin >> statChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            cin.ignore(1000, '\n');
            if (statChoice == 1) player.strength++; else if (statChoice == 2) player.constitution++; else if (statChoice == 3) player.agility++; else if (statChoice == 4) player.lucky++;
            player.statPoints--; player.updateStats();
        }

        if (wave > 1 && wave % 2 == 0) {
            bool inShop = true;
            while (inShop) {
                clearScreen();
                cout << "=================================\n         🏪 流浪商人營地         \n=================================\n";
                cout << " 金幣: ✨ " << gold << " | 藥水: 🧪 " << potions << " | 勇者等級: Lv." << player.level << "\n---------------------------------\n";
                cout << "1. 🛒 購買生命藥水 (💰 20 金幣)\n2. 💾 儲存目前遊戲進度\n3. 🏃 離開營地繼續前進\n";
                if (isControlMode) cout << "0. ⚙️ 開啟控制台 (修改數值)\n";
                cout << "請選擇: ";
                int shopChoice; if (!(cin >> shopChoice)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
                cin.ignore(1000, '\n');

                if (shopChoice == 1) {
                    if (gold >= 20) { gold -= 20; potions++; cout << "🛒 購買成功！\n"; } else { cout << "❌ 金幣不足！\n"; } waitPlayer();
                }
                else if (shopChoice == 2) { saveGame(player, gold, potions, wave, currentSaveSlot); }
                else if (shopChoice == 3) { inShop = false; }
                else if (isControlMode && shopChoice == 0) { int dummy = -1; adminPanel(player, gold, potions, wave, dummy, dummy, ""); }
            }
        }

        string currentChapter = ""; string mName = ""; int mHp = 50, mAtk = 10;
        if (wave <= 3) {
            currentChapter = "第一章：🌲 迷霧森林";
            if (wave == 3) { mName = "👑 【章節BOSS】哥布林酋長"; mHp = 170; mAtk = 24; }
            else { mName = ch1Monsters[rand() % 7] + " (Lv." + to_string(wave) + ")"; mHp = 50 + (wave * 15); mAtk = 11 + (wave * 2); }
        }
        else if (wave <= 6) {
            currentChapter = "第二章：💀 地下墓穴";
            if (wave == 6) { mName = "🔮 【章節BOSS】不死巫妖王"; mHp = 380; mAtk = 38; }
            else { mName = ch2Monsters[rand() % 6] + " (Lv." + to_string(wave) + ")"; mHp = 110 + (wave * 22); mAtk = 18 + (wave * 3); }
        }
        else {
            currentChapter = "第三章：🌋 終焉戰場";
            if (wave == 10) { mName = "🐉 【最終滅世BOSS】遠古滅世巨龍"; mHp = 900; mAtk = 68; }
            else { mName = ch3Monsters[rand() % 6] + " (Lv." + to_string(wave) + ")"; mHp = 220 + (wave * 26); mAtk = 28 + (wave * 4); }
        }

        // 🌟 狀態變數 (包含玩家中毒)
        int mBurnDuration = 0, mBurnDamage = 0; bool mStunned = false;
        int pBurnDuration = 0, pBurnDamage = 0; // 玩家中毒/燃燒狀態
        int pAtkBuff = 0, pDefBuff = 0;

        while (player.isAlive() && mHp > 0) {
            clearScreen();
            cout << "=================================================\n";
            cout << " 🗺️ " << currentChapter << " (第 " << wave << " 波)\n";
            cout << "=================================================\n";
            cout << " 【" << player.name << "】(" << player.job << ") Lv." << player.level << "\n";
            cout << "  💖 HP: " << player.hp << "/" << player.maxHp << " | ✨ MP: " << player.mp << "/" << player.maxMp << " | 🧪 x" << potions << "\n";
            if (pAtkBuff > 0) cout << "  [狀態] ⚔️ 攻擊提升 +" << pAtkBuff << "\n";
            if (pDefBuff > 0) cout << "  [狀態] 🛡️ 防禦提升 +" << pDefBuff << "\n";
            if (pBurnDuration > 0) cout << "  [異常] 🤢 中毒流血中 (剩餘 " << pBurnDuration << " 回合)\n";
            cout << "-------------------------------------------------\n";
            cout << " 【" << mName << "】 HP: " << mHp << "\n";
            if (mBurnDuration > 0) cout << "  [異常] 🔥 燃燒中 (每回合 -" << mBurnDamage << ")\n";
            if (mStunned) cout << "  [異常] ⚡ 麻痺暈眩中\n";
            cout << "=================================================\n\n";

            // 🌟 結算持續傷害狀態
            if (pBurnDuration > 0) {
                cout << "🤢 中毒發作！你受到了 " << pBurnDamage << " 點毒性傷害！\n";
                player.hp -= pBurnDamage; pBurnDuration--;
                if (!player.isAlive()) { cout << "💀 你毒發身亡了...\n"; waitPlayer(); break; }
            }
            if (mBurnDuration > 0) {
                cout << "🔥 燃燒效果發作！" << mName << " 受到了 " << mBurnDamage << " 點灼燒傷害！\n";
                mHp -= mBurnDamage; mBurnDuration--;
                if (mHp <= 0) { cout << "💀 魔物被燒死了！\n"; cout << "\n🎉 勝利！獲得經驗值與金幣！\n"; gold += 25 + (wave * 2); waitPlayer(); break; }
            }

            cout << "1. 🗡️ 普通攻擊 | 2. 🔮 使用技能 | 3. 🧪 喝藥水\n";
            if (isControlMode) cout << "0. ⚙️ 開啟控制台\n";
            cout << "選擇行動: ";
            int choice; cin >> choice;

            int finalDamage = 0; bool tookAction = false;

            if (isControlMode && choice == 0) { adminPanel(player, gold, potions, wave, mHp, mAtk, mName); continue; }
            else if (choice == 1) {
                cout << "\n========== ⚔️ 你的回合 ==========\n";
                cout << "🗡️ 你朝著 " << mName << " 揮出一擊！\n";
                finalDamage = player.atk + pAtkBuff + (rand() % 6 - 3);
                if (rand() % 100 < player.critChance) { finalDamage *= 2; cout << "🔥 暴擊！威力翻倍！\n"; }
                tookAction = true;
            } else if (choice == 2) {
                cout << "\n========== ⚔️ 你的回合 ==========\n";
                for (int i=0; i<3; i++) {
                    Skill sk = skillDatabase[jobIndex][i];
                    if (player.job == "創造神" || player.level >= sk.reqLevel) {
                        cout << "[" << i+1 << "] " << sk.name << " (耗魔:" << sk.mpCost << ") " << sk.effect << "\n";
                    } else cout << "[" << i+1 << "] ??? (Lv." << sk.reqLevel << " 解鎖)\n";
                }
                cout << "選擇技能 (1-3): "; int skChoice; cin >> skChoice;
                if (skChoice >= 1 && skChoice <= 3) {
                    Skill sk = skillDatabase[jobIndex][skChoice-1];
                    if (player.level < sk.reqLevel && player.job != "創造神") cout << "❌ 等級不足，尚未領悟此技能！\n";
                    else if (player.mp >= sk.mpCost) {
                        player.mp -= sk.mpCost;
                        finalDamage = (player.atk + pAtkBuff) * sk.damageMult;
                        if (player.job == "刺客" && skChoice >= 2) finalDamage *= 2;
                        cout << "\n🌟 使出絕招【" << sk.name << "】！\n";
                        if (sk.attribute == "BURN") { mBurnDamage = sk.attrValue; mBurnDuration = 3; cout << " ➥ 附加：施加了烈焰/劇毒！\n"; }
                        else if (sk.attribute == "STUN") { mStunned = true; cout << " ➥ 附加：強大的衝擊讓魔物暈眩了！\n"; }
                        else if (sk.attribute == "HEAL") { player.hp = min(player.maxHp, player.hp + sk.attrValue); cout << " ➥ 附加：治癒了自身 " << sk.attrValue << " 點生命！\n"; }
                        else if (sk.attribute == "BUFF_ATK") { pAtkBuff += sk.attrValue; cout << " ➥ 附加：戰意高昂！攻擊力提升！\n"; }
                        else if (sk.attribute == "BUFF_DEF") { pDefBuff += sk.attrValue; cout << " ➥ 附加：堅如磐石！防禦力提升！\n"; }
                        tookAction = true;
                    } else cout << "❌ 魔力不足！\n";
                }
            } else if (choice == 3) {
                if (potions > 0) { potions--; player.hp = min(player.maxHp, player.hp + 60); cout << "🧪 回復了 60 點生命值！\n"; tookAction = true; }
                else { cout << "❌ 沒有藥水了！\n"; waitPlayer(); continue; }
            }

            if (tookAction) {
                if (finalDamage > 0) { mHp -= finalDamage; cout << " 👉 造成了 " << finalDamage << " 點直接傷害！ (" << mName << " 剩餘 HP: " << (mHp < 0 ? 0 : mHp) << ")\n"; }
                player.mp = min(player.maxMp, player.mp + player.mpRegen);
            } else continue;

            if (mHp <= 0) { cout << "\n🎉 勝利！擊敗了 " << mName << "！獲得經驗值與金幣！\n"; gold += 25 + (wave * 2); waitPlayer(); break; }

            waitPlayer(); // 先讓玩家看清自己這回合的攻擊結果，再進入敵方回合

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

                int monsterDamage = rawMonsterDam - pDefBuff;
                if (monsterDamage < 1) monsterDamage = 1;

                if (pDefBuff > 0) cout << "🛡️ 你的防禦抵消了 " << (rawMonsterDam - monsterDamage) << " 點傷害！\n";
                player.takeDamage(monsterDamage);
            }

            if (!player.isAlive()) { cout << "💀 你倒下了... 請重新讀檔再來吧！\n"; waitPlayer(); break; }
            waitPlayer();
        }
        if(player.isAlive()){
            wave++;
            cout << "\n=================================================\n";
            cout << " 📈 【戰後結算】目前已推進至第 " << wave << " 波\n";
            cout << "=================================================\n";
            player.gainExp(50 + wave * 5);
            if (wave > 10) { cout << "\n🏆🏆🏆 恭喜你！已擊敗最終滅世BOSS，拯救了世界！ 🏆🏆🏆\n"; waitPlayer(); break; }
            waitPlayer();
        }
    }
    if (!player.isAlive()) {
        clearScreen();
        cout << "=================================================\n";
        cout << "                💀 遊戲結束 💀                 \n";
        cout << "=================================================\n";
        cout << " 勇者 " << player.name << " (" << player.job << " Lv." << player.level << ") 倒下了...\n";
        cout << " 你堅持到了第 " << wave << " 波。下次讀取存檔再來挑戰吧！\n";
        cout << "=================================================\n";
        waitPlayer();
    }
    return 0;
}
