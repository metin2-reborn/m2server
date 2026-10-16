#include <metin2/core/utils.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <random>
#include <source_location>
#include <utility>

#include <sys/random.h>
#include <unistd.h>

namespace metin2::inline core {

    auto time_str(time_t ct) -> std::string {
        using namespace std::chrono;
        const zoned_time local{current_zone(), floor<seconds>(system_clock::from_time_t(ct))};
        return std::format("{:%b %e %H:%M:%S}", local);
    }

    void trim_and_lower(std::string_view src, std::span<char> dest) {
        if (dest.empty()) {
            return;
        }

        const auto blank = [](char ch) -> bool { return is_space(ch); };
        src.remove_prefix(std::ranges::find_if_not(src, blank) - src.begin());
        src = src.substr(0, dest.size() - 1);

        auto end = std::ranges::transform(src, dest.begin(), ascii_lower).out;
        while (end != dest.begin() && is_space(end[-1])) {
            --end;
        }
        *end = '\0';
    }

    void trim_and_lower(const char *src, std::span<char> dest) {
        if (!src || dest.empty()) {
            trim_and_lower(std::string_view{}, dest);
            return;
        }

        while (*src && is_space(*src)) {
            ++src;
        }
        trim_and_lower(std::string_view(src, strnlen(src, dest.size() - 1)), dest);
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

    auto utf8_truncated_size(std::string_view s, size_t max) -> size_t {
        size_t pos = 0;

        while (pos < s.size()) {
            const size_t seq = utf8_sequence_length(static_cast<unsigned char>(s[pos]));

            if (pos + seq > s.size() || pos + seq > max) {
                break;
            }

            pos += seq;
        }

        return pos;
    }

    namespace {
        auto utf8_copy(std::span<char> dst, std::string_view src) -> size_t {
            const size_t n = utf8_truncated_size(src, dst.size() - 1);
            std::ranges::copy(src.substr(0, n), dst.begin());
            dst[n] = '\0';
            return n;
        }
    }

    auto utf8_strnlcpy(std::span<char> dst, std::span<const char> src) -> size_t {
        if (dst.empty()) {
            return 0;
        }
        return utf8_copy(dst, std::string_view(src.data(), strnlen(src.data(), src.size())));
    }

    auto utf8_strlcpy(std::span<char> dst, std::string_view src) -> size_t {
        if (!dst.empty()) {
            utf8_copy(dst, src);
        }
        return src.size();
    }
}
