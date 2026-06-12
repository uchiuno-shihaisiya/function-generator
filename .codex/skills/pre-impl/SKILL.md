---
name: pre-impl
description: Use when the user asks Codex to investigate the impact scope of a change before implementation begins; fetches the issue, maps affected AuroraBlink firmware layers, runs a cross-project keyword search, produces a layer-by-layer change file list, and waits for user approval before any code is written.
---

# AuroraBlink Pre-Implementation Investigation

Use this skill when the user asks Codex to investigate before implementing a change, or when a task touches more than one or two files and the full scope is unclear.

**Language: Always respond to the user in Japanese.**

## Purpose

Codex tends to treat the issue description as the complete list of things to do. This skill exists to catch the changes that are NOT written in the issue — callers that need updating, settings fields that need persisting, menu items that need adding, test cases that need extending — before implementation begins rather than after.

## References

Load these reference files as needed:

- `references/architecture.md` — AuroraBlink firmware layer diagram and cross-cutting concern checklist

## Process

### Step 1 — Understand the task

Read the argument.

If no argument is provided, respond immediately in Japanese:
「使用方法: pre-impl <Issue番号 または タスクの説明>」
Do not proceed further.

If the argument is a number, fetch the issue. Prefer GitHub connector tools; use `gh` as fallback:
```
gh issue view <N> --repo uchiuno-shihaisiya/AuroraBlink --json title,body,comments
```
If the fetch fails or the issue is not found, stop and report in Japanese: 「Issue #N が見つかりませんでした。番号を確認してください。」 Do not proceed.

If the argument is a free-form description, treat it directly as the task summary and skip the issue fetch.

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
- The task description names one file but its callers or callees are plausibly affected
- Changes span more than one module boundary
- An existing public function or struct changes its signature, return type, or field layout

If none of the skip rules and none of the proceed bullets apply, default to proceeding with the full investigation.

**If skipping, present this message and wait:**
```
## 事前準備の実施判断

このタスクについて、事前準備の実施は **不要** と判断しました。

**理由：**
{不要と判断した具体的な理由}

**変更の見通し：**
{変更が限定的である根拠}

このまま実装に進んでよいですか？
それとも念のため調査を実施しますか？（「実施する」とお伝えください）
```

If the user replies "実施する", continue from Step 3. Any other reply ends the skill and proceeds to implementation.

---

### Step 3 — Map the processing flow

Before starting, inform the user:
「[issue title / task summary] の事前調査を開始します。コードベース横断検索を含むため、しばらく時間がかかります。」
(For free-form descriptions, substitute a brief summary of the task for the title.)

Load `references/architecture.md`.

For each layer in the architecture diagram, explicitly state: **touched / not touched / needs investigation**.

Answer every cross-cutting question in `references/architecture.md` that is relevant to this change.

---

### Step 4 — Cross-project keyword search

Use the keywords from Step 1 to search the entire codebase. The goal is to find locations the task description did NOT mention.

Run searches for all keywords in a single parallelized call to reduce round-trips:

```bash
grep -rn "<keyword>" firmware/esp32/main/ test/ --include="*.c" --include="*.h"
```

Do not summarize results — read the surrounding code for every match and judge:
- Is this a location that needs updating?
- Is this a hardcoded value that will break or become inconsistent if the change is applied elsewhere?
- Is this a caller or callee that is affected but not named in the task description?

**Specific patterns to look for in this codebase:**
- Hardcoded pulse timing constants in `app_config.h` — new signal timings should follow the existing pattern
- `switch` statements over `aurora_mode_t` or `pattern_kind_t` — every new enum value needs a case in every such switch
- `app_settings_t` struct usage — `settings_validate()`, `settings_set_defaults()`, NVS struct versions, menu edit screens, web UI JSON handlers
- `g_left_slot` / `g_right_slot` playback logic in `system_state.c` — new pattern kinds may need special handling
- `led_driver_apply()` branch logic — new modes or pattern kinds need render paths
- Test stubs and host-side tests in `test/host/` — new public functions may need coverage

---

### Step 5 — Produce the change file list

Based on Steps 3 and 4, list every file that needs changing, grouped by layer:

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
- Start from "what does the system need" rather than "what does the task description say". The task description is a starting point, not a complete specification.
- When in doubt about whether a location needs changing, read it and decide — do not omit it from the investigation.
