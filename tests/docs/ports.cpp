/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

bool platform_spi_init() noexcept;
bool platform_spi_write(const uint8_t* data, std::size_t len) noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
bool platform_gpio_read(int pin) noexcept;
uint32_t platform_millis() noexcept;
void platform_delay_us(uint32_t us) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public isf15acp4::SpiInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_gpio_write(kVccGpio, false) && platform_gpio_write(kResGpio, true) &&
           platform_spi_init();
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    return platform_gpio_write(kDcGpio, kind == isf15acp4::DataKind::Data) &&
           platform_spi_write(data, len);
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    const bool active = (signal == isf15acp4::GpioSignal::ACTIVE);
    return (pin == isf15acp4::CtrlPin::Reset) ? platform_gpio_write(kResGpio, !active)
                                              : platform_gpio_write(kVccGpio, active);
  }
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: the switch pin's physical level. The driver applies the polarity.
  bool ReadSwitch(bool& high) noexcept {
    high = platform_gpio_read(kSwitchGpio);
    return true;
  }
  // Optional: a free-running millisecond counter; it may wrap.
  uint32_t NowMs() noexcept {
    return platform_millis();
  }
  // Optional: one line per lifecycle event or failure. The driver never formats; the port does.
  void Log(isf15acp4::LogLevel level, const char* message) noexcept {
    if (level > isf15acp4::LogLevel::Warning) {
      return;
    }
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kDcGpio = 3;
  static constexpr int kVccGpio = 4;
  static constexpr int kSwitchGpio = 5;
  static constexpr int kResGpio = 11;
};
// --8<-- [end:full-port]

// --8<-- [start:reinitialize]
isf15acp4::Error RecoverFromDarkPanel(isf15acp4::ISF15ACP4<FullPort>& panel) {
  // The bus is write only: a brown-out of the logic supply cannot be detected, only repaired. This
  // runs the power-down, the start-up and the display-on again.
  return panel.Reinitialize().error;
}
// --8<-- [end:reinitialize]
