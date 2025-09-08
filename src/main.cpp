/*

 * Function Generator for ESP32 (Pseudo-turn-signal)

 * 

 * License: Custom License

 * - Free to use for personal purposes only (at your own risk).

 * - Commercial use is prohibited.

 * - See the LICENSE file for details.

 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>  // NVS

#define def_hazard "hazard"
#define def_ess "ess"
#define def_position "position"
#define def_l_turn "L_turn"
#define def_r_tuurn "R_turn"

portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;

// ====== エンコーダー関連 ======
static uint32_t lastEncMove = 0; // 前回の回転時刻

// ====== 出力ピン割り当て（4ch）======
static const int PINS[4] = {18, 19, 23, 5}; // CH1..CH4

// ====== プリセット（us）======
static inline uint32_t ms2us(uint32_t ms){ return ms * 1000UL; }
static inline uint32_t us2ms(uint32_t us){ return us / 1000UL; }

static const uint32_t MIN_MS = 1;
static const uint32_t MAX_MS = 3000;        // ms 入力の上限（phase,setコマンド等）
static const uint32_t MAX_US = 3000000UL;   // us 内部値の上限（3秒）

struct PatternUS { uint32_t on_us, off_us; };
static PatternUS PRESET_POSITION_US = { ms2us(8),   ms2us(1)   };
static PatternUS PRESET_R_TURN_US     = { ms2us(380), ms2us(190) };
static PatternUS PRESET_L_TURN_US     = { ms2us(380), ms2us(190) };
static PatternUS PRESET_TURN_US     = { ms2us(380), ms2us(190) };
static PatternUS PRESET_ESS_US      = { ms2us(120), ms2us(120) };

static inline uint32_t clamp_us(uint32_t v){
  return v == 0 ? 1u : (v > MAX_US ? MAX_US : v);
}

// ==== SAVED! オーバーレイ表示 ====
static bool     g_showSaved = false;
static uint32_t g_savedUntil = 0;

void showSavedBanner(uint32_t ms){
  g_showSaved = true;
  g_savedUntil = millis() + ms;
}

// ====== チャネル状態（on/offをµsで持つ） ======
struct Channel {
  int pin;
  bool running;
  bool level;
  bool muted;
  uint32_t on_us;
  uint32_t off_us;
  uint32_t phase_us;
  uint32_t next_us;
} ch[4];

// 単位表示/編集用
enum TimeUnit { UNIT_MS, UNIT_US };
TimeUnit uiUnit = UNIT_MS;           // 表示単位（ms/usを選べる）
uint32_t enc_step_us = 1000;         // エンコーダの1クリック増分（既定=1ms=1000us）

// 編集対象：CHとON/OFF
int selectedCh = 0;                  // 0..3（CH1..CH4）
enum EditTarget { EDIT_ON, EDIT_OFF, EDIT_CH, EDIT_STEP };
EditTarget editTarget = EDIT_ON;

void apply_level(int i, bool hi){
  ch[i].level = hi;
  if (ch[i].muted) {
    digitalWrite(ch[i].pin, LOW);          // ★ミュート中は常にLOW
  } else {
    digitalWrite(ch[i].pin, hi ? HIGH : LOW);
  }
}

void start_channel(int i){
  ch[i].running = true;
  apply_level(i, true);
  ch[i].next_us = micros() + (ch[i].on_us + ch[i].phase_us);
}

void stop_channel(int i){
  ch[i].running = false;
  apply_level(i, LOW);
}

// --- ビットマスクで CH を開始／停止するヘルパー ---
// ビット0=CH1, ビット1=CH2, ...（例: 0b0011 なら CH1/CH2）
void start_with_mask(uint8_t mask){
  for(int i=0;i<4;i++){
    if(mask & (1 << i)) start_channel(i);
  }
}
void stop_with_mask(uint8_t mask){
  for(int i=0;i<4;i++){
    if(mask & (1 << i)) stop_channel(i);
  }
}

void update_channel_timers(){
  const uint32_t now = micros();
  for(int i=0;i<4;i++){
    if(!ch[i].running) continue;
    while((int32_t)(now - ch[i].next_us) >= 0){   // ★複数周期分を追従
      const bool nextLevel = !ch[i].level;
      apply_level(i, nextLevel);
      const uint32_t dur_us = nextLevel ? ch[i].on_us : ch[i].off_us;
      ch[i].next_us += dur_us;
    }
  }
}

void set_running_mask(uint8_t mask){
  for(int i=0;i<4;i++){
    if(mask & (1<<i)) start_channel(i);
    else              stop_channel(i);
  }
}

// ---- Step 候補（Unit別）----
static const uint16_t STEP_MS_LIST[] = {1,5,10,50,100,250,500,1000,2000,3000}; // ms
static const uint32_t STEP_US_LIST[] = {10,50,100,200,500,1000,2000,5000,10000,50000,100000}; // us

// いまの enc_step_us を基準に、最も近い候補のインデックスを返す（ms候補/ us候補）
int nearest_ms_index(uint32_t step_us){
  int best = 0; uint32_t bestd = 0xFFFFFFFF;
  for(size_t i=0;i<sizeof(STEP_MS_LIST)/sizeof(STEP_MS_LIST[0]);++i){
    uint32_t cand = STEP_MS_LIST[i]*1000UL;
    uint32_t d = (cand>step_us)? (cand-step_us) : (step_us-cand);
    if(d < bestd){ bestd=d; best=(int)i; }
  }
  return best;
}
int nearest_us_index(uint32_t step_us){
  int best = 0; uint32_t bestd = 0xFFFFFFFF;
  for(size_t i=0;i<sizeof(STEP_US_LIST)/sizeof(STEP_US_LIST[0]);++i){
    uint32_t cand = STEP_US_LIST[i];
    uint32_t d = (cand>step_us)? (cand-step_us) : (step_us-cand);
    if(d < bestd){ bestd=d; best=(int)i; }
  }
  return best;
}

// Unitに応じて enc_step_us を次の候補に更新
void stepcycle_next(){
  if(uiUnit == UNIT_MS){
    int i = nearest_ms_index(enc_step_us);
    i = (i + 1) % (int)(sizeof(STEP_MS_LIST)/sizeof(STEP_MS_LIST[0]));
    enc_step_us = (uint32_t)STEP_MS_LIST[i] * 1000UL;
  }else{
    int i = nearest_us_index(enc_step_us);
    i = (i + 1) % (int)(sizeof(STEP_US_LIST)/sizeof(STEP_US_LIST[0]));
    enc_step_us = STEP_US_LIST[i];
  }
  Serial.printf("Step cycled -> %lu %s\n",
    (unsigned long)(uiUnit==UNIT_MS? enc_step_us/1000 : enc_step_us),
    (uiUnit==UNIT_MS? "ms":"us"));
}

// ====== 左右マスク（必要なら使用）======
uint8_t left_mask  = 0b0011; // CH1,CH2
uint8_t right_mask = 0b1100; // CH3,CH4

// ====== OLED（SSD1306 I2C）======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDR    0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== OLED 表示ヘルパ =====
constexpr int OLED_COLS = 21;

// 2カラムを1行に収める（必要なら末尾をトリム）
void printlnTwoCols(const char* l1, String v1, const char* l2, String v2) {
  String s1 = String(l1) + v1;     // 例: "Mode:left"
  String s2 = String(l2) + v2;     // 例: "Unit:us"
  String line = s1 + " " + s2;     // 1スペースで連結

  // 21文字に収まるまで右端から削る（まずs2→それでもダメならs1）
  while (line.length() > OLED_COLS) {
    if (s2.length() > 0) s2.remove(s2.length() - 1);
    else if (s1.length() > 0) s1.remove(s1.length() - 1);
    line = s1 + " " + s2;
  }
  display.println(line);
}

// CH行を「n R/S on/off」で出す（長いときは末尾トリム）(ミュートはch番号の直後にm表示あり)
void printlnChLine(int i, bool isSelected) {
  String line;

  // 先頭マーク
  line += (isSelected ? ">" : " ");

  // CH番号
  line += String(i + 1);

  // ★ ミュート時は "m" を番号直後に追加
  if (ch[i].muted) line += "m";
  else line += " ";

  // 状態：mute に関係なく running で決める
  if (ch[i].running) line += "R ";
  else               line += "S ";

  // 時間
  if (uiUnit == UNIT_MS) {
    line += String(us2ms(ch[i].on_us));
    line += "/";
    line += String(us2ms(ch[i].off_us));
  } else {
    line += String(ch[i].on_us);
    line += "/";
    line += String(ch[i].off_us);
  }

  if ((int)line.length() > OLED_COLS) line.remove(OLED_COLS);
  display.println(line);
}

// 表示用：現在のプリセット名
const char* currentPreset = def_hazard; // 既定は従来どおり

// currentPreset をユーザー名に差し替えるための作業バッファ
static char userPresetName[32]; // 31文字+終端


// ====== NVS（設定保存）======
Preferences prefs;
bool autostart = false; // 電源投入時に自動スタートするか

void saveSettings(){
  prefs.begin("fn-gen", false);
  prefs.putString("preset", currentPreset);
  prefs.putBool("autostart", autostart);
  // 代表値ではなく「個別」を保存（CH1..4）
  for(int i=0;i<4;i++){
    char key_on[8], key_off[8];
    sprintf(key_on,  "on%u",  i+1);
    sprintf(key_off, "off%u", i+1);
    prefs.putUInt(key_on,  ch[i].on_us);
    prefs.putUInt(key_off, ch[i].off_us);
  }
  prefs.putUChar("unit", (uint8_t)uiUnit);
  prefs.putUInt("stepus", enc_step_us);
  prefs.end();
}

void loadSettings(){
  prefs.begin("fn-gen", true);
  String p = prefs.getString("preset", def_hazard);
  autostart = prefs.getBool("autostart", false);
  uiUnit = (TimeUnit)prefs.getUChar("unit", (uint8_t)UNIT_MS);
  enc_step_us = prefs.getUInt("stepus", 1000);

  // デフォルト＝turn
  for(int i=0;i<4;i++){
    ch[i].on_us  = PRESET_TURN_US.on_us;
    ch[i].off_us = PRESET_TURN_US.off_us;
  }
  // あれば個別復元
  for(int i=0;i<4;i++){
    char key_on[8], key_off[8];
    sprintf(key_on,  "on%u",  i+1);
    sprintf(key_off, "off%u", i+1);
    ch[i].on_us  = prefs.getUInt(key_on,  ch[i].on_us);
    ch[i].off_us = prefs.getUInt(key_off, ch[i].off_us);
  }

  for (int i=0;i<4;i++){
    ch[i].on_us  = clamp_us(ch[i].on_us);
    ch[i].off_us = clamp_us(ch[i].off_us);
    ch[i].phase_us = (ch[i].phase_us > MAX_US) ? MAX_US : ch[i].phase_us;
  }

  prefs.end();

  if(p==def_position){ currentPreset=def_position; }
  else if(p==def_ess){ currentPreset=def_ess; }
  else if(p==def_hazard){ currentPreset=def_hazard; }
  else if(p==def_l_turn){ currentPreset=def_l_turn; }
  else if(p==def_r_tuurn){ currentPreset=def_r_tuurn; }
  else { 
    // ユーザー名などカスタム名だった場合
    snprintf(userPresetName, sizeof(userPresetName), "%s", p.c_str());
    currentPreset = userPresetName;
  }

  if (uiUnit == UNIT_MS) {
    enc_step_us = (uint32_t)STEP_MS_LIST[ nearest_ms_index(enc_step_us) ] * 1000UL;
  } else {
    enc_step_us = STEP_US_LIST[ nearest_us_index(enc_step_us) ];
  }
}

// ---- NVS safe getters (存在しないキーでエラーを出さない) ----
inline String nvsGetStringSafe(Preferences& p, const char* key) {
  return p.isKey(key) ? p.getString(key, "") : String();
}
inline uint32_t nvsGetUIntSafe(Preferences& p, const char* key, uint32_t defval){
  return p.isKey(key) ? p.getUInt(key, defval) : defval;
}
inline uint8_t nvsGetUCharSafe(Preferences& p, const char* key, uint8_t defval){
  return p.isKey(key) ? p.getUChar(key, defval) : defval;
}

// ====== ユーザープリセット（NVSスロット0..3） ======
static const uint8_t PRESET_SLOTS = 4;

// 名前→スロット検索
int findPresetSlotByName(const String& name) {
  Preferences p; p.begin("fn-gen", true);
  for (uint8_t s = 0; s < PRESET_SLOTS; ++s) {
    char key[20]; snprintf(key, sizeof(key), "slot%u_name", s);
    String n = nvsGetStringSafe(p, key);   // ← 安全版
    if (n.length() && n == name) { p.end(); return (int)s; }
  }
  p.end(); return -1;
}

// スロットの有無確認（name が存在するか）
bool hasPresetSlot(uint8_t slot) {
  Preferences p; p.begin("fn-gen", true);
  char key[20]; snprintf(key, sizeof(key), "slot%u_name", slot);
  bool ok = p.isKey(key);                 // ← 存在チェックだけ
  p.end(); return ok;
}

// スロットに保存
void savePresetSlot(uint8_t slot, const char* nameOpt) {
  if (slot >= PRESET_SLOTS) { Serial.println("slot=0..3"); return; }

  Preferences p; p.begin("fn-gen", false);

  // 名前（指定なしなら既存名は保持。どちらもなければ "slotN"）
  char key[24];
  snprintf(key, sizeof(key), "slot%u_name", slot);
  String cur = p.getString(key, "");
  String nm  = cur;
  if (nameOpt && *nameOpt) nm = nameOpt;
  if (!nm.length()) { char tmp[10]; snprintf(tmp, sizeof(tmp), "slot%u", slot); nm = tmp; }
  p.putString(key, nm);

  // CH1..4 の on/off/phase
  for (int i = 0; i < 4; ++i) {
    snprintf(key, sizeof(key), "slot%u_on%u",  slot, i+1); p.putUInt(key, ch[i].on_us);
    snprintf(key, sizeof(key), "slot%u_off%u", slot, i+1); p.putUInt(key, ch[i].off_us);
    snprintf(key, sizeof(key), "slot%u_ph%u",  slot, i+1); p.putUInt(key, ch[i].phase_us);
  }
  // ついでに UI状態（任意）：単位・ステップ・マスク
  snprintf(key, sizeof(key), "slot%u_unit", slot);   p.putUChar(key, (uint8_t)uiUnit);
  snprintf(key, sizeof(key), "slot%u_step", slot);   p.putUInt (key, enc_step_us);
  snprintf(key, sizeof(key), "slot%u_lmsk", slot);   p.putUChar(key, left_mask);
  snprintf(key, sizeof(key), "slot%u_rmsk", slot);   p.putUChar(key, right_mask);

  p.end();

  // 表示名を反映
  snprintf(userPresetName, sizeof(userPresetName), "%s", nm.c_str());
  currentPreset = userPresetName;

  Serial.printf("saved preset slot %u as '%s'\n", slot, nm.c_str());
  showSavedBanner(800);
}

// スロットから読込
bool loadPresetSlot(uint8_t slot) {
  if (slot >= PRESET_SLOTS) { Serial.println("slot=0..3"); return false; }
  Preferences p; p.begin("fn-gen", true);

  char key[24];
  snprintf(key, sizeof(key), "slot%u_name", slot);
  String nm = nvsGetStringSafe(p, key);   // ← 安全版
  if (!nm.length()) { p.end(); Serial.println("empty slot"); return false; }

  for (int i=0;i<4;++i) {
    snprintf(key,sizeof(key),"slot%u_on%u",  slot,i+1); ch[i].on_us    = nvsGetUIntSafe(p,key,ch[i].on_us);
    snprintf(key,sizeof(key),"slot%u_off%u", slot,i+1); ch[i].off_us   = nvsGetUIntSafe(p,key,ch[i].off_us);
    snprintf(key,sizeof(key),"slot%u_ph%u",  slot,i+1); ch[i].phase_us = nvsGetUIntSafe(p,key,ch[i].phase_us);
  }

  for (int i=0;i<4;i++){
    ch[i].on_us  = clamp_us(ch[i].on_us);
    ch[i].off_us = clamp_us(ch[i].off_us);
    ch[i].phase_us = (ch[i].phase_us > MAX_US) ? MAX_US : ch[i].phase_us;
  }

  snprintf(key,sizeof(key),"slot%u_unit",slot); uiUnit      = (TimeUnit)nvsGetUCharSafe(p,key,(uint8_t)uiUnit);
  snprintf(key,sizeof(key),"slot%u_step",slot); enc_step_us = nvsGetUIntSafe (p,key,enc_step_us);
  snprintf(key,sizeof(key),"slot%u_lmsk",slot); left_mask   = nvsGetUCharSafe(p,key,left_mask);
  snprintf(key,sizeof(key),"slot%u_rmsk",slot); right_mask  = nvsGetUCharSafe(p,key,right_mask);

  if (uiUnit == UNIT_MS) {
    enc_step_us = (uint32_t)STEP_MS_LIST[ nearest_ms_index(enc_step_us) ] * 1000UL;
  } else {
    enc_step_us = STEP_US_LIST[ nearest_us_index(enc_step_us) ];
  }

  p.end();
  for (int i=0;i<4;++i) if (ch[i].running) ch[i].next_us = micros() + 1000;
  snprintf(userPresetName, sizeof(userPresetName), "%s", nm.c_str());
  currentPreset = userPresetName;
  Serial.printf("loaded preset slot %u ('%s')\n", slot, nm.c_str());
  return true;
}

// 全スロット一覧
void listPresetSlots() {
  Preferences p; p.begin("fn-gen", true);
  for (uint8_t s = 0; s < PRESET_SLOTS; ++s) {
    char key[20]; snprintf(key, sizeof(key), "slot%u_name", s);
    String nm = nvsGetStringSafe(p, key); // ← 安全版
    if (nm.length()) Serial.printf("slot%u: '%s'\n", s, nm.c_str());
    else             Serial.printf("slot%u: (empty)\n", s);
  }
  p.end();
}

// ====== プリセット適用 ======
void apply_preset_us(const PatternUS& p,const char* currentPset){
  if(String(currentPset) == def_l_turn){
    ch[0].on_us=p.on_us;ch[0].off_us=p.off_us; ch[0].phase_us=0;
    ch[1].on_us=p.on_us;ch[1].off_us=p.off_us; ch[1].phase_us=0;
    ch[2].on_us=PRESET_POSITION_US.on_us;ch[2].off_us=PRESET_POSITION_US.off_us; ch[2].phase_us=0;
    ch[3].on_us=PRESET_POSITION_US.on_us;ch[3].off_us=PRESET_POSITION_US.off_us; ch[3].phase_us=0;
  }else if(String(currentPset) == def_r_tuurn){
    ch[0].on_us=PRESET_POSITION_US.on_us;ch[0].off_us=PRESET_POSITION_US.off_us; ch[0].phase_us=0;
    ch[1].on_us=PRESET_POSITION_US.on_us;ch[1].off_us=PRESET_POSITION_US.off_us; ch[1].phase_us=0;
    ch[2].on_us=p.on_us;ch[2].off_us=p.off_us; ch[2].phase_us=0;
    ch[3].on_us=p.on_us;ch[3].off_us=p.off_us; ch[3].phase_us=0;  
  }else{
    for(int i=0;i<4;i++){ ch[i].on_us=p.on_us; ch[i].off_us=p.off_us; ch[i].phase_us=0; }
  }
}

// --- Mode 表示用 ---
const char* currentMode = "custom";

// いま動いているCHのビットマスク（bit0=CH1 ... bit3=CH4）
uint8_t get_running_mask(){
  uint8_t m = 0;
  for(int i=0;i<4;i++){
    if(ch[i].running) m |= (1 << i);
  }
  return m;
}

// running状態から currentMode を更新
void updateModeFromRunning(){
  uint8_t m   = get_running_mask();
  uint8_t L   = left_mask;
  uint8_t R   = right_mask;
  uint8_t HRD = (uint8_t)(L | R);   // hazard = left|right

  if(m == L)              currentMode = "left";
  else if(m == R)         currentMode = "right";
  else if(m == HRD)       currentMode = "hazard";
  else                    currentMode = "custom";  // 停止含むその他
}

// ====== OLED描画 ======
void oledInit() {
  Wire.begin(21, 22);  // SDA=21, SCL=22
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("FunctionGen");
  display.println("OLED Ready");
  display.display();
}

void oledDraw() {
  static uint32_t last = 0;
  if (millis() - last < 200) return;

  // SAVED! オーバーレイ
  if (g_showSaved && (int32_t)(g_savedUntil - millis()) > 0) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor((128 - 6*2*6)/2, (64 - 8*2)/2);
    display.println("SAVED!");
    display.display();
    return;
  } else if (g_showSaved) {
    g_showSaved = false;
  }
  last = millis();

  // 通常描画：8行きっちり構成
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);

  // 1行目: Mode / Unit
  printlnTwoCols("Mode:", String(currentMode),
                 "Unit:", String(uiUnit==UNIT_MS ? "ms" : "us"));

  // 2行目: Preset
  {
    String line = String("Preset: ") + currentPreset;
    if ((int)line.length() > OLED_COLS) line.remove(OLED_COLS);
    display.println(line);
  }

  // 3行目: Sel / AStart
  {
    String selStr = String(selectedCh + 1);
    if (editTarget == EDIT_CH) selStr = ">" + selStr;  // ★ここで > を付与
    else selStr = " " + selStr;
    printlnTwoCols("Sel:", selStr,
                   "AStart: ", String(autostart ? "ON" : "OFF"));
  }

  // 4行目: Step
  {
    String stepStr = (uiUnit==UNIT_MS) ? String(enc_step_us/1000) : String(enc_step_us);
    String line = String("Step: ") + stepStr;
    if (editTarget == EDIT_STEP) line = String("Step:>") + stepStr;   // ★ここで > を付与
    if ((int)line.length() > OLED_COLS) line.remove(OLED_COLS);
    display.println(line);
  }

  // 5〜8行目: CH1..4
  for (int i = 0; i < 4; i++) {
    // ★ON/OFF編集中だけ > を出す（CH/STEP編集中は出さない）
    bool isSelected = ((editTarget == EDIT_ON) || (editTarget == EDIT_OFF)) && (i == selectedCh);
    printlnChLine(i, isSelected);
  }

  display.display();
}

// ====== シリアルコマンド ======
void print_state(){
  Serial.println(F("=== Function Generator State ==="));
  for(int i=0;i<4;i++){
    Serial.printf("CH%d pin=%d run=%d muted=%d on=%luus off=%luus phase=%luus level=%d\n",
      i+1, ch[i].pin, ch[i].running, ch[i].muted,
      (unsigned long)ch[i].on_us, (unsigned long)ch[i].off_us,
      (unsigned long)ch[i].phase_us, ch[i].level);
  }
  Serial.printf("Preset: %s  AutoStart=%d  Unit:%s  Step:%lu%s\n",
    currentPreset, autostart, (uiUnit==UNIT_MS?"ms":"us"),
    (unsigned long)(uiUnit==UNIT_MS ? enc_step_us/1000 : enc_step_us),
    (uiUnit==UNIT_MS?"ms":"us"));
}

bool parse_u32(const char* s, uint32_t& out){
  char* end=nullptr; unsigned long v = strtoul(s, &end, 10);
  if(end==s) return false;
  if(v < MIN_MS) v = MIN_MS;
  if(v > MAX_MS) v = MAX_MS;
  out = (uint32_t)v; return true;
}

void handle_command(String line){
  line.trim();
  if(line.isEmpty()) return;
  line.toLowerCase();

  // トークン分割
  const int MAXTOK=6;
  String t[MAXTOK]; int n=0;
  int idx=0;
  while(n<MAXTOK && idx < line.length()){
    int sp = line.indexOf(' ', idx);
    if(sp<0) sp = line.length();
    t[n++] = line.substring(idx, sp);
    idx = sp+1;
    while(idx<line.length() && line[idx]==' ') idx++;
  }

  if (t[0]=="stepcycle") {
    stepcycle_next();
    print_state();
    return;
  }

  if(t[0]=="help"){
    Serial.println(F(
      "start all|left|right|hazard|1..4\n"
      "stop  all|left|right|hazard|1..4\n"
      "set CH ONms OFFms\n"
      "setus CH ONus OFFus\n"
      "phase CH ms\n"
      "preset R_turn|L_turn|hazard|ess|position\n"
      "autostart on|off\n"
      "unit ms|us\n"
      "step N   (unitに従う)\n"
      "leftmask  bNN (e.g. b0011)\n"
      "rightmask bNN\n"
      "save\n"
      "add\n"
      "mute CH|all\n"
      "unmute CH|all\n"
      "savepreset N [name]\n"
      "loadpreset N | loadpreset name\n"
      "listpresets\n"
      "state"));
    return;
  }

  // ---- ユーザープリセット ----
  if (t[0]=="listpresets") {
    listPresetSlots();
    return;
  }

  if (t[0]=="savepreset") {
    // savepreset N [name]  または  savepreset name（→空きスロット or slot0）
    if (n >= 2) {
      // 引数が数字ならそのスロット、そうでなければ名前で slot を自動選択
      bool isNum = true;
      for (size_t k=0;k<t[1].length();++k) if (!isDigit(t[1][k])) { isNum=false; break; }

      if (isNum) {
        uint8_t slot = (uint8_t)t[1].toInt();
        const char* nameOpt = (n>=3) ? t[2].c_str() : nullptr;
        savePresetSlot(slot, nameOpt);
      } else {
        // 名前だけ渡された → 既に同名があれば上書き。無ければ空きスロットへ。空き無ければ slot0 を上書き。
        String name = t[1];
        int slot = findPresetSlotByName(name);
        if (slot < 0) {
          // 空き探し
          for (uint8_t s=0;s<PRESET_SLOTS;++s) { if (!hasPresetSlot(s)) { slot = (int)s; break; } }
          if (slot < 0) slot = 0; // すべて埋まってたら slot0
        }
        savePresetSlot((uint8_t)slot, name.c_str());
      }
    } else {
      Serial.println("usage: savepreset N [name]  |  savepreset name");
    }
    return;
  }

  if (t[0]=="loadpreset") {
    if (n >= 2) {
      bool isNum = true;
      for (size_t k=0;k<t[1].length();++k) if (!isDigit(t[1][k])) { isNum=false; break; }

      bool ok = false;
      if (isNum) {
        uint8_t slot = (uint8_t)t[1].toInt();
        ok = loadPresetSlot(slot);
      } else {
        int slot = findPresetSlotByName(t[1]);
        if (slot >= 0) ok = loadPresetSlot((uint8_t)slot);
        else Serial.println("not found");
      }
      if (ok) {
        updateModeFromRunning();
        print_state();
      }
    } else {
      Serial.println("usage: loadpreset N  |  loadpreset name");
    }
    return;
  }


  if(t[0]=="start"){
    if(n>=2){
      if(t[1]=="all"){
        set_running_mask(0b1111);                 // 置き換え
      }else if(t[1]=="left"){
        set_running_mask(left_mask);               // 置き換え
      }else if(t[1]=="right"){
        set_running_mask(right_mask);              // 置き換え
      }else if(t[1]=="hazard"){
        set_running_mask((uint8_t)(left_mask | right_mask)); // 置き換え
      }else{
        int chn = t[1].toInt();
        if(chn>=1 && chn<=4){
          set_running_mask((uint8_t)(1u << (chn-1)));        // 置き換え（単独）
        }else{
          Serial.println("bad arg");
          return;
        }
      }
    }else{
      Serial.println("usage: start all|left|right|hazard|1..4");
      return;
    }
    updateModeFromRunning();
    print_state();
    return;
  }

  if(t[0]=="stop"){
    if(n>=2){
      if(t[1]=="all"){
        for(int i=0;i<4;i++) stop_channel(i);
      }else if(t[1]=="left"){
        stop_with_mask(left_mask);
      }else if(t[1]=="right"){
        stop_with_mask(right_mask);
      }else if(t[1]=="hazard"){
        stop_with_mask(left_mask | right_mask);
      }else{
        int chn=t[1].toInt();
        if(chn>=1&&chn<=4) stop_channel(chn-1);
        else { Serial.println("bad arg"); return; }
      }
    }else{
      Serial.println("usage: stop all|left|right|hazard|1..4");
      return;
    }
    updateModeFromRunning();
    print_state();
    return;
  }

  if(t[0]=="phase" && n>=3){
    int chn=t[1].toInt(); if(chn<1||chn>4){ Serial.println("bad ch"); return; }
    uint32_t pms; if(!parse_u32(t[2].c_str(), pms)){ Serial.println("bad ms"); return; }
    ch[chn-1].phase_us = ms2us(pms);  // ★ここを修正（µsで保持）
    if(ch[chn-1].running){ ch[chn-1].next_us = micros() + ch[chn-1].phase_us; }
    print_state(); return;
  }

  if(t[0]=="setphaseus" && n>=3){
    int chn = t[1].toInt();
    if(chn<1 || chn>4){ Serial.println("bad ch"); return; }
    uint32_t pus = strtoul(t[2].c_str(), nullptr, 10);
    if (pus > MAX_US) pus = MAX_US;   // 上限クリップ
    ch[chn-1].phase_us = pus;
    if(ch[chn-1].running){
      // 次エッジの基準を更新（今すぐ位相だけ反映する）
      ch[chn-1].next_us = micros() + ch[chn-1].phase_us;
    }
    print_state(); 
    return;
  }

  if(t[0]=="preset" && n>=2){
    if(t[1]==def_position){ apply_preset_us(PRESET_POSITION_US,def_position); currentPreset = def_position; }
    else if(t[1]==def_hazard){ apply_preset_us(PRESET_TURN_US,def_hazard);   currentPreset = def_hazard; }
    else if(t[1]==def_ess){  apply_preset_us(PRESET_ESS_US,def_ess);    currentPreset = def_ess; }
    else if(t[1]==def_l_turn){  apply_preset_us(PRESET_L_TURN_US,def_l_turn);    currentPreset = def_l_turn; }
    else if(t[1]==def_r_tuurn){  apply_preset_us(PRESET_R_TURN_US,def_r_tuurn);    currentPreset = def_r_tuurn; }
    else { Serial.println("unknown preset"); return; }
    print_state(); return;
  }

  if(t[0]=="autostart" && n>=2){
    if(t[1]=="on") autostart = true;
    else if(t[1]=="off") autostart = false;
    else { Serial.println("use: autostart on|off"); return; }
    print_state(); return;
  }

  if(t[0]=="leftmask" && n>=2 && t[1].startsWith("b")){
    left_mask = strtoul(t[1].c_str()+1, nullptr, 2); print_state(); return;
  }
  if(t[0]=="rightmask" && n>=2 && t[1].startsWith("b")){
    right_mask = strtoul(t[1].c_str()+1, nullptr, 2); print_state(); return;
  }

  if(t[0]=="save"){ saveSettings(); Serial.println("saved."); return; }
  if(t[0]=="state"){ print_state(); return; }

  // 単位切替： unit ms | us
  if(t[0]=="unit" && n>=2){
    if(t[1]=="ms") uiUnit = UNIT_MS;
    else if(t[1]=="us") uiUnit = UNIT_US;
    else { Serial.println("use: unit ms|us"); return; }

    if (uiUnit == UNIT_MS) {
      int i = nearest_ms_index(enc_step_us);
      enc_step_us = (uint32_t)STEP_MS_LIST[i] * 1000UL;
    } else {
      int i = nearest_us_index(enc_step_us);
      enc_step_us = STEP_US_LIST[i];
    }

    Serial.printf("unit=%s\n", uiUnit==UNIT_MS?"ms":"us");
    return;
  }

  // エンコーダの1クリック増分： step <value>  (単位は現在の unit に従う)
  if(t[0]=="step" && n>=2){
    uint32_t v = strtoul(t[1].c_str(), nullptr, 10);
    if(v==0) v=1;
    enc_step_us = (uiUnit==UNIT_MS) ? ms2us(v) : v;
    Serial.printf("step=%lu %s\n", (unsigned long)(uiUnit==UNIT_MS? v: v), uiUnit==UNIT_MS?"ms":"us");
    return;
  }

  // 個別設定： setus CH ONus OFFus  （µsで直接）
  // setus CH ONus OFFus  ← us入力は [1..MAX_US] に丸め
  if(t[0]=="setus" && n>=4){
    int chn = t[1].toInt(); if(chn<1||chn>4){ Serial.println("bad ch"); return; }
    uint32_t onus  = clamp_us(strtoul(t[2].c_str(), nullptr, 10));
    uint32_t offus = clamp_us(strtoul(t[3].c_str(), nullptr, 10));
    ch[chn-1].on_us  = onus;
    ch[chn-1].off_us = offus;
    if(ch[chn-1].running) ch[chn-1].next_us = micros() + 1000;
    print_state(); return;
  }

  // 既存の set CH ONms OFFms も生かす（ms→us換算）
  // set CH ONms OFFms  ← ms入力は parse_u32 で [MIN_MS..MAX_MS] に丸め
  if(t[0]=="set" && n>=4){
    int chn=t[1].toInt(); if(chn<1||chn>4){ Serial.println("bad ch"); return; }
    uint32_t onms, offms;
    if(!parse_u32(t[2].c_str(), onms) || !parse_u32(t[3].c_str(), offms)){ Serial.println("bad ms"); return; }
    ch[chn-1].on_us = ms2us(onms);
    ch[chn-1].off_us= ms2us(offms);
    if(ch[chn-1].running) ch[chn-1].next_us = micros() + 1000;
    print_state(); return;
  }

  if(t[0]=="add"){
    if(n>=2){
      if(t[1]=="all"){
        start_with_mask(0b1111);
      }else if(t[1]=="left"){
        start_with_mask(left_mask);
      }else if(t[1]=="right"){
        start_with_mask(right_mask);
      }else if(t[1]=="hazard"){
        start_with_mask((uint8_t)(left_mask | right_mask));
      }else{
        int chn=t[1].toInt();
        if(chn>=1&&chn<=4) start_channel(chn-1);
        else { Serial.println("bad arg"); return; }
      }
    }else{
      Serial.println("usage: add all|left|right|hazard|1..4");
      return;
    }
    updateModeFromRunning();
    print_state();
    return;
  }

  if (t[0]=="mute" && n>=2) {
    if (t[1]=="all") {
      for (int i=0;i<4;i++){ ch[i].muted = true;  apply_level(i, ch[i].level); }
      Serial.println("muted all");
    } else {
      int chn = t[1].toInt();
      if (chn<1 || chn>4){ Serial.println("bad ch"); return; }
      ch[chn-1].muted = true;
      apply_level(chn-1, ch[chn-1].level);   // 即LOWに反映
      Serial.printf("CH%d muted\n", chn);
    }
    print_state(); 
    return;
  }

  if (t[0]=="unmute" && n>=2) {
    if (t[1]=="all") {
      for (int i=0;i<4;i++){ ch[i].muted = false; apply_level(i, ch[i].level); }
      Serial.println("unmuted all");
    } else {
      int chn = t[1].toInt();
      if (chn<1 || chn>4){ Serial.println("bad ch"); return; }
      ch[chn-1].muted = false;
      apply_level(chn-1, ch[chn-1].level);   // 直ちに状態に復帰
      Serial.printf("CH%d unmuted\n", chn);
    }
    print_state();
    return;
  }

}

// ====== ボタン&エンコーダ ======
#define PIN_BTN_START   26
#define PIN_BTN_PRESET  27
// ---- エンコーダ安定版 ----
#define PIN_ENC_A 32
#define PIN_ENC_B 33
#define PIN_ENC_SW 25

volatile int32_t enc_ticks = 0;
volatile uint8_t enc_state = 0;      // 現在のAB 2bit
volatile uint32_t enc_last_us = 0;

// 2bit遷移テーブル（00,01,11,10）間の合法移動のみカウント
const int8_t ENC_LUT[16] = {
/* 00->00 01 11 10 */   0,  +1,   0,  -1,
/* 01->00 01 11 10 */  -1,   0,  +1,   0,
/* 11->00 01 11 10 */   0,  -1,   0,  +1,
/* 10->00 01 11 10 */  +1,   0,  -1,   0
};

inline uint8_t readAB() {
  uint8_t a = (uint8_t)digitalRead(PIN_ENC_A);
  uint8_t b = (uint8_t)digitalRead(PIN_ENC_B);
  return (a << 1) | b;
}

void IRAM_ATTR enc_isr() {
  uint32_t now = micros();
  if (now - enc_last_us < 2000) return; // ≈2msデバウンス（1〜3msで調整可）
  enc_last_us = now;

  uint8_t prev = enc_state;
  uint8_t curr = readAB();
  enc_state = curr;

  int8_t step = ENC_LUT[(prev << 2) | curr];
  portENTER_CRITICAL_ISR(&encMux);
  enc_ticks += step;
  portEXIT_CRITICAL_ISR(&encMux);
}

// デバウンス用
struct Btn {
  int pin;
  bool last;
  uint32_t t_last;
} btnStart{PIN_BTN_START, true, 0}, btnPreset{PIN_BTN_PRESET, true, 0}, btnEnc{PIN_ENC_SW, true, 0};

bool readBtn(Btn& b, bool& fell, bool& rose){
  // プルアップ前提：押下でLOW
  bool now = digitalRead(b.pin);
  fell = rose = false;
  uint32_t t=millis();
  if(now != b.last && (t - b.t_last) > 20){ // 20msデバウンス
    if(now==LOW && b.last==HIGH) fell=true;
    if(now==HIGH && b.last==LOW) rose=true;
    b.last = now; b.t_last = t;
    return true;
  }
  return false;
}

// ====== セットアップ／ループ ======
void setup(){
  Serial.begin(115200);

  for(int i=0;i<4;i++){
    ch[i].pin = PINS[i];
    pinMode(ch[i].pin, OUTPUT);
    ch[i].on_us  = PRESET_TURN_US.on_us;
    ch[i].off_us = PRESET_TURN_US.off_us;
    ch[i].phase_us = 0;
    ch[i].muted = false;
    stop_channel(i);
  }

  // UIピン
  pinMode(PIN_BTN_START,  INPUT_PULLUP);
  pinMode(PIN_BTN_PRESET, INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  enc_state = readAB();  // 初期状態を保存
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), enc_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), enc_isr, CHANGE);

  // OLED
  oledInit();

  // NVSロード
  loadSettings();

  // 自動スタート
  if(autostart){ for(int i=0;i<4;i++) start_channel(i); }
  updateModeFromRunning();

  Serial.println("FunctionGen ready. type 'help'");
}

void loop(){
  update_channel_timers();
  oledDraw();

  // シリアル入力
  static String buf;
  while(Serial.available()){
    char c = Serial.read();
    if(c=='\r') continue;
    if(c=='\n'){ handle_command(buf); buf=""; }
    else { buf += c; if(buf.length()>200) buf=""; }
  }

  // ボタン処理
  bool fell, rose;

  // Start/Stop トグル（BTN26）
  if(readBtn(btnStart, fell, rose) && fell){
    bool anyRunning=false; for(int i=0;i<4;i++) anyRunning |= ch[i].running;
    if(anyRunning){ for(int i=0;i<4;i++) stop_channel(i); }
    else { for(int i=0;i<4;i++) start_channel(i); }
  }

  // --- Preset切替（GPIO27）：短押し＝プリセット循環 / 長押し＝AutoStartトグル ---
  static bool     preset_down    = false;
  static bool     preset_held    = false;
  static uint32_t preset_down_at = 0;

  bool preset_now = (digitalRead(PIN_BTN_PRESET) == LOW); // 押すとLOW（INPUT_PULLUP）

  if (preset_now && !preset_down) {
    // 押し始め
    preset_down    = true;
    preset_held    = false;
    preset_down_at = millis();
  }

  // 押しっぱなし中の監視（ここがポイント！）
  if (preset_now && preset_down && !preset_held) {
    if (millis() - preset_down_at >= 800) {
      // 長押し判定：AutoStartトグル
      autostart = !autostart;
      preset_held = true;   // 1回だけ反応
    }
  }

  // 離したときの処理
  if (!preset_now && preset_down) {
    // 離したタイミングで短押し判定
    if (!preset_held) {
      Serial.println("pushed");
      // 短押し：プリセット循環
      if (String(currentPreset) == def_hazard) {
        apply_preset_us(PRESET_ESS_US,def_ess);      currentPreset = def_ess;
      } else if (String(currentPreset) == def_ess) {
        apply_preset_us(PRESET_POSITION_US,def_position); currentPreset = def_position;
      } else if(String(currentPreset) == def_position){
        apply_preset_us(PRESET_L_TURN_US,def_l_turn);     currentPreset = def_l_turn;
      } else if(String(currentPreset) == def_l_turn){
        apply_preset_us(PRESET_R_TURN_US,def_r_tuurn);     currentPreset = def_r_tuurn;
      } else{
        apply_preset_us(PRESET_TURN_US,def_hazard);     currentPreset = def_hazard;
      }
      Serial.println(currentPreset);
    }
    // 状態リセット
    preset_down = false;
    preset_held = false;
  }

  // --- エンコーダ回転の取り出し＆反映（1tick=1ms） ---
  int32_t ticks;
  portENTER_CRITICAL(&encMux);
  ticks = enc_ticks;
  enc_ticks = 0;
  portEXIT_CRITICAL(&encMux);
  if (ticks != 0) {
    uint32_t now = millis();
    uint32_t dt = now - lastEncMove;
    lastEncMove = now;

    // 加速倍率（Step編集には使わない）
    uint8_t accel = 1;
    if (dt < 50) accel = 4;
    else if (dt < 120) accel = 2;

    if (editTarget == EDIT_CH) {
      // CH選択を回転で 1..4 を循環
      int v = selectedCh + (ticks > 0 ? 1 : -1);
      if (v < 0) v = 3; if (v > 3) v = 0;
      selectedCh = v;

    } else if (editTarget == EDIT_STEP) {
      // ★STEP 候補を左右に回す（加速なし / 1tick=1候補）
      if (uiUnit == UNIT_MS) {
        int i = nearest_ms_index(enc_step_us);
        int n = (int)(sizeof(STEP_MS_LIST)/sizeof(STEP_MS_LIST[0]));
        int dir = (ticks > 0) ? 1 : -1;
        i = (i + dir + n) % n;
        enc_step_us = (uint32_t)STEP_MS_LIST[i] * 1000UL;
      } else {
        int i = nearest_us_index(enc_step_us);
        int n = (int)(sizeof(STEP_US_LIST)/sizeof(STEP_US_LIST[0]));
        int dir = (ticks > 0) ? 1 : -1;
        i = (i + dir + n) % n;
        enc_step_us = STEP_US_LIST[i];
      }

    } else {
      // ON / OFF を選択CHだけ編集（加速あり）
      int i = selectedCh;
      int32_t val = (editTarget==EDIT_ON) ? (int32_t)ch[i].on_us : (int32_t)ch[i].off_us;
      int32_t delta = ticks * (int32_t)enc_step_us * accel;
      val += delta;
      if (val < 1) val = 1;
      if (val > (int32_t)MAX_US) val = (int32_t)MAX_US;
      if (editTarget==EDIT_ON) ch[i].on_us = (uint32_t)val;
      else                     ch[i].off_us= (uint32_t)val;
      if (ch[i].running) ch[i].next_us = micros() + 1000;
    }
  }

  // --- エンコーダSW（GPIO25）：単/複/三クリック & 長押し ---
  static bool     enc_down    = false;
  static bool     enc_held    = false;
  static uint32_t enc_down_at = 0;

  // クリック数カウント方式に変更
  static uint32_t click_window_start = 0;
  static uint8_t  click_count        = 0;

  static uint32_t enc_last_edge_ms = 0; // 30ms デバウンス

  bool enc_now = (digitalRead(PIN_ENC_SW) == LOW);  // 押すとLOW

  uint32_t nowms = millis();
  if (nowms - enc_last_edge_ms < 30) {
    // 物理バウンス無視
  } else {
    enc_last_edge_ms = nowms;

    if (enc_now && !enc_down) {
      // 押し始め
      enc_down    = true;
      enc_held    = false;
      enc_down_at = nowms;
    }

    // 長押し：0.8s
    if (enc_now && enc_down && !enc_held) {
      if (nowms - enc_down_at >= 800) {
        // 長押し成立 → 保存＆SAVED!
        saveSettings();
        showSavedBanner(1200);
        enc_held = true;

        // 長押し成立時はクリック系列をリセット
        click_count = 0;
        click_window_start = 0;
      }
    }

    // 離した
    if (!enc_now && enc_down) {
      if (!enc_held) {
        // クリックカウント
        if (click_count == 0) {
          click_window_start = nowms;
        }
        click_count++;
      }
      enc_down = false;
      enc_held = false;
    }
  }

  // クリック確定（350ms内に連続でカウント）
  if (click_count > 0 && (millis() - click_window_start > 350)) {
    uint8_t n = click_count;
    click_count = 0;

    if (n >= 3) {
      // ★（不要になったが、万が一残っても無視してOK）
      // 以前はトリプルクリックで stepcycle_next() していた
      // ここは何もしない or 好きなら stepcycle_next();
    } else if (n == 2) {
      // ダブルクリック：Unit 切替（スナップ付き）
      uiUnit = (uiUnit == UNIT_MS) ? UNIT_US : UNIT_MS;
      Serial.printf("unit=%s\n", uiUnit==UNIT_MS?"ms":"us");
      if (uiUnit == UNIT_MS) {
        int i = nearest_ms_index(enc_step_us);
        enc_step_us = (uint32_t)STEP_MS_LIST[i] * 1000UL;
      } else {
        int i = nearest_us_index(enc_step_us);
        enc_step_us = STEP_US_LIST[i];
      }
    } else {
      // ★シングルクリック：ON→OFF→CH→STEP→ON...
      if      (editTarget == EDIT_ON)   editTarget = EDIT_OFF;
      else if (editTarget == EDIT_OFF)  editTarget = EDIT_CH;
      else if (editTarget == EDIT_CH)   editTarget = EDIT_STEP;
      else                              editTarget = EDIT_ON;
    }
  }
}
