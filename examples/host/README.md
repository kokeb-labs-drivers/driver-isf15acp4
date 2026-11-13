# ISF15ACP4 host example

The driver on your PC, with no hardware: the host port in
[`host_isf15acp4_port.hpp`](host_isf15acp4_port.hpp) connects the real driver to a simulated display controller (the model
the host tests use, behind a write-only bus), and [`main.cpp`](main.cpp) starts it, draws a test picture, follows a
simulated long press of the switch and powers down.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/isf15acp4_host_example
```

## Expected output

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

(The timestamps depend on the simulated clock.) The host tests build this folder too and run the example (CTest
`isf15acp4_host_example`), so CI fails if it stops compiling or a call fails.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: start, draw, follow the button, power down |
| [`host_isf15acp4_port.hpp`](host_isf15acp4_port.hpp) | The host port: the hooks over the simulated chip, `DelayUs()` and `Log()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
