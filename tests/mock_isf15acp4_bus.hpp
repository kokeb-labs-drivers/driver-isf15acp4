/**
 * @file mock_isf15acp4_bus.hpp
 * @brief The test ports: the controller model behind a write-only bus with pins and a virtual
 * clock, and the faults the tests and the fuzz target inject.
 *
 * `FullPort` has every hook; `MinimalPort` only the required ones (no switch, no clock, no log).
 * Time is virtual: `DelayUs()` and every transfer advance the model's clock, and `NowMs()` reads
 * it.
 */
#ifndef ISF15ACP4_MOCK_ISF15ACP4_BUS_HPP
#define ISF15ACP4_MOCK_ISF15ACP4_BUS_HPP

#include <cstddef>
#include <cstdint>

#include "isf15acp4_spi_interface.hpp"
#include "mock_isf15acp4.hpp"

namespace isf15acp4_test {

/// The chip, the pins' faults and the counters, shared by both ports.
struct Rig {
  Ssd1331Model chip;
  bool bus_up_ok{true};           ///< `EnsureInitialized()` result.
  int fail_write_at{-1};          ///< Fail the write call with this index (0-based); -1 = none.
  int fail_writes_from{-1};       ///< Fail every write call from this index on; -1 = none.
  bool fail_vcc_on{false};        ///< The VCC enable cannot be driven on.
  bool fail_vcc_off{false};       ///< The VCC enable cannot be driven off.
  bool fail_reset_pin{false};     ///< RES# cannot be driven.
  bool fail_reset_assert{false};  ///< RES# cannot be pulled low.
  bool fail_reset_release{false}; ///< RES# cannot be released.
  bool fail_switch{false};        ///< The switch read fails.
  bool switch_high{true};         ///< Physical level of the switch pin (pull-up: open = high).
  bool bounce{false};             ///< The switch level flips on every read (contact bounce).
  int writes{0};                  ///< Write calls so far.
  int failed_writes{0};           ///< Write calls that returned false.
  int gpio_calls{0};
  int log_lines{0};
  int bus_inits{0};
  uint32_t log_errors{0};

  bool Init() noexcept {
    bus_inits++;
    return bus_up_ok;
  }

  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    const int index = writes++;
    const bool fail =
        (index == fail_write_at) || ((fail_writes_from >= 0) && (index >= fail_writes_from));
    if (fail) {
      failed_writes++;
      return false;
    }
    chip.SetDc(kind == isf15acp4::DataKind::Data);
    chip.SetCs(true);
    for (std::size_t i = 0; i < len; ++i) {
      chip.Byte(data[i]);
    }
    chip.SetCs(false);
    chip.Elapse(static_cast<uint32_t>(len) * 2U);
    return true;
  }

  bool Gpio(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal level) noexcept {
    gpio_calls++;
    const bool active = (level == isf15acp4::GpioSignal::ACTIVE);
    if (pin == isf15acp4::CtrlPin::VccEnable) {
      if (active ? fail_vcc_on : fail_vcc_off) {
        return false;
      }
      chip.SetVcc(active);
      return true;
    }
    if (fail_reset_pin || (active ? fail_reset_assert : fail_reset_release)) {
      return false;
    }
    chip.SetRes(active);
    return true;
  }

  bool ReadSwitch(bool& high) noexcept {
    if (fail_switch) {
      return false;
    }
    if (bounce) {
      switch_high = !switch_high;
    }
    high = switch_high;
    return true;
  }

  void Delay(uint32_t us) noexcept {
    chip.Elapse(us);
  }
  [[nodiscard]] uint32_t NowMs() const noexcept {
    return static_cast<uint32_t>(chip.NowUs() / 1000U);
  }
};

/// A port with every hook.
class FullPort : public isf15acp4::SpiInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return rig.Init();
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    return rig.Write(kind, data, len);
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal level) noexcept {
    return rig.Gpio(pin, level);
  }
  void DelayUs(uint32_t us) noexcept {
    rig.Delay(us);
  }
  bool ReadSwitch(bool& high) noexcept {
    return rig.ReadSwitch(high);
  }
  [[nodiscard]] uint32_t NowMs() const noexcept {
    return rig.NowMs();
  }
  void Log(isf15acp4::LogLevel level, const char* message) noexcept {
    rig.log_lines +=
        ((message != nullptr) && (static_cast<unsigned char>(message[0]) != 0U)) ? 1 : 0;
    rig.log_errors += (level == isf15acp4::LogLevel::Error) ? 1U : 0U;
  }
  Rig rig{};
};

/// A port with only the required hooks.
class MinimalPort : public isf15acp4::SpiInterface<MinimalPort> {
public:
  bool EnsureInitialized() noexcept {
    return rig.Init();
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    return rig.Write(kind, data, len);
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal level) noexcept {
    return rig.Gpio(pin, level);
  }
  void DelayUs(uint32_t us) noexcept {
    rig.Delay(us);
  }
  Rig rig{};
};

} // namespace isf15acp4_test

#endif // ISF15ACP4_MOCK_ISF15ACP4_BUS_HPP
