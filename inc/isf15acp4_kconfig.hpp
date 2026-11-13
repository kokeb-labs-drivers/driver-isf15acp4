/**
 * @file isf15acp4_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_ISF15ACP4_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to the default of `Config{}`, so
 * `KconfigDefaults()` equals `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so an absent
 * `CONFIG_ISF15ACP4_RED_ON_SEGMENT_A` or `CONFIG_ISF15ACP4_BUTTON_ACTIVE_LOW` is read as off only
 * when the option menu is present (`CONFIG_ISF15ACP4`); without the menu the defaults stay on.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h`
 * per build; each host test is its own program). No driver or library header includes this one
 * (handbook KB-CPP20-018).
 */
#ifndef ISF15ACP4_KCONFIG_HPP
#define ISF15ACP4_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "isf15acp4_types.hpp"

#ifndef CONFIG_ISF15ACP4_MASTER_CURRENT
/** Master current step when Kconfig does not set it: the full current, `Config{}`. */
#define CONFIG_ISF15ACP4_MASTER_CURRENT 15
#endif
#ifndef CONFIG_ISF15ACP4_BUTTON_DEBOUNCE_MS
/** Debounce time when Kconfig does not set it: `ButtonConfig{}`. */
#define CONFIG_ISF15ACP4_BUTTON_DEBOUNCE_MS 20
#endif
#ifndef CONFIG_ISF15ACP4_BUTTON_LONG_PRESS_MS
/** Long-press time when Kconfig does not set it: `ButtonConfig{}`. */
#define CONFIG_ISF15ACP4_BUTTON_LONG_PRESS_MS 800
#endif
#ifndef CONFIG_ISF15ACP4_IDLE_DIM_MS
/** Idle dim time when Kconfig does not set it: never. */
#define CONFIG_ISF15ACP4_IDLE_DIM_MS 0
#endif
#ifndef CONFIG_ISF15ACP4_IDLE_SLEEP_MS
/** Idle sleep time when Kconfig does not set it: never. */
#define CONFIG_ISF15ACP4_IDLE_SLEEP_MS 0
#endif

namespace isf15acp4 {

/**
 * @brief The configuration menuconfig selected, on top of the module's preset.
 * @return `PresetFor(variant)` with the master current, colour wiring, switch and idle times taken
 *         from Kconfig.
 */
constexpr Config KconfigDefaults() noexcept {
#ifdef CONFIG_ISF15ACP4_VARIANT_ISC15ANP4
  Config c = PresetFor(Variant::Isc15anp4);
#elif defined(CONFIG_ISF15ACP4_VARIANT_ISC01P)
  Config c = PresetFor(Variant::Isc01p);
#else
  Config c = PresetFor(Variant::Isf15acp4);
#endif
  c.master_current = static_cast<uint8_t>(CONFIG_ISF15ACP4_MASTER_CURRENT);
#ifdef CONFIG_ISF15ACP4
  c.red_on_segment_a =
#ifdef CONFIG_ISF15ACP4_RED_ON_SEGMENT_A
      true;
#else
      false;
#endif
  c.button.active_low =
#ifdef CONFIG_ISF15ACP4_BUTTON_ACTIVE_LOW
      true;
#else
      false;
#endif
#endif
  c.button.debounce_ms = static_cast<uint32_t>(CONFIG_ISF15ACP4_BUTTON_DEBOUNCE_MS);
  c.button.long_press_ms = static_cast<uint32_t>(CONFIG_ISF15ACP4_BUTTON_LONG_PRESS_MS);
  c.idle_dim_ms = static_cast<uint32_t>(CONFIG_ISF15ACP4_IDLE_DIM_MS);
  c.idle_sleep_ms = static_cast<uint32_t>(CONFIG_ISF15ACP4_IDLE_SLEEP_MS);
  return c;
}

} // namespace isf15acp4

#endif // ISF15ACP4_KCONFIG_HPP
