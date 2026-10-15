#include "check.h"

#include <cstdio>
#include <cstring>

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
        std::printf("%s %s\n", check::failures == failures_before ? "PASS" : "FAIL", test.name);
        ++run;
    }

    if (run == 0) {
        std::fprintf(stderr, "no test matched\n");
        return 1;
    }
    return check::failures == 0 ? 0 : 1;
}
