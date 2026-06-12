# AuroraBlink Firmware Architecture

## Layer Diagram

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

## Cross-Cutting Concerns

Answer every question that is relevant to the change under investigation.

### Enum / pattern kind changes
- If a new enum value or pattern kind is introduced, does `system_state.c` need a new handling path?
- Does `led_driver.c` need a new render case?
- Do all `switch` statements over `aurora_mode_t` or `pattern_kind_t` cover the new value?

### Settings propagation
- If a new settings field is added, is it included in the NVS serialization struct and bumped to the next version?
- Does `settings_validate()` need a normalization rule for the new field?
- Does `settings_set_defaults()` need a default value?
- Does `menu_ui.c` need a new edit screen?
- Does `web_ui.c` need a new JSON key in its HTTP handlers?

### LED mode changes
- If a new LED mode is added, does `mode_manager.c` need a new enum value and display name?
- Does `led_driver_apply()` need a new branch?
- Does `menu_ui.c` list modes for user selection?

### Signal classification
- If signal classification logic changes, are the timing and tolerance constants in `app_config.h` the right location for new values?
- Does `system_state.c` handle the new classification result in `g_left_slot` / `g_right_slot` playback logic?

### Test coverage
- If a new firmware module is added or an existing module gains a new ESP-IDF dependency, does `test/host/stubs/` already provide a stub for the required header? If not, a new stub file must be created.
- Do new public functions need test cases in `test/host/`?

### ISR safety
- Does the change affect ISR-context code (`signal_input.c` ISR or `esp_timer` callbacks)?
- If yes, flag all of the following constraints:
  - No heap allocation (`malloc`/`free`) in the ISR path
  - No blocking calls (`vTaskDelay`, mutex lock, etc.)
  - Variables shared between ISR and task context must be accessed under `portENTER_CRITICAL_ISR` / `portEXIT_CRITICAL_ISR`
  - Any function called from ISR must be declared `IRAM_ATTR`
