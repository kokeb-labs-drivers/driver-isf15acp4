/**
 * @file test_isf15acp4.cpp
 * @brief The driver against the controller model: power order, display states, pixels in every
 * depth and increment mode, the drawing engine, scrolling, the command lock and the setters.
 */
#include <array>
#include <cstdint>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

TEST_CASE("init leaves the supply off, the controller configured and RAM cleared") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK(f.drv.IsInitialized());
  CHECK(!f.GetChip().Vcc());
  CHECK(!f.drv.IsSupplyEnabled());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK(f.GetChip().RamDefined());
  CHECK_EQ(f.GetChip().DefinedCells(), 96 * 64);
  CHECK_EQ(f.GetChip().VccOnCount(), 0U);
  CHECK_EQ(f.GetChip().Violations(), 0U);
  CHECK(f.drv.EnsureInitialized().ok());
  CHECK_EQ(f.GetRig().bus_inits, 1); // a second call does nothing
}

TEST_CASE("init writes the configuration the model can read back") {
  is::Config c{};
  c.contrast_a = 0x21;
  c.contrast_b = 0x22;
  c.contrast_c = 0x23;
  c.master_current = 9;
  Fixture f(c);
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK_EQ(f.GetChip().Contrast(0), 0x21U);
  CHECK_EQ(f.GetChip().Contrast(1), 0x22U);
  CHECK_EQ(f.GetChip().Contrast(2), 0x23U);
  CHECK_EQ(f.GetChip().Reg(0x87), 9U);
  CHECK_EQ(f.GetChip().Mux(), 64);
  CHECK_EQ(f.GetChip().Mode(), 0xA4U);
}

TEST_CASE("display on enables the supply after the init, display off disables it") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  const uint64_t t0 = f.GetChip().NowUs();
  REQUIRE(f.drv.DisplayOn().ok());
  CHECK(f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::On);
  CHECK(f.GetChip().Lit());
  CHECK(f.drv.GetOutput() == is::Output::Normal);
  CHECK((f.GetChip().NowUs() - t0) >= 150000U); // vcc settle 50 ms + tAF 100 ms
  REQUIRE(f.drv.DisplayDim().ok());
  CHECK(f.GetChip().DisplayState() == Display::Dim);
  CHECK(f.drv.GetOutput() == is::Output::Dim);
  REQUIRE(f.drv.DisplayOff().ok());
  CHECK(!f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK(!f.drv.IsSupplyEnabled());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("deinitialize is the power-down: display off, supply off, then the tOFF wait") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK(!f.drv.IsInitialized());
  CHECK(!f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK((f.GetChip().NowUs() - f.GetChip().VccOffAtUs()) >= 100000U);
  CHECK_EQ(f.GetChip().Violations(), 0U);
  // The driver starts again from state 0.
  REQUIRE(f.Up());
  CHECK(f.GetChip().Lit());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("deinitialize on a driver that never started still drives the supply off") {
  Fixture f;
  f.GetChip().SetVcc(true); // something else left it on
  f.GetChip().ClearViolations();
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK(!f.GetChip().Vcc());
}

TEST_CASE("reinitialize restores the state that was on") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.DisplayDim().ok());
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK(f.GetChip().DisplayState() == Display::Dim);
  CHECK(f.GetChip().Vcc());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("every call but the start-up needs the start-up") {
  Fixture f;
  CHECK(f.drv.DisplayOn().error == is::Error::NotInitialized);
  CHECK(f.drv.DisplayDim().error == is::Error::NotInitialized);
  CHECK(f.drv.Clear().error == is::Error::NotInitialized);
  CHECK(f.drv.SetPixel(0, 0, is::kRed).error == is::Error::NotInitialized);
  CHECK(f.drv.SetContrast(1, 2, 3).error == is::Error::NotInitialized);
  CHECK(f.drv.DrawLine(0, 0, 5, 5, is::kRed).error == is::Error::NotInitialized);
  CHECK(f.drv.ActivateScroll().error == is::Error::NotInitialized);
  CHECK_EQ(f.GetRig().writes, 0);
  CHECK_EQ(f.GetRig().gpio_calls, 0);
  CHECK(f.drv.DisplayOff().ok()); // the one exception: the power-down is never refused
}

TEST_CASE("pixels: clear, one pixel, a window, a frame (65k)") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.Clear(is::kRed).ok());
  CHECK_EQ(Shown(f, 0, 0), is::kRed.rgb565);
  CHECK_EQ(Shown(f, 95, 63), is::kRed.rgb565);
  REQUIRE(f.drv.SetPixel(10, 20, is::kBlue).ok());
  CHECK_EQ(Shown(f, 10, 20), is::kBlue.rgb565);
  CHECK_EQ(Shown(f, 11, 20), is::kRed.rgb565);
  std::array<is::Color, 6> win{is::kGreen, is::kBlue, is::kWhite,
                               is::kBlack, is::kRed,  is::MakeColor(10, 100, 200)};
  REQUIRE(f.drv.WriteWindow(5, 6, 3, 2, win).ok());
  for (int i = 0; i < 6; ++i) {
    CHECK_EQ(Shown(f, 5 + (i % 3), 6 + (i / 3)), win[static_cast<std::size_t>(i)].rgb565);
  }
  static std::array<is::Color, std::size_t{6144}> frame{};
  for (std::size_t i = 0; i < frame.size(); ++i) {
    frame[i] = is::Color{static_cast<uint16_t>(i * 7U)};
  }
  REQUIRE(f.drv.Present(frame).ok());
  CHECK_EQ(Shown(f, 0, 0), frame[0].rgb565);
  CHECK_EQ(Shown(f, 95, 0), frame[95].rgb565);
  CHECK_EQ(Shown(f, 3, 17), frame[(17U * 96U) + 3U].rgb565);
  CHECK_EQ(Shown(f, 95, 63), frame[95U + (63U * 96U)].rgb565);
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("pixels land in the same place with vertical RAM increment") {
  is::Config c{};
  c.remap.vertical_increment = true;
  Fixture f(c);
  REQUIRE(f.Up());
  std::array<is::Color, 12> win{};
  for (std::size_t i = 0; i < win.size(); ++i) {
    win[i] = is::Color{static_cast<uint16_t>(0x1111U * (i + 1U))};
  }
  REQUIRE(f.drv.WriteWindow(20, 30, 4, 3, win).ok());
  for (int i = 0; i < 12; ++i) {
    CHECK_EQ(Shown(f, 20 + (i % 4), 30 + (i / 4)), win[static_cast<std::size_t>(i)].rgb565);
  }
  REQUIRE(f.drv.Clear(is::kGreen).ok());
  CHECK_EQ(Shown(f, 50, 50), is::kGreen.rgb565);
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("pixels in 256 colours and in 65k format 2") {
  for (const is::ColorDepth depth : {is::ColorDepth::Colors256, is::ColorDepth::Colors65kFormat2}) {
    is::Config c{};
    c.remap.depth = depth;
    Fixture f(c);
    REQUIRE(f.Up());
    REQUIRE(f.drv.Clear(is::kWhite).ok());
    CHECK_EQ(Shown(f, 7, 7), 0xFFFFU);
    REQUIRE(f.drv.SetPixel(1, 1, is::kBlack).ok());
    CHECK_EQ(Shown(f, 1, 1), 0x0000U);
    REQUIRE(f.drv.SetPixel(2, 2, is::kRed).ok());
    const is::Channels red = is::Split(is::Color{Shown(f, 2, 2)});
    CHECK(red.red >= 28U);
    CHECK_EQ(red.green, 0U);
    CHECK_EQ(red.blue, 0U);
    REQUIRE(f.drv.SetPixel(3, 3, is::kBlue).ok());
    const is::Channels blue = is::Split(is::Color{Shown(f, 3, 3)});
    CHECK(blue.blue >= 28U);
    CHECK_EQ(blue.red, 0U);
    CHECK_EQ(f.GetChip().Violations(), 0U);
  }
}

TEST_CASE("the colour order follows the panel wiring and the remap bit") {
  for (int variant = 0; variant < 4; ++variant) {
    is::Config c{};
    c.red_on_segment_a = ((variant % 2) == 0);
    c.remap.reverse_colour_order = (variant >= 2);
    Fixture f(c);
    f.GetChip().segment_a_is_red = c.red_on_segment_a;
    REQUIRE(f.Up());
    REQUIRE(f.drv.SetPixel(4, 4, is::kRed).ok());
    CHECK_EQ(Shown(f, 4, 4), is::kRed.rgb565);
    REQUIRE(f.drv.SetPixel(5, 4, is::kBlue).ok());
    CHECK_EQ(Shown(f, 5, 4), is::kBlue.rgb565);
  }
}

TEST_CASE("raw window bytes are sent as given") {
  Fixture f;
  REQUIRE(f.Up());
  const std::array<uint8_t, 4> raw{0xF8, 0x00, 0x07, 0xE0};
  REQUIRE(f.drv.WriteWindowRaw(0, 0, 2, 1, raw).ok());
  CHECK_EQ(f.GetChip().Ram(0, 0), 0xF800U);
  CHECK_EQ(f.GetChip().Ram(1, 0), 0x07E0U);
}

TEST_CASE("the drawing engine: line, rectangle, fill, copy, dim, clear") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.DrawLine(2, 2, 20, 2, is::kGreen).ok());
  CHECK_EQ(Shown(f, 2, 2), is::kGreen.rgb565);
  CHECK_EQ(Shown(f, 20, 2), is::kGreen.rgb565);
  CHECK_EQ(Shown(f, 10, 2), is::kGreen.rgb565);
  REQUIRE(f.drv.DrawRectangle(30, 10, 40, 20, is::kRed).ok());
  CHECK_EQ(Shown(f, 30, 10), is::kRed.rgb565);
  CHECK_EQ(Shown(f, 40, 20), is::kRed.rgb565);
  CHECK_EQ(Shown(f, 35, 15), 0U); // not filled
  REQUIRE(f.drv.FillRectangle(50, 10, 60, 20, is::kRed, is::kBlue).ok());
  CHECK_EQ(Shown(f, 50, 10), is::kRed.rgb565);
  CHECK_EQ(Shown(f, 55, 15), is::kBlue.rgb565);
  REQUIRE(f.drv.CopyWindow(50, 10, 60, 20, {70, 40}).ok());
  CHECK_EQ(Shown(f, 75, 45), is::kBlue.rgb565);
  REQUIRE(f.drv.ClearWindow(50, 10, 60, 20).ok());
  CHECK_EQ(Shown(f, 55, 15), 0U);
  REQUIRE(f.drv.DimWindow(70, 40, 80, 50).ok());
  CHECK_EQ(f.GetChip().Violations(), 0U);
  // The fill bit is left as it was found: a later rectangle is an outline again.
  REQUIRE(f.drv.DrawRectangle(0, 40, 10, 50, is::kWhite).ok());
  CHECK_EQ(Shown(f, 5, 45), 0U);
}

TEST_CASE("the engine wait keeps the controller from being given a command while it is busy") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetChip().engine_busy_us = 2500; // below the default settle of 3 ms
  REQUIRE(f.drv.DrawLine(0, 0, 90, 60, is::kWhite).ok());
  REQUIRE(f.drv.DrawLine(0, 60, 90, 0, is::kWhite).ok());
  CHECK_EQ(f.GetChip().Violations(), 0U);
  // A controller slower than the setting is a violation the model reports: it is the knob's reason.
  f.GetChip().engine_busy_us = 50000;
  REQUIRE(f.drv.DrawLine(0, 0, 90, 60, is::kWhite).ok());
  REQUIRE(f.drv.DrawLine(0, 60, 90, 0, is::kWhite).ok());
  CHECK((f.GetChip().Violations() & kEngineBusy) != 0U);
}

TEST_CASE("scrolling: setup, activate, writes refused, deactivate asks for a rewrite") {
  Fixture f;
  REQUIRE(f.Up());
  CHECK(f.drv.ActivateScroll().error == is::Error::ScrollNotSet);
  is::ScrollSetup s{};
  s.columns = 2;
  s.start_row = 8;
  s.rows = 32;
  s.vertical = 1;
  s.interval = is::ScrollInterval::Frames10;
  REQUIRE(f.drv.SetupScroll(s).ok());
  const std::array<uint8_t, 5> sent = f.GetChip().ScrollParams();
  CHECK_EQ(sent[0], 2U);
  CHECK_EQ(sent[1], 8U);
  CHECK_EQ(sent[2], 32U);
  CHECK_EQ(sent[3], 1U);
  CHECK_EQ(sent[4], 1U);
  REQUIRE(f.drv.ActivateScroll().ok());
  CHECK(f.drv.IsScrolling());
  CHECK(f.GetChip().Scrolling());
  CHECK(f.drv.Clear().error == is::Error::ScrollActive);
  CHECK(f.drv.DrawLine(0, 0, 3, 3, is::kRed).error == is::Error::ScrollActive);
  CHECK(f.drv.SetupScroll(s).error == is::Error::ScrollActive);
  REQUIRE(f.drv.DeactivateScroll().ok());
  CHECK(!f.GetChip().Scrolling());
  CHECK(f.drv.RamNeedsRewrite());
  REQUIRE(f.drv.Clear().ok());
  CHECK(!f.drv.RamNeedsRewrite());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("the power-down runs while scrolling and while locked") {
  Fixture f;
  REQUIRE(f.Up());
  is::ScrollSetup s{};
  REQUIRE(f.drv.SetupScroll(s).ok());
  REQUIRE(f.drv.ActivateScroll().ok());
  REQUIRE(f.drv.SetCommandLock(true).ok());
  CHECK(f.GetChip().Locked());
  CHECK(f.drv.IsLocked());
  CHECK(f.drv.DisplayOn().error == is::Error::Locked);
  CHECK(f.drv.Clear().error == is::Error::Locked);
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK(!f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK(!f.GetChip().Locked());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("display off unlocks first, so the supply can go off") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetCommandLock(true).ok());
  REQUIRE(f.drv.DisplayOff().ok());
  CHECK(!f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("setters reach the controller") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetContrast(0x20, 0x30, 0x31).ok());
  CHECK_EQ(f.GetChip().Contrast(0), 0x20U);
  CHECK_EQ(f.GetChip().Contrast(2), 0x31U);
  REQUIRE(f.drv.SetMasterCurrent(3).ok());
  CHECK_EQ(f.GetChip().Reg(0x87), 3U);
  REQUIRE(f.drv.SetPrechargeSpeed(0x10, 0x20, 0x30).ok());
  CHECK_EQ(f.GetChip().Reg(0x8A), 0x10U);
  CHECK_EQ(f.GetChip().Reg(0x8C), 0x30U);
  REQUIRE(f.drv.SetStartLine(5).ok());
  CHECK_EQ(f.GetChip().Reg(0xA1), 5U);
  REQUIRE(f.drv.SetDisplayOffset(7).ok());
  CHECK_EQ(f.GetChip().Reg(0xA2), 7U);
  REQUIRE(f.drv.SetDisplayMode(is::DisplayMode::Inverse).ok());
  CHECK_EQ(f.GetChip().Mode(), 0xA7U);
  REQUIRE(f.drv.SetDisplayMode(is::DisplayMode::AllOn).ok());
  CHECK_EQ(f.GetChip().Mode(), 0xA5U);
  REQUIRE(f.drv.SetDisplayMode(is::DisplayMode::AllOff).ok());
  CHECK_EQ(f.GetChip().Mode(), 0xA6U);
  REQUIRE(f.drv.SetDisplayMode(is::DisplayMode::Normal).ok());
  CHECK_EQ(f.GetChip().Mode(), 0xA4U);
  REQUIRE(f.drv.SetMultiplexRatio(48).ok());
  CHECK_EQ(f.GetChip().Mux(), 48);
  REQUIRE(f.drv.SetMultiplexRatio(64).ok());
  REQUIRE(f.drv.SetPowerSave(true).ok());
  REQUIRE(f.drv.SetPhasePeriods(4, 5).ok());
  CHECK_EQ(f.GetChip().Reg(0xB1), 0x54U);
  REQUIRE(f.drv.SetClock(2, 9).ok());
  CHECK_EQ(f.GetChip().Reg(0xB3), 0x92U);
  REQUIRE(f.drv.SetPrechargeLevel(10).ok());
  REQUIRE(f.drv.SetVcomh(20).ok());
  REQUIRE(f.drv.UseLinearGrayScale().ok());
  std::array<uint8_t, is::reg::kGrayScaleEntries> table{};
  for (std::size_t i = 0; i < table.size(); ++i) {
    table[i] = static_cast<uint8_t>(i + 1U);
  }
  REQUIRE(f.drv.SetGrayScaleTable(table).ok());
  CHECK_EQ(f.GetChip().Gray(0), 1U);
  CHECK_EQ(f.GetChip().Gray(31), 32U);
  REQUIRE(f.drv.SetDimSettings(is::DimSettings{}).ok());
  REQUIRE(f.drv.SetRemap(is::Remap{}).ok());
  REQUIRE(f.drv.Nop().ok());
  CHECK_EQ(f.GetChip().Violations(), 0U);
  CHECK_EQ(f.GetChip().UnknownOpcodes(), 0U);
}

TEST_CASE("apply config: stored before the start, sent after it") {
  Fixture f;
  is::Config c{};
  c.contrast_a = 0x31;
  REQUIRE(f.drv.ApplyConfig(c).ok());
  CHECK_EQ(f.GetRig().writes, 0);
  CHECK_EQ(f.drv.GetConfig().contrast_a, 0x31U);
  REQUIRE(f.Up());
  CHECK_EQ(f.GetChip().Contrast(0), 0x31U);
  c.contrast_a = 0x41;
  REQUIRE(f.drv.ApplyConfig(c).ok());
  CHECK_EQ(f.GetChip().Contrast(0), 0x41U);
  CHECK(f.GetChip().Lit()); // the panel stays on
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE("variants: geometry, offset and no switch on the display-only module") {
  const is::Config small = is::PresetFor(is::Variant::Isc15anp4);
  Fixture f(small);
  REQUIRE(f.Up());
  CHECK_EQ(f.drv.GetGeometry().width, 64U);
  CHECK_EQ(f.drv.GetGeometry().height, 48U);
  CHECK_EQ(f.GetChip().Mux(), 48);
  REQUIRE(f.drv.Clear(is::kRed).ok());
  CHECK_EQ(Shown(f, 16, 0), is::kRed.rgb565); // RAM column 16 is the first visible one
  CHECK_EQ(Shown(f, 79, 47), is::kRed.rgb565);
  CHECK(f.drv.SetPixel(64, 0, is::kRed).error == is::Error::InvalidArgument);
  CHECK(is::HasSwitch(is::Variant::Isf15acp4));
  CHECK(is::HasSwitch(is::Variant::Isc15anp4));
  CHECK(!is::HasSwitch(is::Variant::Isc01p));
  Fixture g(is::PresetFor(is::Variant::Isc01p));
  REQUIRE(g.Up());
  CHECK_EQ(g.drv.GetGeometry().width, 52U);
  CHECK(g.drv.Poll().error == is::Error::Unsupported);
}

TEST_CASE("raw commands are tracked as doubt") {
  Fixture f;
  REQUIRE(f.Up());
  CHECK(!f.drv.IsShadowInDoubt());
  const std::array<uint8_t, 1> one{0x40};
  REQUIRE(f.drv.TraceCommand(0x81, one).ok());
  CHECK_EQ(f.GetChip().Contrast(0), 0x40U);
  CHECK_EQ(f.drv.GetLastCommandOpcode(), 0x81U);
  CHECK_EQ(f.drv.GetLastTransferBytes(), 2U);
  CHECK(f.drv.IsShadowInDoubt());
}

TEST_CASE("the minimal port: no switch, no clock, no log") {
  MinimalFixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  REQUIRE(f.drv.DisplayOn().ok());
  CHECK(f.drv.Poll().error == is::Error::Unsupported);
  CHECK(f.drv.ReadSwitchRaw().error == is::Error::Unsupported);
  CHECK(f.drv.NotifyActivity().error == is::Error::Unsupported);
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK_EQ(f.port.rig.chip.Violations(), 0U);
}

} // namespace
