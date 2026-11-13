/**
 * @file host_isf15acp4_port.hpp
 * @brief The host port: the driver's hooks over a simulated ISF15ACP4, so the example runs on any
 * PC.
 *
 * `isf15acp4_test::FullPort` (tests/mock_isf15acp4_bus.hpp) puts the controller model behind the
 * write-only bus, the two control pins, a virtual clock and a switch pin. This class adds what a
 * real port adds: a console `Log()` and the check of its bus settings against the driver's
 * requirements. A port for a real bus implements the same hooks: see docs/porting.md.
 */
#ifndef ISF15ACP4_HOST_ISF15ACP4_PORT_HPP
#define ISF15ACP4_HOST_ISF15ACP4_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "isf15acp4.hpp"
#include "mock_isf15acp4_bus.hpp"

/// The host port: simulated chip, real console.
class HostIsf15acp4Port : public isf15acp4::SpiInterface<HostIsf15acp4Port> {
public:
  /// The bus settings a real port would use; checked against the driver's bus requirements.
  static constexpr uint32_t kClockHz = 4'000'000;
  static constexpr uint8_t kSpiMode = 0;
  static_assert(kClockHz <= isf15acp4::kMaxClockHz, "clock above the SSD1331's SCLK maximum");
  static_assert(kSpiMode == isf15acp4::kSpiMode, "the controller is tested in SPI mode 0");

  bool EnsureInitialized() noexcept {
    return rig.Init();
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    return rig.Write(kind, data, len);
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    return rig.Gpio(pin, signal);
  }
  /// Time passes in the simulated chip; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    rig.Delay(us);
  }
  bool ReadSwitch(bool& high) noexcept {
    return rig.ReadSwitch(high);
  }
  [[nodiscard]] uint32_t NowMs() const noexcept {
    return rig.NowMs();
  }
  /// Driver messages (they carry their own `isf15acp4:` prefix) go to the console, one line each.
  void Log(isf15acp4::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    static_cast<void>(std::puts(message)); // plain text: never pass it as a format string
  }

  isf15acp4_test::Rig rig{};
};

#endif // ISF15ACP4_HOST_ISF15ACP4_PORT_HPP
