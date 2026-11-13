// clang-format off: the hook table below must keep one row per line.
/**
 * @file isf15acp4_spi_interface.hpp
 * @brief The write-only SPI port of the ISF15ACP4 driver: the CRTP base, its pin enums, the
 * `SpiPort` concept and the bus requirements.
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `isf15acp4::SpiInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the SPI bus and every pin up at its idle level on the first call (VCC enable off, RES# high, CS# high); `true` at once afterwards |
 * | `bool Write(DataKind kind, const uint8_t* data, std::size_t len) noexcept` | One write-only transfer of `len` >= 1 bytes: set D/C# for `kind` (`Command` low, `Data` high), CS# low, clock every byte out, CS# high |
 * | `bool GpioSet(CtrlPin pin, GpioSignal signal) noexcept` | Drive RES# or the VCC enable; `false` when the pin could not be driven |
 * | `void DelayUs(uint32_t us) noexcept` | Wait at least `us` microseconds |
 *
 * Optional hooks, described in `isf15acp4.hpp`: `bool ReadSwitch(bool& high) noexcept` (the switch
 * pin's physical level), `uint32_t NowMs() noexcept` (a free-running millisecond counter) and
 * `void Log(LogLevel, const char*) noexcept`.
 *
 * The bus is write only: serial mode has no read (SSD1331 section 8.1) and the module has no MISO
 * pin (NKK sheet E6). `Write()` returning `true` means the bytes were clocked out, never that the
 * controller received them. What the controller needs from the bus is below as `constexpr` values
 * (`kSpiMode` to `kContinuousWord`, SSD1331 section 7.1.3 and Table 21, NKK sheet E7); a port checks
 * its settings against them with `static_assert`.
 *
 * Pins and their idle levels (handbook KB-HW-018): before it enables an output the port drives it to
 * its idle level, and before the first transfer. The VCC enable idles OFF (`GpioSignal::INACTIVE`:
 * the 15 V OLED supply is off): this is the safety pin, and the driver also drives it off before it
 * resets the controller. RES# idles high (`INACTIVE`: not in reset). CS# idles high. D/C# is set by
 * `Write()` for every transfer. SCK idles low in mode 0.
 */
// clang-format on
#ifndef ISF15ACP4_SPI_INTERFACE_HPP
#define ISF15ACP4_SPI_INTERFACE_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

#include "isf15acp4_detail.hpp"
#include "isf15acp4_registers.hpp"
#include "isf15acp4_types.hpp"

namespace isf15acp4 {

/**
 * @name Bus requirements
 * What the controller needs from the SPI bus (SSD1331 section 7.1.3, Table 21; NKK sheet E7). A
 * port and a board configuration check their settings against these with `static_assert` (handbook
 * KB-CPP-123).
 * @{
 */
/// SPI mode the driver is tested with: CPOL 0, CPHA 0. Data is taken on the SCLK rising edge, so
/// mode 3 also works (Figure 39).
inline constexpr uint8_t kSpiMode = 0;
/// Each byte is clocked most significant bit first, D7 first (section 7.1.3).
inline constexpr bool kMsbFirst = true;
/// Highest SCK frequency: tcycle 150 ns minimum (Table 21), 6.66 MHz.
inline constexpr uint32_t kMaxClockHz = 6'600'000;
/// Shortest time CS# stays high between two transfers: the datasheet gives none, so one SCLK period
/// (tcycle) is used; not verified on hardware.
inline constexpr uint32_t kMinCsHighNs = 150;
/// CS# low to the first SCK edge, tCSS (Table 21).
inline constexpr uint32_t kCsSetupNs = 75;
/// Last SCK edge to CS# high, tCSH (Table 21).
inline constexpr uint32_t kCsHoldNs = 60;
/// `false`: the datasheet sets no limit on idle SCK time between the bytes of one transfer; D/C# is
/// sampled on every eighth clock.
inline constexpr bool kContinuousWord = false;
/// D/C# valid before the clock edge that starts a byte, tAS (Table 21).
inline constexpr uint32_t kDcSetupNs = 40;
/// D/C# held after the byte, tAH (Table 21).
inline constexpr uint32_t kDcHoldNs = 40;
/// There is no MISO: nothing can be read back.
inline constexpr bool kWriteOnly = true;
/** @} */

static_assert((kMaxClockHz * 150ULL) <= 1'000'000'000ULL, "tcycle 150 ns minimum");

/** @brief The pins the port drives through `GpioSet()`. */
enum class CtrlPin : uint8_t {
  Reset = 0, ///< RES#, active low on the pin: ACTIVE holds the controller in reset.
  VccEnable =
      1, ///< The enable of the 15 V OLED supply (module pin 9), active high: ACTIVE = supply on.
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/** @brief What a transfer carries: it sets the D/C# pin (section 7.2). */
enum class DataKind : uint8_t {
  Command = 0, ///< D/C# low: an opcode or a parameter byte.
  Data = 1,    ///< D/C# high: GDDRAM data.
};

/**
 * @brief CRTP base of the platform SPI port.
 *
 * Derive `class MyPort : public isf15acp4::SpiInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data; the driver checks your class with the `SpiPort`
 * concept, so a missing or misdeclared hook is a compile error that names it.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class SpiInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface(const SpiInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface& operator=(const SpiInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  SpiInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~SpiInterface() noexcept = default;
};

namespace detail {

/// The port drives the control pins: exactly `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

} // namespace detail

/**
 * @brief Satisfied by a class that derives from `SpiInterface<Port>` and implements the required
 * hooks with exactly the types above, `noexcept` and returning `bool`: `EnsureInitialized()`,
 * `Write()` and `GpioSet()` (RES# and the VCC enable).
 *
 * Each hook's address is matched with `detail::ExactHook`, which takes a member function of exactly
 * the signature and converts no value: a hook declared with a narrower length (`uint16_t len`) or
 * without `noexcept` does not satisfy the concept, instead of truncating every transfer silently
 * (KB-CPP20-006). `DelayUs()` is checked by the driver with a message that names it.
 *
 * @tparam Port The port class.
 */
template <typename Port>
concept SpiPort =
    std::derived_from<Port, SpiInterface<Port>> && detail::HasGpioSet<Port> && requires {
      detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
      detail::ExactHook<Port, bool(DataKind, const uint8_t*, std::size_t) noexcept>::Take(
          &Port::Write);
    };

namespace detail {

/// Longest command the driver sends in one transfer, in bytes: B8h and its 32 pulse widths.
inline constexpr std::size_t kMaxCommandBytes = 33;
/// Pixel bytes per data transfer: 96 divides by 1, 2 and 3 bytes per pixel.
inline constexpr std::size_t kDataChunkBytes = 96;

/**
 * Write access over the port: one transfer per call, spans for the buffers (KB-CPP20-042). The
 * bytes of a command go out in one transfer with D/C# low, the pixels in chunks with D/C# high.
 */
template <SpiPort Port>
class WriteBus {
public:
  explicit WriteBus(Port& port) noexcept : port_(port) {}

  Port& GetPort() & noexcept {
    return port_;
  }

  /// Send `bytes` as one command transfer. `InvalidArgument` (nothing sent) for 0 or more than
  /// `kMaxCommandBytes` bytes, `Bus` on a failed transfer.
  Error Command(std::span<const uint8_t> bytes) noexcept {
    if (bytes.empty() || (bytes.size() > kMaxCommandBytes)) {
      return Error::InvalidArgument;
    }
    return port_.Write(DataKind::Command, bytes.data(), bytes.size()) ? Error::Ok : Error::Bus;
  }

  /// Send `bytes` as one data transfer. `InvalidArgument` (nothing sent) for 0 or more than
  /// `kDataChunkBytes` bytes, `Bus` on a failed transfer.
  Error Data(std::span<const uint8_t> bytes) noexcept {
    if (bytes.empty() || (bytes.size() > kDataChunkBytes)) {
      return Error::InvalidArgument;
    }
    return port_.Write(DataKind::Data, bytes.data(), bytes.size()) ? Error::Ok : Error::Bus;
  }

private:
  Port& port_;
};

} // namespace detail

} // namespace isf15acp4

#endif // ISF15ACP4_SPI_INTERFACE_HPP
