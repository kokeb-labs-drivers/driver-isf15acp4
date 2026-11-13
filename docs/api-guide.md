# API guide

The driver's API by task, with code that compiles: start, draw, scroll, follow the button, handle errors and shut down.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Switch circuit and contact data: NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed) none printed, Switch specifications (sheet E5), [p. 2](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=2)
    - GDDRAM and colour depth: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.5 Graphic Display Data RAM; Table 9, [p. 20](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=20)
    - Gray scale decoder: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.6 Gray Scale Decoder, [p. 21](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=21)
    - Command table and parameter counts: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 8 Command Table, Table 10, [p. 28](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=28)
    - Column and row window: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.1 Set Column Address; 9.1.2 Set Row Address, [p. 35](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=35)
    - Contrast, master current and second pre-charge speed: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.3 to 9.1.5, [p. 36](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=36)
    - Display start line, offset, mode and multiplex ratio: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.7 to 9.1.10, [p. 42](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=42)
    - Dim mode, master configuration and display on and off: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.11 to 9.1.13, [p. 45](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=45)
    - Command lock: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.22 Set Command Lock (FDh), [p. 47](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=47)
    - Drawing engine: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.2.1 to 9.2.6 Graphic Acceleration Command Set, [p. 48](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=48)
    - Continuous scrolling: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.2.7 to 9.2.9 Scrolling, [p. 51](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=51)
    - Dim mode, power-down and scrolling rules: NKK Switches OLED application notes none printed (web page), Display Commands Available; Scrolling (Screensaver), Table 4, p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

Every call returns `Result<T>` (`.ok()`, `.error`, `.value`); an `Error` is `[[nodiscard]]`, so a dropped result is a
compiler warning. Coordinates are in the visible area of the module (`GetGeometry()`): 96 x 64 for the ISF15ACP4.

## Start and power up

```cpp
--8<-- "tests/docs/api_tasks.cpp:lifecycle"
```

`EnsureInitialized()` leaves the panel dark with the supply off (state 1); `DisplayOn()` is the only call that enables
the supply (`DisplayDim()` does too, at the dim settings). `DisplayOff()` returns to state 1. `Reinitialize()` is the
power-down, start-up and the previous display state again: the repair after a brown-out you cannot see
([Limitations](limitations.md#the-bus-is-write-only)).

## Pixels

`Color` is RGB565 (`MakeColor(r, g, b)` builds one from 8-bit channels; `kBlack`, `kWhite`, `kRed`, `kGreen`, `kBlue`
are provided). The driver encodes it for the colour depth in the configuration: two bytes in 65k mode, one in 256 and
three in 65k format 2.

| Call | Does |
| --- | --- |
| `Clear(color)` | Fills the visible area |
| `SetPixel(x, y, color)` | One pixel |
| `FillWindow(x, y, w, h, color)` | A rectangle by streaming data (no engine) |
| `WriteWindow(x, y, w, h, pixels)` | A window of `w * h` colours, row by row |
| `WriteWindowRaw(x, y, w, h, bytes)` | A window of bytes you encoded yourself |
| `Present(frame)` | A whole frame of `width * height` colours |

```cpp
--8<-- "tests/docs/api_tasks.cpp:pixels"
```

With `Remap::vertical_increment` the controller fills columns first; the driver reorders the pixels, so the picture is
the same. A window outside the visible area, or a span of the wrong length, is `InvalidArgument` and sends nothing.

## The drawing engine

The controller draws lines and rectangles, copies, dims and clears a window by itself, from a few bytes of command.
The driver waits `Config::engine_settle_us` after each, because the datasheet gives no busy time
([Ambiguities](ambiguities.md#drawing-engine-busy-time)).

```cpp
--8<-- "tests/docs/api_tasks.cpp:engine"
```

`DrawLine`, `DrawRectangle`, `FillRectangle`, `CopyWindow`, `DimWindow` and `ClearWindow` check that corners are ordered
and inside the visible area. The engine's fill option is switched on only for `FillRectangle` and off again after it.

## Scroll

```cpp
--8<-- "tests/docs/api_tasks.cpp:scroll"
```

While scrolling, RAM and engine calls, `SetupScroll()` and `ApplyConfig()` return `ScrollActive` (a raw 2Fh or 2Eh
through `TraceCommand()` is tracked too). `DeactivateScroll()` stops it; the RAM content must then be
rewritten (`RamNeedsRewrite()` is true until a full-area write). `ScrollSetup` needs a multiplex ratio of 64 for the
vertical offset.

## Follow the button

`Poll()` reads the switch through the port, debounces it and returns one `ButtonUpdate`; call it at least twice per
debounce time (every 5 ms is comfortable). The first sample after the start, `Reset()`, `SetButtonConfig()`, or an
`ApplyConfig()` or `Reinitialize()` that changes the switch settings sets the level without an event (one that leaves
them alone keeps a press in progress): a switch already held at that moment gives a `LongPress` (and `was_long` on the
release) with no `Pressed` before it, and no `Repeat`. `repeat_delay_ms` of 0 repeats at the first held poll, and a
repeat delay shorter than `long_press_ms` delivers `Repeat` before `LongPress`. If the wake `DisplayOn()` fails,
`Poll()` returns its error and the `Pressed` event is lost, although `GetButtonPressCount()` has counted it.

| `ButtonEvent` | When | Fields |
| --- | --- | --- |
| `Pressed` | the level was closed for `debounce_ms` | `timestamp_ms`: when it first changed |
| `Released` | open for `debounce_ms` | `held_ms`, `was_long` |
| `LongPress` | held for `long_press_ms`, once per press | `held_ms` |
| `Repeat` | every `repeat_interval_ms` after `repeat_delay_ms`, if non-zero | `held_ms` |

```cpp
--8<-- "tests/docs/api_tasks.cpp:button"
```

With `Config::idle_dim_ms` and `idle_sleep_ms` set, `Poll()` dims and then turns the display off after that long without
a press, and a press wakes it (`woke_display` is true: treat that press as a wake,
not as a command). Only a display that the idle
timers lowered is woken: one that you dimmed or switched off yourself stays as it is. The idle timers run inside
`Poll()`, which needs the switch: on the display-only `Isc01p`, `Poll()` returns `Unsupported` and the idle timers never
act; dim or switch off from your own timer there.
`NotifyActivity()` restarts the idle timers. `ButtonDebouncer` can also be used alone, with your own samples.

## Configuration at run time

Setters take effect at once and are checked first: `SetContrast`, `SetMasterCurrent`, `SetPrechargeSpeed`, `SetRemap`,
`SetStartLine`, `SetDisplayOffset`, `SetDisplayMode` (normal, all on, all off, inverse), `SetMultiplexRatio`,
`SetDimSettings`, `SetPowerSave`, `SetPhasePeriods`, `SetClock`, `SetGrayScaleTable`, `UseLinearGrayScale`,
`SetPrechargeLevel`, `SetVcomh`, `Nop`. `ApplyConfig()` sends a whole `Config`. `SetCommandLock(true)` locks the
controller against commands; every call but the power-down then returns `Locked`. `SetDimSettings` refuses a dim
contrast above the start-up contrast of its colour, and `SetContrast` a contrast below the dim contrast (NKK notes,
ACh). Call `SetupScroll` with the display off, as the NKK notes require for 27h; NKK Note 1 warns that changed set-up
values may damage the module.

What a restart (`Reinitialize()`, or `ApplyConfig(GetConfig())`) keeps: every setter stores its value in `GetConfig()`
and the restart sends it again, with three exceptions. `SetMultiplexRatio` is not stored (the module's ratio comes
back), `SetGrayScaleTable` is not stored (the start-up gray scale comes back: B9h when `linear_gray_scale` is set), and
`SetPrechargeSpeed` sets `send_precharge_speeds`, so its values are sent again even on the two smaller modules. A setter
whose transfer failed leaves `GetConfig()` as it was.

`TraceCommand(opcode, parameters)` sends one raw command for bring-up; it checks the opcode and parameter count against
the command table and marks the shadow in doubt. It bypasses the guards of the typed calls: a raw A8h or 27h can
break the rule that a vertical scroll needs 64 MUX, and the driver will not notice. A raw 2Fh or 2Eh keeps
`IsScrolling()` in step, FDh takes only 12h or 16h and ADh only 8Eh.

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| `Error` | What to do |
| --- | --- |
| `InvalidArgument` | Fix the argument; nothing was sent |
| `NotInitialized` | Call `EnsureInitialized()` |
| `Bus` | The state of the controller is unknown: `Reinitialize()`, then redraw |
| `Unsupported` | Add the optional hook, or the module has no switch |
| `ScrollActive` | `DeactivateScroll()` first, then rewrite RAM |
| `Locked` | `SetCommandLock(false)` |
| `SupplyOff` | `DisplayOn()` first |

## Shut down

```cpp
--8<-- "tests/docs/api_tasks.cpp:deinit"
```

`Deinitialize()` is never refused. Cut VDD only after it returns.

Next: [Configuration](configuration.md).
