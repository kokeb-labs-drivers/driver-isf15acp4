/**
 * @file test_support.hpp
 * @brief Shared helpers of the host tests: a driver on the full port, and model queries.
 */
#ifndef ISF15ACP4_TEST_SUPPORT_HPP
#define ISF15ACP4_TEST_SUPPORT_HPP

#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>

#include "isf15acp4.hpp"
#include "mock_isf15acp4_bus.hpp"
#include "test_harness.hpp"

namespace isf15acp4_test {

namespace is = ::isf15acp4;

/// An enumeration value that no enumerator has: for the tests of the argument checks.
template <class E>
E Raw(std::underlying_type_t<E> value) noexcept {
  return std::bit_cast<E>(value);
}

/// A full port with a driver on it.
struct Fixture {
  explicit Fixture(const is::Config& config = {}) : drv(port, config) {}
  FullPort port;
  is::ISF15ACP4<FullPort> drv;
  Rig& GetRig() noexcept {
    return port.rig;
  }
  Ssd1331Model& GetChip() noexcept {
    return port.rig.chip;
  }
  /// Initialise and turn the panel on; false when either failed.
  bool Up() noexcept {
    return drv.EnsureInitialized().ok() && drv.DisplayOn().ok();
  }
};

/// A driver on the minimal port (no switch, no clock, no log).
struct MinimalFixture {
  explicit MinimalFixture(const is::Config& config = {}) : drv(port, config) {}
  MinimalPort port;
  is::ISF15ACP4<MinimalPort> drv;
};

/// The model's pixel as an RGB565 word.
inline uint16_t Shown(Fixture& f, int col, int row) noexcept {
  return f.GetChip().Rgb565At(col, row);
}

} // namespace isf15acp4_test

#endif // ISF15ACP4_TEST_SUPPORT_HPP
