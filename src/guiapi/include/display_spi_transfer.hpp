#pragma once

#include <device/hal.h>
#include <cmsis_os.h>
#include <cstdint>
#include <cstring>
#include <utility>

namespace display {

/// GUI-owned SPI state. A failed frame is quarantined until the next health
/// check, rather than timing out once for every command/pixel in that frame.
/// No panel reset is needed when its registers still read back correctly.
class SpiTransfer {
public:
    // Even a 65535-byte transfer at the supported LCD/LED clocks fits here.
    static constexpr uint32_t timeout_ms = 100;

    bool transmit(SPI_HandleTypeDef *spi, osThreadId task, int32_t signal,
        const uint8_t *data, uint16_t size, bool dma) {
        if (failed_) {
            return false;
        }
        if (!dma) {
            return finish(spi, HAL_SPI_Transmit(spi, const_cast<uint8_t *>(data), size, timeout_ms) == HAL_OK);
        }

        // A start failure never produces a completion signal. Never wait for
        // it, and never let an old signal permit reuse of an active DMA buffer.
        (void)task;
        // This CMSIS port declares osSignalClear but does not implement it.
        // A nonblocking wait consumes the previous completion notification.
        osSignalWait(signal, 0);
        if (HAL_SPI_Transmit_DMA(spi, const_cast<uint8_t *>(data), size) != HAL_OK) {
            return finish(spi, false);
        }
        const auto event = osSignalWait(signal, timeout_ms);
        return finish(spi, event.status == osEventSignal && (event.value.signals & signal) && HAL_SPI_GetState(spi) == HAL_SPI_STATE_READY && HAL_SPI_GetError(spi) == HAL_SPI_ERROR_NONE);
    }

    bool receive(SPI_HandleTypeDef *spi, uint8_t *data, uint16_t size) {
        const bool ok = !failed_ && finish(spi, HAL_SPI_Receive(spi, data, size, timeout_ms) == HAL_OK);
        if (!ok && size) {
            // Pixel/screenshot callers must not consume uninitialized bytes.
            std::memset(data, 0, size);
        }
        return ok;
    }

    bool take_error() {
        return std::exchange(failed_, false);
    }

    void reset_error() {
        failed_ = false;
    }

private:
    bool finish(SPI_HandleTypeDef *spi, bool ok) {
        if (!ok) {
            HAL_SPI_Abort(spi);
            failed_ = true;
        }
        return ok;
    }

    bool failed_ = false;
};

} // namespace display
