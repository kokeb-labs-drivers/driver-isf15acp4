/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

namespace {

// A port whose transfers all succeed and go nowhere: enough to instantiate the whole driver
// template.
class NullPort : public isf15acp4::SpiInterface<NullPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    static_cast<void>(kind);
    return (data != nullptr) && (len > 0U);
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

} // namespace

int main() {
  NullPort port;
  isf15acp4::ISF15ACP4<NullPort> panel(port);
  // The start-up and the display-on must succeed on a bus that accepts every byte: proof the
  // template compiled and ran.
  const bool up = panel.EnsureInitialized().ok() && panel.DisplayOn().ok() && panel.Clear().ok();
  const bool down = panel.Deinitialize().ok();
  return (up && down && (static_cast<unsigned char>(isf15acp4::GetDriverVersion()[0]) != 0U)) ? 0
                                                                                              : 1;
}
