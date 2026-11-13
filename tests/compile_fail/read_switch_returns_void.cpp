/**
 * @file read_switch_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: ReadSwitch hook must be
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

class VoidSwitchPort : public isf15acp4::SpiInterface<VoidSwitchPort> {
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
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
  void ReadSwitch(bool& high) noexcept {
    high = true;
  }
};

int main() {
  VoidSwitchPort port;
  isf15acp4::ISF15ACP4<VoidSwitchPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
