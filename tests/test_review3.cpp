/**
 * @file test_review3.cpp
 * @brief Tests from review round 3: the stored configuration stays valid whatever order the
 * setters run in and whatever a failed transfer left; the three second pre-charge speeds go in one
 * session (NKK application notes, Table 3, note 2); a whole configuration is refused while
 * scrolling; a press on a locked, idle-dimmed controller is delivered; a raw FDh takes only the two
 * defined parameters.
 */
#include <array>
#include <cstdint>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

TEST_CASE("review3: a lower contrast then a higher dim is refused and a restart still works") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetContrast(0x10, 0x20, 0x20).ok()); // above dim 0Eh/17h/16h, below the start-up
  const uint32_t before = f.GetChip().Transfers();
  is::DimSettings d = f.drv.GetConfig().dim;
  d.contrast_a = 0x1C; // within the start-up contrast but above the contrast now set
  CHECK(f.drv.SetDimSettings(d).error == is::Error::InvalidArgument);
  d = f.drv.GetConfig().dim;
  d.contrast_b = 0x2E;
  CHECK(f.drv.SetDimSettings(d).error == is::Error::InvalidArgument);
  d = f.drv.GetConfig().dim;
  d.contrast_c = 0x2C;
  CHECK(f.drv.SetDimSettings(d).error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetChip().Transfers(), before);
  d = f.drv.GetConfig().dim;
  d.contrast_a = 0x10;
  d.contrast_b = 0x20;
  d.contrast_c = 0x20;
  CHECK(f.drv.SetDimSettings(d).ok()); // equal to the contrast in force is allowed
  CHECK(is::Validate(f.drv.GetConfig()) == is::Error::Ok);
  CHECK(f.drv.Reinitialize().ok());
  CHECK_EQ(f.GetChip().Reg(0x81), 0x10U);
}

TEST_CASE("review3: a failed ApplyConfig does not move the dim limit") {
  Fixture f;
  REQUIRE(f.Up());
  is::Config c = f.drv.GetConfig();
  c.contrast_a = 0x60;
  f.GetRig().fail_writes_from = f.GetRig().writes + 8; // the bus dies part way through
  CHECK(f.drv.ApplyConfig(c).error == is::Error::Bus);
  f.GetRig().fail_writes_from = -1;
  CHECK_EQ(f.drv.GetConfig().contrast_a, 0x1CU);
  is::DimSettings d = f.drv.GetConfig().dim;
  d.contrast_a = 0x50; // below the refused 60h, above the contrast in force
  CHECK(f.drv.SetDimSettings(d).error == is::Error::InvalidArgument);
  CHECK(is::Validate(f.drv.GetConfig()) == is::Error::Ok);
  CHECK(f.drv.Reinitialize().ok());
}

TEST_CASE("review3: a failed ApplyConfig at its last transfer does not move the dim limit") {
  Fixture f;
  REQUIRE(f.Up());
  is::Config c = f.drv.GetConfig();
  c.contrast_a = 0x60;
  // Count the transfers of a whole configuration, then fail exactly the last one.
  const int start = f.GetRig().writes;
  REQUIRE(f.drv.ApplyConfig(f.drv.GetConfig()).ok());
  const int per_config = f.GetRig().writes - start;
  f.GetRig().fail_write_at = f.GetRig().writes + per_config - 1;
  CHECK(f.drv.ApplyConfig(c).error == is::Error::Bus);
  f.GetRig().fail_write_at = -1;
  is::DimSettings d = f.drv.GetConfig().dim;
  d.contrast_a = 0x50;
  CHECK(f.drv.SetDimSettings(d).error == is::Error::InvalidArgument);
}

TEST_CASE("review3: a failed ApplyConfig with a lower contrast does not lower the dim limit") {
  Fixture f;
  REQUIRE(f.Up());
  is::Config c = f.drv.GetConfig();
  c.contrast_a = 0x10; // lower than the 1Ch in force; dim A 0Eh stays valid
  f.GetRig().fail_writes_from = f.GetRig().writes + 8;
  CHECK(f.drv.ApplyConfig(c).error == is::Error::Bus);
  f.GetRig().fail_writes_from = -1;
  CHECK_EQ(f.drv.GetConfig().contrast_a, 0x1CU);
  is::DimSettings d = f.drv.GetConfig().dim;
  d.contrast_a = 0x18; // above the refused 10h, within the contrast in force
  CHECK(f.drv.SetDimSettings(d).ok());
}

TEST_CASE("review3: the three second pre-charge speeds go in one session (Table 3, note 2)") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  const Ssd1331Model& m = f.GetChip();
  CHECK(m.OpTransfer(0x8A) != 0U);
  CHECK_EQ(m.OpTransfer(0x8B), m.OpTransfer(0x8A));
  CHECK_EQ(m.OpTransfer(0x8C), m.OpTransfer(0x8A));
  CHECK(m.OpTransfer(0x87) != m.OpTransfer(0x8A)); // other commands are their own sessions
  REQUIRE(f.drv.SetPrechargeSpeed(0x11, 0x22, 0x33).ok());
  const uint32_t t = m.OpTransfer(0x8A);
  CHECK(t > m.OpTransfer(0x87));
  CHECK_EQ(m.OpTransfer(0x8B), t);
  CHECK_EQ(m.OpTransfer(0x8C), t);
}

TEST_CASE("review3: ApplyConfig is refused while scrolling and sends nothing") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetupScroll(is::ScrollSetup{}).ok());
  REQUIRE(f.drv.ActivateScroll().ok());
  const uint32_t before = f.GetChip().Transfers();
  CHECK(f.drv.ApplyConfig(f.drv.GetConfig()).error == is::Error::ScrollActive);
  CHECK_EQ(f.GetChip().Transfers(), before);
  REQUIRE(f.drv.DeactivateScroll().ok());
  CHECK(f.drv.ApplyConfig(f.drv.GetConfig()).ok());
}

TEST_CASE("review3: a press on a locked controller dimmed by the idle timer is delivered") {
  is::Config cfg{};
  cfg.idle_dim_ms = 1000;
  Fixture f(cfg);
  REQUIRE(f.Up());
  for (int i = 0; i < 120; ++i) {
    REQUIRE(f.drv.Poll().ok());
    f.GetChip().Elapse(10000U);
  }
  REQUIRE(f.GetChip().DisplayState() == Display::Dim);
  REQUIRE(f.drv.SetCommandLock(true).ok());
  f.GetRig().switch_high = false; // press
  int pressed = 0;
  for (int i = 0; i < 10; ++i) {
    const is::Result<is::ButtonUpdate> r = f.drv.Poll();
    REQUIRE(r.ok());
    pressed += (r.value.event == is::ButtonEvent::Pressed) ? 1 : 0;
    CHECK(!r.value.woke_display);
    f.GetChip().Elapse(10000U);
  }
  CHECK_EQ(pressed, 1);
  CHECK(f.GetChip().DisplayState() == Display::Dim); // as the lock left it
  CHECK(f.drv.IsLocked());
}

TEST_CASE("review3: a raw FDh takes only 12h or 16h") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  const uint32_t before = f.GetChip().Transfers();
  const std::array<uint8_t, 1> odd{0x14}; // bit 2 set, not a defined value
  CHECK(f.drv.TraceCommand(0xFD, odd).error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetChip().Transfers(), before);
  const std::array<uint8_t, 1> on{0x16};
  CHECK(f.drv.TraceCommand(0xFD, on).ok());
  CHECK(f.drv.IsLocked());
  const std::array<uint8_t, 1> off{0x12};
  CHECK(f.drv.TraceCommand(0xFD, off).ok());
  CHECK(!f.drv.IsLocked());
}

} // namespace
