---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# ISF15ACP4 Driver

<p>A C++20 driver for the NKK ISF15ACP4 frameless OLED pushbutton: a 96 x 64 colour display on a write-only SPI bus
and a debounced switch, on any MCU. It powers the OLED in the order the datasheet requires, draws, scrolls and
reports button presses, long presses and repeats.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-isf15acp4){ .md-button }
</div>

The driver has no platform code. You give it a small class that moves bytes on the SPI bus, drives three pins and
waits; it does the rest: the power sequence, the controller's initialisation, pixel windows in three colour depths, the
drawing engine, scrolling and the switch.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">OLED pushbutton</span>
<span class="kb-chip__name">ISF15ACP4</span>
<span class="kb-chip__vendor">NKK Switches</span>
</div>

Display
:   96 RGB x 64 pixels, passive matrix, 65 536 colours (a 256-colour mode is available)

Interface
:   Write-only SPI, mode 0, up to 6.6 MHz, with D/C#, RES# and CS#; there is no MISO

Switch
:   SPST normally open, two contacts on the module; debounce, press, release, long press and repeat in the driver

Supply
:   VDD 2.4 V to 3.5 V for the logic; VCC for the OLED 14 V to 16 V (ISC15ANP4 and ISC01P: 15 V to 17 V),
    external, enabled by the driver after the controller is configured

Variants
:   ISC15ANP4 (64 x 48) and ISC01P (52 x 36, no switch) as presets
</div>

## What you get

- **A safe power sequence.** The OLED supply is never enabled before the controller has been reset and configured,
  and the power-down always runs. See [Contract](contract.md).
- **A display and a button, both handled.** `ISF15ACP4<YourPort>` draws pixels, rectangles and lines, scrolls, and
  `Poll()` turns the switch into events. See [API guide](api-guide.md).
- **No readback, and it says so.** The bus is write only: the driver cannot know that the controller received a byte.
  What that means and how it is tested: [Limitations](limitations.md).
- **Errors you can act on.** Every call returns `Result<T>`; an invalid argument sends nothing.
- **Zero-overhead portability.** The port is a CRTP class checked by a concept: four required hooks, optional switch,
  clock and log hooks found at compile time. See [Porting](porting.md).
- **Tested against an independent model of the controller.** The model checks the datasheet's power rules and reports
  a violation when the driver breaks one. See [Testing and CI](testing.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: Frameless OLED Pushbutton ISF15ACP4 (NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed)) | none printed, 2019-10-03 | [PDF](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf) · [vendor page](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf) |
| Datasheet: SSD1331 96RGB x 64 Dot Matrix OLED/PLED Segment/Common Driver with Controller (Solomon Systech (copy from the Adafruit mirror) SSD1331) | 1.2, Nov 2007 | [PDF](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf) · [vendor page](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf) |
| Datasheet: Frameless OLED Pushbutton ISC15ANP4 (variant, 64 x 48) (NKK Switches ISC15ANP4 (sheets E4 to E9, no number printed)) | none printed, 2019-10-03 | [PDF](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf) · [vendor page](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf) |
| Datasheet: OLED module ISC01P (variant, 52 x 36, display only) (NKK Switches ISC01P (sheets E4 to E9, no number printed)) | none printed, 2019-10-03 | [PDF](https://www.nkkswitches.com/pdf/ISC01P.pdf) · [vendor page](https://www.nkkswitches.com/pdf/ISC01P.pdf) |
| Application note: Application Notes for the OLED Color SmartDisplay (web page) (NKK Switches OLED application notes) | none printed (web page), not printed | [vendor page](https://docs.nkkswitches.com/docs/application-notes/oled-application-notes/) |
| Application note: IS-CHPMP and IS-CHPMPHP charge pump manual (NKK Switches IS-CHPMP and IS-CHPMPHP Manual) | C, 0617 (as printed on every page) | [vendor page](https://docs.nkkswitches.com/docs/application-notes/oled-application-notes/) |
| Application note: OLED Engineering Kits User Manual (web page) (NKK Switches OLED engineering kits) | none printed (web page), not printed | [vendor page](https://docs.nkkswitches.com/docs/engineering-kits/oled-engineering-kits-user-manual/) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the module, write the port, show something and read the button.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, supplies, bus settings and timing.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, in what order it powers the display, and what it returns.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Start, draw, scroll, follow the button and handle errors.

-   :material-alert-circle-outline: **[Ambiguities](ambiguities.md)**

    ---

    What the datasheets leave open, and the setting that covers each case.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
