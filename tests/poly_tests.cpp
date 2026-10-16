#include "check.h"

#include <metin2/poly/expression.h>

#include <cmath>
#include <string>

namespace {
    auto evaluate(const std::string &text, double k = 0) -> double {
        metin2::poly::expression e;
        if (!e.analyze(text))
            return NAN;
        e.set_var("k", k);
        return e.eval();
    }

    auto near(double a, double b) -> bool {
        return std::fabs(a - b) <= 1e-12 * std::fmax(1.0, std::fabs(b));
    }
}

TEST(poly_parses_numbers) {
    CHECK(near(evaluate("12"), 12));
    CHECK(near(evaluate("3."), 3));
    CHECK(near(evaluate("12.50 + 0.25"), 12.75));
    CHECK(near(evaluate("4000000000"), 4000000000.0));
}

TEST(poly_evaluates_operators_and_functions) {
    CHECK(near(evaluate("-2 + 3 * 4 ^ 2"), 46));
    CHECK(near(evaluate("(1 + 2) / 4"), 0.75));
    CHECK(near(evaluate("min(3, 7) + max(3, 7)"), 10));
    CHECK(near(evaluate("sqrt(16) + abs(-2) + floor(2.7)"), 8));
    CHECK(near(evaluate("40 + 2 * k", 5), 50));
    CHECK(near(evaluate("pi"), 3.141592653589793));
}

TEST(poly_rejects_syntax_errors) {
    metin2::poly::expression e;
    CHECK(!e.analyze("1 +"));
    CHECK(!e.analyze("min(1)"));
    e.analyze("");
    CHECK_EQ(e.eval(), 0.0);
}
