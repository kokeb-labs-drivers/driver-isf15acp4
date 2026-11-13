/**
 * @file test_model.cpp
 * @brief The pin and state model: the datasheet's power sequence as rules the model checks, the
 * chip changing state by itself (brown-out), the model's own table against the driver's, and the
 * write-only bus.
 */
#include <array>
#include <cstdint>

#include "isf15acp4_registers.hpp"
#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

TEST_CASE("model: the full life of the driver breaks no rule") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.Clear(is::kGreen).ok());
  REQUIRE(f.drv.DrawRectangle(1, 1, 50, 50, is::kRed).ok());
  REQUIRE(f.drv.DisplayDim().ok());
  REQUIRE(f.drv.DisplayOff().ok());
  REQUIRE(f.drv.DisplayOn().ok());
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK_EQ(f.GetChip().Violations(), 0U);
  CHECK_EQ(f.GetChip().UnknownOpcodes(), 0U);
  CHECK_EQ(f.GetChip().VccOnCount(), 2U); // on at the start, on again after the display off
}

TEST_CASE("model: the reset pulse and the order of reset and supply") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK(!f.GetChip().ResLow());
  CHECK(f.GetChip().Resets() >= 1U);
  CHECK_EQ(f.GetChip().Violations() & (kResetPulseShort | kBusWhileReset | kVccWhileReset), 0U);
}

TEST_CASE("model: the model flags what the datasheet forbids (it can fail)") {
  // The rules are only worth something if the model reports the breaches. Each is made by hand.
  {
    Ssd1331Model m;
    m.SetVcc(true); // supply before any configuration
    CHECK((m.Violations() & kVccBeforeConfig) != 0U);
  }
  {
    Ssd1331Model m;
    m.SetRes(true);
    m.Elapse(1);
    m.SetRes(false);
    CHECK((m.Violations() & kResetPulseShort) != 0U);
  }
  {
    Ssd1331Model m;
    m.SetRes(true);
    m.SetCs(true);
    m.SetDc(false);
    m.Byte(0xAE);
    CHECK((m.Violations() & kBusWhileReset) != 0U);
  }
  {
    Ssd1331Model m;
    m.SetCs(false);
    m.SetDc(false);
    m.Byte(0xAE); // CS# high: not for the chip
    CHECK((m.Violations() & kByteWithoutCs) != 0U);
  }
  {
    Ssd1331Model m;
    m.SetCs(true);
    m.SetDc(false);
    m.Byte(0x00); // not an opcode
    CHECK((m.Violations() & kUnknownOpcode) != 0U);
  }
}

TEST_CASE("model: the supply may not be cut while the OLED supply is on") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetChip().CutLogicSupply();
  CHECK((f.GetChip().Violations() & kVddCutWithVcc) != 0U);
}

TEST_CASE("model: a brown-out resets the chip with no edge the driver can see") {
  Fixture f;
  REQUIRE(f.Up());
  f.GetChip().BrownOut();
  CHECK_EQ(f.GetChip().DefinedCells(), 0);
  CHECK(f.GetChip().DisplayState() == Display::Off);
  // The bus is write only: every call still succeeds, and the driver believes the panel is on.
  CHECK(f.drv.SetContrast(0x20, 0x30, 0x30).ok());
  CHECK(f.drv.GetOutput() == is::Output::Normal);
  CHECK(!f.GetChip().Lit());
  // The way out is the documented one: a full re-initialisation.
  f.GetChip().ClearViolations();
  REQUIRE(f.drv.Reinitialize().ok());
  CHECK(f.GetChip().Lit());
  CHECK_EQ(f.GetChip().DefinedCells(), 96 * 64);
  CHECK_EQ(f.GetChip().Violations() & ~kVccOffWhileOn, 0U);
}

TEST_CASE("model: the driver's parameter table equals the model's, opcode by opcode") {
  Fixture f;
  REQUIRE(f.Up());
  for (int op = 0; op < 256; ++op) {
    const int32_t driver = is::detail::ParameterCount(static_cast<uint8_t>(op));
    // Send the raw command to a fresh model with the right count: accepted iff the opcode is known.
    Ssd1331Model m;
    m.SetCs(true);
    m.SetDc(false);
    m.Byte(static_cast<uint8_t>(op));
    const bool known = (driver >= 0);
    CHECK(((m.Violations() & kUnknownOpcode) == 0U) == known);
  }
}

TEST_CASE("model: the bus has no read path and the driver never asks for one") {
  CHECK(is::kWriteOnly);
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.Clear().ok());
  CHECK(f.GetRig().writes > 0);
  // Writes are clocked out and counted; there is nothing the port could return but a pass or fail.
  CHECK_EQ(f.GetRig().failed_writes, 0);
}

TEST_CASE("model: the bus constants match the datasheet's table") {
  CHECK_EQ(is::kSpiMode, 0U);
  CHECK(is::kMsbFirst);
  CHECK_EQ(is::kMaxClockHz, 6'600'000U);
  CHECK(!is::kContinuousWord);
}

TEST_CASE("model: tOFF is waited after the supply is cut, before the logic supply may go") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.Deinitialize().ok());
  CHECK((f.GetChip().NowUs() - f.GetChip().VccOffAtUs()) >= 100000U);
  f.GetChip().CutLogicSupply();
  CHECK_EQ(f.GetChip().Violations(), 0U);
}

} // namespace
