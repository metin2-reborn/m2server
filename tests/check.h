#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace check {
    struct test_case {
        const char *name;
        void (*function)();
    };

    inline auto tests() -> std::vector<test_case> & {
        static std::vector<test_case> registered;
        return registered;
    }

    inline int failures = 0;

    struct registrar {
        registrar(const char *name, void (*function)()) { tests().push_back({name, function}); }
    };

    inline auto describe(std::string_view value) -> std::string { return '"' + std::string(value) + '"'; }
    inline auto describe(const char *value) -> std::string { return describe(std::string_view(value)); }
    inline auto describe(const std::string &value) -> std::string { return describe(std::string_view(value)); }
    inline auto describe(bool value) -> std::string { return value ? "true" : "false"; }
    template <typename T>
    auto describe(const T &value) -> std::string {
        return std::to_string(value);
    }

    inline void fail(const char *file, int line, const std::string &message) {
        std::fprintf(stderr, "%s:%d: %s\n", file, line, message.c_str());
        ++failures;
    }
}

#define TEST(name)                                                                                                     \
    static void name();                                                                                                \
    static const check::registrar name##_registrar(#name, name);                                                       \
    static void name()

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr))                                                                                                   \
            check::fail(__FILE__, __LINE__, "CHECK(" #expr ") failed");                                                \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                                     \
    do {                                                                                                               \
        const auto &check_actual = (actual);                                                                           \
        const auto &check_expected = (expected);                                                                       \
        if (!(check_actual == check_expected))                                                                         \
            check::fail(__FILE__, __LINE__,                                                                            \
                        "CHECK_EQ(" #actual ", " #expected ") failed: " + check::describe(check_actual) +             \
                            " != " + check::describe(check_expected));                                                 \
    } while (0)
