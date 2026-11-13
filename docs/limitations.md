# Limitations

What the driver does not do, and what a user should know before relying on it.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Serial interface framing, D/C# and chip select: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.1.3 Serial Interface, [p. 17](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=17)
    - Power on and off sequence: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.9 Power ON and OFF Sequence, [p. 27](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=27)
    - Charge pump shut-down input: NKK Switches IS-CHPMP and IS-CHPMPHP Manual C, 3. Pin Configurations, p. 5

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The bus is write only

The serial interface has no read path (SSD1331 section 7.1.3; the module has no MISO pin). Consequences:

- **No readback and no verification.** `Write()` returning `true` means the bytes were clocked out. The driver cannot
  confirm that the controller saw them, that it is configured, or what is on the screen.
- **A reset or brown-out of the controller is invisible.** If VDD dips or a glitch resets the controller, the driver
  still believes the panel is on and every call still returns `Ok`. A dark or white panel is the symptom. The
  recovery is `Reinitialize()` (power-down, start-up, display on). An application that must be sure the panel
  shows its picture calls it after a known disturbance (a supply dip, a hot-plug) or on a slow timer, and redraws.
  The model's brown-out test shows exactly this behaviour.
- **State is the driver's shadow.** `GetConfig()`, `GetOutput()`, `IsScrolling()` and `IsLocked()` report what the driver
  sent. After a failed transfer, or after `TraceCommand()`, `IsShadowInDoubt()` is true until the next `Reinitialize()`.
- **Verification is by model.** The tests check the driver against an independent model of the controller's pins and
  commands, not against the chip. A mistake the model shares with the driver is not caught: that is why the model is
  written from the datasheet and not from the driver, and why a bench run is still needed.

## Not verified on hardware

Nobody has run this driver on a module. Bench steps, in order:

1. Wire the module as in [Hardware](hardware.md), with the supply enable pulled down (the IS-CHPMP charge pump has no
   pull-down of its own). Run the host example's calls on the board: a rectangle and a
   diagonal. Expect them; a dark panel means the OLED rail, D/C#, RES# or SS#.
2. Compare red and blue pixels; set `red_on_segment_a` if swapped.
3. Scope VCC against `EnsureInitialized()`/`DisplayOn()`: VCC must stay off until `DisplayOn()`. Measure the VCC rise
   time and set `vcc_settle_ms` above it.
4. Draw 20 lines back to back; if one is wrong, raise `engine_settle_us`.
5. Scope the switch and set `debounce_ms`.
6. Run each variant's preset on its own module. The values are NKK's (application notes, Tables 1 to 3) and nobody has
   tried them here. Two points of those notes are open on hardware: BBh bit 0 (Table 3 sends 1Dh, the SSD1331 calls the
   bit reserved) and a vertical-only scroll (the notes' prose says a zero column byte stops scrolling for the session).
7. Run `Deinitialize()` with a scope on VCC and VDD: VCC off first, 100 ms, then you may cut VDD.

The ESP-IDF example is compiled by CI and not run. This repository's own checks ran on a host only: no GCC, no
Cortex-M cross-build, no CodeQL, no clang-tidy.

## No canvas and no font

The driver has no frame buffer, no text and no bitmap layer. It would be cheap to add a monochrome font, and the
decision is to leave it out: a frame buffer is 12 kB of RAM, and a font is the application's data and licence. What is
there is what a graphics layer needs: `Color` (RGB565), `MakeColor()`, `WriteWindow()`, `WriteWindowRaw()` and the
controller's own engine. A graphics library can render into a window and hand it to `WriteWindow()`.

## Blocking calls

Every call blocks for its bus time and waits: `DisplayOn()` about 150 ms, `Deinitialize()` 100 ms, `EnsureInitialized()`
a few milliseconds plus about 12 kB of RAM clearing (about 20 ms at 6 MHz). There is no DMA or asynchronous interface; a
port may use DMA inside `Write()` as long as it returns when the bytes are out.

## Other

- One driver object per module, one task at a time ([Contract](contract.md#threading)).
- The 15 V supply and VDD are yours: the driver switches only the enable, and cannot tell whether the rail came up.
- The switch is read from a GPIO, not from the controller; it needs the `ReadSwitch()` and `NowMs()` hooks.
- The display's contrast, current and clock follow the module's initialisation values, not the datasheet's reset
  values: see [Ambiguities](ambiguities.md#initialisation-values-and-variant-offsets).
