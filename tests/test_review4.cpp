/**
 * @file test_review4.cpp
 * @brief Tests from review round 4: every wait is bounded even on a configuration that was never
 * checked; a restart keeps what the setters stored; a running press survives a configuration that
 * leaves the switch settings alone; raw scroll and master-configuration commands keep the shadow
 * right; a setter whose transfer failed leaves the stored configuration as it was.
 */
#include <array>
#include <cstdint>
#include <functional>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

constexpr uint64_t kMaxWaitUs = static_cast<uint64_t>(is::kWaitMaxMs) * 1000U;

TEST_CASE("review4: the power-down wait is bounded on a configuration never checked") {
  is::Config c;
  c.vcc_off_ms = 0xFFFFFFFFU; // invalid: the constructor does not check it
  Fixture f(c);
  f.GetRig().fail_vcc_off = true; // the pin fails, so the driver waits tOFF
  const uint64_t t0 = f.GetChip().NowUs();
  CHECK(f.drv.Deinitialize().error == is::Error::Bus);
  const uint64_t waited = f.GetChip().NowUs() - t0;
  CHECK(waited >= kMaxWaitUs);
  CHECK(waited < (kMaxWaitUs + 1000U));
}

TEST_CASE("review4: the power-down wait is bounded when every transfer fails") {
  is::Config c;
  c.vcc_off_ms = 4294967U; // 4294967 * 1000 still fits in 32 bits
  Fixture f(c);
  f.GetRig().fail_writes_from = 0;
  const uint64_t t0 = f.GetChip().NowUs();
  CHECK(f.drv.Deinitialize().error == is::Error::Bus);
  CHECK((f.GetChip().NowUs() - t0) < (kMaxWaitUs + 1000U));
}

TEST_CASE("review4: a restart after SetGrayScaleTable loads the start-up gray scale (B9h)") {
  Fixture f;
  REQUIRE(f.Up());
  std::array<uint8_t, 32> widths{};
  for (std::size_t i = 0; i < widths.size(); ++i) {
    widths[i] = static_cast<uint8_t>((3U * i) + 2U);
  }
  REQUIRE(f.drv.SetGrayScaleTable(widths).ok());
  CHECK(f.drv.GetConfig().linear_gray_scale); // the start-up choice is kept
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK(f.GetChip().Seen(0xB9)); // NKK step 4.15 again
  CHECK_EQ(f.GetChip().Gray(31), 63U);
}

TEST_CASE("review4: pre-charge speeds set on a smaller module are sent again by a restart") {
  Fixture f(is::PresetFor(is::Variant::Isc15anp4));
  REQUIRE(f.Up());
  CHECK(!f.GetChip().Seen(0x8A)); // Table 1 does not send them
  REQUIRE(f.drv.SetPrechargeSpeed(0x11, 0x22, 0x33).ok());
  CHECK(f.drv.GetConfig().send_precharge_speeds);
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK_EQ(f.GetChip().Reg(0x8A), 0x11U);
  CHECK_EQ(f.GetChip().Reg(0x8C), 0x33U);
}

TEST_CASE("review4: a restart goes back to the module's multiplex ratio (documented)") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetMultiplexRatio(32).ok());
  CHECK_EQ(f.GetChip().Mux(), 32);
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK_EQ(f.GetChip().Mux(), 64);
}

int CountPresses(Fixture& f, int polls) {
  int pressed = 0;
  for (int i = 0; i < polls; ++i) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    if (!r) {
      return -1; // a failed poll: the caller's count check fails
    }
    pressed += (r.value.event == is::ButtonEvent::Pressed) ? 1 : 0;
    f.GetChip().Elapse(10000U);
  }
  return pressed;
}

TEST_CASE("review4: a press in progress survives ApplyConfig and Reinitialize") {
  Fixture f;
  REQUIRE(f.Up());
  CHECK_EQ(CountPresses(f, 3), 0);
  f.GetRig().switch_high = false;  // press
  CHECK_EQ(CountPresses(f, 1), 0); // still bouncing
  REQUIRE(f.drv.ApplyConfig(f.drv.GetConfig()).ok());
  CHECK_EQ(CountPresses(f, 10), 1);
  f.GetRig().switch_high = true;
  CHECK_EQ(CountPresses(f, 10), 0);
  f.GetRig().switch_high = false;
  CHECK_EQ(CountPresses(f, 1), 0);
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK_EQ(CountPresses(f, 10), 1);
}

TEST_CASE("review4: a changed switch configuration still takes effect at once") {
  Fixture f;
  REQUIRE(f.Up());
  is::Config c = f.drv.GetConfig();
  c.button.debounce_ms = 200;
  c.button.long_press_ms = 800;
  REQUIRE(f.drv.ApplyConfig(c).ok());
  CHECK_EQ(CountPresses(f, 2), 0);
  f.GetRig().switch_high = false;
  CHECK_EQ(CountPresses(f, 10), 0); // 100 ms is below the new debounce time
  CHECK_EQ(CountPresses(f, 15), 1);
}

TEST_CASE("review4: raw 2Fh and 2Eh keep IsScrolling() in step") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetupScroll(is::ScrollSetup{}).ok());
  REQUIRE(f.drv.TraceCommand(0x2F, {}).ok());
  CHECK(f.drv.IsScrolling());
  CHECK(f.drv.Clear(is::kRed).error == is::Error::ScrollActive);
  CHECK(f.drv.ApplyConfig(f.drv.GetConfig()).error == is::Error::ScrollActive);
  REQUIRE(f.drv.TraceCommand(0x2E, {}).ok());
  CHECK(!f.drv.IsScrolling());
  CHECK(f.drv.Clear(is::kRed).ok());
}

TEST_CASE("review4: a raw ADh takes only 8Eh (A[0] must be 0, Table 10)") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  const uint32_t before = f.GetChip().Transfers();
  const std::array<uint8_t, 1> bad{0x8F};
  CHECK(f.drv.TraceCommand(0xAD, bad).error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetChip().Transfers(), before);
  const std::array<uint8_t, 1> good{0x8E};
  CHECK(f.drv.TraceCommand(0xAD, good).ok());
}

TEST_CASE("review4: a lowering ApplyConfig that fails at its last transfer keeps the dim limit") {
  Fixture f;
  REQUIRE(f.Up());
  const int start = f.GetRig().writes;
  REQUIRE(f.drv.ApplyConfig(f.drv.GetConfig()).ok());
  const int per_config = f.GetRig().writes - start;
  is::Config c = f.drv.GetConfig();
  c.contrast_a = 0x10;
  f.GetRig().fail_write_at = f.GetRig().writes + per_config - 1;
  CHECK(f.drv.ApplyConfig(c).error == is::Error::Bus);
  f.GetRig().fail_write_at = -1;
  is::DimSettings d = f.drv.GetConfig().dim;
  d.contrast_a = 0x18; // within the contrast in force (1Ch), above the refused 10h
  CHECK(f.drv.SetDimSettings(d).ok());
}

/// Every field a setter can change, as numbers.
std::array<uint32_t, 25> Fields(const is::Config& c) {
  const is::Remap& r = c.remap;
  return {c.contrast_a,
          c.contrast_b,
          c.contrast_c,
          c.precharge_speed_a,
          c.precharge_speed_b,
          c.precharge_speed_c,
          c.send_precharge_speeds ? 1U : 0U,
          c.master_current,
          (r.vertical_increment ? 1U : 0U) | (r.column_reverse ? 2U : 0U) |
              (r.reverse_colour_order ? 4U : 0U) | (r.com_left_right_swap ? 8U : 0U) |
              (r.com_scan_reverse ? 16U : 0U) | (r.com_split_odd_even ? 32U : 0U),
          static_cast<uint32_t>(r.depth),
          c.start_line,
          c.display_offset,
          static_cast<uint32_t>(c.mode),
          c.dim.contrast_a,
          c.dim.contrast_b,
          c.dim.contrast_c,
          c.dim.precharge,
          c.power_save ? 1U : 0U,
          c.phase1,
          c.phase2,
          c.clock_divide,
          c.clock_frequency,
          c.precharge_level,
          c.vcomh_level,
          c.linear_gray_scale ? 1U : 0U};
}

TEST_CASE("review4: a setter whose transfer fails leaves GetConfig() as it was") {
  using Drv = is::ISF15ACP4<FullPort>;
  const std::array<std::function<is::Result<void>(Drv&)>, 14> setters{{
      [](Drv& d) { return d.SetContrast(0x20, 0x30, 0x30); },
      [](Drv& d) { return d.SetMasterCurrent(3); },
      [](Drv& d) { return d.SetPrechargeSpeed(1, 2, 3); },
      [](Drv& d) {
        is::Remap r = d.GetConfig().remap;
        r.column_reverse = true;
        return d.SetRemap(r);
      },
      [](Drv& d) { return d.SetStartLine(5); },
      [](Drv& d) { return d.SetDisplayOffset(6); },
      [](Drv& d) { return d.SetDisplayMode(is::DisplayMode::Inverse); },
      [](Drv& d) {
        is::DimSettings s = d.GetConfig().dim;
        s.contrast_a = 0x05;
        return d.SetDimSettings(s);
      },
      [](Drv& d) { return d.SetPowerSave(true); },
      [](Drv& d) { return d.SetPhasePeriods(3, 4); },
      [](Drv& d) { return d.SetClock(1, 2); },
      [](Drv& d) { return d.SetPrechargeLevel(3); },
      [](Drv& d) { return d.SetVcomh(4); },
      [](Drv& d) { return d.UseLinearGrayScale(); },
  }};
  for (std::size_t i = 0; i < setters.size(); ++i) {
    Fixture f;
    REQUIRE(f.Up());
    if (i == 13U) {
      // Start from a configuration without B9h so the setter has something to change.
      is::Config c = f.drv.GetConfig();
      c.linear_gray_scale = false;
      REQUIRE(f.drv.ApplyConfig(c).ok());
    }
    const std::array<uint32_t, 25> before = Fields(f.drv.GetConfig());
    f.GetRig().fail_write_at = f.GetRig().writes; // the setter's own transfer
    CHECK(setters[i](f.drv).error == is::Error::Bus);
    CHECK(Fields(f.drv.GetConfig()) == before);
  }
}

} // namespace
