/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: GpioSet drives RES# and the supply enable: a hook that returns void
 * could not report a failed pin write, so the port concept rejects it.
 *
 * EXPECT: SpiPort
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

class VoidGpioPort : public isf15acp4::SpiInterface<VoidGpioPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    static_cast<void>(kind);
    static_cast<void>(data);
    return len > 0U;
  }
  void GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  VoidGpioPort port;
  isf15acp4::ISF15ACP4<VoidGpioPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
