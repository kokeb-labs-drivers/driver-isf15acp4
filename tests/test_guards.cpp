/**
 * @file test_guards.cpp
 * @brief Tests added after mutation testing: each one pins a behaviour that a one-edit change of
 * the driver had left unnoticed (stale supply flag, failure clean-up, idle policy edges, the press
 * counter).
 */
#include <cstdint>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

void Elapse(Fixture& f, uint32_t ms) {
  f.GetChip().Elapse(ms * 1000U);
}

TEST_CASE(
    "guards: a start-up after a power-down whose pin failed starts from a supply that is off") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().fail_vcc_off = true;
  CHECK(f.drv.Deinitialize().error == is::Error::Bus);
  CHECK(f.drv.IsSupplyEnabled()); // the driver reports the doubt
  f.GetRig().fail_vcc_off = false;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK(!f.drv.IsSupplyEnabled()); // the start-up drove the pin off
  CHECK(!f.GetChip().Vcc());
  REQUIRE(f.drv.DisplayOn().ok()); // and so the supply really comes on before the display does
  CHECK(f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::On);
  CHECK_EQ(f.GetChip().Violations() & kDisplayOnWithoutVcc, 0U);
}

TEST_CASE("guards: the controller is not touched within 2 us of the reset release") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK_EQ(f.GetChip().Violations() & kBusAfterReset, 0U);
  Ssd1331Model m;
  m.SetRes(true);
  m.Elapse(5);
  m.SetRes(false);
  m.SetCs(true);
  m.SetDc(false);
  m.Byte(0xAE);
  CHECK((m.Violations() & kBusAfterReset) !=
        0U); // the model flags it, so the check above means something
}

TEST_CASE("guards: start-up sends display-off before the configuration") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK_EQ(f.GetChip().FirstCommand(), 0xAEU);
}

TEST_CASE("guards: the power-down waits tOFF also when the supply state is in doubt") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().fail_vcc_off = true;
  const uint64_t t0 = f.GetChip().NowUs();
  CHECK(f.drv.Deinitialize().error == is::Error::Bus);
  CHECK((f.GetChip().NowUs() - t0) >=
        (static_cast<uint64_t>(f.drv.GetConfig().vcc_off_ms) * 1000U));
}

TEST_CASE("guards: the power-down unlocks the controller with the defined parameter") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetCommandLock(true).ok());
  CHECK(f.GetChip().Locked());
  CHECK(f.drv.Deinitialize().ok());
  CHECK(!f.GetChip().Locked());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("guards: the supply pin fails on: the driver forces the supply off again") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  f.GetRig().fail_vcc_on = true;
  const int gpio = f.GetRig().gpio_calls;
  const int writes = f.GetRig().writes;
  CHECK(f.drv.DisplayOn().error == is::Error::Bus);
  CHECK_EQ(f.GetRig().gpio_calls - gpio, 2); // the failed enable and the forced-off
  CHECK(f.GetRig().writes > writes);         // and a display-off went out
}

TEST_CASE("guards: the display command fails after the supply came on: the supply goes off again") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  f.GetRig().fail_write_at = f.GetRig().writes; // the next transfer is the display-on command
  CHECK(f.drv.DisplayOn().error == is::Error::Bus);
  CHECK(!f.GetChip().Vcc());
  CHECK(!f.drv.IsSupplyEnabled());
  CHECK(f.drv.GetOutput() == is::Output::Off);
}

TEST_CASE("guards: turning on twice sends nothing the second time") {
  Fixture f;
  REQUIRE(f.Up());
  const int writes = f.GetRig().writes;
  const int gpio = f.GetRig().gpio_calls;
  const uint64_t t0 = f.GetChip().NowUs();
  REQUIRE(f.drv.DisplayOn().ok());
  CHECK_EQ(f.GetRig().writes, writes);
  CHECK_EQ(f.GetRig().gpio_calls, gpio);
  CHECK_EQ(f.GetChip().NowUs(), t0);
}

TEST_CASE("guards: dim to on waits no tAF, only off to on does") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.DisplayDim().ok());
  const uint64_t t0 = f.GetChip().NowUs();
  REQUIRE(f.drv.DisplayOn().ok());
  CHECK((f.GetChip().NowUs() - t0) <
        50000U); // one command, far below the 100 ms of the first turn-on
}

TEST_CASE("guards: idle sleep acts once; a sleeping display stays quiet") {
  is::Config cfg{};
  cfg.idle_sleep_ms = 500;
  Fixture f(cfg);
  REQUIRE(f.Up());
  for (int i = 0; i < 80; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK(f.GetChip().DisplayState() == Display::Off);
  const int writes = f.GetRig().writes;
  for (int i = 0; i < 50; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK_EQ(f.GetRig().writes, writes);
}

TEST_CASE("guards: the idle dim never turns a display that is off back on") {
  is::Config cfg{};
  cfg.idle_dim_ms = 300; // no sleep timer
  Fixture f(cfg);
  REQUIRE(f.Up());
  REQUIRE(f.drv.DisplayOff().ok());
  for (int i = 0; i < 100; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK(!f.GetChip().Vcc());
}

TEST_CASE("guards: the idle dim does not touch a display that is already dim") {
  is::Config cfg{};
  cfg.idle_dim_ms = 300;
  Fixture f(cfg);
  REQUIRE(f.Up());
  REQUIRE(f.drv.DisplayDim().ok());
  const int writes = f.GetRig().writes;
  for (int i = 0; i < 100; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK_EQ(f.GetRig().writes, writes);
}

TEST_CASE("guards: a locked controller is not dimmed by the idle policy, and Poll still succeeds") {
  is::Config cfg{};
  cfg.idle_dim_ms = 300;
  Fixture f(cfg);
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetCommandLock(true).ok());
  for (int i = 0; i < 100; ++i) {
    CHECK(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK(f.GetChip().DisplayState() == Display::On);
}

TEST_CASE("guards: a press on a display that is already on does not report a wake") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().switch_high = true;
  for (int i = 0; i < 10; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  f.GetRig().switch_high = false;
  bool pressed = false;
  for (int i = 0; i < 20; ++i) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    REQUIRE(r.ok());
    if (r.value.event == is::ButtonEvent::Pressed) {
      pressed = true;
      CHECK(!r.value.woke_display);
    }
    Elapse(f, 10);
  }
  CHECK(pressed);
}

TEST_CASE("guards: the press counter counts presses") {
  is::ButtonConfig cfg{};
  cfg.debounce_ms = 5;
  is::ButtonDebouncer d(cfg);
  uint32_t t = 0;
  static_cast<void>(d.Update(false, t));
  for (uint32_t n = 1; n <= 3; ++n) {
    for (int i = 0; i < 4; ++i) {
      static_cast<void>(d.Update(true, t));
      t += 5;
    }
    for (int i = 0; i < 4; ++i) {
      static_cast<void>(d.Update(false, t));
      t += 5;
    }
    CHECK_EQ(d.PressCount(), n);
  }
}

TEST_CASE("guards: a supply flag left set by a failed enable does not skip the next enable") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  f.GetRig().fail_vcc_on = true;
  f.GetRig().fail_vcc_off = true;
  CHECK(f.drv.DisplayOn().error == is::Error::Bus);
  CHECK(
      f.drv
          .IsSupplyEnabled()); // reported as possibly on: the forced-off could not be driven either
  CHECK(!f.GetChip().Vcc());   // but it never came on
  f.GetRig().fail_vcc_on = false;
  f.GetRig().fail_vcc_off = false;
  REQUIRE(f.drv.DisplayOn().ok());
  CHECK(f.GetChip().Vcc()); // the pin was driven again, so the display has its supply
  CHECK(f.GetChip().DisplayState() == Display::On);
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("guards: DisplayOff is never refused: before the start it sends the power-down and "
          "drives the pin off") {
  Fixture f;
  const int writes = f.GetRig().writes;
  const int gpio = f.GetRig().gpio_calls;
  CHECK(f.drv.DisplayOff().ok());
  CHECK(f.GetRig().writes > writes);   // unlock and display-off went out
  CHECK(f.GetRig().gpio_calls > gpio); // and the supply enable was driven off
}

TEST_CASE("guards: DisplayOff switches off a supply that a failed power-down left on, and does not "
          "wait tOFF") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().fail_vcc_off = true;
  CHECK(f.drv.Deinitialize().error == is::Error::Bus);
  CHECK(f.GetChip().Vcc());
  f.GetRig().fail_vcc_off = false;
  const uint64_t t0 = f.GetChip().NowUs();
  CHECK(f.drv.DisplayOff().ok()); // the driver is not initialised: still performed
  CHECK(!f.GetChip().Vcc());
  CHECK(!f.drv.IsSupplyEnabled());
  CHECK((f.GetChip().NowUs() - t0) < (static_cast<uint64_t>(f.drv.GetConfig().vcc_off_ms) * 1000U));
}

TEST_CASE(
    "guards: lock shadow: the power-down clears it, a restart clears it, a raw lock sets it") {
  {
    Fixture f;
    REQUIRE(f.Up());
    REQUIRE(f.drv.SetCommandLock(true).ok());
    CHECK(f.drv.IsLocked());
    CHECK(f.drv.DisplayOff().ok()); // the unlock rides in the same transfer
    CHECK(!f.GetChip().Locked());
    CHECK(!f.drv.IsLocked());
    CHECK(f.drv.SetContrast(0x20, 0x30, 0x30).ok());
  }
  {
    Fixture f;
    REQUIRE(f.Up());
    REQUIRE(f.drv.SetCommandLock(true).ok());
    f.GetRig().fail_writes_from = f.GetRig().writes;
    CHECK(f.drv.Deinitialize().error == is::Error::Bus); // the unlock never got through
    CHECK(f.drv.IsLocked());
    f.GetRig().fail_writes_from = -1;
    REQUIRE(f.drv.EnsureInitialized().ok()); // the reset unlocks the controller
    CHECK(!f.drv.IsLocked());
    CHECK(f.drv.Clear().ok());
  }
  {
    Fixture f;
    REQUIRE(f.Up());
    const std::array<uint8_t, 1> lock{0x16};
    const std::array<uint8_t, 1> unlock{0x12};
    REQUIRE(f.drv.TraceCommand(0xFD, lock).ok());
    CHECK(f.drv.IsLocked());
    CHECK(f.drv.SetContrast(0x20, 0x30, 0x30).error == is::Error::Locked);
    REQUIRE(f.drv.TraceCommand(0xFD, unlock).ok());
    CHECK(!f.drv.IsLocked());
    CHECK(f.drv.SetContrast(0x20, 0x30, 0x30).ok());
  }
}

TEST_CASE("guards: an outline rectangle does not fill; a copy with reverse swaps the levels") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.FillRectangle(10, 10, 30, 30, is::kWhite, is::kBlue).ok());
  REQUIRE(f.drv.DrawRectangle(10, 10, 30, 30, is::kRed).ok());
  CHECK_EQ(Shown(f, 20, 20), is::kBlue.rgb565); // the interior stayed
  CHECK_EQ(Shown(f, 10, 10), is::kRed.rgb565);
  CHECK(!f.GetChip().FillEnabled());
  REQUIRE(f.drv.CopyWindow(15, 15, 25, 25, {50, 40}, true).ok());
  CHECK_EQ(Shown(f, 55, 45), static_cast<uint16_t>(0xFFFFU ^ is::kBlue.rgb565));
  REQUIRE(f.drv.CopyWindow(15, 15, 25, 25, {70, 40}, false).ok());
  CHECK_EQ(Shown(f, 75, 45), is::kBlue.rgb565);
}

TEST_CASE("guards: ApplyConfig on a running driver is what GetConfig and the geometry report") {
  Fixture f;
  REQUIRE(f.Up());
  is::Config c = is::PresetFor(is::Variant::Isc15anp4);
  c.contrast_a = 0x33;
  REQUIRE(f.drv.ApplyConfig(c).ok());
  CHECK(f.drv.GetConfig().variant == is::Variant::Isc15anp4);
  CHECK_EQ(f.drv.GetConfig().contrast_a, 0x33U);
  CHECK(f.drv.SetPixel(70, 5, is::kRed).error == is::Error::InvalidArgument); // 64 wide now
  CHECK(f.drv.SetPixel(63, 47, is::kRed).ok());
  CHECK(f.drv.ApplyConfig(is::Config{}).ok());
  CHECK(f.drv.GetConfig().variant == is::Variant::Isf15acp4);
}

TEST_CASE("guards: sleep-only idle policy: a press wakes what the idle timer put to sleep, and "
          "only that") {
  is::Config cfg{};
  cfg.idle_sleep_ms = 500; // no dim step
  Fixture f(cfg);
  REQUIRE(f.Up());
  f.GetRig().switch_high = true;
  for (int i = 0; i < 80; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK(f.GetChip().DisplayState() == Display::Off);
  f.GetRig().switch_high = false;
  int woke = 0;
  for (int i = 0; i < 20; ++i) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    REQUIRE(r.ok());
    woke += r.value.woke_display ? 1 : 0;
    Elapse(f, 10);
  }
  CHECK_EQ(woke, 1);
  CHECK(f.GetChip().DisplayState() == Display::On);
  // An application switch-off is not undone by a press.
  f.GetRig().switch_high = true;
  for (int i = 0; i < 10; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  REQUIRE(f.drv.DisplayOff().ok());
  f.GetRig().switch_high = false;
  for (int i = 0; i < 20; ++i) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    REQUIRE(r.ok());
    CHECK(!r.value.woke_display);
    Elapse(f, 10);
  }
  CHECK(f.GetChip().DisplayState() == Display::Off);
}

TEST_CASE("guards: idle timers act at exactly their time, not before") {
  is::Config cfg{};
  cfg.idle_dim_ms = 1000;
  cfg.idle_sleep_ms = 3000;
  Fixture f(cfg);
  REQUIRE(f.Up());
  f.GetRig().switch_high = true;
  REQUIRE(f.drv.Poll().ok());
  REQUIRE(f.drv.NotifyActivity().ok());
  Elapse(f, 999);
  REQUIRE(f.drv.Poll().ok());
  CHECK(f.GetChip().DisplayState() == Display::On);
  Elapse(f, 1);
  REQUIRE(f.drv.Poll().ok());
  CHECK(f.GetChip().DisplayState() == Display::Dim);
  Elapse(f, 1999);
  REQUIRE(f.drv.Poll().ok());
  CHECK(f.GetChip().DisplayState() == Display::Dim);
  Elapse(f, 1);
  REQUIRE(f.drv.Poll().ok());
  CHECK(f.GetChip().DisplayState() == Display::Off);
}

TEST_CASE(
    "guards: vertical scrolling needs 64 MUX, in the setup, the ratio and the configuration") {
  Fixture f;
  REQUIRE(f.Up());
  is::ScrollSetup v{};
  v.columns = 0;
  v.vertical = 1;
  v.start_row = 0;
  v.rows = 64;
  REQUIRE(f.drv.SetMultiplexRatio(48).ok());
  CHECK(f.drv.SetupScroll(v).error == is::Error::InvalidArgument);
  REQUIRE(f.drv.SetMultiplexRatio(64).ok());
  REQUIRE(f.drv.SetupScroll(v).ok());
  const int writes = f.GetRig().writes;
  CHECK(f.drv.SetMultiplexRatio(48).error == is::Error::InvalidArgument);
  CHECK(f.drv.ApplyConfig(is::PresetFor(is::Variant::Isc15anp4)).error ==
        is::Error::InvalidArgument);
  CHECK_EQ(f.GetRig().writes, writes); // nothing sent
  CHECK(f.drv.GetConfig().variant == is::Variant::Isf15acp4);
  CHECK(f.drv.SetMultiplexRatio(64).ok());
}

TEST_CASE("guards: the scroll window ends at the last row, not after it") {
  Fixture f;
  REQUIRE(f.Up());
  is::ScrollSetup s{};
  s.start_row = 60;
  s.rows = 4;
  CHECK(f.drv.SetupScroll(s).ok());
  s.rows = 5;
  CHECK(f.drv.SetupScroll(s).error == is::Error::InvalidArgument);
}

TEST_CASE("guards: gray-scale table limits are 125 and strictly rising") {
  Fixture f;
  REQUIRE(f.Up());
  std::array<uint8_t, 32> t{};
  for (std::size_t i = 0; i < t.size(); ++i) {
    t[i] = static_cast<uint8_t>((4U * i) + 1U);
  }
  CHECK_EQ(t[31], 125U);
  CHECK(f.drv.SetGrayScaleTable(t).ok());
  t[31] = 126;
  CHECK(f.drv.SetGrayScaleTable(t).error == is::Error::InvalidArgument);
  t[31] = 125;
  t[30] = 125; // equal to its successor
  CHECK(f.drv.SetGrayScaleTable(t).error == is::Error::InvalidArgument);
}

TEST_CASE("guards: the power-down ends a scroll in the shadow") {
  Fixture f;
  REQUIRE(f.Up());
  is::ScrollSetup s{};
  REQUIRE(f.drv.SetupScroll(s).ok());
  REQUIRE(f.drv.ActivateScroll().ok());
  CHECK(f.drv.IsScrolling());
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK(!f.drv.IsScrolling());
}

TEST_CASE("guards: the small panel's column origin applies to lines, copies and fills") {
  Fixture f(is::PresetFor(is::Variant::Isc15anp4));
  REQUIRE(f.Up());
  REQUIRE(f.drv.DrawLine(0, 0, 10, 0, is::kGreen).ok());
  CHECK_EQ(f.GetChip().Ram(16, 0), static_cast<uint32_t>(is::kGreen.rgb565));
  CHECK_EQ(f.GetChip().Ram(26, 0), static_cast<uint32_t>(is::kGreen.rgb565));
  CHECK_EQ(f.GetChip().Ram(15, 0), 0U); // and nothing left of the origin
  CHECK_EQ(f.GetChip().Ram(27, 0), 0U);
  REQUIRE(f.drv.CopyWindow(0, 0, 10, 0, {5, 10}).ok());
  CHECK_EQ(f.GetChip().Ram(16 + 5, 10), static_cast<uint32_t>(is::kGreen.rgb565));
  REQUIRE(f.drv.FillRectangle(0, 20, 4, 24, is::kRed, is::kBlue).ok());
  CHECK_EQ(f.GetChip().Rgb565At(16 + 2, 22), is::kBlue.rgb565);
}

TEST_CASE("guards: a failed pixel-data transfer marks the shadow in doubt") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(!f.drv.IsShadowInDoubt());
  f.GetRig().fail_write_at = f.GetRig().writes + 1; // the window command goes, the data does not
  CHECK(f.drv.Clear(is::kRed).error == is::Error::Bus);
  CHECK(f.drv.IsShadowInDoubt());
}

TEST_CASE("guards: the stop is never refused while scrolling, and still drives the pin when the "
          "bus is down") {
  for (int which = 0; which < 2; ++which) {
    Fixture f;
    REQUIRE(f.Up());
    is::ScrollSetup sc{};
    REQUIRE(f.drv.SetupScroll(sc).ok());
    REQUIRE(f.drv.ActivateScroll().ok());
    CHECK(f.drv.IsScrolling());
    const int writes = f.GetRig().writes;
    const is::Result<void> r = (which == 0) ? f.drv.DisplayOff() : f.drv.Deinitialize();
    CHECK(r.ok());
    CHECK(f.GetRig().writes > writes); // the unlock and display-off went out
    CHECK(!f.GetChip().Vcc());
    CHECK(f.GetChip().DisplayState() == Display::Off);
  }
  for (int which = 0; which < 2; ++which) {
    Fixture f;
    REQUIRE(f.Up());
    f.GetRig().bus_up_ok = false; // the port cannot bring its bus up any more
    const int writes = f.GetRig().writes;
    const is::Result<void> r = (which == 0) ? f.drv.DisplayOff() : f.drv.Deinitialize();
    CHECK(r.error == is::Error::Bus);
    CHECK(!f.GetChip().Vcc());         // the pin was driven anyway
    CHECK(f.GetRig().writes > writes); // and the transfer was still tried
  }
}

TEST_CASE("guards: tOFF is waited after a failed unlock transfer even when the supply was off") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok()); // supply never enabled
  f.GetRig().fail_writes_from = f.GetRig().writes;
  const uint64_t t0 = f.GetChip().NowUs();
  CHECK(f.drv.Deinitialize().error == is::Error::Bus);
  CHECK((f.GetChip().NowUs() - t0) >=
        (static_cast<uint64_t>(f.drv.GetConfig().vcc_off_ms) * 1000U));
}

TEST_CASE("guards: the start-up reports a pin that will not switch and sends nothing") {
  {
    Fixture f;
    f.GetRig().fail_vcc_off = true; // the first act: drive the supply enable off
    const int writes = f.GetRig().writes;
    CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
    CHECK(!f.drv.IsInitialized());
    CHECK_EQ(f.GetRig().writes, writes); // no controller traffic with the supply state unknown
    CHECK_EQ(f.GetChip().Resets(), 0U);
  }
  {
    // A supply flag left set by a failed power-down stays set when the enable still cannot be
    // driven.
    Fixture f;
    REQUIRE(f.Up());
    f.GetRig().fail_vcc_off = true;
    CHECK(f.drv.Deinitialize().error == is::Error::Bus);
    CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
    CHECK(f.drv.IsSupplyEnabled());
    CHECK(!f.drv.IsInitialized());
  }
  {
    Fixture f;
    f.GetRig().fail_reset_assert = true;
    CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
    CHECK(!f.drv.IsInitialized());
    CHECK_EQ(f.GetChip().Resets(), 0U);
  }
  {
    Fixture f;
    f.GetRig().fail_reset_release = true;
    const int writes = f.GetRig().writes;
    CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
    CHECK(!f.drv.IsInitialized());
    CHECK_EQ(f.GetRig().writes, writes); // never configured a chip held in reset
    CHECK(!f.GetChip().Vcc());
  }
}

TEST_CASE("guards: Reinitialize reports the first failure of its three steps") {
  {
    Fixture f;
    REQUIRE(f.Up());
    f.GetRig().fail_vcc_off = true;
    CHECK(f.drv.Reinitialize().error == is::Error::Bus); // the power-down step
  }
  {
    Fixture f;
    REQUIRE(f.Up());
    f.GetRig().fail_reset_assert = true;
    CHECK(f.drv.Reinitialize().error == is::Error::Bus); // the start-up step
    CHECK(!f.drv.IsInitialized());
    CHECK(!f.GetChip().Vcc());
  }
  {
    Fixture f;
    REQUIRE(f.Up());
    f.GetRig().fail_vcc_on = true;
    CHECK(f.drv.Reinitialize().error == is::Error::Bus); // turning the panel on again
    CHECK(f.drv.IsInitialized());
    CHECK(f.drv.GetOutput() == is::Output::Off);
    CHECK(!f.GetChip().Vcc());
  }
}

TEST_CASE("guards: a failed ApplyConfig changes nothing the driver reports") {
  Fixture f;
  REQUIRE(f.Up());
  const is::Config before = f.drv.GetConfig();
  is::Config c = is::PresetFor(is::Variant::Isc15anp4);
  c.contrast_a = 0x55;
  f.GetRig().fail_write_at = f.GetRig().writes + 3; // part-way through the configuration
  CHECK(f.drv.ApplyConfig(c).error == is::Error::Bus);
  CHECK(f.drv.GetConfig().variant == before.variant);
  CHECK_EQ(f.drv.GetConfig().contrast_a, before.contrast_a);
  CHECK(f.drv.IsShadowInDoubt());
  REQUIRE(f.drv.SetCommandLock(true).ok());
  CHECK(f.drv.ApplyConfig(c).error == is::Error::Locked);
  CHECK(f.drv.GetConfig().variant == before.variant);
}

TEST_CASE("guards: ApplyConfig sets the ratio the vertical-scroll guard tests") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.ApplyConfig(is::PresetFor(is::Variant::Isc15anp4)).ok()); // 48 rows
  is::ScrollSetup v{};
  v.columns = 0;
  v.vertical = 1;
  CHECK(f.drv.SetupScroll(v).error == is::Error::InvalidArgument);
  REQUIRE(f.drv.ApplyConfig(is::Config{}).ok()); // 64 rows again
  CHECK(f.drv.SetupScroll(v).ok());
}

TEST_CASE("guards: Poll returns the error of a failed wake and of a failed idle sleep") {
  is::Config cfg{};
  cfg.idle_sleep_ms = 300;
  {
    Fixture f(cfg);
    REQUIRE(f.Up());
    f.GetRig().switch_high = true;
    for (int i = 0; i < 50; ++i) {
      REQUIRE(f.drv.Poll().ok());
      Elapse(f, 10);
    }
    CHECK(f.GetChip().DisplayState() == Display::Off);
    f.GetRig().switch_high = false;
    f.GetRig().fail_vcc_on = true; // the wake cannot enable the supply
    bool failed = false;
    for (int i = 0; i < 10; ++i) {
      failed = failed || (f.drv.Poll().error == is::Error::Bus);
      Elapse(f, 10);
    }
    CHECK(failed);
    CHECK(!f.GetChip().Vcc());
  }
  {
    Fixture f(cfg);
    REQUIRE(f.Up());
    f.GetRig().switch_high = true;
    REQUIRE(f.drv.Poll().ok());
    f.GetRig().fail_vcc_off = true; // the idle sleep cannot drive the supply off
    bool failed = false;
    for (int i = 0; i < 50; ++i) {
      failed = failed || (f.drv.Poll().error == is::Error::Bus);
      Elapse(f, 10);
    }
    CHECK(failed);
  }
}

TEST_CASE("guards: the idle dim, like the sleep, lets a press wake the display") {
  is::Config cfg{};
  cfg.idle_dim_ms = 300;
  Fixture f(cfg);
  REQUIRE(f.Up());
  f.GetRig().switch_high = true;
  for (int i = 0; i < 50; ++i) {
    REQUIRE(f.drv.Poll().ok());
    Elapse(f, 10);
  }
  CHECK(f.GetChip().DisplayState() == Display::Dim);
  f.GetRig().switch_high = false;
  int woke = 0;
  for (int i = 0; i < 20; ++i) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    REQUIRE(r.ok());
    woke += r.value.woke_display ? 1 : 0;
    Elapse(f, 10);
  }
  CHECK_EQ(woke, 1);
  CHECK(f.GetChip().DisplayState() == Display::On);
}

TEST_CASE("guards: the small panel's column origin applies to fill, dim and clear windows") {
  Fixture f(is::PresetFor(is::Variant::Isc15anp4));
  REQUIRE(f.Up());
  REQUIRE(f.drv.FillRectangle(0, 20, 4, 24, is::kRed, is::kBlue).ok());
  CHECK_EQ(f.GetChip().Rgb565At(16, 20), is::kRed.rgb565);
  CHECK_EQ(f.GetChip().Ram(15, 20), 0U);
  CHECK_EQ(f.GetChip().Ram(21, 20), 0U);
  REQUIRE(f.drv.Clear(is::kGreen).ok());
  REQUIRE(f.drv.ClearWindow(0, 0, 10, 0).ok());
  CHECK_EQ(f.GetChip().Ram(26, 0), 0U); // the last column of the window
  CHECK_EQ(f.GetChip().Ram(27, 0), static_cast<uint32_t>(is::kGreen.rgb565)); // not one more
  CHECK_EQ(f.GetChip().Ram(16, 1), static_cast<uint32_t>(is::kGreen.rgb565));
  REQUIRE(f.drv.DimWindow(0, 5, 10, 5).ok());
  CHECK(f.GetChip().Ram(26, 5) != static_cast<uint32_t>(is::kGreen.rgb565));
  CHECK_EQ(f.GetChip().Ram(27, 5), static_cast<uint32_t>(is::kGreen.rgb565));
}

TEST_CASE("guards: the model wants the whole start-up before VCC") {
  Ssd1331Model m;
  const auto feed = [&m](std::initializer_list<uint8_t> bytes) {
    m.SetCs(true);
    m.SetDc(false);
    for (const uint8_t b : bytes) {
      m.Byte(b);
    }
    m.SetCs(false);
  };
  feed({0xAD, 0x8E});
  feed({0xA0, 0x60});
  m.SetVcc(true);
  CHECK((m.Violations() & kVccBeforeConfig) != 0U); // only two of the commands arrived
}

TEST_CASE("guards: a copy must fit inside the panel at its destination, on both axes") {
  Fixture f;
  REQUIRE(f.Up());
  const int writes = f.GetRig().writes;
  CHECK(f.drv.CopyWindow(0, 0, 9, 9, {86, 0}).ok()); // right edge exactly
  CHECK(f.drv.CopyWindow(0, 0, 9, 9, {87, 0}).error == is::Error::InvalidArgument);
  CHECK(f.drv.CopyWindow(0, 0, 9, 9, {0, 54}).ok()); // bottom edge exactly
  CHECK(f.drv.CopyWindow(0, 0, 9, 9, {0, 55}).error == is::Error::InvalidArgument);
  CHECK(f.GetRig().writes > writes);
}

TEST_CASE("guards: only a window that covers the whole panel ends the rewrite request") {
  Fixture f;
  REQUIRE(f.Up());
  is::ScrollSetup sc{};
  REQUIRE(f.drv.SetupScroll(sc).ok());
  REQUIRE(f.drv.ActivateScroll().ok());
  REQUIRE(f.drv.DeactivateScroll().ok());
  CHECK(f.drv.RamNeedsRewrite());
  static std::array<is::Color, std::size_t{960}> strip{};
  REQUIRE(f.drv.WriteWindow(0, 0, 96, 10, strip).ok()); // full width, not full height
  CHECK(f.drv.RamNeedsRewrite());
  static std::array<is::Color, std::size_t{640}> column{};
  REQUIRE(f.drv.WriteWindow(0, 0, 10, 64, column).ok()); // full height, not full width
  CHECK(f.drv.RamNeedsRewrite());
  REQUIRE(f.drv.Clear().ok());
  CHECK(!f.drv.RamNeedsRewrite());
}

TEST_CASE("guards: Poll returns the error of a failed idle dim") {
  is::Config cfg{};
  cfg.idle_dim_ms = 300;
  Fixture f(cfg);
  REQUIRE(f.Up());
  f.GetRig().switch_high = true;
  REQUIRE(f.drv.Poll().ok());
  f.GetRig().fail_writes_from = f.GetRig().writes; // the dim command cannot be sent
  bool failed = false;
  for (int i = 0; i < 50; ++i) {
    failed = failed || (f.drv.Poll().error == is::Error::Bus);
    Elapse(f, 10);
  }
  CHECK(failed);
}

} // namespace
