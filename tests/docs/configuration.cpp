/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "isf15acp4.hpp"
#include "isf15acp4_kconfig.hpp"
#include "my_port.hpp"

// --8<-- [start:custom]
isf15acp4::Config DimmerConfig() {
  isf15acp4::Config c = isf15acp4::PresetFor(isf15acp4::Variant::Isf15acp4);
  c.master_current = 8;          // half the segment current: less heat, longer life
  c.idle_dim_ms = 30'000;        // Poll() dims after 30 s without a press
  c.idle_sleep_ms = 120'000;     // and turns the display off after 2 min; a press wakes it
  c.button.long_press_ms = 1000; // the long-press event
  return c;                      // isf15acp4::Validate(c) checks it without the chip
}

isf15acp4::Error Start(MyPort& port) {
  static isf15acp4::ISF15ACP4<MyPort> panel(port, DimmerConfig()); // stored, no I/O
  return panel.EnsureInitialized().error; // InvalidArgument if the config is out of range
}
// --8<-- [end:custom]

// --8<-- [start:checked]
// A configuration that Validate() rejects does not compile.
constexpr isf15acp4::CheckedConfig kBoard{isf15acp4::Config{.master_current = 12}};

const isf15acp4::Config& BoardConfig() {
  return kBoard.Get();
}
// --8<-- [end:checked]

// --8<-- [start:kconfig]
isf15acp4::Error StartFromMenuconfig(MyPort& port) {
  static isf15acp4::ISF15ACP4<MyPort> panel(port, isf15acp4::KconfigDefaults());
  return panel.EnsureInitialized().error;
}
// --8<-- [end:kconfig]
