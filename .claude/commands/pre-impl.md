Perform pre-implementation investigation for an AuroraBlink issue before writing any code.

**Usage:** `/pre-impl <issue number or task description>`
**Language: Always respond to the user in Japanese.**

---

## Purpose

Claude Code tends to treat the issue description as the complete list of things to do. This skill exists to catch the changes that are NOT written in the issue — callers that need updating, settings fields that need persisting, menu items that need adding, test cases that need extending — before implementation begins rather than after.

---

## Process

### Step 1 — Understand the task

Read the argument.

If no argument is provided, respond immediately in Japanese:
「使用方法: /pre-impl <Issue番号 または タスクの説明>」
Do not proceed further.

If the argument is a number, fetch the issue:
```
gh issue view <N> --repo uchiuno-shihaisiya/AuroraBlink --json title,body,comments
```
If the command fails or returns an error (e.g. "issue not found", non-zero exit), stop and report: 「Issue #N が見つかりませんでした。番号を確認してください。」 Do not proceed.

If the argument is a free-form description, treat it directly as the task summary and skip the gh command.

Extract and summarize:
- **What is changing:** new feature / parameter / behavior / bug fix / refactor
- **Locations explicitly named in the task:** specific files or functions mentioned
- **Keywords for cross-project search:** type names, constant names, function names, enum values, config keys — anything that will appear in related code

---

### Step 2 — Decide whether to proceed

Evaluate whether the full investigation is warranted.

**Skip the investigation if Rule A or Rule B is met:**

**Rule A — ALL of the following are true:**
- The change is clearly contained in 1–2 files with no cross-layer ripple
- The task description names the exact files and no other layer is plausibly affected
- The change is a text correction, constant tweak, or config-only modification that does not touch control flow

**Rule B:**
- The change is a pure internal refactor with no public interface or behavior change visible to any caller

**Proceed if ANY of the following is true:**
- A new signal type, pattern kind, or LED behavior is introduced
- A new settings field is added (must propagate through NVS, defaults, menu, web UI)
- A new LED mode is introduced (mode_manager, led_driver, system_state, menu_ui all need updating)
- An existing hardcoded value is being made dynamic or configurable
- The issue names one file but its callers or callees are plausibly affected
- Changes span more than one module boundary
- An existing public function or struct changes its signature, return type, or field layout

If none of the skip rules and none of the proceed bullets apply, default to proceeding with the full investigation.

**If skipping, present this message and wait:**
```
## 事前準備スキルの実施判断

このタスクについて、事前準備スキルの実施は **不要** と判断しました。

**理由：**
{不要と判断した具体的な理由}

**変更の見通し：**
{変更が限定的である根拠}

このまま実装に進んでよいですか？
それとも念のためスキルを実施しますか？（「実施する」とお伝えください）
```

If the user replies "実施する", continue from Step 3. Any other reply ends the skill and proceeds to implementation.

---

### Step 3 — Map the processing flow

Inform the user before starting:
「[issue title] の事前調査を開始します。コードベース横断検索を含むため、しばらく時間がかかります。」
(For free-form descriptions, substitute a brief summary of the task for `[issue title]`.)

Draw the relevant data and control flow for the changed area, annotating which layers are touched by this change.

The standard AuroraBlink firmware architecture is:

```
[Signal Input layer]
  signal_input.c — GPIO pulse detection, debouncing, classification
  Output: pattern_event_t → input_snapshot_t
  ↓
[State Resolution layer]
  system_state.c — priority, playback slots, timing
  Output: output_command_t
  ↓
[LED Driver layer]
  led_driver.c — mode rendering, color/timing computation, preview management
  mode_manager.c — mode enum and display names
  Output: pixel data via led_backend API
  ↓
[LED Backend layer]
  led_backend.c / led_backend_ws2812b.c — hardware abstraction, RMT output
  ↓
[Hardware: WS2812B / WS2815 LED strips]

[Persistent Config]
  settings.c ↔ NVS flash — app_settings_t struct, load/save/validate/defaults
  app_config.h — compile-time defaults and constants

[User Interface]
  menu_ui.c — button input, menu state machine, settings editing
  oled_display.c — OLED rendering

[Web Interface]
  web_ui.c — HTTP server, JSON API, color/settings preview

[Orchestration]
  app_main.c — FreeRTOS task creation and main control loop
```

For each layer, explicitly state: **touched / not touched / needs investigation**.

Key cross-cutting questions to answer at this step:
- If a new enum value or pattern kind is introduced, does `system_state.c` need a new handling path? Does `led_driver.c` need a new render case?
- If a new settings field is added, is it in the NVS struct version? Does `settings_validate()` need a normalization rule? Does the menu need a new edit screen? Does the web UI need a new JSON key?
- If a new LED mode is added, does `mode_manager.c` need a new enum value and name? Does `led_driver_apply()` need a new branch? Does `menu_ui.c` list modes for selection?
- If signal classification logic changes, are the tolerance constants in `app_config.h` the right place for the new values?
- If a new firmware module is added or an existing module gains a new ESP-IDF dependency, check whether `test/host/stubs/` already provides a stub for that header. If not, a new stub file will be needed.
- Does any change affect ISR-context code (`signal_input.c` ISR, `esp_timer` callbacks)? If so, flag all of the following:
  - No heap allocation (`malloc`/`free`) in the ISR path
  - No blocking calls (`vTaskDelay`, mutex lock, etc.)
  - Variables shared between ISR and task context must be accessed under `portENTER_CRITICAL_ISR` / `portEXIT_CRITICAL_ISR`
  - Any function called from ISR must be declared `IRAM_ATTR`

---

### Step 4 — Cross-project keyword search

Use the keywords from Step 1 to search the entire codebase. The goal is to find locations the issue did NOT mention.

Run the firmware and test searches for all keywords in a single parallelized call to reduce round-trips. Combine both paths in one grep command or run all per-keyword commands in parallel.

```bash
# Search firmware source and headers
grep -rn "<keyword>" firmware/esp32/main/ --include="*.c" --include="*.h"

# Search test files separately
grep -rn "<keyword>" test/ --include="*.c" --include="*.h"

# Search for related hardcoded constants or enum values
grep -rn "<related_constant_or_value>" firmware/esp32/main/ --include="*.c" --include="*.h"
```

Run searches for each keyword. Do not summarize results — read the surrounding code for every match and judge:
- Is this a location that needs updating?
- Is this a hardcoded value that will break or become inconsistent if the change is applied elsewhere?
- Is this a caller or callee that is affected but not named in the issue?

**Specific patterns to look for in this codebase:**
- Hardcoded pulse timing constants in `app_config.h` — if new signal timings are introduced, are there existing constants that follow the same pattern?
- `switch` statements over `aurora_mode_t` or `pattern_kind_t` — every new enum value needs a case in every such switch
- `app_settings_t` struct usage — `settings_validate()`, `settings_set_defaults()`, NVS serialization struct versions, menu edit screens, web UI JSON handlers
- `g_left_slot` / `g_right_slot` playback logic in `system_state.c` — new pattern kinds may need special handling
- `led_driver_apply()` branch logic — new modes or pattern kinds need render paths
- Test stubs and host-side tests in `test/host/` — new public functions may need coverage

---

### Step 5 — Produce the change file list

Based on Steps 3 and 4, list every file that needs changing, grouped by layer. Use this format:

```
## 変更ファイル一覧

### シグナル入力層
- [ ] firmware/esp32/main/signal_input.c — {変更内容の一言説明}
- [ ] firmware/esp32/main/signal_input.h — {変更内容の一言説明}

### 設定・定数
- [ ] firmware/esp32/main/app_config.h — {変更内容の一言説明}
- [ ] firmware/esp32/main/settings.c — {変更内容の一言説明}
- [ ] firmware/esp32/main/settings.h — {変更内容の一言説明}

### 状態解決層
- [ ] firmware/esp32/main/system_state.c — {変更内容の一言説明}
- [ ] firmware/esp32/main/system_state.h — {変更内容の一言説明}

### LEDドライバ層
- [ ] firmware/esp32/main/mode_manager.c — {変更内容の一言説明}
- [ ] firmware/esp32/main/led_driver.c — {変更内容の一言説明}

### ユーザーインターフェース
- [ ] firmware/esp32/main/menu_ui.c — {変更内容の一言説明}
- [ ] firmware/esp32/main/oled_display.c — {変更内容の一言説明}

### Web インターフェース
- [ ] firmware/esp32/main/web_ui.c — {変更内容の一言説明}

### テスト
- [ ] test/host/test_<module>.c — {変更内容の一言説明}

### !! Issueに記載されていないが対応が必要な箇所
- [ ] {ファイルパス} — {なぜ対応が必要かの説明}
```

Omit layers with no changes. If there are no unlisted-but-required files, omit the `!!` section entirely. If the `!!` section has entries, highlight them with **!!** and always include the reason.

---

### Step 6 — Confirm with user and begin implementation

Present the change file list and request confirmation:

```
## 実装前準備 完了

上記の変更ファイル一覧で実装を進めてよいですか？

確認いただきたい点：
- Issueに記載されていない対応箇所がある場合、その対応方針に問題はないか
- 変更ファイル一覧に漏れや過剰な変更がないか

「進めてください」または修正指示をいただければ実装を開始します。
```

Wait for the user's reply before writing any code.
- If approved ("進めてください" or equivalent), begin implementation.
- If corrections are given, update the list and ask again. If corrections have been exchanged more than twice without approval, present the current list and ask: 「この内容で確定してよいですか？はい / いいえでお答えください。」

---

## Rules

- Read actual code before making any claim. Never infer from file names or function signatures alone.
- Run every keyword search fully. Do not skip results or summarize without reading.
- Start from "what does the system need" rather than "what does the issue say". The issue is a starting point, not a complete specification.
- When in doubt about whether a location needs changing, read it and decide — do not omit it from the investigation.
