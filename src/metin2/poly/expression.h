#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace metin2::poly {

class expression {
  public:
    expression();

    auto analyze(const char *str = nullptr) -> bool;
    auto eval() const -> double;
    auto set_var(const std::string &name, double value) -> bool;
    auto var(const std::string &name) const -> double;

  private:
    struct symbol {
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
    std::vector<int> token_base_;
    std::vector<double> num_base_;
    std::vector<symbol> symbols_;
    std::map<std::string, int, std::less<>> symbol_index_;
    std::string data_;
};
}

namespace metin2 {
    using poly::expression;
}
