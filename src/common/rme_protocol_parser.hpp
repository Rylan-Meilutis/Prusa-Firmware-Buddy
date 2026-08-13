#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>

namespace rme_protocol {

// Serial command dispatch may indirectly service the main loop while a file
// operation blocks.  Use this small non-owning guard around stateful byte
// stream consumers to reject recursive dispatch without heap allocation.
class ScopedDispatchGuard {
public:
    explicit ScopedDispatchGuard(bool &active)
        : active_(active)
        , entered_(!active) {
        if (entered_) active_ = true;
    }
    ~ScopedDispatchGuard() {
        if (entered_) active_ = false;
    }
    explicit operator bool() const { return entered_; }

    ScopedDispatchGuard(const ScopedDispatchGuard &) = delete;
    ScopedDispatchGuard &operator=(const ScopedDispatchGuard &) = delete;

private:
    bool &active_;
    bool entered_;
};

inline std::optional<std::string_view> value(const std::string_view command, const std::string_view key) {
    size_t token = command.find(' ');
    while (token != std::string_view::npos) {
        ++token;
        const size_t end = command.find_first_of(" *", token);
        const size_t count = (end == std::string_view::npos ? command.size() : end) - token;
        if (count > key.size() && command[token + key.size()] == '=' && command.substr(token, key.size()) == key) {
            return command.substr(token + key.size() + 1, count - key.size() - 1);
        }
        token = end;
    }
    return std::nullopt;
}

inline std::optional<long> signed_number(const std::string_view command, const std::string_view key, const int base = 10) {
    const auto text = value(command, key);
    if (!text || text->empty() || text->size() >= 16) return std::nullopt;
    std::array<char, 16> buffer {};
    std::copy(text->begin(), text->end(), buffer.begin());
    const char *start = buffer.data();
    if (base == 16 && *start == '#') ++start;
    char *end = nullptr;
    errno = 0;
    const long parsed = strtol(start, &end, base);
    if (end == start || *end || errno == ERANGE) return std::nullopt;
    return parsed;
}

inline std::optional<uint32_t> unsigned_number(const std::string_view command, const std::string_view key) {
    const auto text = value(command, key);
    if (!text || text->empty() || text->size() > 10) return std::nullopt;
    uint32_t parsed = 0;
    for (const char c : *text) {
        if (c < '0' || c > '9') return std::nullopt;
        const uint32_t digit = static_cast<uint32_t>(c - '0');
        if (parsed > (std::numeric_limits<uint32_t>::max() - digit) / 10) return std::nullopt;
        parsed = parsed * 10 + digit;
    }
    return parsed;
}

inline std::optional<uint32_t> transaction(const std::string_view command) {
    return unsigned_number(command, "tx");
}

inline bool is_service_frame(const std::string_view command) {
    size_t start = command.find_first_not_of(' ');
    if (start == std::string_view::npos) return false;
    if (command[start] == 'N') {
        ++start;
        if (start < command.size() && command[start] == '-') ++start;
        const size_t digits = start;
        while (start < command.size() && command[start] >= '0' && command[start] <= '9') ++start;
        if (start == digits) return false;
        start = command.find_first_not_of(' ', start);
        if (start == std::string_view::npos) return false;
    }
    return command.substr(start).starts_with("@RME ");
}

inline int hex_nibble(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

template <size_t Capacity>
inline std::optional<std::array<char, Capacity>> percent_decode(const std::string_view encoded) {
    std::array<char, Capacity> result {};
    if (encoded.empty()) return std::nullopt;
    size_t out = 0;
    for (size_t i = 0; i < encoded.size(); ++i) {
        char decoded = encoded[i];
        if (decoded == '%') {
            if (i + 2 >= encoded.size()) return std::nullopt;
            const int high = hex_nibble(encoded[i + 1]);
            const int low = hex_nibble(encoded[i + 2]);
            if (high < 0 || low < 0) return std::nullopt;
            decoded = static_cast<char>((high << 4) | low);
            i += 2;
        }
        if (!decoded || out + 1 >= result.size()) return std::nullopt;
        result[out++] = decoded;
    }
    return result;
}

template <size_t Capacity>
inline std::optional<std::array<char, Capacity>> usb_path(const std::string_view encoded) {
    constexpr std::string_view root = "/usb/";
    std::array<char, Capacity> path {};
    if (path.size() <= root.size()) return std::nullopt;
    size_t write = 0;
    for (const char c : root) path[write++] = c;
    size_t read = 0;
    while (read < encoded.size() && encoded[read] == '/') ++read;
    if (read < encoded.size()) {
        const auto relative = percent_decode<Capacity>(encoded.substr(read));
        if (!relative) return std::nullopt;
        for (const char c : *relative) {
            if (!c) break;
            if (write + 1 >= path.size()) return std::nullopt;
            path[write++] = c;
        }
    }
    path[write] = '\0';
    const std::string_view normalized { path.data() + root.size() };
    if (normalized.find("..") != std::string_view::npos || normalized.find("//") != std::string_view::npos) return std::nullopt;
    return path;
}

inline bool parse_sha256(const std::string_view text, std::array<uint8_t, 32> &result) {
    if (text.size() != result.size() * 2) return false;
    for (size_t i = 0; i < result.size(); ++i) {
        const int high = hex_nibble(text[i * 2]);
        const int low = hex_nibble(text[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        result[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

} // namespace rme_protocol
