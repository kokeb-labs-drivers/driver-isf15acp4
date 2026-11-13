/**
 * @file isf15acp4.hpp
 * @brief ISF15ACP4 driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `isf15acp4::ISF15ACP4<Port>` drives one NKK ISF15ACP4 frameless OLED pushbutton through a
 * platform port you write (see `isf15acp4_spi_interface.hpp`): the SSD1331-class display controller
 * over write-only SPI, the 15 V OLED supply through an enable pin, and the SPST switch through a
 * pin read. The driver has no platform code, allocates nothing, throws nothing, takes no lock and
 * is not thread-safe: give each object one owner, or guard it with one mutex.
 *
 * The controller cannot be read: the driver knows only what it sent (docs/contract.md).
 */
#ifndef ISF15ACP4_HPP
#define ISF15ACP4_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "isf15acp4_button.hpp"
#include "isf15acp4_detail.hpp"
#include "isf15acp4_registers.hpp"
#include "isf15acp4_spi_interface.hpp"
#include "isf15acp4_types.hpp"
#include "isf15acp4_version.h"

namespace isf15acp4 {

/** @brief The scroll parameters of command 27h. */
struct ScrollSetup {
  uint8_t columns{1};   ///< Columns per step, 0 to 95 (0 = no horizontal scroll).
  uint8_t start_row{0}; ///< First row that scrolls horizontally, 0 to 63.
  uint8_t rows{64};     ///< Rows that scroll horizontally, 1 to 64; start_row + rows <= 64.
  uint8_t vertical{0};  ///< Rows per step, 0 to 63 (0 = none); needs a multiplex ratio of 64.
  ScrollInterval interval{ScrollInterval::Frames6}; ///< Time between steps.
};

namespace detail {

/// The port declares something called `ReadSwitch`.
template <typename Port>
concept DeclaresReadSwitch = requires(Port& port, bool& high) { port.ReadSwitch(high); };

/// The port reads the switch pin: `bool ReadSwitch(bool& high) noexcept`; `high` is the physical
/// level.
template <typename Port>
concept HasReadSwitch =
    requires { ExactHook<Port, bool(bool&) noexcept>::Take(&Port::ReadSwitch); };

/// The port declares something called `NowMs`.
template <typename Port>
concept DeclaresNowMs = requires(Port& port) { port.NowMs(); };

/// The port has a clock: `uint32_t NowMs() noexcept`.
template <typename Port>
concept HasNowMs = requires { ExactHook<Port, uint32_t() noexcept>::Take(&Port::NowMs); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`. A narrower parameter would truncate long
/// waits.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the reset, supply and engine times are requirements, never skipped.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "isf15acp4: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log(Port& port, LogLevel level, const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    // A port without Log(): every parameter is used in each instantiation (Rule 0.2.2).
    static_cast<void>(port);
    static_cast<void>(level);
    static_cast<void>(message);
  }
}

/// The bytes of one encoded pixel: `count` of the three are used.
struct PixelBytes {
  std::array<uint8_t, 3> bytes{}; ///< The bytes, first sent first.
  std::size_t count{0};           ///< 1, 2 or 3.
};

/// Bytes per pixel of a colour depth (Table 9): 1, 2 or 3.
constexpr std::size_t BytesPerPixel(ColorDepth depth) noexcept {
  switch (depth) {
  case ColorDepth::Colors256:
    return 1U;
  case ColorDepth::Colors65k:
    return 2U;
  case ColorDepth::Colors65kFormat2:
    return 3U;
  default:
    break;
  }
  return 2U;
}

/// Whether the C field of the RAM word (the first one sent) is blue: with colour order A, B, C
/// the segment group C is blue when group A carries red; the reversed order swaps them.
constexpr bool BlueFirst(const Config& config) noexcept {
  return config.red_on_segment_a != config.remap.reverse_colour_order;
}

/// One pixel as the bytes of the colour depth (SSD1331 Table 9, Figure 11).
constexpr PixelBytes EncodePixel(ColorDepth depth, Color color, bool blue_first) noexcept {
  const Channels ch = Split(color);
  const uint32_t first = blue_first ? ch.blue : ch.red; // colour C, 5 bits
  const uint32_t second = ch.green;                     // colour B, 6 bits
  const uint32_t third = blue_first ? ch.red : ch.blue; // colour A, 5 bits
  PixelBytes p{};
  switch (depth) {
  case ColorDepth::Colors256:
    p.bytes[0] =
        static_cast<uint8_t>(((first >> 2U) << 5U) | ((second >> 3U) << 2U) | (third >> 3U));
    p.count = 1U;
    break;
  case ColorDepth::Colors65k:
    p.bytes[0] = static_cast<uint8_t>((first << 3U) | (second >> 3U));
    p.bytes[1] = static_cast<uint8_t>(((second & 0x07U) << 5U) | third);
    p.count = 2U;
    break;
  case ColorDepth::Colors65kFormat2:
    p.bytes[0] = static_cast<uint8_t>(first << 1U);
    p.bytes[1] = static_cast<uint8_t>(second);
    p.bytes[2] = static_cast<uint8_t>(third << 1U);
    p.count = 3U;
    break;
  default:
    break;
  }
  return p;
}

/// The parameter bytes that follow an opcode, or -1 for an opcode that is not in Table 10.
constexpr int32_t ParameterCount(uint8_t opcode) noexcept {
  switch (opcode) {
  case reg::kSetColumnAddress:
  case reg::kSetRowAddress:
    return 2;
  case reg::kDimWindow:
  case reg::kClearWindow:
    return 4;
  case reg::kSetContrastA:
  case reg::kSetContrastB:
  case reg::kSetContrastC:
  case reg::kMasterCurrent:
  case reg::kPrechargeSpeedA:
  case reg::kPrechargeSpeedB:
  case reg::kPrechargeSpeedC:
  case reg::kRemap:
  case reg::kStartLine:
  case reg::kDisplayOffset:
  case reg::kMultiplex:
  case reg::kMasterConfig:
  case reg::kPowerSave:
  case reg::kPhasePeriods:
  case reg::kClockDivider:
  case reg::kPrechargeLevel:
  case reg::kVcomh:
  case reg::kCommandLock:
  case reg::kFillEnable:
    return 1;
  case reg::kModeNormal:
  case reg::kModeAllOn:
  case reg::kModeAllOff:
  case reg::kModeInverse:
  case reg::kDisplayDim:
  case reg::kDisplayOff:
  case reg::kDisplayOn:
  case reg::kLinearGrayScale:
  case reg::kNopBC:
  case reg::kNopBD:
  case reg::kNopE3:
  case reg::kScrollOff:
  case reg::kScrollOn:
    return 0;
  case reg::kDimMode:
  case reg::kScrollSetup:
    return 5;
  case reg::kGrayScaleTable:
    return 32;
  case reg::kDrawLine:
    return 7;
  case reg::kDrawRectangle:
    return 10;
  case reg::kCopy:
    return 6;
  default:
    break;
  }
  return -1;
}

static_assert((ParameterCount(reg::kSetColumnAddress) == 2) &&
                  (ParameterCount(reg::kDrawRectangle) == 10) &&
                  (ParameterCount(reg::kGrayScaleTable) == 32) && (ParameterCount(0x00_u8) == -1),
              "Table 10: the parameter counts");
static_assert(EncodePixel(ColorDepth::Colors65k, kRed, true).bytes[1] == 0x1FU,
              "C first: with blue as C, red is the low five bits");
static_assert(EncodePixel(ColorDepth::Colors65k, kBlue, true).bytes[0] == 0xF8U,
              "C = blue is first");

} // namespace detail

// clang-format off: the table below must keep one row per line.
/**
 * @brief Driver for one ISF15ACP4 on a platform port.
 *
 * Lifecycle (NKK sheet E7, states 0 to 2): construct with the configuration (no I/O);
 * `EnsureInitialized()` resets the controller, sends the configuration and clears GDDRAM with the
 * OLED supply still off (state 1); `DisplayOn()` enables the supply and turns the panel on (state 2);
 * `DisplayOff()` turns it off and the supply off (state 1); `Deinitialize()` is the power-down and
 * always runs. The supply is never enabled before the controller has been configured. Every call that
 * touches the chip returns `Error::NotInitialized` until `EnsureInitialized()` has succeeded.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `bool ReadSwitch(bool& high) noexcept` | `Poll()`, `ReadSwitchRaw()`: the switch pin | Those calls return `Unsupported` |
 * | `uint32_t NowMs() noexcept` | Timestamps of the debouncer, the idle timers | `Poll()` and `NotifyActivity()` return `Unsupported` |
 * | `void Log(LogLevel, const char* message) noexcept` | One line per lifecycle event or failure | Nothing is logged |
 *
 * A hook declared with the wrong signature is a compile error. Required: `EnsureInitialized`, `Write`,
 * `GpioSet`, `DelayUs`.
 *
 * @tparam Port Your class derived from `SpiInterface<Port>`.
 */
// clang-format on
template <SpiPort Port>
class ISF15ACP4 {
  static_assert(
      !detail::DeclaresReadSwitch<Port> || detail::HasReadSwitch<Port>,
      "isf15acp4: the optional ReadSwitch hook must be `bool ReadSwitch(bool& high) noexcept`");
  static_assert(!detail::DeclaresNowMs<Port> || detail::HasNowMs<Port>,
                "isf15acp4: the optional NowMs hook must be `uint32_t NowMs() noexcept`");
  static_assert(
      !detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
      "isf15acp4: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`, so that no "
      "wait is truncated");
  static_assert(!detail::DeclaresLog<Port> || detail::HasLog<Port>,
                "isf15acp4: the optional Log hook must be `void Log(LogLevel, const char* message) "
                "noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config What `EnsureInitialized()` will apply; checked there, before any I/O.
   */
  explicit ISF15ACP4(Port& port, const Config& config = {}) noexcept
      : bus_(port), config_(config), button_(config.button),
        multiplex_(GeometryOf(config.variant).multiplex) {}

  /** @brief One driver object per module. */
  ISF15ACP4(const ISF15ACP4&) = delete;
  /** @brief One driver object per module. */
  ISF15ACP4& operator=(const ISF15ACP4&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Power up to state 1: the controller is reset, configured and cleared, the supply is off.
   *
   * Drives the VCC enable off, pulses RES# for `kResetPulseMinUs`, sends display off and every
   * configuration command, clears GDDRAM (`Config::clear_ram`), and returns. Blocks for the SPI
   * traffic (about 12 kB with the clear) and a few microseconds of waits. After it succeeds, later
   * calls return `Ok` at once with no I/O. After it fails the driver stays uninitialised, the
   * supply is off, and the next call starts again.
   *
   * @return `Ok`, `InvalidArgument` (the stored configuration fails `Validate()`; no I/O) or `Bus`.
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()` and before `Deinitialize()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief The power-down: command unlock, display off, supply off, wait `vcc_off_ms`. Never
   * refused.
   *
   * It runs every step whatever the state (also on a driver that never started, also with the
   * controller locked or scrolling) and whatever fails: the supply enable is driven off even when
   * the display-off transfer failed. The driver is uninitialised afterwards. When it returns, the
   * logic supply may be cut (SSD1331 section 7.9, tOFF).
   *
   * @return `Ok`, or the first `Bus` of the steps.
   */
  Result<void> Deinitialize() noexcept;

  /**
   * @brief Power-down, then start again, then restore the display state that was on.
   * @return `InvalidArgument` (the stored configuration fails `Validate()`: the driver never
   * started, nothing is sent) or the errors of `Deinitialize()`, `EnsureInitialized()` and
   *         `DisplayOn()`/`DisplayDim()`.
   */
  Result<void> Reinitialize() noexcept;

  /// @}
  /// @name Display on and off
  /// @{

  /**
   * @brief Turn the panel on at normal brightness (state 1 to 2): supply on, wait `vcc_settle_ms`,
   * AFh, wait tAF (100 ms). From dim mode only AFh is sent.
   * @return `Ok`, `NotInitialized`, `Locked`, or `Bus` (the supply is switched off again).
   */
  Result<void> DisplayOn() noexcept;

  /**
   * @brief Turn the panel on in dim mode (ACh; the ABh settings), as `DisplayOn()`.
   * @return As `DisplayOn()`.
   */
  Result<void> DisplayDim() noexcept;

  /**
   * @brief Turn the panel off and the supply off (state 2 to 1): unlock if locked, AEh, supply off.
   *
   * Never refused, also before the start and while locked or scrolling. It does not wait
   * `vcc_off_ms` (the logic supply stays on); `Deinitialize()` waits.
   * @return `Ok` or `Bus` (the supply is driven off whatever failed).
   */
  Result<void> DisplayOff() noexcept;

  /** @return What the driver last sent: off, normal or dim. No I/O. */
  [[nodiscard]] Output GetOutput() const noexcept {
    return output_;
  }
  /** @return true while the driver has the supply enable on. No I/O. */
  [[nodiscard]] bool IsSupplyEnabled() const noexcept {
    return vcc_on_;
  }

  /// @}
  /// @name Configuration
  /// @{

  /**
   * @brief Change the whole configuration.
   *
   * Running: every configuration command is sent again (the panel stays as it is: no display off).
   * Not started yet: `config` is only stored, with no I/O.
   *
   * @param config Checked with `Validate()` before any I/O.
   * @return `Ok`, `InvalidArgument`, `Locked`, `ScrollActive` (a running scroll: deactivate it
   *         first; the driver's own rule, not NKK's, which covers only the 27h parameters) or
   * `Bus`. A configuration whose switch settings are unchanged keeps the debouncer's state. A
   * failed transfer leaves the stored configuration as it was.
   */
  Result<void> ApplyConfig(const Config& config) noexcept;

  /** @return The configuration last applied or stored. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetConfig() const&& = delete;

  /** @return The visible area of the module. No I/O. */
  [[nodiscard]] Geometry GetGeometry() const noexcept {
    return GeometryOf(config_.variant);
  }

  /**
   * @brief Set the contrast of the three colours (81h, 82h, 83h).
   * @param a Value for colour A (segment group A).
   * @param b Value for colour B.
   * @param c Value for colour C.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (a value below the dim contrast of its
   *         colour: ACh may not show more than the contrast; lower `SetDimSettings()` first),
   *         `Locked`, `Bus`.
   *
   * NKK Note 1: values other than the module's table values may damage the OLED.
   */
  Result<void> SetContrast(uint8_t a, uint8_t b, uint8_t c) noexcept;
  /**
   * @brief Set the master current attenuation, 0 to 15 (87h).
   * @param step Master current step, 0 to 15.
   * @return `Ok`, `InvalidArgument`, ...
   */
  Result<void> SetMasterCurrent(uint8_t step) noexcept;
  /**
   * @brief Set the second pre-charge speed of the three colours; all six bytes go out together (8Ah
   * to 8Ch). The values are stored and `send_precharge_speeds` is set, so a restart sends them
   * again, also on the two smaller modules whose NKK tables leave them out.
   * @param a Value for colour A (segment group A).
   * @param b Value for colour B.
   * @param c Value for colour C.
   * @return As above.
   */
  Result<void> SetPrechargeSpeed(uint8_t a, uint8_t b, uint8_t c) noexcept;
  /**
   * @brief Set the remap and colour depth (A0h). Changes how pixels are sent: `ColorDepth`,
   * `vertical_increment`.
   * @param remap The remap and colour-depth settings.
   * @return As above.
   */
  Result<void> SetRemap(const Remap& remap) noexcept;
  /**
   * @brief Set the display start line, 0 to 63 (A1h).
   * @param line First displayed RAM row, 0 to 63.
   * @return As above.
   */
  Result<void> SetStartLine(uint8_t line) noexcept;
  /**
   * @brief Set the display offset, 0 to 63 (A2h).
   * @param offset Rows, 0 to 63.
   * @return As above.
   */
  Result<void> SetDisplayOffset(uint8_t offset) noexcept;
  /**
   * @brief Set what the pixels show (A4h to A7h).
   * @param mode The display mode.
   * @return As above.
   */
  Result<void> SetDisplayMode(DisplayMode mode) noexcept;
  /**
   * @brief Set the multiplex ratio, 16 to 64 rows (A8h). Vertical scrolling needs 64. Not stored in
   * `Config`: a restart (`Reinitialize()`, `ApplyConfig()`) goes back to the module's ratio.
   * @param rows Rows scanned, 16 to 64.
   * @return As above.
   */
  Result<void> SetMultiplexRatio(uint8_t rows) noexcept;
  /**
   * @brief Set the dim mode parameters (ABh); they apply at the next `DisplayDim()`.
   *
   * A dim contrast may not exceed the start-up contrast of its colour (NKK notes, ACh) nor the
   * contrast now set, so the stored configuration stays valid for `Reinitialize()`.
   *
   * @param dim The dim-mode settings.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (nothing sent), `Locked`, `Bus`.
   */
  Result<void> SetDimSettings(const DimSettings& dim) noexcept;
  /**
   * @brief Enable or disable the power save mode (B0h).
   * @param enable true to enable.
   * @return As above.
   */
  Result<void> SetPowerSave(bool enable) noexcept;
  /**
   * @brief Set phase 1 and phase 2 periods, 1 to 15 DCLK each (B1h).
   * @param phase1 Phase 1 in DCLK, 1 to 15.
   * @param phase2 Phase 2 in DCLK, 1 to 15.
   * @return As above.
   */
  Result<void> SetPhasePeriods(uint8_t phase1, uint8_t phase2) noexcept;
  /**
   * @brief Set the clock divide ratio (this + 1) and oscillator frequency, 0 to 15 each (B3h).
   * @param divide Divide ratio minus one, 0 to 15.
   * @param frequency Oscillator frequency setting, 0 to 15.
   * @return As above.
   */
  Result<void> SetClock(uint8_t divide, uint8_t frequency) noexcept;
  /**
   * @brief Set the 32 gray scale pulse widths GS1, GS3 ... GS63, each 1 to 125 and rising (B8h).
   * The table is not stored: a restart loads the start-up gray scale again (B9h when
   * `linear_gray_scale` is set, NKK step 4.15).
   * @param widths The 32 gray-scale pulse widths.
   * @return As above.
   */
  Result<void> SetGrayScaleTable(std::span<const uint8_t, reg::kGrayScaleEntries> widths) noexcept;
  /** @brief Reload the built-in linear gray scale table (B9h). @return As above. */
  Result<void> UseLinearGrayScale() noexcept;
  /**
   * @brief Set the pre-charge voltage level, 0 to 31 (0.10 to 0.50 x VCC) (BBh).
   * @param level Level, 0 to 31.
   * @return As above.
   */
  Result<void> SetPrechargeLevel(uint8_t level) noexcept;
  /**
   * @brief Set the VCOMH level, 0 to 31 (0.44 to 0.83 x VCC) (BEh).
   * @param level Level, 0 to 31.
   * @return As above.
   */
  Result<void> SetVcomh(uint8_t level) noexcept;
  /**
   * @brief Lock or unlock the controller's command interface (FDh).
   * @param lock true to lock: every other call then returns `Locked` and sends nothing until
   * unlocked.
   * @return `Ok`, `NotInitialized` or `Bus`.
   */
  Result<void> SetCommandLock(bool lock) noexcept;
  /** @brief Send a no-operation command (E3h). @return `Ok`, `NotInitialized`, `Locked`, `Bus`. */
  Result<void> Nop() noexcept;

  /// @}
  /// @name Pixels (GDDRAM)
  /// @{

  /**
   * @brief Show a whole frame: `GetGeometry().width * height` colours, row by row.
   * @param frame The pixels; any other size is `InvalidArgument` (nothing sent).
   * @return `Ok`, `NotInitialized`, `Locked`, `ScrollActive`, `InvalidArgument` or `Bus`.
   */
  Result<void> Present(std::span<const Color> frame) noexcept;

  /**
   * @brief Write a rectangle of colours into GDDRAM, row by row.
   *
   * Sets the column and row window (15h, 75h), then streams the pixels as data in chunks of at most
   * `detail::kDataChunkBytes`. With `vertical_increment` set the driver sends them column by
   * column, so the picture is the same.
   *
   * @param x First column, 0-based in the visible area.
   * @param y First row.
   * @param width Columns, at least 1; `x + width` within the visible width.
   * @param height Rows, at least 1; `y + height` within the visible height.
   * @param pixels `width * height` colours.
   * @return As `Present()`.
   */
  Result<void> WriteWindow(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                           std::span<const Color> pixels) noexcept;

  /**
   * @brief Write raw RAM bytes into a window: the caller has encoded them for the colour depth
   * (`2` bytes per pixel in 65k format 1). Sent in the order given.
   * @param x First column.
   * @param y First row.
   * @param width Columns.
   * @param height Rows.
   * @param bytes Exactly `width * height * bytes-per-pixel` bytes.
   * @return As `Present()`.
   */
  Result<void> WriteWindowRaw(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                              std::span<const uint8_t> bytes) noexcept;

  /**
   * @brief Fill a rectangle of GDDRAM with one colour by streaming data (no drawing engine).
   * @param x Column, 0-based in the visible area.
   * @param y Row, 0-based in the visible area.
   * @param width Columns, at least 1.
   * @param height Rows, at least 1.
   * @param color The colour.
   * @return As `Present()`.
   */
  Result<void> FillWindow(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                          Color color) noexcept;

  /**
   * @brief Set one pixel (a 1 x 1 window and its data).
   * @param x Column, 0-based in the visible area.
   * @param y Row, 0-based in the visible area.
   * @param color The colour.
   * @return As `Present()`.
   */
  Result<void> SetPixel(uint8_t x, uint8_t y, Color color) noexcept;

  /**
   * @brief Fill the whole visible area with one colour.
   * @param color The colour of every pixel; black by default.
   * @return As `Present()`.
   */
  Result<void> Clear(Color color = kBlack) noexcept;

  /** @return true after a scroll was deactivated and before a full-area write: RAM must be
   * rewritten. No I/O. */
  [[nodiscard]] bool RamNeedsRewrite() const noexcept {
    return ram_rewrite_;
  }

  /// @}
  /// @name Drawing engine
  /// @{

  /**
   * @brief Draw a line with the controller's engine (21h), then wait `engine_settle_us`.
   * @param x0 Start column.
   * @param y0 Start row.
   * @param x1 End column.
   * @param y1 End row.
   * @param color The line colour.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Locked`, `ScrollActive` or `Bus`.
   */
  Result<void> DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Color color) noexcept;

  /**
   * @brief Draw a rectangle outline (26h, 22h), then wait `engine_settle_us`.
   * @param x0 Left column.
   * @param y0 Top row.
   * @param x1 Right column, at least `x0`.
   * @param y1 Bottom row, at least `y0`.
   * @param outline The outline colour.
   * @return As `DrawLine()`.
   */
  Result<void> DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                             Color outline) noexcept;

  /**
   * @brief Draw a filled rectangle (26h, 22h) with an outline colour and a fill colour.
   * @param x0 Left column.
   * @param y0 Top row.
   * @param x1 Right column.
   * @param y1 Bottom row.
   * @param outline The outline colour.
   * @param fill The fill colour.
   * @return As `DrawLine()`.
   */
  Result<void> FillRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Color outline,
                             Color fill) noexcept;

  /**
   * @brief Copy a window to another place (26h, 23h); the destination must fit in the visible area.
   * @param x0 Source left column.
   * @param y0 Source top row.
   * @param x1 Source right column.
   * @param y1 Source bottom row.
   * @param dest Destination top-left corner.
   * @param reverse true to swap GS0 <-> GS63 in the copy.
   * @return As `DrawLine()`.
   */
  Result<void> CopyWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Point dest,
                          bool reverse = false) noexcept;

  /**
   * @brief Dim a window (24h): gray levels from GS16 are reduced.
   * @param x0 Left column.
   * @param y0 Top row.
   * @param x1 Right column.
   * @param y1 Bottom row.
   * @return As `DrawLine()`.
   */
  Result<void> DimWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) noexcept;

  /**
   * @brief Set a window of GDDRAM to zero (25h).
   * @param x0 Left column.
   * @param y0 Top row.
   * @param x1 Right column.
   * @param y1 Bottom row.
   * @return As `DrawLine()`.
   */
  Result<void> ClearWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) noexcept;

  /// @}
  /// @name Scrolling
  /// @{

  /**
   * @brief Set the scroll parameters (27h). Not while scrolling is on.
   * @param setup The parameters; both directions zero is `InvalidArgument`.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Locked`, `ScrollActive` or `Bus`.
   */
  Result<void> SetupScroll(const ScrollSetup& setup) noexcept;

  /** @brief Start scrolling (2Fh). @return `Ok`, `NotInitialized`, `Locked`, `ScrollNotSet` or
   * `Bus`. */
  Result<void> ActivateScroll() noexcept;

  /** @brief Stop scrolling (2Eh); the RAM content must be rewritten afterwards
   * (`RamNeedsRewrite()`). @return `Ok`, `NotInitialized`, `Locked` or `Bus`. */
  Result<void> DeactivateScroll() noexcept;

  /** @return true while the driver has scrolling on. No I/O. */
  [[nodiscard]] bool IsScrolling() const noexcept {
    return scrolling_;
  }

  /// @}
  /// @name Switch
  /// @{

  /**
   * @brief Sample the switch, debounce it and apply the idle timers.
   *
   * Reads the pin, passes the level and the time to the debouncer and returns its event. When the
   * display was lowered by the idle timers (dimmed or put to sleep by `Poll()` itself), a `Pressed`
   * event wakes it (`DisplayOn()`, `woke_display`); a display that the application dimmed or
   * switched off is not woken, and neither is a locked controller (the event is delivered and the
   * display stays as the lock left it). If that `DisplayOn()` fails the call returns its error, the
   * event is not delivered, and `GetButtonPressCount()` has already counted the press. When
   * `idle_dim_ms` or `idle_sleep_ms` has passed since the last event, `DisplayDim()` or
   * `DisplayOff()` runs. Call it at least twice per debounce time.
   *
   * @return The update, or `Unsupported` (no `ReadSwitch()` or `NowMs()` hook), `Bus` (the switch
   *         read failed, or a transfer of the wake or the idle policy),
   * or the errors of the display change.
   */
  Result<ButtonUpdate> Poll() noexcept;

  /** @brief Read the switch pin once, as a logical level (true = closed, polarity applied). @return
   * The level, `Unsupported` (no `ReadSwitch()` hook, or the display-only module) or `Bus`. */
  Result<bool> ReadSwitchRaw() noexcept;

  /**
   * @brief Change the switch configuration; the debouncer restarts.
   * @param config The configuration; checked with `Validate()` before any I/O.
   * @return `Ok` or `InvalidArgument`.
   */
  Result<void> SetButtonConfig(const ButtonConfig& config) noexcept;

  /** @return The debounced level: true while the switch is down. No I/O. */
  [[nodiscard]] bool IsButtonDown() const noexcept {
    return button_.IsDown();
  }
  /** @return Presses counted by the debouncer. No I/O. */
  [[nodiscard]] uint32_t GetButtonPressCount() const noexcept {
    return button_.PressCount();
  }

  /** @brief Restart the idle timers from now. @return `Ok` or `Unsupported` (no `NowMs()`). */
  Result<void> NotifyActivity() noexcept;

  /// @}
  /// @name Diagnostics
  /// @{

  /**
   * @brief Send one command once: one transfer, no retry, no decoding of its effect (handbook
   * KB-CPP-129).
   *
   * For bring-up. The opcode and parameter count must match Table 10. The driver cannot track what
   * a raw command changed: `IsShadowInDoubt()` is true afterwards. AFh and ACh need the supply on.
   * The checks of the typed calls do not apply: A8h and 27h can break the rule that a vertical
   * scroll needs 64 MUX, a raw AEh, AFh or ACh does not change `GetOutput()`, and a raw B8h is not
   * checked for rising widths. FDh takes only 12h (unlock) or 16h (lock), so that `IsLocked()`
   * follows it; ADh takes only 8Eh (A[0] must be 0); a raw 2Fh or 2Eh updates `IsScrolling()`.
   *
   * @param opcode A command of Table 10.
   * @param parameters Exactly its parameter bytes.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (nothing sent), `SupplyOff`, `Locked` or
   * `Bus`.
   */
  Result<void> TraceCommand(uint8_t opcode, std::span<const uint8_t> parameters) noexcept;

  /** @return The opcode of the last command transfer; 0 before the first. No I/O. */
  [[nodiscard]] uint8_t GetLastCommandOpcode() const noexcept {
    return last_opcode_;
  }
  /** @return The length in bytes of the last transfer, command or data; 0 before the first. No I/O.
   */
  [[nodiscard]] std::size_t GetLastTransferBytes() const noexcept {
    return last_len_;
  }
  /** @return true after `TraceCommand()` or a failed transfer: the chip may differ from the shadow.
   * No I/O. */
  [[nodiscard]] bool IsShadowInDoubt() const noexcept {
    return shadow_doubt_;
  }
  /** @return true while the driver has the command interface locked. No I/O. */
  [[nodiscard]] bool IsLocked() const noexcept {
    return locked_;
  }

  /// @}

private:
  Result<void> powerUp(const Config& config) noexcept;
  Result<void> sendConfig(const Config& config) noexcept;
  Result<void> safeOff(bool wait_discharge) noexcept;
  Result<void> turnOn(bool dim) noexcept;
  Result<void> ready(bool ram) const noexcept;
  Result<void> send(std::size_t length) noexcept;
  Result<void> send1(uint8_t opcode) noexcept;
  Result<void> send2(uint8_t opcode, uint8_t a) noexcept;
  Result<void> sendWindow(uint8_t col0, uint8_t row0, uint8_t col1, uint8_t row1) noexcept;
  Result<void> sendData(std::size_t length) noexcept;
  Result<void> openWindow(uint8_t x, uint8_t y, uint8_t width, uint8_t height) noexcept;
  void windowDone(uint8_t width, uint8_t height) noexcept;
  Result<void> streamPixels(uint8_t width, uint8_t height, std::span<const Color> colors,
                            Color fill) noexcept;
  Result<void> streamBytes(std::span<const uint8_t> bytes) noexcept;
  Result<void> checkWindow(uint8_t x, uint8_t y, uint8_t width, uint8_t height) const noexcept;
  Result<void> engineCheck(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                           bool ordered) const noexcept;
  Result<void> engineEnd() noexcept;
  Result<void> setFillBit(bool filled) noexcept;
  Result<void> sendRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Color outline,
                             Color fill) noexcept;
  Result<void> windowCommand(uint8_t opcode, uint8_t x0, uint8_t y0, uint8_t x1,
                             uint8_t y1) noexcept;
  [[nodiscard]] std::array<uint8_t, 3> triple(Color color) const noexcept;
  Result<void> applyIdle(uint32_t idle) noexcept;
  /// Reset the debouncer only when the switch configuration changed.
  void configureButton(const ButtonConfig& b) noexcept;
  /// Whether a raw command has its parameter count and, for FDh and ADh, a defined parameter.
  [[nodiscard]] static bool rawCommandValid(uint8_t opcode,
                                            std::span<const uint8_t> parameters) noexcept;
  /// Keep the scroll shadow in step with a raw 2Fh or 2Eh.
  void trackRawScroll(uint8_t opcode) noexcept;
  /// Whether a `Poll()` update should wake the display.
  [[nodiscard]] bool wakeWanted(const ButtonUpdate& u) const noexcept;
  /// The byte of BBh: the level and the reserved bit 0 as the configuration says.
  [[nodiscard]] static uint8_t prechargeByte(uint8_t level, const Config& config) noexcept;
  Result<void> sendTiming(const Config& config) noexcept;
  Result<void> pin(CtrlPin which, GpioSignal level) noexcept;
  Result<void> fail(Error error, const char* message) noexcept;
  void touch() noexcept;

  detail::WriteBus<Port> bus_;
  Config config_{};
  ButtonDebouncer button_;
  std::array<uint8_t, detail::kMaxCommandBytes> command_{}; ///< Command transfer buffer.
  std::array<uint8_t, detail::kDataChunkBytes> data_{};     ///< Pixel transfer buffer.
  std::size_t last_len_{0};
  uint32_t last_activity_ms_{0};
  uint8_t multiplex_{64};
  std::array<uint8_t, 3> contrast_cap_{0x80, 0x80, 0x80}; ///< Start-up contrasts: the dim limit.
  uint8_t last_opcode_{0};
  ScrollSetup scroll_{};
  Output output_{Output::Off};
  bool initialized_{false};
  bool vcc_on_{false};
  bool locked_{false};
  bool scrolling_{false};
  bool scroll_set_{false};
  bool ram_rewrite_{false};
  bool shadow_doubt_{false};
  bool activity_valid_{false};
  bool idle_lowered_{false};
};

} // namespace isf15acp4

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/isf15acp4.ipp" // IWYU pragma: export

#endif // ISF15ACP4_HPP
