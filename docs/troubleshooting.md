# Troubleshooting

Causes of an error or a wrong picture, by symptom. The bus is write only, so most hardware faults show as a panel that
is dark, white, tinted or shifted, with every call returning `Ok`.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2:

    - Continuous scrolling: 9.2.7 to 9.2.9 Scrolling, [p. 51](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=51)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## EnsureInitialized returns Bus

The port reported a failure: its `EnsureInitialized()` returned `false`, or a transfer or a pin write did.

1. Check that your port's `EnsureInitialized()` returns `true` on its own (log it).
2. Check the pins in your port against the board: SCLK, MOSI, SS, D/C, RES and the supply enable.
3. With a logic analyser, check that RES# pulses low for a few microseconds, then SS# frames appear with D/C# low.

## EnsureInitialized returns InvalidArgument

The `Config` fails `Validate()`; nothing was sent and no pin changed. Call `isf15acp4::Validate(config)` on your
configuration and look at the fields in [Configuration](configuration.md).

## The panel stays dark, no error

Every call succeeds because the bus cannot say otherwise. Work down this list:

| Check | How |
| --- | --- |
| The 15 V rail | Measure it after `DisplayOn()`; it must be 14 V to 16 V (ISC15ANP4, ISC01P: 15 V to 17 V) at the module's pin 9 |
| The supply enable | Does the pin go high in `DisplayOn()`, and is it the pin your regulator listens to? A P-channel switch needs the port to invert it |
| D/C# | A command with D/C# high is taken as data: the controller is never configured. Scope it |
| RES# | Held low keeps the controller in reset; it must be high after `EnsureInitialized()` |
| SS# and the clock | Mode 0, at most 6.6 MHz, SS# low for each transfer |
| The controller after a dip | A brown-out of VDD resets it silently: call `Reinitialize()` |
| Contrast | `master_current` 0 or contrast 0 is nearly black; try the all-on mode: `SetDisplayMode(DisplayMode::AllOn)` |

## Colours are wrong

Red and blue swapped: set `Config::red_on_segment_a = false`, or `Remap::reverse_colour_order`
([Ambiguities](ambiguities.md#colour-order)). Washed out or too dark: `master_current`, the contrast fields, or the
gray scale table (`UseLinearGrayScale()`). Everything tinted by one colour: one `contrast_*` value is off.

## The picture is shifted, cropped or mirrored

Shifted by a few columns: the variant's column offset (`PresetFor()` for the ISC15ANP4 and ISC01P). Mirrored:
`Remap::column_reverse` or `com_scan_reverse`. Upside down: `com_scan_reverse` and `column_reverse` together. The
initialisation values are carried from the vendor's notes ([Ambiguities](ambiguities.md)).

## A line or rectangle is missing or wrong

The engine was still busy when the next command arrived: raise `Config::engine_settle_us`
([Ambiguities](ambiguities.md#drawing-engine-busy-time)).

## Calls return ScrollActive

A RAM write, drawing call, `SetupScroll()` or `ApplyConfig()` while scrolling. Call `DeactivateScroll()`, then
rewrite the screen (`RamNeedsRewrite()`).

## Calls return Locked

`SetCommandLock(true)` was called. `SetCommandLock(false)` unlocks; the power-down unlocks by itself.

## Poll returns Unsupported

The port has no `ReadSwitch()` or no `NowMs()` hook, or the module is the display-only ISC01P.

## The button reports several presses

Contact bounce longer than `ButtonConfig::debounce_ms`: raise it (scope the switch), and poll faster than half the
debounce time. Presses shorter than the poll period are missed.

## The button seems inverted

`ButtonConfig::active_low` is true for a switch to ground with a pull-up (closed reads low). Set it false for a switch
to VDD with a pull-down.

## The display wakes with a press but the press does nothing

By design: a press that wakes a dimmed or sleeping display has `woke_display` set. Treat it as a wake, not a command.

## The display went white or dark after weeks of running

A brown-out or a noise glitch reset the controller; the driver cannot know ([Limitations](limitations.md#the-bus-is-write-only)).
`Reinitialize()` repairs it. Check the VDD supply and its decoupling.
