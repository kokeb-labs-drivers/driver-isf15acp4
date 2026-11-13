# Quickstart

Run the driver on your PC first, with no hardware; then wire the module, write a small port class and call four driver
functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NKK Switches ISF15ACP4_FramelessOLED (sheets E4 to E9, no number printed) none printed:

    - Module pins, switch contacts and block diagram: Block diagram and pin configurations (sheet E6), [p. 3](https://www.nkkswitches.com/pdf/ISF15ACP4_FramelessOLED.pdf#page=3)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Try it on your PC

[`examples/host/`](../examples/host/README.md) runs the real driver against a simulated controller, through a host
port. From the repository root, with CMake and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/isf15acp4_host_example
```

```text
ISF15ACP4 driver 0.1.0 on the host
isf15acp4: initialised, supply off
isf15acp4: supply on
  event 1 at 1235 ms
  event 3 at 2035 ms
  event 2, held 1000 ms
isf15acp4: display and supply off
3 button events; panel lit: yes; power-down clean: yes
```

The calls in [`main.cpp`](../examples/host/main.cpp) are the ones below; only the port differs on an MCU. CI builds
and runs this example on every pull request.

## 1. Wire it

| MCU | Module pin | Notes |
| --- | --- | --- |
| SPI SCLK | 7 SCK | Mode 0, up to 6.6 MHz; 1 to 4 MHz is a good start |
| SPI MOSI | 8 SDI | There is no data out: do not connect MISO |
| GPIO chip select | 4 SS | Low for the whole transfer |
| GPIO | 6 D/C | Low for a command, high for pixel data |
| GPIO | 5 RES | Idles high |
| GPIO | enable of the 15 V supply | Active high; must idle low through reset and boot (pull-down) |
| 3.3 V | 3 VDD | 2.4 V to 3.5 V |
| 15 V supply | 9 VCC | 14 V to 16 V (ISC15ANP4, ISC01P: 15 V to 17 V), current 11 mA typical with all pixels on |
| GPIO input with pull-up | 1 SW | The switch to ground on pin 2; closed reads low |
| GND | 10 GND, 2 SW | |

The 15 V OLED supply is not part of the module: it is yours, with an enable the driver controls. [Hardware](hardware.md)
draws the circuit.

## 2. Write the port

The port is the only platform code: it brings the bus and the pins up at their idle levels, moves bytes with D/C# set
for the kind of transfer, and waits. Replace the `platform_*` calls with your SDK's.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

[Porting](porting.md) explains each hook and the optional ones (switch, clock, log).

## 3. Show something

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `EnsureInitialized()` brings your bus up, drives the supply enable off, resets the controller, sends the whole
   configuration and clears the display RAM. The panel stays dark and the 15 V supply stays off.
2. `DisplayOn()` enables the supply, waits for it to settle and turns the panel on.
3. The drawing calls send short commands (a rectangle) or pixel data (a window of colours).
4. `Deinitialize()` turns the panel off, switches the supply off and waits. It always runs.

Reading the switch is one more call, `Poll()`, described in the [API guide](api-guide.md#follow-the-button).

## If it does not work

The bus is write only, so a wrong wire never gives an error. A dark panel is the symptom for most faults.

| Symptom | First thing to check |
| --- | --- |
| `EnsureInitialized()` returns `Bus` | Your port's `EnsureInitialized()`, or a pin that cannot be driven |
| `EnsureInitialized()` returns `InvalidArgument` | The `Config` fails `Validate()`: nothing was sent |
| Panel dark, no error | 15 V rail present and enabled? D/C#, RES#, SS# wired? [Troubleshooting](troubleshooting.md) |
| Colours swapped | `Config::red_on_segment_a` or `Remap::reverse_colour_order` ([Ambiguities](ambiguities.md)) |
| `Poll()` returns `Unsupported` | Your port has no `ReadSwitch()` or `NowMs()` hook |

## Next

- [API guide](api-guide.md): pixels, the drawing engine, scrolling, the button, errors.
- [Configuration](configuration.md): every `Config` field and the variant presets.
- [Examples](examples.md): the host example and the ESP-IDF example.
- [Limitations](limitations.md): what the write-only bus and the missing hardware test mean for you.
