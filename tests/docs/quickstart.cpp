/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "isf15acp4.hpp"

int show_hello(MyPort& port) {
  isf15acp4::ISF15ACP4<MyPort> panel(port); // no I/O yet

  if (auto r = panel.EnsureInitialized();
      !r) {                           // 1. reset, configure, clear: the supply is still off
    return static_cast<int>(r.error); //    Bus or InvalidArgument: check wiring and Config
  }
  if (auto r = panel.DisplayOn(); !r) { // 2. only now is the 15 V supply enabled and the panel on
    return static_cast<int>(r.error);
  }
  if (auto r = panel.DrawRectangle(0, 0, 95, 63, isf15acp4::kWhite); !r) { // 3. draw
    return static_cast<int>(r.error);
  }
  static_cast<void>(panel.Deinitialize()); // 4. display off, supply off, wait: always runs
  return 0;
}
// --8<-- [end:quickstart]
