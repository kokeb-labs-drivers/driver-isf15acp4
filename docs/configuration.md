# Configuration

`isf15acp4::Config` holds every setting. `Config{}` is the ISF15ACP4's configuration; `PresetFor()` gives the other
modules'.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Switch circuit and contact data: NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed) none printed, Switch specifications (sheet E5), [p. 2](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=2)
    - Gray scale decoder: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.6 Gray Scale Decoder, [p. 21](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=21)
    - Contrast, master current and second pre-charge speed: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.3 to 9.1.5, [p. 36](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=36)
    - Re-map and data format: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.6 Set Re-map and Data Format (A0h), [p. 37](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=37)
    - Display start line, offset, mode and multiplex ratio: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.7 to 9.1.10, [p. 42](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=42)
    - Power save, phase periods, clock and pre-charge: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.14 to 9.1.20, [p. 46](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=46)
    - Variant geometry and column offsets: NKK Switches ISC15ANP4 (sheets E4 to E9, no number printed) none printed, Block diagram and pin configurations (sheet E6), [p. 3](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf#page=3)
    - Display-only variant: NKK Switches ISC01P (sheets E4 to E9, no number printed) none printed, Switch and OLED specifications (sheets E5, E6), [p. 2](https://www.nkkswitches.com/pdf/ISC01P.pdf#page=2)
    - Start-up values of each module: NKK Switches OLED application notes none printed (web page), Initialization, Tables 1 to 3, steps 4.1 to 4.18 (Note 1, Note 2), p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Fields

The defaults are the ISF15ACP4's table (NKK application notes, Table 3, steps 4.1 to 4.18); `PresetFor()` gives Tables 1
and 2. **NKK Note 1: changing the set-up values (contrast, display offset, multiplex ratio, phase periods, clock, linear
gray scale, pre-charge level, VCOMH) may damage the OLED module.**

| Field | Default | Valid | Command |
| --- | --- | --- | --- |
| `variant` | `Isf15acp4` | the three modules | geometry |
| `remap` | see below | | A0h |
| `contrast_a`, `_b`, `_c` | 0x1C, 0x2E, 0x2C | 0 to 255 | 81h, 82h, 83h |
| `precharge_speed_a`, `_b`, `_c` | 0x4F, 0x74, 0x88 | 0 to 255 | 8Ah, 8Bh, 8Ch (one transfer) |
| `send_precharge_speeds` | true (false for the two smaller modules) | | whether 8Ah to 8Ch are sent: only Table 3 has them |
| `master_current` | 15 (full) | 0 to 15 | 87h |
| `start_line`, `display_offset` | 0, 0 | 0 to 63 | A1h, A2h |
| `mode` | `Normal` | normal, all on, all off, inverse | A4h to A7h |
| `dim` | 0x0E, 0x17, 0x16, pre-charge 0x12 | contrast at most the colour's `contrast_*`, pre-charge 0 to 31 | ABh |
| `power_save` | false | | B0h |
| `phase1`, `phase2` | 7, 1 | 1 to 15 | B1h |
| `clock_divide`, `clock_frequency` | 0, 15 | 0 to 15 | B3h |
| `precharge_level`, `vcomh_level` | 14, 16 | 0 to 31 | BBh, BEh |
| `precharge_bit0` | true (false for the two smaller modules) | | bit 0 of BBh: Table 3 sends 1Dh, Tables 1 and 2 send 12h |
| `linear_gray_scale` | true | | B9h |
| `clear_ram` | true | | RAM zeroed before the supply can light it |
| `red_on_segment_a` | true | | panel wiring ([Ambiguities](ambiguities.md#colour-order)) |
| `writes_while_scrolling` | false | | allow RAM writes during a scroll |
| `vcc_settle_ms` | 50 | 1 to 1000 | wait after the supply enable |
| `vcc_off_ms` | 100 | 0 to 1000 | wait after the supply goes off |
| `engine_settle_us` | 3000 | 0 to 100 000 | wait after a drawing command |
| `idle_dim_ms`, `idle_sleep_ms` | 0, 0 (never) | up to 6 000 000; sleep above dim | `Poll()` policy |
| `button` | see below | | the switch |

`Remap`: horizontal RAM increment, no column mirror, colour order A-B-C, COM left-right swap on, COM scan normal,
odd-even split on, 65k colour format 1.

`ButtonConfig`: active low (pull-up, closed reads low), `debounce_ms` 20 (1 to 1000), `long_press_ms` 800 (above the
debounce time, to 60 000), `repeat_interval_ms` 0 (none; else 10 to 10 000), `repeat_delay_ms` 500.

The defaults after the first three rows of timing are the module's initialisation values, not the SSD1331's reset values:
[Ambiguities](ambiguities.md#initialisation-values-and-variant-offsets) says where they come from.

## Presets

| Preset | Module | What differs from `Config{}` |
| --- | --- | --- |
| `PresetFor(Variant::Isf15acp4)` | 96 x 64, switch | none |
| `PresetFor(Variant::Isc15anp4)` | 64 x 48, switch | geometry (column offset 16, 48 rows), COM scan reversed, contrast, offset, clock, pre-charge, VCOMH |
| `PresetFor(Variant::Isc01p)` | 52 x 36, display only | geometry (column offset 22, 36 rows), as above; `Poll()` returns `Unsupported` |

```cpp
--8<-- "tests/docs/configuration.cpp:custom"
```

## Validation

`isf15acp4::Validate(config)` checks a configuration without touching the module, and `EnsureInitialized()`,
`Reinitialize()` and `ApplyConfig()` call it before any I/O. It returns `InvalidArgument` when an enum holds a value that
is not an enumerator, a level or time is outside its range, a dim contrast is above the contrast of its colour (NKK: ACh
may not exceed the start-up contrasts), the sleep time is not above the dim time, or the switch configuration fails.
`CheckedConfig` runs the same check at compile time:

```cpp
--8<-- "tests/docs/configuration.cpp:checked"
```

## Kconfig

Under ESP-IDF, `idf.py menuconfig` shows **ISF15ACP4 OLED pushbutton** when the driver is a component of the project
([Installation](installation.md)); the menu is [`_config/Kconfig`](../_config/Kconfig). The options reach the code only
through [`inc/isf15acp4_kconfig.hpp`](../inc/isf15acp4_kconfig.hpp), never through a driver signature:

```cpp
--8<-- "tests/docs/configuration.cpp:kconfig"
```

| Option | Default | `Config` field |
| --- | --- | --- |
| `CONFIG_ISF15ACP4_VARIANT_*` | `ISF15ACP4` | `variant` and its preset |
| `CONFIG_ISF15ACP4_MASTER_CURRENT` | 15 | `master_current` |
| `CONFIG_ISF15ACP4_RED_ON_SEGMENT_A` | y | `red_on_segment_a` |
| `CONFIG_ISF15ACP4_BUTTON_ACTIVE_LOW` | y | `button.active_low` |
| `CONFIG_ISF15ACP4_BUTTON_DEBOUNCE_MS` | 20 | `button.debounce_ms` |
| `CONFIG_ISF15ACP4_BUTTON_LONG_PRESS_MS` | 800 | `button.long_press_ms` |
| `CONFIG_ISF15ACP4_IDLE_DIM_MS` | 0 | `idle_dim_ms` |
| `CONFIG_ISF15ACP4_IDLE_SLEEP_MS` | 0 | `idle_sleep_ms` |

Without Kconfig (plain CMake, other platforms) `KconfigDefaults()` returns `Config{}`. Both cases are host tests.

Next: [Porting](porting.md).
