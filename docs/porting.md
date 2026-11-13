# Porting

Write the port for your MCU's SDK: a class with a few small methods that moves bytes and drives pins.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Serial interface framing, D/C# and chip select: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 7.1.3 Serial Interface, [p. 17](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=17)
    - Serial interface timing: Solomon Systech (copy from the Adafruit mirror) SSD1331 1.2, 12 AC Characteristics, Table 21, [p. 57](https://cdn-shop.adafruit.com/datasheets/SSD1331_1.2.pdf#page=57)
    - Communication, SCK frequency and chip select: NKK Switches OLED application notes none printed (web page), Design Considerations 2 and 5, p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `isf15acp4::SpiInterface<Self>` (in
[`inc/isf15acp4_spi_interface.hpp`](../inc/isf15acp4_spi_interface.hpp)). The base uses CRTP and declares no hook
itself; the driver template is constrained by the `isf15acp4::SpiPort` concept, so calls are resolved at compile
time (no virtual functions, nothing to allocate) and a missing or misdeclared hook is a compile error at the point
of use.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus and the pins up, each at its idle level, on the first call; `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Write(DataKind, const uint8_t* data, std::size_t len) noexcept` | yes | One write-only transfer of 1 to 96 bytes (33 for a command; a bus limited to fewer bytes, such as ESP-IDF's SPI without DMA at 64, needs DMA or its own splitting): D/C# low for `Command`, high for `Data`, CS# low for all bytes, CS# high at the end. `false` on any bus error |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | yes | Drive RES# or the supply enable; `false` if the pin write failed |
| `void DelayUs(uint32_t us) noexcept` | yes | Wait at least `us` microseconds |
| `bool ReadSwitch(bool& high) noexcept` | no | The switch pin's physical level; `false` on a read failure |
| `uint32_t NowMs() noexcept` | no | A free-running millisecond counter, may wrap |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

There is no read hook: the bus is write only.

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Rules the transfer must follow:

- **One call, one transfer.** Keep CS# low for all `len` bytes. A command and its parameters are one transfer: the
  controller takes the parameters as the bytes that follow the opcode while D/C# stays low, and splitting them is not
  allowed.
- **D/C# before CS#.** Set D/C# for the kind first; it must be stable before the first clock (tAS 40 ns) and held after
  the last (tAH 40 ns).
- **The bus requirements below**: mode 0 (mode 3 also works), MSB first, up to `kMaxClockHz`. Start at 1 MHz on
  jumper wires.
- **Raw bytes only.** Do not interpret, retry or reorder bytes.
- **Return `false` for a bus error.** The driver turns it into `Error::Bus` and never ignores it. Return `true` only
  when the bytes were clocked out: it does not mean the controller received them, and the driver does not claim that.
- **Bounded time-outs.** A stuck bus fails the call; it does not block forever.

## Bus requirements

The driver states what the controller needs from the bus as `constexpr` values in
[`inc/isf15acp4_spi_interface.hpp`](../inc/isf15acp4_spi_interface.hpp) (handbook KB-CPP-123). Check your port's
settings against them with `static_assert`, as both example ports do, so a port that clocks too fast or in the
wrong mode does not compile:

| Constant | Value | Meaning (SSD1331 section 7.1.3, Table 21) |
| --- | --- | --- |
| `kSpiMode` | `0` | CPOL 0, CPHA 0; mode 3 also works |
| `kMsbFirst` | `true` | Most significant bit first |
| `kMaxClockHz` | `6'600'000` | SCK cycle 150 ns minimum |
| `kMinCsHighNs` | `150` | CS# high between two transfers: assumed, the table gives none |
| `kCsSetupNs`, `kCsHoldNs` | `75`, `60` | CS# low to the first SCK edge, last SCK edge to CS# high |
| `kDcSetupNs`, `kDcHoldNs` | `40`, `40` | D/C# valid before and after the byte |
| `kContinuousWord` | `false` | Idle SCK time between the bytes of one transfer is allowed |
| `kWriteOnly` | `true` | There is no MISO |

## Control pins

Each control pin has a defined idle level, and the port drives it there before it enables the pin's output and
before the driver's first transfer (handbook KB-HW-018). Configuring a pin as an output first and setting its level
afterwards gives the module a pulse it did not ask for: a reset, or worse, the 15 V supply on.

| `CtrlPin` | Module pin | Polarity | Idle (`INACTIVE`) level | Before the first transfer |
| --- | --- | --- | --- | --- |
| `Reset` | 5 RES | active low | high | Set high, then enable the output |
| `VccEnable` | enable of the 15 V supply | active high | low (supply off) | Set low, then enable the output |

`GpioSignal::ACTIVE` always means "function asserted"; the port maps it to the electrical level (RES# ACTIVE is low,
the enable ACTIVE is the supply on, inverted for a low-side or P-channel switch). The supply enable must also be off
while the MCU resets and boots: use a pull-down on the regulator's enable, because the port's code does not run then.
D/C# is set inside `Write()`.

The bytes passed to `Write()` live in the driver object (`command_`, `data_`). A port that uses DMA must be given a
driver object in memory the DMA engine can read (internal RAM on an ESP32, not flash or PSRAM): declare it `static`
or on the heap with DMA-capable capabilities, not in a section the engine cannot reach.

## Optional hooks

The driver finds the optional hooks at compile time with named concepts in `isf15acp4::detail`. A missing hook
falls back in a documented, safe way; a misdeclared one is a `static_assert` that names it; neither skips a step
silently.

| Hook | Used for | Without it |
| --- | --- | --- |
| `ReadSwitch()` | `Poll()`, `ReadSwitchRaw()` | Those calls return `Unsupported`; the display works |
| `NowMs()` | `Poll()` timestamps and idle timers, `NotifyActivity()` | `Poll()` and `NotifyActivity()` return `Unsupported` |
| `Log()` | One line per lifecycle event or failure | Nothing is logged |

A port with every hook:

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

```cpp
--8<-- "tests/docs/ports.cpp:reinitialize"
```

A `GpioSet()` that returns `void` is a compile error: the driver must see a failed pin write, because it drives the
supply with it. The [compile-fail tests](testing.md) check that rule and the signatures of every hook.

## Reference implementations

Every port lives with its example, in `examples/<platform>/`; the driver has none. [Platforms](platforms.md) lists
them and how to add one.

- [`examples/host/host_isf15acp4_port.hpp`](../examples/host/host_isf15acp4_port.hpp): any PC, against a simulated
  controller. Start from it for a host build or a test bench.
- [`examples/esp32/main/esp32_isf15acp4_port.hpp`](../examples/esp32/main/esp32_isf15acp4_port.hpp): ESP-IDF, with
  every hook. Compiled by CI, not run on hardware.

## Check your port

1. Build it: the ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_port.hpp),
   with the driver instantiated over them.
2. Scope the supply enable: it must stay low through `EnsureInitialized()` and rise only in `DisplayOn()`.
3. Draw a rectangle and a diagonal ([Quickstart](quickstart.md)). A dark panel with no error means a wire, the 15 V
   rail or the supply enable: the bus gives no other answer.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex. When
several devices share a bus, the port must also serialise the bus. See [Contract](contract.md#threading).

## Editor setup

The clang-format and clang-tidy configuration is in `_config/`, and the repository root has no copy of it. Tools
that take a path use it directly; CI does the same:

| Tool | Setting |
| --- | --- |
| Command line | `clang-format --style=file:_config/.clang-format`, `clang-tidy --config-file=_config/.clang-tidy -p build` |
| VS Code (C/C++ extension) | `"C_Cpp.clang_format_style": "file:${workspaceFolder}/_config/.clang-format"` and `"C_Cpp.codeAnalysis.clangTidy.args": ["--config-file=${workspaceFolder}/_config/.clang-tidy"]` |
| clangd (VS Code clangd, Neovim, Emacs), CLion | These find `.clang-format` and `.clang-tidy` only by name, from the file upwards: copy both to the root once (`cp _config/.clang-format _config/.clang-tidy .`). `.gitignore` lists the copies, so they never reach a commit; copy again after `_config/` changes |

Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (clangd: `--compile-commands-dir=build`) so the editor sees the
same flags as the build.
