/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "isf15acp4_kconfig.hpp"
#include "test_harness.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr isf15acp4::Config kC = isf15acp4::KconfigDefaults();
  CHECK_EQ(kC.variant, isf15acp4::Variant::Isc15anp4);
  CHECK_EQ(kC.display_offset, 0x10U); // the preset of the module
  CHECK_EQ(kC.master_current, 7U);
  CHECK(!kC.red_on_segment_a);
  CHECK(!kC.button.active_low);
  CHECK_EQ(kC.button.debounce_ms, 30U);
  CHECK_EQ(kC.button.long_press_ms, 1200U);
  CHECK_EQ(kC.idle_dim_ms, 5000U);
  CHECK_EQ(kC.idle_sleep_ms, 20000U);
  CHECK_EQ(isf15acp4::Validate(kC), isf15acp4::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr isf15acp4::Config kC = isf15acp4::KconfigDefaults();
  constexpr isf15acp4::Config kD{};
  CHECK_EQ(kC.variant, kD.variant);
  CHECK_EQ(kC.master_current, kD.master_current);
  CHECK_EQ(kC.red_on_segment_a, kD.red_on_segment_a);
  CHECK_EQ(kC.contrast_a, kD.contrast_a);
  CHECK_EQ(kC.button.active_low, kD.button.active_low);
  CHECK_EQ(kC.button.debounce_ms, kD.button.debounce_ms);
  CHECK_EQ(kC.button.long_press_ms, kD.button.long_press_ms);
  CHECK_EQ(kC.idle_dim_ms, kD.idle_dim_ms);
  CHECK_EQ(kC.idle_sleep_ms, kD.idle_sleep_ms);
}
#endif
