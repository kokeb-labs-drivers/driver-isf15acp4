/**
 * @file hook_narrowed_parameter.cpp
 * @brief Must NOT compile: a Write hook with a narrower length would truncate every transfer, so
 * the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: SpiPort
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

class NarrowPort : public isf15acp4::SpiInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, uint16_t len) noexcept {
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
  NarrowPort port;
  isf15acp4::ISF15ACP4<NarrowPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
