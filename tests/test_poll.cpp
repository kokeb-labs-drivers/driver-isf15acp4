/**
 * @file test_poll.cpp
 * @brief The switch through the driver: polarity, the port's physical level, events, the idle
 * dim and sleep policy and the wake on a press.
 */
#include <cstdint>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

/// Poll every `step_ms` for `ms`; collect the events by count.
struct Counts {
  int pressed{0};
  int released{0};
  int longp{0};
  int repeat{0};
  int errors{0};
  int woke{0};
};

void Run(Fixture& f, uint32_t ms, uint32_t step_ms, Counts& c) {
  for (uint32_t t = 0; t < ms; t += step_ms) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    if (!r) {
      c.errors++;
    } else {
      c.pressed += (r.value.event == is::ButtonEvent::Pressed) ? 1 : 0;
      c.released += (r.value.event == is::ButtonEvent::Released) ? 1 : 0;
      c.longp += (r.value.event == is::ButtonEvent::LongPress) ? 1 : 0;
      c.repeat += (r.value.event == is::ButtonEvent::Repeat) ? 1 : 0;
      c.woke += r.value.woke_display ? 1 : 0;
    }
    f.GetChip().Elapse(step_ms * 1000U);
  }
}

TEST_CASE("poll: active-low switch, pull-up: high is open") {
  Fixture f;
  REQUIRE(f.Up());
  Counts c;
  f.GetRig().switch_high = true;
  Run(f, 100, 5, c);
  CHECK_EQ(c.pressed, 0);
  f.GetRig().switch_high = false; // closed
  Run(f, 100, 5, c);
  CHECK_EQ(c.pressed, 1);
  CHECK(f.drv.IsButtonDown());
  CHECK_EQ(f.drv.GetButtonPressCount(), 1U);
  f.GetRig().switch_high = true;
  Run(f, 100, 5, c);
  CHECK_EQ(c.released, 1);
  CHECK(!f.drv.IsButtonDown());
  CHECK_EQ(c.errors, 0);
}

TEST_CASE("poll: active-high polarity") {
  is::Config cfg{};
  cfg.button.active_low = false;
  Fixture f(cfg);
  REQUIRE(f.Up());
  Counts c;
  f.GetRig().switch_high = false;
  Run(f, 100, 5, c);
  CHECK_EQ(c.pressed, 0);
  f.GetRig().switch_high = true;
  Run(f, 100, 5, c);
  CHECK_EQ(c.pressed, 1);
  const is::Result<bool> raw = f.drv.ReadSwitchRaw();
  REQUIRE(raw.ok());
  CHECK(raw.value);
}

TEST_CASE("poll: long press and repeat reach the application") {
  is::Config cfg{};
  cfg.button.long_press_ms = 300;
  cfg.button.repeat_interval_ms = 100;
  cfg.button.repeat_delay_ms = 400;
  Fixture f(cfg);
  REQUIRE(f.Up());
  Counts c;
  f.GetRig().switch_high = true;
  Run(f, 100, 5, c);
  f.GetRig().switch_high = false;
  Run(f, 1000, 5, c);
  CHECK_EQ(c.pressed, 1);
  CHECK_EQ(c.longp, 1);
  CHECK(c.repeat >= 5);
  CHECK(c.repeat <= 7);
}

TEST_CASE("poll: a bouncing contact never produces an event faster than the debounce time") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().bounce = true; // the level flips on every read
  Counts c;
  Run(f, 500, 5, c);
  CHECK_EQ(c.pressed, 0);
  CHECK_EQ(c.released, 0);
}

TEST_CASE("poll: a failing switch read is a Bus error and changes nothing") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().fail_switch = true;
  CHECK(f.drv.Poll().error == is::Error::Bus);
  CHECK(f.drv.ReadSwitchRaw().error == is::Error::Bus);
  CHECK(!f.drv.IsButtonDown());
}

TEST_CASE("poll works before the start-up: the switch is the application's input") {
  Fixture f;
  f.GetRig().switch_high = false;
  Counts c;
  Run(f, 100, 5, c);
  CHECK_EQ(c.errors, 0);
  CHECK_EQ(c.pressed, 0); // the first sample is adopted: it was down already
  CHECK(f.drv.IsButtonDown());
}

TEST_CASE("idle policy: dim, then sleep, and a press wakes the display") {
  is::Config cfg{};
  cfg.idle_dim_ms = 1000;
  cfg.idle_sleep_ms = 3000;
  Fixture f(cfg);
  REQUIRE(f.Up());
  Counts c;
  Run(f, 500, 10, c);
  CHECK(f.GetChip().DisplayState() == Display::On);
  Run(f, 1000, 10, c);
  CHECK(f.GetChip().DisplayState() == Display::Dim);
  Run(f, 2500, 10, c);
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK(!f.GetChip().Vcc());
  f.GetRig().switch_high = false; // press
  Run(f, 100, 10, c);
  CHECK_EQ(c.pressed, 1);
  CHECK_EQ(c.woke, 1);
  CHECK(f.GetChip().DisplayState() == Display::On);
  CHECK(f.GetChip().Vcc());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("idle policy: activity restarts the timers") {
  is::Config cfg{};
  cfg.idle_dim_ms = 1000;
  Fixture f(cfg);
  REQUIRE(f.Up());
  Counts c;
  for (int i = 0; i < 5; ++i) {
    Run(f, 800, 10, c);
    REQUIRE(f.drv.NotifyActivity().ok());
  }
  CHECK(f.GetChip().DisplayState() == Display::On);
  Run(f, 1200, 10, c);
  CHECK(f.GetChip().DisplayState() == Display::Dim);
}

TEST_CASE("idle policy: a held button counts as activity") {
  is::Config cfg{};
  cfg.idle_dim_ms = 1000;
  Fixture f(cfg);
  REQUIRE(f.Up());
  Counts c;
  Run(f, 100, 10, c);
  f.GetRig().switch_high = false;
  Run(f, 3000, 10, c);
  CHECK(f.GetChip().DisplayState() == Display::On);
}

TEST_CASE("idle policy is off by default") {
  Fixture f;
  REQUIRE(f.Up());
  Counts c;
  Run(f, 20000, 100, c);
  CHECK(f.GetChip().DisplayState() == Display::On);
}

TEST_CASE("set button config: checked, restarts the debouncer") {
  Fixture f;
  REQUIRE(f.Up());
  is::ButtonConfig bad{};
  bad.debounce_ms = 0;
  CHECK(f.drv.SetButtonConfig(bad).error == is::Error::InvalidArgument);
  is::ButtonConfig good{};
  good.debounce_ms = 5;
  good.long_press_ms = 50;
  REQUIRE(f.drv.SetButtonConfig(good).ok());
  CHECK_EQ(f.drv.GetConfig().button.debounce_ms, 5U);
}

} // namespace
