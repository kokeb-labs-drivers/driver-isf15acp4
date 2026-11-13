/**
 * @file test_bus_faults.cpp
 * @brief A failed transfer or pin write at every point of a full run: the driver reports it, the
 * supply is never left on without a configured controller, and the power-down always completes.
 */
#include <cstdint>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

/// One run of the driver through its calls; returns how many calls failed.
int Scenario(Fixture& f) {
  int failed = 0;
  const auto note = [&failed](const is::Result<void>& r) { failed += r.ok() ? 0 : 1; };
  note(f.drv.EnsureInitialized());
  note(f.drv.DisplayOn());
  note(f.drv.Clear(is::kBlue));
  note(f.drv.SetPixel(3, 3, is::kRed));
  note(f.drv.DrawLine(0, 0, 40, 30, is::kGreen));
  note(f.drv.SetContrast(0x20, 0x30, 0x30));
  note(f.drv.DisplayDim());
  note(f.drv.DisplayOn());
  note(f.drv.DisplayOff());
  return failed;
}

/// Violations that a failed power-down may legitimately cause: the supply is forced off even when
/// the display-off transfer did not get through.
constexpr uint32_t kForcedOff = kVccOffWhileOn;

TEST_CASE("faults: a failed write at every index is reported and never unsafe") {
  Fixture clean;
  REQUIRE(Scenario(clean) == 0);
  const int total = clean.GetRig().writes;
  CHECK(total > 100);
  int reported = 0;
  for (int i = 0; i < total; ++i) {
    Fixture f;
    f.GetRig().fail_write_at = i;
    const int failed = Scenario(f);
    reported += (failed > 0) ? 1 : 0;
    CHECK(failed >= 1); // a write that fails is never swallowed
    const uint32_t v = f.GetChip().Violations() & ~kForcedOff;
    if (v != 0U) {
      CHECK_EQ(v, 0U);
      std::printf("      at write index %d: violations 0x%x\n", i, static_cast<unsigned>(v));
      break;
    }
    // Whatever failed, a power-down with a working bus ends with the supply off.
    f.GetRig().fail_write_at = -1;
    CHECK(f.drv.Deinitialize().ok());
    CHECK(!f.GetChip().Vcc());
    CHECK(f.GetChip().DisplayState() == Display::Off);
  }
  CHECK_EQ(reported, total);
}

TEST_CASE("faults: a bus that dies for good: the supply is off and stays off") {
  for (int from = 0; from < 40; ++from) {
    Fixture f;
    f.GetRig().fail_writes_from = from;
    static_cast<void>(Scenario(f));
    CHECK(!f.GetChip().Vcc() || (from > 30)); // with no bus nothing after the start may leave it on
    static_cast<void>(f.drv.Deinitialize());
    CHECK(!f.GetChip().Vcc());
  }
}

TEST_CASE("faults: the supply is never enabled before the configuration reached the controller") {
  for (int i = 0; i < 40; ++i) {
    Fixture f;
    f.GetRig().fail_write_at = i;
    static_cast<void>(f.drv.EnsureInitialized());
    static_cast<void>(f.drv.DisplayOn()); // refused when the start-up failed
    CHECK((f.GetChip().Violations() & kVccBeforeConfig) == 0U);
    CHECK((f.GetChip().Violations() & kDisplayOnWithoutVcc) == 0U);
    if (!f.drv.IsInitialized()) {
      CHECK(!f.GetChip().Vcc());
    }
  }
}

TEST_CASE("faults: the supply pin cannot be driven on") {
  Fixture f;
  f.GetRig().fail_vcc_on = true;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK(f.drv.DisplayOn().error == is::Error::Bus);
  CHECK(!f.GetChip().Vcc());
  CHECK(f.GetChip().DisplayState() == Display::Off);
  CHECK(f.drv.GetOutput() == is::Output::Off);
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

TEST_CASE(
    "faults: the supply pin cannot be driven off: the power-down reports it and keeps going") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().fail_vcc_off = true;
  const is::Result<void> r = f.drv.Deinitialize();
  CHECK(r.error == is::Error::Bus);
  CHECK(!f.drv.IsInitialized());
  CHECK(f.GetChip().DisplayState() == Display::Off); // the display-off command still went out first
  CHECK(f.GetChip().Vcc());       // the pin did not move: the application is told
  CHECK(f.drv.IsSupplyEnabled()); // and the driver does not claim otherwise
  f.GetRig().fail_vcc_off = false;
  CHECK(f.drv.Deinitialize().ok()); // a repeat drives it off
  CHECK(!f.GetChip().Vcc());
  CHECK(!f.drv.IsSupplyEnabled());
}

TEST_CASE("faults: the reset pin cannot be driven: the start-up fails with the supply off") {
  Fixture f;
  f.GetRig().fail_reset_pin = true;
  CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
  CHECK(!f.drv.IsInitialized());
  CHECK(!f.GetChip().Vcc());
  CHECK(f.drv.DisplayOn().error == is::Error::NotInitialized);
}

TEST_CASE("faults: display-off fails on the bus: the supply is still driven off") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetRig().fail_writes_from = f.GetRig().writes; // every transfer from now on fails
  CHECK(f.drv.DisplayOff().error == is::Error::Bus);
  CHECK(!f.GetChip().Vcc());
  CHECK(!f.drv.IsSupplyEnabled());
  CHECK(f.drv.IsShadowInDoubt());
}

TEST_CASE("faults: a failed transfer marks the shadow in doubt; reinitialize clears it") {
  Fixture f;
  REQUIRE(f.Up());
  CHECK(!f.drv.IsShadowInDoubt());
  f.GetRig().fail_write_at = f.GetRig().writes;
  CHECK(f.drv.SetContrast(0x20, 0x30, 0x30).error == is::Error::Bus);
  CHECK(f.drv.IsShadowInDoubt());
  f.GetRig().fail_write_at = -1;
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK(!f.drv.IsShadowInDoubt());
  CHECK(f.GetChip().Lit());
}

TEST_CASE("faults: a retried start-up after a failure starts from the top") {
  Fixture f;
  f.GetRig().fail_write_at = 5;
  CHECK(f.drv.EnsureInitialized().error == is::Error::Bus);
  CHECK(f.drv.EnsureInitialized().ok());
  CHECK_EQ(f.GetChip().DefinedCells(), 96 * 64);
  CHECK(f.drv.DisplayOn().ok());
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

} // namespace
