# Contract

What the ISF15ACP4 driver promises, what it needs, and the rules a caller follows.

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it. A change to anything here is a breaking change, listed under
**Breaking** in the [changelog](changelog.md).

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Supply voltages, current and the three power states: NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed) none printed, Recommended operating conditions (sheets E5, E7), [p. 2](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=2)
    - Reset circuit and reset pulse: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.4 Reset Circuit; 12 AC Characteristics, Table 18, [p. 19](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=19)
    - Power on and off sequence: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.9 Power ON and OFF Sequence, [p. 27](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=27)
    - Dim mode, master configuration and display on and off: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.11 to 9.1.13, [p. 45](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=45)
    - Command lock: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 9.1.22 Set Command Lock (FDh), [p. 47](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=47)
    - Supplies, VCC order and the charge pump: NKK Switches OLED application notes none printed (web page), Pin-outs; Design Considerations 1, Power Requirements VCC (16V); Part Numbers, p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` is the only way to start the driver. Its first call checks the stored configuration
  (`InvalidArgument`, nothing sent), brings the bus up, drives the supply enable off, resets the controller, sends
  the whole configuration and clears the display RAM. On any failure the driver stays uninitialised, the supply is off,
  and the next call starts again from the beginning; once it has succeeded, later calls return `Ok` with no I/O.
- Every call that touches the controller returns `Error::NotInitialized` before `EnsureInitialized()` succeeds.
  `ApplyConfig()` before the start only stores the configuration. The switch works before the start: `Poll()` reads the
  pin and needs no controller.
- **`InvalidArgument` means nothing was sent**: no transfer and no pin change. `Validate()` and each call's own checks
  run before the first byte.
- Every port failure propagates as `Error::Bus`; no call reports success for a transfer or a pin write that failed.
- Every wait is a bounded delay on the port's `DelayUs()`; nothing waits on a flag the bus cannot read.
- `Config{}` holds the module's initialisation values; `Validate()` the documented ranges.
- A missing required hook, or a hook with the wrong signature, is a compile error that names it; the optional
  `ReadSwitch()`, `NowMs()` and `Log()` hooks are found at compile time, and the calls that need them return
  `Unsupported` without them.

## Power order

These rules protect the OLED, which can be damaged by its drive supply being on while the controller is not
configured. They are the reason for most of the design, and each has a test against the controller model that fails
when the driver breaks it.

1. **The OLED supply is never enabled before the controller has been reset and configured.** `EnsureInitialized()` drives
   the enable off, resets, configures and clears the RAM; only `DisplayOn()` and `DisplayDim()` enable it, and only
   when the driver is initialised. A failed start leaves the supply off. From an unknown state (a fresh object after a
   warm reset of the MCU) the driver cannot know that the panel is lit: its first act is to drive the enable off, which
   removes VCC without the display-off command and the `vcc_off_ms` wait of section 7.9. And if that first pin write
   fails the supply may still be on: `EnsureInitialized()` then returns `Bus`.
2. **The power-down always runs.** `Deinitialize()` and `DisplayOff()` are never refused: not before the start, not
   while the controller is command-locked or scrolling, not after a failed transfer. They send the unlock and the
   display-off command in one transfer and drive the supply enable off whether or not that transfer succeeded.
   `Deinitialize()` then waits `vcc_off_ms` (tOFF) because the logic supply may be cut next; `DisplayOff()` does not
   wait, the logic supply stays on. A pin that will not switch is reported as `Bus` and the driver keeps
   `IsSupplyEnabled()` true, so the
   caller is told rather than assured.
3. **The display is off before the supply is.** The normal order is display off, then supply off. If the display-off
   transfer fails, the supply is still driven off: the supply is the hazard, so its switch-off never waits on a
   transfer that failed. The failure is reported, and the controller model flags the unsequenced switch-off as a
   violation the tests allow only after an injected failure.
4. **A failure while turning on switches the supply off again.** `DisplayOn()` that cannot send the display-on command
   drives the supply off before it returns `Bus`.
5. **VDD is cut only after `Deinitialize()` returns.** The driver waits the datasheet's tOFF; cutting the logic
   supply sooner leaves the OLED supply on without the controller, which the driver cannot prevent from outside.

## The bus is write only

The serial interface has no read path: the module has no MISO pin. The driver therefore:

- never reads from the controller, and has no readback, verify or status call; a `Verify` argument does not exist;
- treats `Write()` returning `true` as "the bytes were clocked out", never as "the controller received them";
- keeps a shadow of what it has sent (`GetConfig()`, `GetOutput()`, `IsScrolling()`, `IsLocked()`), which is the driver's
  belief, not the chip's state;
- cannot detect a brown-out or reset of the controller; the way out is `Reinitialize()`, and `IsShadowInDoubt()` is true
  after a failed transfer or a raw command;
- is verified by a pin and state model: [Testing and CI](testing.md#the-model-stands-alone).

[Limitations](limitations.md#the-bus-is-write-only) lists what an application should do about it.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `isf15acp4_version.h` |
| Your port | | The bus, within the bus requirements; D/C#, RES# and the supply enable at their idle levels before the first transfer; the delay; optionally the switch pin, a millisecond clock and logging ([Porting](porting.md)) |

Nothing else: no RTOS, no SDK, no other repository of the org.

## Threading

The driver is not thread-safe and takes no lock. Use each driver object from one task at a time, or guard every
call on it with one mutex; the layer that owns the driver owns that lock, and the bus lock when several devices share
a bus. Nothing may run in an ISR: every call can block for a bus transfer or a delay (`DisplayOn()` blocks for about
150 ms, `Deinitialize()` for 100 ms, a full-screen write for about 12 kB of bus time). Separate objects share no state.

## Errors

`isf15acp4::Error` inside `isf15acp4::Result<T>`. Values 0 to 9 are the shared base set; this driver returns the ones
below, and four chip-specific values from 16 (each is returned by some call and tested).

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since |
| `InvalidArgument` | 2 | An argument or a `Config` field is out of range; nothing was sent |
| `Bus` | 3 | A port hook returned `false` |
| `Unsupported` | 9 | The call needs a hook the port lacks (`ReadSwitch()`, `NowMs()`), or the module has no switch |
| `ScrollActive` | 16 | A RAM, drawing or setup call while scrolling is on |
| `ScrollNotSet` | 17 | `ActivateScroll()` before `SetupScroll()` succeeded |
| `SupplyOff` | 18 | A raw display-on while the supply is off |
| `Locked` | 19 | The controller is command-locked (`SetCommandLock(true)`) |

`Timeout` (4), `DeviceIdMismatch` (5), `Crc` (6), `Busy` (7) and `DeviceFault` (8) keep their values and are not
returned: there is nothing to read back, no checksum, no busy flag and no fault flag.

## Memory

No heap, no exceptions, no RTTI, no `std::function`, no `std::string`, no `printf` and no static mutable state. A
driver object holds a reference to the port, a `Config` (76 bytes), the button state, the shadow flags and the two
transfer buffers (33 command bytes, 96 pixel bytes): 296 bytes on a 64-bit host. The transfer buffers are members, so
the pointer a port's `Write()` gets stays valid for the whole call and the stack stays small.

## Platform

Platform-free: the same code runs on any target with a C++20 compiler, including the host tests. The ESP-IDF port
in [`examples/esp32`](examples.md) is an example, not part of the driver.

## Deviations from the standard

None.
