# ESP-Base: Waveshare ESP32-S3-Touch-LCD-1.69 Template

Reusable ESP-IDF firmware template for the Waveshare ESP32-S3-Touch-LCD-1.69 with:
- LVGL baseline via `esp_lvgl_port`
- Board wiring + touch integration (rotation-aware)
- Task framework (`ui_task` + `io_task` + worker pool)
- Demo + Widgets UI screens
- Telemetry/logging scaffolding for battery/RTC/IMU/network

## Assumptions
- ESP-IDF v5.2+ is used.
- CST816S touch is available via `espressif/esp_lcd_touch_cst816s` component.
- RTC/IMU folders are scaffolds; expand low-level register drivers per your project.
- `SYS_EN` is driven high and `SYS_OUT` is input/pulled-up as a safe default bring-up strategy.

## Prerequisites
- ESP-IDF installed and exported
- Python deps for `idf.py`
- USB-C data cable (USB CDC console enabled by default)

## Build / Flash
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Pin Map (from board schematic)
- LCD SPI: `DC=4 CS=5 SCK=6 MOSI=7 RST=8 BL=15`
- I2C: `SCL=10 SDA=11`
- Touch: `RST=13 INT=14`
- RTC INT: `41`
- Battery ADC: `1`
- Buzzer: `33`
- SYS rails: `SYS_EN=35 SYS_OUT=36`

## Kconfig overview
Under `Component config -> Waveshare 1.69 Board`:
- SPI host/frequency
- I2C port/frequency
- Rotation (0/90/180/270)
- Backlight invert + default brightness
- Touch enable + INT/polling mode
- Battery enable + divider ratio constants
- Buzzer enable
- RTC/IMU enable

## Task Framework Rules
- **Only `ui_task` may call LVGL APIs**.
- `io_task` runs fast event callbacks and event-bus subscriptions.
- Worker pool executes blocking jobs (`taskmgr_submit_work`).
- Any external module updates UI with `taskmgr_post_ui(...)` (no direct LVGL calls outside UI path).

## Modules
- `board_waveshare_169`: panel/touch/I2C/backlight/battery/buzzer/safe SYS init
- `taskmgr`: queues, UI/IO/worker execution, metrics, tiny event bus
- `ui_app`: LVGL setup + Demo/Widgets screens
- `sensors`: telemetry polling task (5s) + posting UI updates
- `net`: event loop setup + async HTTP placeholder pattern

## Adding a new module
1. Create component (`components/<name>`) with `include/<name>.h` and source.
2. Use `taskmgr_post_io` for short callbacks.
3. Use `taskmgr_submit_work` for blocking I/O.
4. Push UI data through `taskmgr_post_ui` only.
5. If useful across modules, publish/subscribe on `taskmgr_topic_t`.

## Reusing this template
1. Rename project in root `CMakeLists.txt`.
2. Keep folder structure; replace board component if using different hardware.
3. Update pin map + Kconfig defaults in `board_waveshare_169`.
4. Extend `sensors` and `net` with real protocol/device logic.

## Updating pinned component versions
Edit `main/idf_component.yml` with explicit versions:
- `lvgl/lvgl`
- `espressif/esp_lvgl_port`
- `espressif/esp_lcd_touch_cst816s`
Then run `idf.py reconfigure` / `idf.py build`.

## Troubleshooting
- Touch inverted or axis swapped: change board rotation Kconfig and rebuild.
- Backlight too dim/inverted: toggle backlight invert Kconfig.
- No serial logs over USB-C: confirm `CONFIG_ESP_CONSOLE_USB_CDC=y` and cable supports data.
- If touch module missing, UI remains functional and shows `TOUCH: missing`.
