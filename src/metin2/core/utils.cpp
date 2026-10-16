#include <metin2/core/utils.h>

#include <metin2/log/log.h>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <source_location>
#include <utility>

#include <sys/random.h>
#include <unistd.h>

namespace metin2::inline core {

    auto time_str(time_t ct) -> char * {
        char *time_s = std::asctime(std::localtime(&ct));
        time_s[std::strlen(time_s) - 6] = '\0';
        return time_s + 4;
    }

    void trim_and_lower(const char *src, char *dest, size_t dest_size) {
        if (!dest || dest_size == 0) {
            return;
        }

        if (!src) {
            *dest = '\0';
            return;
        }

        while (*src && is_space(*src)) {
            ++src;
        }

        size_t len = 0;
        while (src[len] && len < dest_size - 1) {
            dest[len] = static_cast<char>(std::tolower(static_cast<unsigned char>(src[len])));
            ++len;
        }

        while (len > 0 && is_space(dest[len - 1])) {
            --len;
        }

        dest[len] = '\0';
    }

    void parse_token(char *src, char *token, char *value) {
        char *tmp = src;

        for (; *tmp && *tmp != ':'; ++tmp) {
            {
                if (is_space(*tmp)) {
                    continue;
                }
            }

            *(token++) = static_cast<char>(std::tolower(static_cast<unsigned char>(*tmp)));
        }

        *token = '\0';

        for (tmp += 2; *tmp; ++tmp) {
            {
                if (*tmp == '\n' || *tmp == '\r') {
                    continue;
                }
            }

            *(value++) = *tmp;
        }

        *value = '\0';
    }

    auto random_engine() -> std::mt19937 & {
        thread_local std::mt19937 engine{std::random_device{}()};
        return engine;
    }

    auto number(int from, int to, std::source_location where) -> int {
        if (from > to) {
            metin2::log::error("Random range is reversed ({:d} > {:d}), called from {}:{:d}", from, to, where.file_name(), where.line());
            std::swap(from, to);
        }
        return std::uniform_int_distribution<int>(from, to)(random_engine());
    }

    auto fnumber(float from, float to) -> float {
        if (from > to)
            std::swap(from, to);
        return std::uniform_real_distribution<float>(from, to)(random_engine());
    }

    auto prob(int percent) -> bool {
        return number(1, 100) <= percent;
    }

    auto gauss(float mean, float sigma) -> float {
        return std::normal_distribution<float>(mean, sigma)(random_engine());
    }

    auto secure_random_u32() -> uint32_t {
        uint32_t value = 0;
        while (getrandom(&value, sizeof(value), 0) != sizeof(value))
            ;
        return value;
    }

    void core_dump(std::source_location where) {
        metin2::log::error("Dumping core at {}:{:d}", where.file_name(), where.line());

        std::fflush(stdout);
        std::fflush(stderr);

        if (fork() == 0)
            std::abort();
    }

    namespace {
        auto uptime() -> std::chrono::steady_clock::duration {
            static const auto boot = std::chrono::steady_clock::now();
            return std::chrono::steady_clock::now() - boot;
        }
    }

    auto get_float_time() -> float {
        return std::chrono::duration<float>(uptime()).count();
    }

    auto get_dword_time() -> uint32_t {
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(uptime()).count());
    }

    auto utf8_sequence_length(unsigned char c) -> size_t {
        return c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2
                          : (c >> 4) == 0xE   ? 3
                          : (c >> 3) == 0x1E  ? 4
                                              : 1;
    }

    auto utf8_truncated_size(const char *s, size_t len, size_t max) -> size_t {
        size_t pos = 0;

        while (pos < len) {
            const size_t seq = utf8_sequence_length(static_cast<unsigned char>(s[pos]));

            if (pos + seq > len || pos + seq > max) {
                break;
            }

            pos += seq;
        }

        return pos;
    }

    auto utf8_strnlcpy(char *dst, size_t dst_size, const char *src, size_t src_max) -> size_t {
        if (dst_size == 0) {
            return 0;
        }

        const size_t n = utf8_truncated_size(src, strnlen(src, src_max), dst_size - 1);
        std::memcpy(dst, src, n);
        dst[n] = '\0';
        return n;
    }

    auto utf8_strlcpy(char *dst, const char *src, size_t size) -> size_t {
        const size_t src_len = std::strlen(src);

        if (size == 0) {
            return src_len;
        }

        const size_t n = utf8_truncated_size(src, src_len, size - 1);
        std::memcpy(dst, src, n);
        dst[n] = '\0';
        return src_len;
    }
}
