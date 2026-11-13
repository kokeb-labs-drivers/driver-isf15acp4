/**
 * @file hook_not_noexcept.cpp
 * @brief Must NOT compile: a Write hook that is not noexcept breaks the port contract, so the
 * port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: SpiPort
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

class ThrowingPort : public isf15acp4::SpiInterface<ThrowingPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) {
    static_cast<void>(kind);
    static_cast<void>(data);
    return len > 0U;
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  ThrowingPort port;
  isf15acp4::ISF15ACP4<ThrowingPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
