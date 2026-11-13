/**
 * @file test_errors.cpp
 * @brief Error values: every chip-specific one is returned somewhere, and InvalidArgument sends
 * nothing (no transfer, no pin change).
 */
#include <array>
#include <cstdint>
#include <string_view>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

TEST_CASE("errors: chip-specific values are returned by the calls their docs name") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  // SupplyOff: a raw display-on without the supply.
  const std::array<uint8_t, 0> none{};
  CHECK(f.drv.TraceCommand(0xAF, none).error == is::Error::SupplyOff);
  CHECK(f.drv.TraceCommand(0xAC, none).error == is::Error::SupplyOff);
  REQUIRE(f.drv.DisplayOn().ok());
  // ScrollNotSet, ScrollActive.
  CHECK(f.drv.ActivateScroll().error == is::Error::ScrollNotSet);
  REQUIRE(f.drv.SetupScroll(is::ScrollSetup{}).ok());
  REQUIRE(f.drv.ActivateScroll().ok());
  CHECK(f.drv.FillWindow(0, 0, 2, 2, is::kRed).error == is::Error::ScrollActive);
  REQUIRE(f.drv.DeactivateScroll().ok());
  // Locked.
  REQUIRE(f.drv.SetCommandLock(true).ok());
  CHECK(f.drv.SetContrast(1, 1, 1).error == is::Error::Locked);
  CHECK(f.drv.DrawLine(0, 0, 1, 1, is::kRed).error == is::Error::Locked);
  CHECK(f.drv.SetCommandLock(false).ok());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("errors: the base set keeps its values and every value has a name") {
  CHECK_EQ(static_cast<int>(is::Error::Ok), 0);
  CHECK_EQ(static_cast<int>(is::Error::NotInitialized), 1);
  CHECK_EQ(static_cast<int>(is::Error::InvalidArgument), 2);
  CHECK_EQ(static_cast<int>(is::Error::Bus), 3);
  CHECK_EQ(static_cast<int>(is::Error::Timeout), 4);
  CHECK_EQ(static_cast<int>(is::Error::DeviceIdMismatch), 5);
  CHECK_EQ(static_cast<int>(is::Error::Unsupported), 9);
  CHECK_EQ(static_cast<int>(is::Error::ScrollActive), 16);
  for (int i = 0; i < 20; ++i) {
    const char* name = is::ToString(static_cast<is::Error>(i));
    CHECK(name != nullptr);
    const bool defined = (i <= 9) || ((i >= 16) && (i <= 19));
    CHECK((std::string_view(name) != "Unknown") == defined);
  }
}

/// Every call with an invalid argument: the port sees no transfer and no pin change.
TEST_CASE("errors: InvalidArgument sends nothing") {
  Fixture f;
  REQUIRE(f.Up());
  const int writes = f.GetRig().writes;
  const int gpio = f.GetRig().gpio_calls;
  const std::array<is::Color, 4> four{};
  const std::array<uint8_t, 3> three{};
  const is::Error bad = is::Error::InvalidArgument;
  CHECK(f.drv.SetMasterCurrent(16).error == bad);
  CHECK(f.drv.SetPrechargeLevel(32).error == bad);
  CHECK(f.drv.SetVcomh(32).error == bad);
  CHECK(f.drv.SetStartLine(64).error == bad);
  CHECK(f.drv.SetDisplayOffset(64).error == bad);
  CHECK(f.drv.SetMultiplexRatio(15).error == bad);
  CHECK(f.drv.SetMultiplexRatio(65).error == bad);
  CHECK(f.drv.SetPhasePeriods(0, 1).error == bad);
  CHECK(f.drv.SetPhasePeriods(1, 16).error == bad);
  CHECK(f.drv.SetClock(16, 0).error == bad);
  CHECK(f.drv.SetClock(0, 16).error == bad);
  CHECK(f.drv.SetDisplayMode(Raw<is::DisplayMode>(9)).error == bad);
  CHECK(f.drv.SetDimSettings(is::DimSettings{1, 2, 3, 32}).error == bad);
  is::Remap remap{};
  remap.depth = Raw<is::ColorDepth>(3);
  CHECK(f.drv.SetRemap(remap).error == bad);
  CHECK(f.drv.SetPixel(96, 0, is::kRed).error == bad);
  CHECK(f.drv.SetPixel(0, 64, is::kRed).error == bad);
  CHECK(f.drv.FillWindow(0, 0, 0, 1, is::kRed).error == bad);
  CHECK(f.drv.FillWindow(90, 0, 7, 1, is::kRed).error == bad);
  CHECK(f.drv.WriteWindow(0, 0, 3, 2, four).error == bad);
  CHECK(f.drv.WriteWindowRaw(0, 0, 2, 1, three).error == bad);
  CHECK(f.drv.Present(four).error == bad);
  CHECK(f.drv.DrawLine(0, 0, 96, 0, is::kRed).error == bad);
  CHECK(f.drv.DrawLine(0, 0, 0, 64, is::kRed).error == bad);
  CHECK(f.drv.DrawRectangle(10, 10, 5, 20, is::kRed).error == bad);
  CHECK(f.drv.DrawRectangle(10, 20, 20, 10, is::kRed).error == bad);
  CHECK(f.drv.FillRectangle(0, 0, 100, 5, is::kRed, is::kBlue).error == bad);
  CHECK(f.drv.CopyWindow(0, 0, 10, 10, {90, 60}).error == bad);
  CHECK(f.drv.CopyWindow(10, 10, 5, 5, {0, 0}).error == bad);
  CHECK(f.drv.DimWindow(5, 5, 4, 4).error == bad);
  CHECK(f.drv.ClearWindow(0, 0, 96, 1).error == bad);
  is::ScrollSetup none{};
  none.columns = 0;
  none.vertical = 0;
  CHECK(f.drv.SetupScroll(none).error == bad);
  is::ScrollSetup over{};
  over.start_row = 40;
  over.rows = 30;
  CHECK(f.drv.SetupScroll(over).error == bad);
  is::ScrollSetup cols{};
  cols.columns = 96;
  CHECK(f.drv.SetupScroll(cols).error == bad);
  is::ScrollSetup vert{};
  vert.vertical = 64;
  CHECK(f.drv.SetupScroll(vert).error == bad);
  is::ButtonConfig badbutton{};
  badbutton.debounce_ms = 0;
  CHECK(f.drv.SetButtonConfig(badbutton).error == bad);
  const std::array<uint8_t, 2> two{};
  CHECK(f.drv.TraceCommand(0x00, three).error == bad); // not an opcode
  CHECK(f.drv.TraceCommand(0x81, two).error == bad);   // wrong parameter count
  CHECK(f.drv.TraceCommand(0xAF, three).error == bad);
  is::Config cfg{};
  cfg.master_current = 99;
  CHECK(f.drv.ApplyConfig(cfg).error == bad);
  CHECK_EQ(f.GetRig().writes, writes + 0);
  CHECK_EQ(f.GetRig().gpio_calls, gpio);
  CHECK_EQ(f.GetChip().Violations(), 0U);
  CHECK(f.drv.WriteWindow(0, 0, 2, 2, four).ok()); // the same window with the right count is sent
  CHECK(f.GetRig().writes > writes);
}

TEST_CASE("errors: an invalid configuration fails the start-up before any I/O") {
  is::Config cfg{};
  cfg.master_current = 16;
  Fixture f(cfg);
  CHECK(f.drv.EnsureInitialized().error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetRig().writes, 0);
  CHECK_EQ(f.GetRig().gpio_calls, 0);
  CHECK_EQ(f.GetRig().bus_inits, 0);
  CHECK(!f.drv.IsInitialized());
  CHECK(f.drv.Reinitialize().error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetRig().writes, 0);
  CHECK_EQ(f.GetRig().gpio_calls, 0);
}

TEST_CASE("errors: every field of Validate has a failing value") {
  const auto bad = [](auto edit) {
    is::Config c{};
    edit(c);
    return is::Validate(c) == is::Error::InvalidArgument;
  };
  CHECK(bad([](is::Config& c) { c.variant = Raw<is::Variant>(3); }));
  CHECK(bad([](is::Config& c) { c.remap.depth = Raw<is::ColorDepth>(3); }));
  CHECK(bad([](is::Config& c) { c.mode = Raw<is::DisplayMode>(4); }));
  CHECK(bad([](is::Config& c) { c.master_current = 16; }));
  CHECK(bad([](is::Config& c) { c.start_line = 64; }));
  CHECK(bad([](is::Config& c) { c.display_offset = 64; }));
  CHECK(bad([](is::Config& c) { c.precharge_level = 32; }));
  CHECK(bad([](is::Config& c) { c.vcomh_level = 32; }));
  CHECK(bad([](is::Config& c) { c.dim.precharge = 32; }));
  CHECK(bad([](is::Config& c) { c.phase1 = 0; }));
  CHECK(bad([](is::Config& c) { c.phase1 = 16; }));
  CHECK(bad([](is::Config& c) { c.phase2 = 0; }));
  CHECK(bad([](is::Config& c) { c.phase2 = 16; }));
  CHECK(bad([](is::Config& c) { c.clock_divide = 16; }));
  CHECK(bad([](is::Config& c) { c.clock_frequency = 16; }));
  CHECK(bad([](is::Config& c) { c.vcc_settle_ms = 0; }));
  CHECK(bad([](is::Config& c) { c.vcc_settle_ms = 1001; }));
  CHECK(bad([](is::Config& c) { c.vcc_off_ms = 1001; }));
  CHECK(bad([](is::Config& c) { c.engine_settle_us = 100001; }));
  CHECK(bad([](is::Config& c) { c.idle_dim_ms = 6000001; }));
  CHECK(bad([](is::Config& c) { c.idle_sleep_ms = 6000001; }));
  CHECK(bad([](is::Config& c) {
    c.idle_dim_ms = 500;
    c.idle_sleep_ms = 500;
  }));
  CHECK(bad([](is::Config& c) { c.button.debounce_ms = 0; }));
  CHECK(is::Validate(is::Config{}) == is::Error::Ok);
}

TEST_CASE("errors: a port whose bus cannot come up fails the start-up and sends nothing") {
  Fixture f;
  f.GetRig().bus_up_ok = false;
  CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
  CHECK_EQ(f.GetRig().writes, 0);
  CHECK(!f.GetChip().Vcc());
}

} // namespace
