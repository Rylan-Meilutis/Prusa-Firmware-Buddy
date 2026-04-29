/// @file
#include <buddy/digest.hpp>

#include <bsod.h>
#include <logging/log.hpp>
#include <mbedtls/sha256.h>
#include <timing.h>
#include <unistd.h>
#include <array>

LOG_COMPONENT_REF(Buddy);

bool buddy::compute_file_digest(const int fd, const uint32_t salt, Digest output) {
    if (lseek(fd, 0, SEEK_SET) == -1) {
        return false;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const uint8_t *>(&salt), sizeof(salt));

    std::array<uint8_t, 128> buffer;
    for (;;) {
        const ssize_t nread = read(fd, buffer.data(), buffer.size());
        if (nread == -1) {
            mbedtls_sha256_free(&ctx);
            return false;
        } else if (nread == 0) {
            mbedtls_sha256_finish_ret(&ctx, (unsigned char *)output.data());
            mbedtls_sha256_free(&ctx);
            return true;
        } else {
            mbedtls_sha256_update_ret(&ctx, buffer.data(), nread);
        }
    }
}

void buddy::compute_file_digest_with_retry(const int fd, const uint32_t salt, Digest output) {
    const auto started = ticks_ms();
    for (;;) {
        if (buddy::compute_file_digest(fd, salt, output)) {
            return;
        }
        if (ticks_diff(ticks_ms(), started) < 1'000) {
            log_error(Buddy, "compute_file_digest() failed, retrying");
        } else {
            bsod("compute_file_digest");
        }
    }
}
