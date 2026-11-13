/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

class ShortDelayPort : public isf15acp4::SpiInterface<ShortDelayPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    static_cast<void>(kind);
    static_cast<void>(data);
    return len > 0U;
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
    return true;
  }
  void DelayUs(uint16_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  ShortDelayPort port;
  isf15acp4::ISF15ACP4<ShortDelayPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
