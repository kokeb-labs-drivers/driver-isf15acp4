/**
 * @file isf15acp4_types.hpp
 * @brief Public types of the ISF15ACP4 driver: errors, `Result<T>`, the display and button
 * configuration, the module variants and the colour helpers.
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data with no platform dependency. `Config{}` describes the ISF15ACP4
 * (96 x 64 pixels) with the controller settings the module is driven with; `PresetFor()` gives the
 * two smaller modules of the family. The controller has no readable register, so none of these
 * values can be confirmed from the chip: they are what the driver sends (docs/contract.md). The
 * start-up values of each module are the ones in NKK's application notes for the OLED Color
 * SmartDisplay: Table 1 (ISC15ANP4), Table 2 (ISC01P) and Table 3 (ISF15ACP4), steps 4.1 to 4.18.
 * NKK Note 1: changing those set-up values may damage the OLED module. None has been tried on a
 * panel here.
 */
#ifndef ISF15ACP4_TYPES_HPP
#define ISF15ACP4_TYPES_HPP

#include <cstdint>

#include "isf15acp4_detail.hpp"
#include "isf15acp4_registers.hpp"
#include "isf15acp4_version.h"

/** @brief The ISF15ACP4 driver: everything it declares lives here; internals in `detail`. */
namespace isf15acp4 {

/**
 * @defgroup isf15acp4_types Driver types
 * @brief Errors, results, configuration, geometry and colours.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  Ok = 0,              ///< The call succeeded.
  NotInitialized = 1,  ///< `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since.
  InvalidArgument = 2, ///< An argument or a `Config` field is out of range; nothing was sent.
  Bus = 3,             ///< A port hook returned `false` (a transfer or a pin write).
  Timeout = 4,         ///< Not returned by this driver: nothing the driver waits for is observable.
  DeviceIdMismatch =
      5,           ///< Not returned by this driver: the controller has no ID and cannot answer.
  Crc = 6,         ///< Not returned by this driver: the serial interface has no checksum.
  Busy = 7,        ///< Not returned by this driver: every wait blocks until it has elapsed.
  DeviceFault = 8, ///< Not returned by this driver: the controller reports no fault.
  Unsupported = 9, ///< The port lacks the optional hook this call needs (switch, clock) or the
                   ///< pixel format does not allow the call.
  // Chip-specific values start at 16; each one is returned by some call (tests/test_errors.cpp).
  ScrollActive = 16, ///< A RAM, drawing or setup call while scrolling is on (`Deactivate` first).
  ScrollNotSet = 17, ///< `ActivateScroll()` before `SetupScroll()` succeeded.
  SupplyOff = 18,    ///< The call needs the OLED supply on (raw display-on).
  Locked = 19,       ///< The controller is command-locked by `SetCommandLock(true)`.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Bus"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::ScrollActive:
    return "ScrollActive";
  case Error::ScrollNotSet:
    return "ScrollNotSet";
  case Error::SupplyOff:
    return "SupplyOff";
  case Error::Locked:
    return "Locked";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events: initialised, supply on or off.
  Debug,     ///< Detail for bring-up.
};

/** @brief The three modules of the family; they share the controller and differ in the panel. */
enum class Variant : uint8_t {
  Isf15acp4 = 0, ///< 96 x 64 pixels, frameless pushbutton (NKK sheet E4 to E7).
  Isc15anp4 = 1, ///< 64 x 48 pixels, pushbutton (NKK sheet ISC15ANP4).
  Isc01p = 2,    ///< 52 x 36 pixels, display only (NKK sheet ISC01P): no switch.
};

/** @brief The pixel format of GDDRAM (A0h A[7:6]). */
enum class ColorDepth : uint8_t {
  Colors256 = 0,        ///< 1 byte per pixel: C[4:2] B[5:3] A[4:3].
  Colors65k = 1,        ///< 2 bytes per pixel, format 1: C[4:0] B[5:0] A[4:0]. Reset value.
  Colors65kFormat2 = 2, ///< 3 bytes per pixel, format 2: six bits per byte (Table 9).
};

/** @brief What the pixels show (A4h to A7h). */
enum class DisplayMode : uint8_t {
  Normal = 0,  ///< The RAM content (A4h).
  AllOn = 1,   ///< Every pixel at GS63 (A5h).
  AllOff = 2,  ///< Every pixel at GS0 (A6h).
  Inverse = 3, ///< Gray levels swapped, GS0 <-> GS63 (A7h).
};

/** @brief What the panel is doing, as far as the driver knows. */
enum class Output : uint8_t {
  Off = 0,    ///< Sleep (AEh): segments and commons are high impedance.
  Normal = 1, ///< Display on at normal brightness (AFh).
  Dim = 2,    ///< Display on in dim mode (ACh).
};

/** @brief The time between two scroll steps (27h E). */
enum class ScrollInterval : uint8_t {
  Frames6 = 0,   ///< 6 frames.
  Frames10 = 1,  ///< 10 frames.
  Frames100 = 2, ///< 100 frames.
  Frames200 = 3, ///< 200 frames.
};

/** @brief What a switch poll can report. */
enum class ButtonEvent : uint8_t {
  None = 0,      ///< Nothing happened.
  Pressed = 1,   ///< The switch closed and stayed closed for the debounce time.
  Released = 2,  ///< The switch opened and stayed open for the debounce time.
  LongPress = 3, ///< Held for `long_press_ms`; once per press.
  Repeat = 4,    ///< Held on: one per `repeat_interval_ms` after `repeat_delay_ms`.
};

/** @brief The controller settings that depend on the wiring of the panel (Remap, A0h). */
struct Remap {
  bool vertical_increment{false};          ///< A[0]: the RAM address runs down a column first.
  bool column_reverse{false};              ///< A[1]: RAM column 0 drives SEG95 (mirror).
  bool reverse_colour_order{false};        ///< A[2]: segment order SC, SB, SA.
  bool com_left_right_swap{true};          ///< A[3]: left-right swap on COM.
  bool com_scan_reverse{false};            ///< A[4]: scan COM[N-1] to COM0.
  bool com_split_odd_even{true};           ///< A[5]: odd-even split of the COM pins.
  ColorDepth depth{ColorDepth::Colors65k}; ///< A[7:6].
};

/** @brief A position in the visible area. */
struct Point {
  uint8_t x{0}; ///< Column.
  uint8_t y{0}; ///< Row.
};

/** @brief Dim mode parameters (ABh): used when the display is on in dim mode (ACh). */
struct DimSettings {
  uint8_t contrast_a{0x0E}; ///< Contrast of colour A in dim mode.
  uint8_t contrast_b{0x17}; ///< Contrast of colour B in dim mode.
  uint8_t contrast_c{0x16}; ///< Contrast of colour C in dim mode.
  uint8_t precharge{0x12};  ///< Pre-charge level in dim mode, 0 to 31 (0.10 to 0.50 x VCC).
};

/** @brief Geometry of a module: its visible pixels inside the 96 x 64 RAM. */
struct Geometry {
  uint8_t width{96};        ///< Visible columns.
  uint8_t height{64};       ///< Visible rows.
  uint8_t column_origin{0}; ///< RAM column of the first visible column.
  uint8_t multiplex{64};    ///< Rows scanned (A8h: N + 1).
};

/**
 * @brief The geometry of a module (NKK sheets: pixel format; the column origin of the smaller
 * panels is the centred one and not given by any NKK document: not verified on hardware).
 * @param variant The module.
 * @return Its geometry.
 */
constexpr Geometry GeometryOf(Variant variant) noexcept {
  switch (variant) {
  case Variant::Isf15acp4:
    return Geometry{.width = 96, .height = 64, .column_origin = 0, .multiplex = 64};
  case Variant::Isc15anp4:
    return Geometry{.width = 64, .height = 48, .column_origin = 16, .multiplex = 48};
  case Variant::Isc01p:
    return Geometry{.width = 52, .height = 36, .column_origin = 22, .multiplex = 36};
  default:
    break;
  }
  return Geometry{.width = 96, .height = 64, .column_origin = 0, .multiplex = 64};
}

/**
 * @brief Whether a module has the pushbutton (the ISC01P is a display only).
 * @param variant The module.
 * @return true when the module has the switch.
 */
constexpr bool HasSwitch(Variant variant) noexcept {
  return variant != Variant::Isc01p;
}

/** @brief Switch handling: polarity, debounce, long press and repeat. */
struct ButtonConfig {
  bool active_low{true};          ///< The closed switch pulls the pin low (pull-up wiring).
  uint32_t debounce_ms{20};       ///< Time a level must be stable to count, 1 to 1000.
  uint32_t long_press_ms{800};    ///< Hold time of `LongPress`, above the debounce time, to 60000.
  uint32_t repeat_interval_ms{0}; ///< 0 = no repeat; else 10 to 10000 between `Repeat` events.
  uint32_t repeat_delay_ms{500};  ///< Hold time of the first `Repeat`, to 60000.
};

/**
 * @brief Whether two switch configurations are the same, field by field.
 * @param a One configuration.
 * @param b The other.
 * @return true when all five fields are equal.
 */
constexpr bool SameButtonConfig(const ButtonConfig& a, const ButtonConfig& b) noexcept {
  return (a.active_low == b.active_low) && (a.debounce_ms == b.debounce_ms) &&
         (a.long_press_ms == b.long_press_ms) && (a.repeat_interval_ms == b.repeat_interval_ms) &&
         (a.repeat_delay_ms == b.repeat_delay_ms);
}

/// Longest button time the driver accepts, in milliseconds (keeps every difference below 2^31).
inline constexpr uint32_t kButtonTimeMaxMs = 60000;
/// Longest wait the driver accepts for the supply and engine times, in milliseconds.
inline constexpr uint32_t kWaitMaxMs = 1000;
/// Longest engine settle time, in microseconds.
inline constexpr uint32_t kEngineSettleMaxUs = 100000;

/**
 * @brief Everything the driver sends at start-up, and its own time knobs.
 *
 * `Config{}` is the ISF15ACP4 (NKK Table 3). The controller values are the module's table values
 * (contrast, offset, multiplex ratio, master configuration, phase periods, clock, gray scale,
 * pre-charge, VCOMH): NKK Note 1 says changing them may damage the OLED module. The dim contrasts
 * may not exceed the contrasts. The time knobs are the driver's own and document an open point of
 * the datasheets (docs/ambiguities.md). It has no `operator==`.
 */
struct Config {
  Variant variant{Variant::Isf15acp4};   ///< Which module; sets the geometry.
  Remap remap{};                         ///< A0h.
  uint8_t contrast_a{0x1C};              ///< 81h. Reset value 0x80.
  uint8_t contrast_b{0x2E};              ///< 82h.
  uint8_t contrast_c{0x2C};              ///< 83h.
  uint8_t precharge_speed_a{0x4F};       ///< 8Ah: second pre-charge speed, colour A.
  uint8_t precharge_speed_b{0x74};       ///< 8Bh.
  uint8_t precharge_speed_c{0x88};       ///< 8Ch.
  uint8_t master_current{0x0F};          ///< 87h, 0 to 15 (1/16 to 16/16).
  uint8_t start_line{0};                 ///< A1h, 0 to 63.
  uint8_t display_offset{0};             ///< A2h, 0 to 63.
  DisplayMode mode{DisplayMode::Normal}; ///< A4h to A7h.
  DimSettings dim{};                     ///< ABh.
  bool power_save{false};                ///< B0h: false = disabled (reset value: enabled).
  uint8_t phase1{7};                     ///< B1h phase 1, 1 to 15 DCLK.
  uint8_t phase2{1};                     ///< B1h phase 2, 1 to 15 DCLK.
  uint8_t clock_divide{0};               ///< B3h A[3:0]: ratio is this + 1.
  uint8_t clock_frequency{15};           ///< B3h A[7:4].
  uint8_t precharge_level{14};           ///< BBh A[5:1], 0 to 31.
  bool precharge_bit0{true}; ///< BBh bit 0 (reserved): NKK Table 3 sends 1Dh, Tables 1/2 12h.
  bool send_precharge_speeds{true};   ///< 8Ah to 8Ch: only the ISF15ACP4 table (Table 3) has them.
  uint8_t vcomh_level{16};            ///< BEh A[5:1], 0 to 31.
  bool linear_gray_scale{true};       ///< B9h at start-up (false: the reset table stays).
  bool clear_ram{true};               ///< Zero GDDRAM before the supply is enabled.
  bool red_on_segment_a{true};        ///< Panel wiring: segment group A carries red.
  bool writes_while_scrolling{false}; ///< Allow RAM writes while scrolling (datasheet is silent).
  uint32_t vcc_settle_ms{50};         ///< VCC enable to AFh; the datasheet says only "stable".
  uint32_t vcc_off_ms{reg::kVccOffTypicalMs}; ///< VCC off to the end of power-down (tOFF typical).
  uint32_t engine_settle_us{3000}; ///< Wait after a drawing command; not in the datasheet.
  uint32_t idle_dim_ms{0};         ///< `Poll()`: dim after this idle time; 0 = never.
  uint32_t idle_sleep_ms{0};       ///< `Poll()`: sleep after this idle time; 0 = never.
  ButtonConfig button{};           ///< The switch.
};

/**
 * @brief The working set of a module.
 * @param variant The module.
 * @return `Config{}` for the ISF15ACP4; the settings of the smaller panels otherwise.
 */
constexpr Config PresetFor(Variant variant) noexcept {
  Config c{};
  c.variant = variant;
  if (variant == Variant::Isf15acp4) {
    return c;
  }
  c.remap.com_left_right_swap = false;
  c.remap.com_scan_reverse = true;
  c.start_line = 0;
  c.phase1 = 4;
  c.phase2 = 4;
  c.precharge_level = 9;
  c.precharge_bit0 = false;
  c.send_precharge_speeds = false;
  if (variant == Variant::Isc15anp4) {
    c.contrast_a = 0x15;
    c.contrast_b = 0x1A;
    c.contrast_c = 0x17;
    c.display_offset = 0x10;
    c.clock_frequency = 10;
    c.vcomh_level = 31;
    c.dim =
        DimSettings{.contrast_a = 0x12, .contrast_b = 0x0C, .contrast_c = 0x14, .precharge = 0x12};
  } else {
    c.contrast_a = 0x0E;
    c.contrast_b = 0x13;
    c.contrast_c = 0x12;
    c.display_offset = 0x1C;
    c.clock_frequency = 3;
    c.vcomh_level = 30;
    c.dim =
        DimSettings{.contrast_a = 0x07, .contrast_b = 0x0A, .contrast_c = 0x09, .precharge = 0x12};
  }
  c.precharge_speed_a = c.contrast_a;
  c.precharge_speed_b = c.contrast_b;
  c.precharge_speed_c = c.contrast_c;
  return c;
}

namespace detail {
/// Last enumerator of `Variant` (detail::IsEnumerator).
template <>
struct EnumLast<Variant> {
  static constexpr Variant kValue = Variant::Isc01p; ///< The last enumerator.
};
/// Last enumerator of `ColorDepth`.
template <>
struct EnumLast<ColorDepth> {
  static constexpr ColorDepth kValue = ColorDepth::Colors65kFormat2; ///< The last enumerator.
};
/// Last enumerator of `DisplayMode`.
template <>
struct EnumLast<DisplayMode> {
  static constexpr DisplayMode kValue = DisplayMode::Inverse; ///< The last enumerator.
};
/// Last enumerator of `ScrollInterval`.
template <>
struct EnumLast<ScrollInterval> {
  static constexpr ScrollInterval kValue = ScrollInterval::Frames200; ///< The last enumerator.
};
} // namespace detail

/**
 * @brief Check the switch configuration without touching the chip.
 * @param b The configuration.
 * @return `Ok`, or `InvalidArgument`: debounce outside 1 to 1000 ms, long press not above the
 *         debounce time or above `kButtonTimeMaxMs`, a repeat interval that is not 0 or 10 to
 *         10000 ms, a repeat delay above `kButtonTimeMaxMs`.
 */
constexpr Error Validate(const ButtonConfig& b) noexcept {
  if ((b.debounce_ms == 0U) || (b.debounce_ms > 1000U)) {
    return Error::InvalidArgument;
  }
  if ((b.long_press_ms <= b.debounce_ms) || (b.long_press_ms > kButtonTimeMaxMs)) {
    return Error::InvalidArgument;
  }
  if (b.repeat_interval_ms != 0U) {
    if ((b.repeat_interval_ms < 10U) || (b.repeat_interval_ms > 10000U)) {
      return Error::InvalidArgument;
    }
  }
  if (b.repeat_delay_ms > kButtonTimeMaxMs) {
    return Error::InvalidArgument;
  }
  return Error::Ok;
}

namespace detail {

/// The enumerations hold enumerators and the display levels are inside their fields.
constexpr bool DisplayFieldsValid(const Config& config) noexcept {
  const bool enums =
      IsEnumerator(config.variant) && IsEnumerator(config.remap.depth) && IsEnumerator(config.mode);
  const bool rows = (config.start_line < reg::kRamRows) && (config.display_offset < reg::kRamRows);
  const bool dim_ok = (config.dim.contrast_a <= config.contrast_a) &&
                      (config.dim.contrast_b <= config.contrast_b) &&
                      (config.dim.contrast_c <= config.contrast_c);
  const bool levels = dim_ok && (config.master_current <= reg::kMasterCurrentMax) &&
                      (config.precharge_level <= reg::kPrechargeSteps) &&
                      (config.vcomh_level <= reg::kPrechargeSteps) &&
                      (config.dim.precharge <= reg::kPrechargeSteps);
  return enums && rows && levels;
}

/// The phase periods and the clock are inside their fields.
constexpr bool ClockFieldsValid(const Config& config) noexcept {
  const bool phases = (config.phase1 != 0U) && (config.phase1 <= reg::kPhaseMax) &&
                      (config.phase2 != 0U) && (config.phase2 <= reg::kPhaseMax);
  return phases && (config.clock_divide <= 15U) && (config.clock_frequency <= 15U);
}

/// The time knobs are inside their limits and the sleep time is above the dim time.
constexpr bool TimeKnobsValid(const Config& config) noexcept {
  const bool waits = (config.vcc_settle_ms != 0U) && (config.vcc_settle_ms <= kWaitMaxMs) &&
                     (config.vcc_off_ms <= kWaitMaxMs) &&
                     (config.engine_settle_us <= kEngineSettleMaxUs);
  const bool idle = (config.idle_dim_ms <= (kButtonTimeMaxMs * 100U)) &&
                    (config.idle_sleep_ms <= (kButtonTimeMaxMs * 100U));
  const bool ordered = (config.idle_dim_ms == 0U) || (config.idle_sleep_ms == 0U) ||
                       (config.idle_sleep_ms > config.idle_dim_ms);
  return waits && idle && ordered;
}

} // namespace detail

/**
 * @brief Check a configuration without touching the chip.
 *
 * Fails when an enum holds a value that is not an enumerator, a level is out of its range, the
 * contrast-independent fields leave their field, a time knob is above its limit, the sleep time is
 * not above the dim time, or the switch configuration fails.
 *
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`.
 */
constexpr Error Validate(const Config& config) noexcept {
  if (!detail::DisplayFieldsValid(config) || !detail::ClockFieldsValid(config) ||
      !detail::TimeKnobsValid(config)) {
    return Error::InvalidArgument;
  }
  return Validate(config.button);
}

// Every preset is valid, proved at build time (KB-CPP20-012).
static_assert(Validate(Config{}) == Error::Ok);
static_assert(Validate(PresetFor(Variant::Isc15anp4)) == Error::Ok);
static_assert(Validate(PresetFor(Variant::Isc01p)) == Error::Ok);
static_assert(GeometryOf(Variant::Isf15acp4).width == 96U);
static_assert((GeometryOf(Variant::Isc15anp4).column_origin +
               GeometryOf(Variant::Isc15anp4).width) <= reg::kRamColumns);
static_assert((GeometryOf(Variant::Isc01p).column_origin + GeometryOf(Variant::Isc01p).width) <=
              reg::kRamColumns);

/**
 * @brief Reached only when a `CheckedConfig` is built from an invalid configuration: it is never
 * defined, so the build fails and names it (KB-CPP20-013).
 */
void ConfigIsInvalid() noexcept;

/**
 * @brief A configuration proven valid at build time.
 *
 * `constexpr CheckedConfig kBoard{Config{.master_current = 8}};` fails to compile, naming
 * `ConfigIsInvalid`, when `Validate()` rejects the configuration. Pass `kBoard.Get()` to the
 * driver; the driver still validates at start-up, for configurations built at run time.
 */
class CheckedConfig {
public:
  /**
   * @brief Check `config` at build time.
   * @param config The configuration; it must pass `Validate()`.
   */
  consteval explicit CheckedConfig(const Config& config) noexcept : config_(config) {
    if (Validate(config) != Error::Ok) {
      ConfigIsInvalid();
    }
  }
  /** @return The checked configuration. */
  [[nodiscard]] constexpr const Config& Get() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary: the reference would dangle (KB-CPP20-044). */
  void Get() const&& = delete;

private:
  Config config_;
};

/**
 * @brief A colour as the RGB565 word every call takes: red in bits 15..11, green 10..5, blue 4..0.
 * A strong type, so a panel word or a byte count cannot be passed by mistake (KB-CPP20-030).
 */
struct Color {
  uint16_t rgb565{0}; ///< The word.
};

/**
 * @brief An RGB565 colour from 8-bit channels (the low bits are dropped, not rounded).
 * @param r Red, 0 to 255.
 * @param g Green, 0 to 255.
 * @param b Blue, 0 to 255.
 * @return The colour.
 */
constexpr Color MakeColor(uint8_t r, uint8_t g, uint8_t b) noexcept {
  const uint32_t word = ((static_cast<uint32_t>(r) >> 3U) << 11U) |
                        ((static_cast<uint32_t>(g) >> 2U) << 5U) | (static_cast<uint32_t>(b) >> 3U);
  return Color{static_cast<uint16_t>(word)};
}

/// Black.
inline constexpr Color kBlack{0x0000};
/// White.
inline constexpr Color kWhite{0xFFFF};
/// Pure red.
inline constexpr Color kRed{0xF800};
/// Pure green.
inline constexpr Color kGreen{0x07E0};
/// Pure blue.
inline constexpr Color kBlue{0x001F};

/** @brief The red, green and blue fields of a colour (5, 6 and 5 bits). */
struct Channels {
  uint8_t red{0};   ///< 0 to 31.
  uint8_t green{0}; ///< 0 to 63.
  uint8_t blue{0};  ///< 0 to 31.
};

/**
 * @brief Split an RGB565 colour.
 * @param c The colour.
 * @return Its channels.
 */
constexpr Channels Split(Color c) noexcept {
  const uint32_t w = c.rgb565;
  return Channels{.red = static_cast<uint8_t>((w >> 11U) & 0x1FU),
                  .green = static_cast<uint8_t>((w >> 5U) & 0x3FU),
                  .blue = static_cast<uint8_t>(w & 0x1FU)};
}

static_assert(MakeColor(255, 255, 255).rgb565 == 0xFFFFU);
static_assert((Split(kRed).red == 31U) && (Split(kGreen).green == 63U) &&
              (Split(kBlue).blue == 31U));

/** @return The driver version, e.g. `"0.1.0"`, from the generated `isf15acp4_version.h`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return ISF15ACP4_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return ISF15ACP4_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return ISF15ACP4_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return ISF15ACP4_VERSION_PATCH;
}

/** @} */

} // namespace isf15acp4

#endif // ISF15ACP4_TYPES_HPP
