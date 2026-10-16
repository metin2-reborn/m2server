#include "check.h"

#include <cstring>
#include <print>

auto main(int argc, char **argv) -> int {
    int run = 0;
    for (const check::test_case &test : check::tests()) {
        bool selected = argc < 2;
        for (int i = 1; i < argc; ++i) {
            selected = selected || std::strcmp(argv[i], test.name) == 0;
        }
        if (!selected) {
            continue;
        }

        const int failures_before = check::failures;
        test.function();
        std::println("{} {}", check::failures == failures_before ? "PASS" : "FAIL", test.name);
        ++run;
    }

    if (run == 0) {
        std::println(stderr, "no test matched");
        return 1;
    }
    return check::failures == 0 ? 0 : 1;
}
