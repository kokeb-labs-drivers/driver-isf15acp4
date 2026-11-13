# Ambiguities

Where the datasheets leave a value or a meaning open, the choice the driver makes, and the setting that changes it.
Each one is a bench step in [Limitations](limitations.md#not-verified-on-hardware) until someone runs it.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Switch circuit and contact data: NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed) none printed, Switch specifications (sheet E5), [p. 2](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=2)
    - Power on and off sequence: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.9 Power ON and OFF Sequence, [p. 27](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=27)
    - Re-map and data format: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.6 Set Re-map and Data Format (A0h), [p. 37](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=37)
    - Drawing engine: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.2.1 to 9.2.6 Graphic Acceleration Command Set, [p. 48](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=48)
    - Variant geometry and column offsets: NKK Switches ISC15ANP4 (sheets E4 to E9, no number printed) none printed, Block diagram and pin configurations (sheet E6), [p. 3](https://www.nkkswitches.com/pdf/ISC15ANP4.pdf#page=3)
    - Display-only variant: NKK Switches ISC01P (sheets E4 to E9, no number printed) none printed, Switch and OLED specifications (sheets E5, E6), [p. 2](https://www.nkkswitches.com/pdf/ISC01P.pdf#page=2)
    - Start-up values of each module: NKK Switches OLED application notes none printed (web page), Initialization, Tables 1 to 3, steps 4.1 to 4.18 (Note 1, Note 2), p. 1
    - Communication, SCK frequency and chip select: NKK Switches OLED application notes none printed (web page), Design Considerations 2 and 5, p. 1
    - Memory window of each module: NKK Switches OLED application notes none printed (web page), Specifying a Memory Window for Downloading, p. 1
    - Dim mode, power-down and scrolling rules: NKK Switches OLED application notes none printed (web page), Display Commands Available; Scrolling (Screensaver), Table 4, p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The controller is named in the application notes

The NKK sheet (E4 to E6) gives the pin names SS, RES, D/C, SCK and SDI, the 96 RGB x 64 format and the controller block
diagram, and does not name the controller. NKK's application notes for the OLED Color SmartDisplay do: "All OLED
modules incorporate the same OLED controller (SSD1331)" (Part Numbers), and "The same OLED controller is used for all
the three OLED SmartDisplay" (General Information). The driver follows the SSD1331 of Solomon Systech. That the NKK
modules have the SSD1331's exact command set, and no changes to it, is still confirmed only by the notes' tables using
its opcodes. **Setting:** none; run the host example's picture on the first module.

## Colour order

The controller has three colour groups A, B and C, and the RAM word is sent C first. The SSD1331 examples and the NKK
note drive A as red and C as blue. **Settings:** `Config::red_on_segment_a` says which group carries red on the panel
(default true); `Remap::reverse_colour_order` swaps the segment order (A0h bit 2). The driver combines them so that
`Color` is RGB565 whatever the wiring. If red and blue are swapped on your module, flip `red_on_segment_a`.

## Colour depth of the 256-colour mode

The NKK sheet says 256 colours are 8 bits "R 2 bit / G 3 bit / B 3 bit"; the SSD1331 gives 3 bits to C, 3 to B and 2
to A. With A as red these agree. The driver encodes red with 2 bits, green and blue with 3 in this mode.

## VCC settle time

The sequence in SSD1331 section 7.9 says to wait until VCC is "stable" before the display-on command and gives no
time; NKK's application notes say only that VCC "can be enabled any time from this point [RES# released] to before the
display is turned ON at the end of initialization" and that it must follow VDD. Still open: nothing settles the wait.
**Setting:** `Config::vcc_settle_ms`, default 50 ms, valid 1 to 1000 ms (assumed). The right value depends on your
charge pump's rise time and load; measure the rail.

## Drawing engine busy time

Neither the SSD1331 datasheet nor NKK's application notes say how long the drawing engine (line, rectangle, copy, dim,
clear) is busy, or what happens to a byte sent meanwhile. The notes mention a "two-byte buffer in the OLED
communication" and recommend experimenting with the time between bytes, which is the same open point for the
throughput. Still open. **Setting:** `Config::engine_settle_us`, default 3000 µs (assumed), valid up to 100 000 µs; the
driver waits that long after every engine command. If a bench run shows a drawing command being overwritten, raise
it. The model's `engine_busy_us` makes the same assumption testable.

## CS# high time and CS# release

Table 21 gives no minimum time CS# must stay high between two transfers, and the NKK notes give none either. The
driver's port contract assumes one clock period (`kMinCsHighNs`, 150 ns), which every SPI peripheral driver meets by
itself between two transfers (assumed). What the notes do settle is the other edge: SS is released only after the
transmission has finished (they call releasing it early "a common mistake"), and a command with its data is one
session. A port's `Write()` must return only when the last bit is out, which the contract already says
([Porting](porting.md)); the driver sends each command, and the three second pre-charge speeds of Table 3 (note 2), in
one transfer.

## Writes while scrolling

The datasheet does not say whether RAM writes during a scroll corrupt the display. The driver refuses them with
`ScrollActive`; `Config::writes_while_scrolling` allows them if your bench run shows they are safe.

## Switch bounce

The NKK sheet gives the contact data (SPST, normally open, 200 mΩ maximum, 100 mA at 12 V) and the notes give no bounce
time either (they only say how to wire the switch to a pin with a pull-up). Still open. **Setting:**
`ButtonConfig::debounce_ms`, default 20 ms (assumed), valid 1 to 1000 ms. Measure the switch with a scope and tighten
it; poll at least twice per debounce time.

## Initialisation values and variant offsets

Settled by NKK's application notes, "Initialization": Table 1 for the ISC15ANP4, Table 2 for the ISC01P and Table 3 for
the ISF15ACP4, steps 4.1 to 4.18. `Config{}` and `PresetFor()` hold exactly those numbers (contrast, remap, display
offset, multiplex ratio, dim settings, power save, phase periods, clock, linear gray scale, pre-charge level, VCOMH,
and for the ISF15ACP4 the second pre-charge speeds, steps 4.31 to 4.33), and `tests/test_nkk_tables.cpp` checks each
against the table typed in independently. The column origins 16 and 22 and the widths come from the notes'
memory-window commands ("15H 10H 4FH", "15H 16H 49H", "15H 00H 5FH"), and the display offsets 10h and 1Ch from step
4.7; the earlier "centred" derivation gave the same numbers. Where the driver differs from a table, or the notes leave
a point open:

- Steps 4.31 to 4.33 (second pre-charge speed) exist only in Table 3. Tables 1 and 2 do not send them, so the
  ISC15ANP4 and ISC01P presets set `send_precharge_speeds` to false and leave the controller at its reset values.
- Step 4.16 of Table 3 sends BBh 1Dh; Tables 1 and 2 send 12h. Bit 0 of BBh is a reserved zero in the SSD1331 table
  (9.1.19), so the driver keeps a level (A[5:1], 14 for 1Dh, 9 for 12h) and a flag `precharge_bit0` that reproduces
  NKK's byte exactly; the ISF15ACP4 preset sends 1Dh. Whether the controller ignores bit 0 is not known; the bytes are
  NKK's own.
- Step 4.5 (A0h) is 68h for the ISF15ACP4 and 70h for the others, as the presets send. The notes list the A0h option
  bits and call 10b and 11b in A[7:6] reserved; the SSD1331 gives 10b as 65k colour format 2, and the driver still
  offers it (`ColorDepth::Colors65kFormat2`) with that difference noted here: prefer `Colors65k`.
- Step 4.11 (ADh 8Eh, master configuration) and step 4.12 (B0h 0Bh, power save off) are what the driver sends.
- ACh (dim): "The values cannot exceed the maximum level for each color of initialization as noted in steps 4.1, 4.2,
  and 4.3". `Validate()` and `SetDimSettings()` refuse a dim contrast above the start-up contrast of the same colour,
  and `SetContrast()` refuses a contrast below the dim contrast; `SetDimSettings()` also keeps the dim contrast at or
  below the contrast in force, and a failed `ApplyConfig()` leaves the limit where it was, so that a restart stays
  valid.
- Note 1 on steps 4.1 to 4.3, 4.7, 4.9, 4.11, 4.13 to 4.17: "Changing the set-up values may cause damage to the OLED
  modules." Nothing in the driver stops you from changing them; the field docs say so.
- Scrolling (notes, "Scrolling (Screensaver)"): 27h "can only be transmitted during initialization or when the
  display is off, and scrolling is deactivated"; the driver refuses it only while scrolling is on, so call
  `SetupScroll()` before `DisplayOn()` or after `DisplayOff()`. The notes' prose also says byte A (columns) "should not
  be set to 00H or it will deactivate the scrolling for the rest of the session" and that scrolling cannot be
  reactivated; Table 4 of the same notes and the SSD1331 say 0 means no horizontal scroll. The driver allows columns 0
  with a vertical step (a vertical-only scroll); this is not verified on a module. The notes confirm that vertical
  scrolling needs 64 MUX, which the driver enforces, and that RAM must be rewritten after 2Eh.
- Master current (87h) "should be used while the display is off (after command AEH)". The driver allows it at any
  time; the start-up sends it with the display off.
- The notes' clock limit: the pin list gives a 6.66 MHz maximum SCK and Design Considerations 5 a 6.6 MHz throughput;
  NKK's kit runs 8 MHz and customers report 16 MHz. The driver keeps `kMaxClockHz` at the datasheet's 6.6 MHz.

**Setting:** every field is in `Config` and `PresetFor()`, and `Validate()` checks the ranges against the SSD1331. A
panel that is dim, tinted or offset by a few columns points here. Hardware check: [Limitations](limitations.md#not-verified-on-hardware).

## Reset pulse width

The RES# low pulse is at least 3 µs (Table 18) and the reset completes within 2 µs; the driver holds the pulse for
`DelayUs(3)` and waits `DelayUs(2)` after it. A port whose `DelayUs()` rounds up to a tick holds it longer, which is
allowed. Both are datasheet values, not settings.
