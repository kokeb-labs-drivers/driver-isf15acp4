/**
 * @file test_button.cpp
 * @brief The debouncer: bounce, press, release, long press, repeat, wrap-around, both polarities.
 */
#include <cstdint>

#include "isf15acp4_button.hpp"
#include "test_harness.hpp"

namespace {

namespace is = ::isf15acp4;

is::ButtonConfig Cfg() {
  is::ButtonConfig c{};
  c.debounce_ms = 10;
  c.long_press_ms = 100;
  c.repeat_interval_ms = 0;
  c.repeat_delay_ms = 50;
  return c;
}

TEST_CASE("button: the first sample is adopted without an event") {
  is::ButtonDebouncer d(Cfg());
  CHECK(d.Update(true, 0).event == is::ButtonEvent::None);
  CHECK(d.IsDown());
  CHECK_EQ(d.PressCount(), 0U);
  is::ButtonDebouncer e(Cfg());
  CHECK(e.Update(false, 0).event == is::ButtonEvent::None);
  CHECK(!e.IsDown());
}

TEST_CASE("button: a press is accepted after the debounce time and stamped at its start") {
  is::ButtonDebouncer d(Cfg());
  static_cast<void>(d.Update(false, 0));
  CHECK(d.Update(true, 100).event == is::ButtonEvent::None);
  CHECK(d.Update(true, 109).event == is::ButtonEvent::None);
  const is::ButtonUpdate u = d.Update(true, 110);
  CHECK(u.event == is::ButtonEvent::Pressed);
  CHECK_EQ(u.timestamp_ms, 100U);
  CHECK(d.IsDown());
  CHECK_EQ(d.PressCount(), 1U);
}

TEST_CASE("button: bounce shorter than the debounce time is ignored") {
  is::ButtonDebouncer d(Cfg());
  static_cast<void>(d.Update(false, 0));
  uint32_t t = 100;
  for (int i = 0; i < 20; ++i) {
    CHECK(d.Update((i % 2) == 0, t).event == is::ButtonEvent::None); // flips every 5 ms
    t += 5;
  }
  CHECK_EQ(d.PressCount(), 0U);
  CHECK(!d.IsDown());
}

TEST_CASE("button: release, held time and the long flag") {
  is::ButtonDebouncer d(Cfg());
  static_cast<void>(d.Update(false, 0));
  static_cast<void>(d.Update(true, 100));
  CHECK(d.Update(true, 110).event == is::ButtonEvent::Pressed);
  // A short press.
  static_cast<void>(d.Update(false, 150));
  const is::ButtonUpdate r = d.Update(false, 160);
  CHECK(r.event == is::ButtonEvent::Released);
  CHECK_EQ(r.held_ms, 50U);
  CHECK(!r.was_long);
  CHECK_EQ(r.timestamp_ms, 150U);
}

TEST_CASE("button: a long press fires once and the release says so") {
  is::ButtonDebouncer d(Cfg());
  static_cast<void>(d.Update(false, 0));
  static_cast<void>(d.Update(true, 100));
  static_cast<void>(d.Update(true, 110));
  CHECK(d.Update(true, 190).event == is::ButtonEvent::None);
  const is::ButtonUpdate l = d.Update(true, 200);
  CHECK(l.event == is::ButtonEvent::LongPress);
  CHECK_EQ(l.held_ms, 100U);
  CHECK(d.Update(true, 300).event == is::ButtonEvent::None); // once
  static_cast<void>(d.Update(false, 400));
  const is::ButtonUpdate r = d.Update(false, 410);
  CHECK(r.event == is::ButtonEvent::Released);
  CHECK(r.was_long);
  CHECK_EQ(r.held_ms, 300U);
}

TEST_CASE("button: repeat after the delay, one per interval, no burst after a slow poll") {
  is::ButtonConfig c = Cfg();
  c.repeat_interval_ms = 20;
  c.repeat_delay_ms = 50;
  c.long_press_ms = 1000;
  is::ButtonDebouncer d(c);
  static_cast<void>(d.Update(false, 0));
  static_cast<void>(d.Update(true, 100));
  static_cast<void>(d.Update(true, 110)); // pressed at 100
  CHECK(d.Update(true, 140).event == is::ButtonEvent::None);
  CHECK(d.Update(true, 150).event == is::ButtonEvent::Repeat); // held 50
  CHECK(d.Update(true, 160).event == is::ButtonEvent::None);
  CHECK(d.Update(true, 170).event == is::ButtonEvent::Repeat); // held 70
  // A poll that was late by a long time: one repeat, then the next interval starts from there.
  CHECK(d.Update(true, 500).event == is::ButtonEvent::Repeat);
  CHECK(d.Update(true, 501).event == is::ButtonEvent::None);
  CHECK(d.Update(true, 520).event == is::ButtonEvent::Repeat);
}

TEST_CASE("button: no repeat when the interval is 0") {
  is::ButtonDebouncer d(Cfg());
  static_cast<void>(d.Update(false, 0));
  static_cast<void>(d.Update(true, 100));
  static_cast<void>(d.Update(true, 110));
  for (uint32_t t = 120; t < 190; t += 10) {
    CHECK(d.Update(true, t).event == is::ButtonEvent::None);
  }
}

TEST_CASE("button: the counter may wrap") {
  is::ButtonDebouncer d(Cfg());
  const uint32_t base = 0xFFFFFFF0U;
  static_cast<void>(d.Update(false, base));
  static_cast<void>(d.Update(true, base + 2U));
  const is::ButtonUpdate p = d.Update(true, base + 12U); // wraps past zero
  CHECK(p.event == is::ButtonEvent::Pressed);
  CHECK_EQ(p.timestamp_ms, base + 2U);
  static_cast<void>(d.Update(false, base + 60U));
  const is::ButtonUpdate r = d.Update(false, base + 70U);
  CHECK(r.event == is::ButtonEvent::Released);
  CHECK_EQ(r.held_ms, 58U);
}

TEST_CASE("button: configure forgets the state; press count wraps by definition") {
  is::ButtonDebouncer d(Cfg());
  static_cast<void>(d.Update(false, 0));
  static_cast<void>(d.Update(true, 100));
  static_cast<void>(d.Update(true, 110));
  CHECK(d.IsDown());
  d.Configure(Cfg());
  CHECK(!d.IsDown());
  CHECK(d.Update(true, 200).event == is::ButtonEvent::None); // adopted again
  CHECK(d.IsDown());
}

TEST_CASE("button: validation") {
  is::ButtonConfig c{};
  CHECK(is::Validate(c) == is::Error::Ok);
  c.debounce_ms = 0;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = {};
  c.debounce_ms = 1001;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = {};
  c.long_press_ms = c.debounce_ms;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = {};
  c.long_press_ms = is::kButtonTimeMaxMs + 1U;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = {};
  c.repeat_interval_ms = 9;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c.repeat_interval_ms = 10001;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c.repeat_interval_ms = 10;
  CHECK(is::Validate(c) == is::Error::Ok);
  c.repeat_delay_ms = is::kButtonTimeMaxMs + 1U;
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
}

} // namespace
