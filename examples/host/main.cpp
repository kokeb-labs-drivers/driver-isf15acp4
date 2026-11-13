/**
 * @file main.cpp
 * @brief The ISF15ACP4 driver on your PC: start a simulated module, draw, and follow the button.
 *
 * No hardware and no SDK: the host port (host_isf15acp4_port.hpp) connects the real driver to the
 * controller model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). Exit code 0 when every call succeeded and the model saw no rule
 * broken.
 */
#include <cstdint>
#include <cstdio>

#include "host_isf15acp4_port.hpp"
#include "isf15acp4.hpp"

namespace {

/// Hold the switch for `ms` and poll every 5 ms; returns the events seen.
int PressFor(HostIsf15acp4Port& port, isf15acp4::ISF15ACP4<HostIsf15acp4Port>& panel, uint32_t ms) {
  int events = 0;
  port.rig.switch_high = false; // closed: the pull-up is shorted to ground
  for (uint32_t t = 0; t < ms; t += 5U) {
    const auto u = panel.Poll();
    if (u && (u.value.event != isf15acp4::ButtonEvent::None)) {
      ++events;
      std::printf("  event %d at %u ms\n", static_cast<int>(u.value.event),
                  static_cast<unsigned>(u.value.timestamp_ms));
    }
    port.DelayUs(5'000);
  }
  port.rig.switch_high = true;
  for (uint32_t t = 0; t < 100U; t += 5U) {
    const auto u = panel.Poll();
    if (u && (u.value.event != isf15acp4::ButtonEvent::None)) {
      ++events;
      std::printf("  event %d, held %u ms\n", static_cast<int>(u.value.event),
                  static_cast<unsigned>(u.value.held_ms));
    }
    port.DelayUs(5'000);
  }
  return events;
}

} // namespace

int main() {
  HostIsf15acp4Port port;
  isf15acp4::ISF15ACP4<HostIsf15acp4Port> panel(port); // no I/O yet

  std::printf("ISF15ACP4 driver %s on the host\n", isf15acp4::GetDriverVersion());
  if (const auto r = panel.EnsureInitialized(); !r) { // state 1: configured, cleared, supply off
    std::printf("EnsureInitialized failed: %s\n", isf15acp4::ToString(r.error));
    return 1;
  }
  if (const auto r = panel.DisplayOn(); !r) { // state 2: supply on, panel on
    std::printf("DisplayOn failed: %s\n", isf15acp4::ToString(r.error));
    return 1;
  }
  if (!panel.Clear(isf15acp4::kBlack) || !panel.DrawRectangle(0, 0, 95, 63, isf15acp4::kWhite) ||
      !panel.DrawLine(0, 0, 95, 63, isf15acp4::kRed) ||
      !panel.SetPixel(48, 32, isf15acp4::kGreen)) {
    std::printf("drawing failed\n");
    return 1;
  }
  port.rig.switch_high = true; // open; the first samples set the debouncer's starting level
  for (int i = 0; i < 6; ++i) {
    static_cast<void>(panel.Poll());
    port.DelayUs(5'000);
  }
  const int events = PressFor(port, panel, 1000); // a long press: Pressed, LongPress, Released
  const bool shown = port.rig.chip.Lit();
  if (const auto r = panel.Deinitialize(); !r) { // display off, supply off, wait
    std::printf("Deinitialize failed: %s\n", isf15acp4::ToString(r.error));
    return 1;
  }
  const bool safe = (port.rig.chip.Violations() == 0U) && !port.rig.chip.Vcc();
  std::printf("%d button events; panel lit: %s; power-down clean: %s\n", events,
              shown ? "yes" : "no", safe ? "yes" : "no");
  return ((events >= 3) && shown && safe) ? 0 : 1;
}
