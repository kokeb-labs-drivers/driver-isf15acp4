# Datasheet map

The driver follows the ISF15ACP4 datasheet. This page says which revision, where to get it, and where each part of
the driver is described in it, so you can check any behaviour against the source. Section names and page numbers
are the datasheet's own (the printed page numbers).


<!-- generated: datasheet-map -->
- **Datasheet: Frameless OLED Pushbutton ISF15ACP4**, NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number
  printed), revision none printed (2019-10-03). Get it from the [vendor
  page](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf) or as a
  [PDF](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf).
  SHA-256 of the file used: `e40cda1fb071caa5660a0884b0213fe81689ee7f9c7266b0b311ca21a72cc06b`.
- **Datasheet: SSD1331 96RGB x 64 Dot Matrix OLED/PLED Segment/Common Driver with Controller**, Solomon Systech (copy
  from the Adafruit mirror) SSD1331, revision 1.2 (Nov 2007). Get it from the [vendor
  page](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf) or as a
  [PDF](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf).
  SHA-256 of the file used: `8477a4e837bca1909cd27e76b3d97669a10905b81bf078adfaec602629a6db0b`.
- **Datasheet: Frameless OLED Pushbutton ISC15ANP4 (variant, 64 x 48)**, NKK Switches ISC15ANP4 (sheets E4 to E9, no
  number printed), revision none printed (2019-10-03). Get it from the [vendor
  page](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf) or as a [PDF](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf).
  SHA-256 of the file used: `8c4a595ea9e1a924796523068705331fa93568cc064d7f7783d5755e7829c585`.
- **Datasheet: OLED module ISC01P (variant, 52 x 36, display only)**, NKK Switches ISC01P (sheets E4 to E9, no number
  printed), revision none printed (2019-10-03). Get it from the [vendor page](https://www.nkkswitches.com/pdf/ISC01P.pdf)
  or as a [PDF](https://www.nkkswitches.com/pdf/ISC01P.pdf).
  SHA-256 of the file used: `c150752764d7a4a4321684a7bd87128526eb6a22f6117be6a1ebdf2214f2e27d`.
- **Application note: Application Notes for the OLED Color SmartDisplay (web page)**, NKK Switches OLED application
  notes, revision none printed (web page) (not printed). Get it from the [vendor
  page](https://docs.nkkswitches.com/docs/application-notes/oled-application-notes/).
  SHA-256 of the file used: `025a091dfb89163b13cc94384af2689153abea9b72fb315b8430e7f723151954`.
- **Application note: IS-CHPMP and IS-CHPMPHP charge pump manual**, NKK Switches IS-CHPMP and IS-CHPMPHP Manual,
  revision C (0617 (as printed on every page)). Get it from the [vendor
  page](https://docs.nkkswitches.com/docs/application-notes/oled-application-notes/).
  SHA-256 of the file used: `f3d0bb690e069662f479b1966fa3d29eaaebf6a6fe1cefbf00bc429f9f2307b2`.
- **Application note: OLED Engineering Kits User Manual (web page)**, NKK Switches OLED engineering kits, revision none
  printed (web page) (not printed). Get it from the [vendor
  page](https://docs.nkkswitches.com/docs/engineering-kits/oled-engineering-kits-user-manual/).
  SHA-256 of the file used: `09b68d431d11536ecde6306f6900c3a663b802a7154141d72dc28182164f656c`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Supply voltages, current and the three power states | ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed): Recommended operating conditions (sheets E5, E7) | [p. 2](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=2) | `EnsureInitialized()`, `DisplayOn()`, `DisplayOff()`, `Deinitialize()`, `IsSupplyEnabled()` | [Hardware](hardware.md), [Contract](contract.md), [Architecture](architecture.md) |
| Switch circuit and contact data | ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed): Switch specifications (sheet E5) | [p. 2](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=2) | `ButtonConfig`, `ButtonDebouncer`, `Poll()` | [API guide](api-guide.md), [Configuration](configuration.md), [Ambiguities](ambiguities.md) |
| Module pins, switch contacts and block diagram | ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed): Block diagram and pin configurations (sheet E6) | [p. 3](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=3) | `CtrlPin`, `DataKind`, `ReadSwitch()` hook | [Hardware](hardware.md), [Quickstart](quickstart.md) |
| Serial interface framing, D/C# and chip select | SSD1331: 7.1.3 Serial Interface | [p. 17](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=17) | `SpiInterface`, `SpiPort`, `DataKind`, `kSpiMode`, `kMsbFirst`, `kContinuousWord` | [Hardware](hardware.md), [Porting](porting.md), [Architecture](architecture.md), [Limitations](limitations.md) |
| Reset circuit and reset pulse | SSD1331: 7.4 Reset Circuit; 12 AC Characteristics, Table 18 | [p. 19](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=19) | `CtrlPin::Reset`, `reg::kResetPulseMinUs`, `EnsureInitialized()` | [Hardware](hardware.md), [Contract](contract.md) |
| GDDRAM and colour depth | SSD1331: 7.5 Graphic Display Data RAM; Table 9 | [p. 20](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=20) | `ColorDepth`, `Present()`, `WriteWindow()`, `FillWindow()`, `SetPixel()` | [API guide](api-guide.md), [Architecture](architecture.md) |
| Gray scale decoder | SSD1331: 7.6 Gray Scale Decoder | [p. 21](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=21) | `SetGrayScaleTable()`, `UseLinearGrayScale()` | [API guide](api-guide.md), [Configuration](configuration.md) |
| Power on and off sequence | SSD1331: 7.9 Power ON and OFF Sequence | [p. 27](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=27) | `EnsureInitialized()`, `DisplayOn()`, `DisplayOff()`, `Deinitialize()`, `Config::vcc_settle_ms`, `Config::vcc_off_ms` | [Contract](contract.md), [Hardware](hardware.md), [Architecture](architecture.md), [Ambiguities](ambiguities.md), [Limitations](limitations.md) |
| Command table and parameter counts | SSD1331: 8 Command Table, Table 10 | [p. 28](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=28) | `inc/isf15acp4_registers.hpp`, `TraceCommand()`, `detail::ParameterCount()` | [Architecture](architecture.md), [API guide](api-guide.md) |
| Column and row window | SSD1331: 9.1.1 Set Column Address; 9.1.2 Set Row Address | [p. 35](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=35) | `WriteWindow()`, `FillWindow()` | [API guide](api-guide.md) |
| Contrast, master current and second pre-charge speed | SSD1331: 9.1.3 to 9.1.5 | [p. 36](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=36) | `SetContrast()`, `SetMasterCurrent()`, `SetPrechargeSpeed()`, `Config::contrast_a` | [Configuration](configuration.md), [API guide](api-guide.md) |
| Re-map and data format | SSD1331: 9.1.6 Set Re-map and Data Format (A0h) | [p. 37](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=37) | `Remap`, `SetRemap()`, `Config::red_on_segment_a` | [Configuration](configuration.md), [Ambiguities](ambiguities.md) |
| Display start line, offset, mode and multiplex ratio | SSD1331: 9.1.7 to 9.1.10 | [p. 42](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=42) | `SetStartLine()`, `SetDisplayOffset()`, `SetDisplayMode()`, `SetMultiplexRatio()` | [Configuration](configuration.md), [API guide](api-guide.md) |
| Dim mode, master configuration and display on and off | SSD1331: 9.1.11 to 9.1.13 | [p. 45](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=45) | `SetDimSettings()`, `DisplayDim()`, `DisplayOn()`, `DisplayOff()` | [API guide](api-guide.md), [Contract](contract.md) |
| Power save, phase periods, clock and pre-charge | SSD1331: 9.1.14 to 9.1.20 | [p. 46](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=46) | `SetPowerSave()`, `SetPhasePeriods()`, `SetClock()`, `SetPrechargeLevel()`, `SetVcomh()` | [Configuration](configuration.md) |
| Command lock | SSD1331: 9.1.22 Set Command Lock (FDh) | [p. 47](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=47) | `SetCommandLock()`, `Error::Locked` | [API guide](api-guide.md), [Contract](contract.md) |
| Drawing engine | SSD1331: 9.2.1 to 9.2.6 Graphic Acceleration Command Set | [p. 48](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=48) | `DrawLine()`, `DrawRectangle()`, `FillRectangle()`, `CopyWindow()`, `DimWindow()`, `ClearWindow()`, `Config::engine_settle_us` | [API guide](api-guide.md), [Ambiguities](ambiguities.md) |
| Continuous scrolling | SSD1331: 9.2.7 to 9.2.9 Scrolling | [p. 51](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=51) | `SetupScroll()`, `ActivateScroll()`, `DeactivateScroll()`, `Error::ScrollActive` | [API guide](api-guide.md), [Troubleshooting](troubleshooting.md) |
| Serial interface timing | SSD1331: 12 AC Characteristics, Table 21 | [p. 57](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=57) | `kMaxClockHz`, `kCsSetupNs`, `kCsHoldNs`, `kDcSetupNs`, `kDcHoldNs` | [Hardware](hardware.md), [Porting](porting.md) |
| Variant geometry and column offsets | ISC15ANP4 (sheets E4 to E9, no number printed): Block diagram and pin configurations (sheet E6) | [p. 3](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf#page=3) | `Variant::Isc15anp4`, `GeometryOf()`, `PresetFor()` | [Configuration](configuration.md), [Ambiguities](ambiguities.md) |
| Display-only variant | ISC01P (sheets E4 to E9, no number printed): Switch and OLED specifications (sheets E5, E6) | [p. 2](https://www.nkkswitches.com/pdf/ISC01P.pdf#page=2) | `Variant::Isc01p`, `HasSwitch()` | [Configuration](configuration.md), [Ambiguities](ambiguities.md) |
| Start-up values of each module | OLED application notes: Initialization, Tables 1 to 3, steps 4.1 to 4.18 (Note 1, Note 2) | p. 1 | `Config`, `PresetFor()`, `Config::send_precharge_speeds`, `Config::precharge_bit0` | [Configuration](configuration.md), [Ambiguities](ambiguities.md), [Hardware](hardware.md) |
| Supplies, VCC order and the charge pump | OLED application notes: Pin-outs; Design Considerations 1, Power Requirements VCC (16V); Part Numbers | p. 1 | `CtrlPin::VccEnable`, `EnsureInitialized()`, `Deinitialize()`, `IsSupplyEnabled()` | [Hardware](hardware.md), [Contract](contract.md) |
| Communication, SCK frequency and chip select | OLED application notes: Design Considerations 2 and 5 | p. 1 | `SpiInterface`, `kMaxClockHz`, `Write()` | [Hardware](hardware.md), [Porting](porting.md), [Ambiguities](ambiguities.md) |
| Memory window of each module | OLED application notes: Specifying a Memory Window for Downloading | p. 1 | `GeometryOf()`, `Geometry` | [Hardware](hardware.md), [Ambiguities](ambiguities.md) |
| Dim mode, power-down and scrolling rules | OLED application notes: Display Commands Available; Scrolling (Screensaver), Table 4 | p. 1 | `SetDimSettings()`, `SetContrast()`, `SetupScroll()`, `ActivateScroll()`, `DisplayOff()` | [API guide](api-guide.md), [Ambiguities](ambiguities.md) |
| Charge pump shut-down input | IS-CHPMP and IS-CHPMPHP Manual: 3. Pin Configurations | p. 5 | `CtrlPin::VccEnable` | [Hardware](hardware.md), [Limitations](limitations.md) |
| Charge pump enable wiring in the kit | OLED engineering kits: Hardware Considerations | p. 1 | `CtrlPin::VccEnable` | [Hardware](hardware.md) |
<!-- /generated -->

## When the datasheet and the driver differ

The driver is written against the revision above. If you have a newer revision, compare its revision history with
the topics above; a change to a pin, a register, a timing or a value the driver uses is a bug report for this
repository. Where the driver deliberately goes beyond the datasheet, the [Contract](contract.md) says so.

The repository never contains the PDF itself: vendors own their documents. Download it from the vendor, and compare
its SHA-256 with the one above to confirm you have the same file.
