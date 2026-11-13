/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include <array>
#include <cstdio>

#include "isf15acp4.hpp"
#include "my_port.hpp"

using Panel = isf15acp4::ISF15ACP4<MyPort>;

// --8<-- [start:lifecycle]
isf15acp4::Error BringUp(Panel& panel) {
  // State 1: the controller is reset, configured and cleared; the OLED supply is still off.
  const auto r = panel.EnsureInitialized();
  if (!r) {
    std::printf("ISF15ACP4 init: %s\n", isf15acp4::ToString(r.error));
    return r.error;
  }
  return panel.DisplayOn().error; // State 2: supply on, wait, panel on.
}
// --8<-- [end:lifecycle]

// --8<-- [start:pixels]
isf15acp4::Error ShowIcon(Panel& panel) {
  // A 4 x 2 window of colours, written row by row whatever the RAM increment mode.
  const std::array<isf15acp4::Color, 8> icon{
      isf15acp4::kRed,  isf15acp4::kGreen, isf15acp4::kBlue,  isf15acp4::kWhite,
      isf15acp4::kBlue, isf15acp4::kWhite, isf15acp4::kBlack, isf15acp4::kRed};
  return panel.WriteWindow(10, 20, 4, 2, icon).error; // InvalidArgument sends nothing
}
// --8<-- [end:pixels]

// --8<-- [start:engine]
isf15acp4::Error Frame(Panel& panel) {
  // The controller draws these itself: one short command each, then a wait for the engine.
  if (auto r = panel.FillRectangle(2, 2, 60, 20, isf15acp4::kWhite, isf15acp4::kBlue); !r) {
    return r.error;
  }
  return panel.DrawLine(0, 63, 95, 0, isf15acp4::kGreen).error;
}
// --8<-- [end:engine]

// --8<-- [start:button]
void FollowButton(Panel& panel) {
  // Call this at least twice per debounce time (20 ms by default).
  const auto u = panel.Poll();
  if (!u) {
    std::printf("poll: %s\n", isf15acp4::ToString(u.error));
    return;
  }
  switch (u.value.event) {
  case isf15acp4::ButtonEvent::Pressed:
    std::printf("pressed at %u ms (press %u)\n", static_cast<unsigned>(u.value.timestamp_ms),
                static_cast<unsigned>(panel.GetButtonPressCount()));
    break;
  case isf15acp4::ButtonEvent::LongPress:
    std::printf("long press\n");
    break;
  case isf15acp4::ButtonEvent::Released:
    std::printf("released after %u ms\n", static_cast<unsigned>(u.value.held_ms));
    break;
  default:
    break;
  }
}
// --8<-- [end:button]

// --8<-- [start:scroll]
isf15acp4::Error Ticker(Panel& panel) {
  isf15acp4::ScrollSetup s{};
  s.columns = 1;    // one column per step to the left
  s.start_row = 40; // the bottom 24 rows scroll
  s.rows = 24;
  s.interval = isf15acp4::ScrollInterval::Frames10;
  if (auto r = panel.SetupScroll(s); !r) {
    return r.error; // InvalidArgument: nothing sent
  }
  return panel.ActivateScroll()
      .error; // RAM writes are refused (ScrollActive) until it is deactivated
}
// --8<-- [end:scroll]

// --8<-- [start:errors]
const char* Describe(isf15acp4::Error e) {
  switch (e) {
  case isf15acp4::Error::Bus:
    return "a transfer or pin write failed: the chip state is unknown, call Reinitialize()";
  case isf15acp4::Error::NotInitialized:
    return "call EnsureInitialized() first";
  case isf15acp4::Error::ScrollActive:
    return "deactivate the scroll before writing RAM";
  case isf15acp4::Error::Locked:
    return "SetCommandLock(false) first";
  default:
    return isf15acp4::ToString(e);
  }
}
// --8<-- [end:errors]

// --8<-- [start:deinit]
void PowerDown(Panel& panel) {
  // Runs every step whatever the state, and drives the supply off even when a transfer fails.
  if (auto r = panel.Deinitialize(); !r) {
    std::printf("ISF15ACP4 power-down: %s\n", isf15acp4::ToString(r.error));
  }
}
// --8<-- [end:deinit]
