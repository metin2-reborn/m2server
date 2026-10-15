#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace metin2::poly {

// A formula from the skill and guild tables (e.g. "(atk + 2 * k) * min(lv, 90)"): Analyze
// compiles it to postfix once, SetVar binds its variables and Eval computes it. Supports
// + - * / % ^, parentheses, pi and e, and the functions registered in the constructor.
class CPoly {
  public:
    CPoly();

    auto Analyze(const char *str = nullptr) -> bool;
    auto Eval() const -> double;
    auto SetVar(const std::string &name, double value) -> bool;
    auto GetVar(const std::string &name) const -> double;

  private:
    struct Symbol {
        int token;
        double value;
    };

    auto insert(const std::string &name, int token) -> int;
    auto find(const std::string &name) const -> int;
    void emit(int t, int tval);
    void match(int t);
    void expo();
    void factor();
    void term();
    void expr();
    auto lexan() -> int;
    void error();

    int token_ = 0;
    double num_token_ = 0;
    int look_ahead_ = 0;
    bool error_occur_ = true;
    std::size_t ui_look_pos_ = 0;
    std::vector<int> token_base_;  // postfix program; ID is followed by its symbol index
    std::vector<double> num_base_; // the constants NUM tokens push, in order
    std::vector<Symbol> symbols_;  // functions, constants and variables
    std::map<std::string, int, std::less<>> symbol_index_;
    std::string data_;
};
}

namespace metin2 {
    using poly::CPoly;
}
