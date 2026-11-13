# ISF15ACP4 ESP32 examples

ESP-IDF applications that run the ISF15ACP4 driver on an ESP32-C6, built by CI on every pull request.

## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | ISF15ACP4 | Notes |
| --- | --- | --- |
| GPIO6 | SCLK | SPI2, mode 0, 4 MHz in the example (up to 6.6 MHz) |
| GPIO7 | SDIN (MOSI) | There is no MISO: the bus is write only |
| GPIO10 | CS# | Driven by spi_master, one transfer per command or data run |
| GPIO3 | D/C# | Low for a command, high for pixel data |
| GPIO11 | RES# | Idles high |
| GPIO4 | Enable of the 15 V OLED supply | Active high; idles low (supply off) |
| GPIO5 | Switch | To ground with the internal pull-up; closed reads low |
| 3V3 | VDD | 2.4 to 3.5 V, 100 nF to GND at the pin |
| 15 V supply | VCC (module pin 9) | Switched by the enable above; its own regulator, not from the ESP32 |
| GND | GND | |

The driver never switches the OLED supply on before it has reset and configured the controller. The enable pin
must idle low through the ESP32's reset and boot: use a pull-down on the enable of the 15 V regulator.

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh isf15acp4_basic_example Debug
./scripts/flash_app.sh flash_monitor isf15acp4_basic_example Debug
```

`idf.py menuconfig` shows "ISF15ACP4 OLED pushbutton" under Component config: the module, the master current, the
colour wiring, the switch polarity and times, and the idle dim and sleep times that `isf15acp4::KconfigDefaults()`
returns.

## Expected output

```text
I (312) isf15acp4_example: ISF15ACP4 driver 0.1.0
I (330) isf15acp4: isf15acp4: supply on
```

then the test picture on the panel; a press redraws it, a long press inverts the display. The bus is write only, so a
wrong wire shows as a dark panel, never as an error: check D/C#, RES#, the supply enable and the 15 V rail first.
This example is compiled by CI; it has **not been run on hardware** and could not be built locally (no ESP-IDF). The
port uses SPI DMA because the driver sends up to 96 bytes per transfer; ESP-IDF's `spi_master` limits a transfer to
64 bytes without DMA (from the ESP-IDF documentation, not checked here).

## Files

| File | What it is |
| --- | --- |
| [`main/isf15acp4_basic_example.cpp`](main/isf15acp4_basic_example.cpp) | The example app |
| [`main/esp32_isf15acp4_port.hpp`](main/esp32_isf15acp4_port.hpp) | The ESP-IDF port: write-only bus, D/C#, RES#, supply enable, switch, time, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
