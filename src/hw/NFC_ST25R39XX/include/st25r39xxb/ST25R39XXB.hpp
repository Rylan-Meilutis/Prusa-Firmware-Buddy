#pragma once

#include <cstdint>
#include <optional>
#include <inplace_vector.hpp>
#include <nfcv/commands.hpp>

#include "ST25R39XXB_defs.hpp"
#include "hw_interface.hpp"
#include "system_interface.hpp"

namespace st25r39xxb {

class ST25R39XXB {
public:
    enum class Antenna : uint8_t {
        antenna_1,
        antenna_2,
    };

    ST25R39XXB(HWInterface &hw_int, SystemInterface &sys_int)
        : hw_int(hw_int)
        , sys_int(sys_int)
        , current_antenna(ST25R39XXB::Antenna::antenna_2)
        , buffer() {}

    /// Generic initialization function. Should be always called
    [[nodiscard]] nfcv::Result<void> init();
    /// Sets one of the antennas as output (the other will still be able to receive data)
    void select_antenna(Antenna target_antena);

private:
    HWInterface &hw_int;
    SystemInterface &sys_int;
    Antenna current_antenna = ST25R39XXB::Antenna::antenna_2;
    stdext::inplace_vector<std::byte, constant::FIFO_SIZE> buffer;

    [[nodiscard]] uint16_t get_fifo_len();
    /**
     * @brief Masks which interupt should ST25R39XXb send
     *
     * @attention The @p mask denotes which interrupts should be masked.
     * So if you want to enable an interrupt, then you will need to set the bit 0.
     * (So all 0xffffffff will disable all the interrupts).
     *
     * @param mask Mask of interrupts to "mask" (don't use)
     */
    void set_interrupt_mask(st25r39xxb::IRQType mask);
    [[nodiscard]] st25r39xxb::IRQType read_interrupt();
    /**
     * @brief Block the code execution until we receive all the interrupts from @p irqs_to_wait_for or the @p timeout_ms expires or some inner timer expires.
     *
     * @param irqs_to_wait_for - mask to interrupts to wait for - every interrupt needs to be triggered
     * @param timeout_ms - timeout after we give up trying to read the interupts - unit is in ms
     * @param inner_time_irq - if specified and such interrupt is received we will return Error::timeout - only one can be specified
     * @returns nfcv::Result<void> - return std::unexpected errors on timeout and if we receive any of the error iterrupts
     */
    [[nodiscard]] nfcv::Result<void> await_interrupt(st25r39xxb::IRQType irqs_to_wait_for, uint32_t timeout_ms, st25r39xxb::IRQType inner_timer_irq = st25r39xxb::IRQType::none);

    [[nodiscard]] nfcv::Result<void> turn_on_oscilator();
    void set_output_impedance(st25r39xxb::Impedance target_impedance);
    void set_output_amplitude(st25r39xxb::Amplitude target_amplitude);
    void switch_antennas();

    [[nodiscard]] nfcv::Result<void> nfcv_field_up();
    void nfcv_field_down();

    /**
     * Sends NFC-V command over ST25R39XXB.
     *
     * It returns true if command was successfull, otherwise returns false
     */
    [[nodiscard]] nfcv::Result<void> nfcv_command(nfcv::Command &command);
};

} // namespace st25r39xxb
