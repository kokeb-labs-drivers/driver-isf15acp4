/**
 * @file test_nkk_tables.cpp
 * @brief The start-up values of each module against the NKK application notes for the OLED
 * SmartDisplay: Table 1 (ISC15ANP4), Table 2 (ISC01P) and Table 3 (ISF15ACP4), steps 4.1 to 4.18,
 * and the memory-window commands of the same notes. The expected numbers are typed in from the
 * tables, not read from the driver.
 */
#include <cstdint>

#include "test_support.hpp"

namespace {

using namespace isf15acp4_test;

struct Table {
  is::Variant variant;
  uint8_t contrast[3]; // 4.1 to 4.3
  bool pre_speeds;     // 4.31 to 4.33 present
  uint8_t remap;       // 4.5
  uint8_t offset;      // 4.7
  uint8_t mux;         // 4.9 (A8h parameter, ratio - 1)
  uint8_t dim[4];      // 4.10 (the byte after ABh's 00h)
  uint8_t phases;      // 4.13
  uint8_t clock;       // 4.14
  uint8_t precharge;   // 4.16
  uint8_t vcomh;       // 4.17
};

constexpr Table kTables[] = {
    {is::Variant::Isc15anp4,
     {0x15, 0x1A, 0x17},
     false,
     0x70,
     0x10,
     0x2F,
     {0x12, 0x0C, 0x14, 0x12},
     0x44,
     0xA0,
     0x12,
     0x3E},
    {is::Variant::Isc01p,
     {0x0E, 0x13, 0x12},
     false,
     0x70,
     0x1C,
     0x23,
     {0x07, 0x0A, 0x09, 0x12},
     0x44,
     0x30,
     0x12,
     0x3C},
    {is::Variant::Isf15acp4,
     {0x1C, 0x2E, 0x2C},
     true,
     0x68,
     0x00,
     0x3F,
     {0x0E, 0x17, 0x16, 0x12},
     0x17,
     0xF0,
     0x1D,
     0x20},
};

TEST_CASE("nkk tables: every start-up value of each module is the one in its table") {
  for (const Table& t : kTables) {
    Fixture f(is::PresetFor(t.variant));
    REQUIRE(f.drv.EnsureInitialized().ok());
    const Ssd1331Model& m = f.GetChip();
    CHECK_EQ(m.Reg(0x81), t.contrast[0]);
    CHECK_EQ(m.Reg(0x82), t.contrast[1]);
    CHECK_EQ(m.Reg(0x83), t.contrast[2]);
    CHECK_EQ(m.Seen(0x8A), t.pre_speeds);
    CHECK_EQ(m.Seen(0x8B), t.pre_speeds);
    CHECK_EQ(m.Seen(0x8C), t.pre_speeds);
    CHECK_EQ(m.Reg(0x87), 0x0FU);
    CHECK_EQ(m.Reg(0xA0), t.remap);
    CHECK_EQ(m.Reg(0xA1), 0x00U);
    CHECK_EQ(m.Reg(0xA2), t.offset);
    CHECK_EQ(m.Mode(), 0xA4U);
    CHECK_EQ(m.Mux(), static_cast<int>(t.mux) + 1);
    CHECK_EQ(m.Dim()[0], t.dim[0]);
    CHECK_EQ(m.Dim()[1], t.dim[1]);
    CHECK_EQ(m.Dim()[2], t.dim[2]);
    CHECK_EQ(m.Dim()[3], t.dim[3]);
    CHECK(m.Seen(0xAD));
    CHECK_EQ(m.Reg(0xB0), 0x0BU);
    CHECK_EQ(m.Reg(0xB1), t.phases);
    CHECK_EQ(m.Reg(0xB3), t.clock);
    CHECK(m.Seen(0xB9)); // linear gray scale
    CHECK_EQ(m.Reg(0xBB), t.precharge);
    CHECK_EQ(m.Reg(0xBE), t.vcomh);
    CHECK_EQ(m.Violations(), 0U);
  }
}

TEST_CASE("nkk tables: the second pre-charge speeds of the ISF15ACP4 are 4Fh, 74h and 88h") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  CHECK_EQ(f.GetChip().Reg(0x8A), 0x4FU);
  CHECK_EQ(f.GetChip().Reg(0x8B), 0x74U);
  CHECK_EQ(f.GetChip().Reg(0x8C), 0x88U);
}

TEST_CASE("nkk tables: a later pre-charge level keeps the bit 0 of the module's table") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetPrechargeLevel(10).ok());
  CHECK_EQ(f.GetChip().Reg(0xBB), 0x15U); // 10 << 1, bit 0 as in Table 3
  Fixture g(is::PresetFor(is::Variant::Isc15anp4));
  REQUIRE(g.Up());
  REQUIRE(g.drv.SetPrechargeLevel(10).ok());
  CHECK_EQ(g.GetChip().Reg(0xBB), 0x14U); // Tables 1 and 2 have bit 0 clear
}

TEST_CASE("nkk tables: the memory window of each module is the one in the notes") {
  struct Window {
    is::Variant variant;
    uint8_t col_first, col_last, row_last;
  };
  constexpr Window kWindows[] = {{is::Variant::Isc15anp4, 0x10, 0x4F, 0x2F},
                                 {is::Variant::Isc01p, 0x16, 0x49, 0x23},
                                 {is::Variant::Isf15acp4, 0x00, 0x5F, 0x3F}};
  for (const Window& w : kWindows) {
    const is::Geometry g = is::GeometryOf(w.variant);
    CHECK_EQ(g.column_origin, w.col_first);
    CHECK_EQ(g.column_origin + g.width - 1, static_cast<int>(w.col_last));
    CHECK_EQ(g.height - 1, static_cast<int>(w.row_last));
    CHECK_EQ(g.multiplex, g.height);
  }
}

TEST_CASE("nkk tables: dim contrasts above the start-up contrasts are refused (notes, ACh)") {
  is::Config c;
  CHECK(is::Validate(c) == is::Error::Ok);
  c.dim.contrast_a = static_cast<uint8_t>(c.contrast_a + 1U);
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = is::Config{};
  c.dim.contrast_b = static_cast<uint8_t>(c.contrast_b + 1U);
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = is::Config{};
  c.dim.contrast_c = static_cast<uint8_t>(c.contrast_c + 1U);
  CHECK(is::Validate(c) == is::Error::InvalidArgument);
  c = is::Config{};
  c.dim.contrast_c = c.contrast_c; // equal is the limit and is allowed
  CHECK(is::Validate(c) == is::Error::Ok);
}

TEST_CASE("nkk tables: SetDimSettings refuses what ACh may not show and keeps the old values") {
  Fixture f;
  REQUIRE(f.Up());
  const uint32_t before = f.GetChip().Transfers();
  is::DimSettings over = f.drv.GetConfig().dim;
  over.contrast_a = 0x1DU; // above 1Ch
  CHECK(f.drv.SetDimSettings(over).error == is::Error::InvalidArgument);
  over = f.drv.GetConfig().dim;
  over.contrast_b = 0x2FU; // above 2Eh
  CHECK(f.drv.SetDimSettings(over).error == is::Error::InvalidArgument);
  over = f.drv.GetConfig().dim;
  over.contrast_c = 0x2DU; // above 2Ch
  CHECK(f.drv.SetDimSettings(over).error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetChip().Transfers(), before);
  is::DimSettings top = f.drv.GetConfig().dim;
  top.contrast_a = 0x1CU;
  top.contrast_b = 0x2EU;
  top.contrast_c = 0x2CU;
  CHECK(f.drv.SetDimSettings(top).ok());
}

TEST_CASE("nkk tables: raising the contrast later does not raise the dim limit") {
  Fixture f;
  REQUIRE(f.Up());
  REQUIRE(f.drv.SetContrast(0x30, 0x30, 0x30).ok());
  is::DimSettings over = f.drv.GetConfig().dim;
  over.contrast_a = 0x20U; // above the start-up 1Ch though below the new contrast
  CHECK(f.drv.SetDimSettings(over).error == is::Error::InvalidArgument);
}

TEST_CASE("nkk tables: a contrast below the dim contrast is refused, so a restart stays valid") {
  Fixture f;
  REQUIRE(f.Up());
  const uint32_t before = f.GetChip().Transfers();
  CHECK(f.drv.SetContrast(0x0D, 0x2E, 0x2C).error == is::Error::InvalidArgument); // A below 0Eh
  CHECK(f.drv.SetContrast(0x1C, 0x16, 0x2C).error == is::Error::InvalidArgument); // B below 17h
  CHECK(f.drv.SetContrast(0x1C, 0x2E, 0x15).error == is::Error::InvalidArgument); // C below 16h
  CHECK_EQ(f.GetChip().Transfers(), before);
  CHECK(f.drv.SetContrast(0x0E, 0x17, 0x16).ok()); // equal to the dim values is allowed
  REQUIRE(f.drv.Reinitialize().ok());
}

TEST_CASE("nkk tables: ApplyConfig refuses a dim contrast above its own contrast") {
  Fixture f;
  REQUIRE(f.drv.EnsureInitialized().ok());
  is::Config c = is::PresetFor(is::Variant::Isc01p);
  c.dim.contrast_a = static_cast<uint8_t>(c.contrast_a + 1U);
  const uint32_t before = f.GetChip().Transfers();
  CHECK(f.drv.ApplyConfig(c).error == is::Error::InvalidArgument);
  CHECK_EQ(f.GetChip().Transfers(), before);
}

} // namespace
