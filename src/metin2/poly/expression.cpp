#include <metin2/poly/expression.h>

#include <cctype>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace metin2::poly {

namespace {
    // Tokens beside the operator characters lexan returns as themselves.
    enum token : int {
        NONE = 0,
        ROOT = 1,
        MUL = 2,
        PLU = 3,
        POW = 4,
        MIN = 5,
        DIV = 6,
        NUM = 9,
        ID = 10,
        EOS = 11,
        COS = 12,
        SIN = 13,
        TAN = 14,
        CSC = 15,
        SEC = 16,
        COT = 17,
        LOG = 18,
        LN = 19,
        LOG10 = 20,
        ABS = 21,
        MINF = 22,
        MAXF = 23,
        IRAND = 24,
        FRAND = 25,
        MOD = 26,
        FLOOR = 27,
        SIGN = 28,
    };

    auto random_unit() -> double {
        return static_cast<double>(random()) / 2147483648.0;
    }

    // An integer in [start, end], both rounded.
    auto irandom(double start, double end) -> int {
        const int is = static_cast<int>(start + 0.5);
        const int ie = static_cast<int>(end - start + 0.5) + 1;
        return static_cast<int>(random_unit() * ie + is);
    }

    auto frandom(double start, double end) -> double {
        return random_unit() * (end - start) + start;
    }
}

expression::expression() {
    insert("min", MINF);
    insert("max", MAXF);
    insert("number", IRAND);
    insert("irandom", IRAND);
    insert("irand", IRAND);
    insert("frandom", FRAND);
    insert("frand", FRAND);
    insert("rt", ROOT);
    insert("sqrt", ROOT);
    insert("cos", COS);
    insert("sin", SIN);
    insert("tan", TAN);
    insert("cot", COT);
    insert("csc", CSC);
    insert("cosec", CSC);
    insert("sec", SEC);
    symbols_[insert("pi", ID)].value = std::numbers::pi;
    symbols_[insert("e", ID)].value = std::numbers::e;
    insert("log", LOG);
    insert("ln", LN);
    insert("log10", LOG10);
    insert("abs", ABS);
    insert("mod", MOD);
    insert("floor", FLOOR);
    insert("sign", SIGN);
}

// Any invalid operation (division by zero, a logarithm of a non-positive
// number, ...) makes the whole formula evaluate to 0.
auto expression::eval() const -> double {
    if (error_occur_ || token_base_.empty()) {
        return 0;
    }

    std::vector<double> stack;
    auto num = num_base_.begin();

    for (auto pos = token_base_.begin(); pos != token_base_.end(); ++pos) {
        const int token = *pos;

        if (token == NUM) {
            stack.push_back(*num++);
            continue;
        }

        if (token == ID) {
            stack.push_back(symbols_[*++pos].value);
            continue;
        }

        double &top = stack.back();

        switch (token) {
        case ROOT:
            if (top < 0) {
                return 0;
            }
            top = std::sqrt(top);
            continue;
        case COS:
            top = std::cos(top);
            continue;
        case SIN:
            top = std::sin(top);
            continue;
        case SIGN:
            top = top == 0.0 ? 0.0 : top < 0.0 ? -1.0
                                               : 1.0;
            continue;
        case TAN:
            if (std::cos(top) == 0) {
                return 0;
            }
            top = std::tan(top);
            continue;
        case CSC:
            if (std::sin(top) == 0) {
                return 0;
            }
            top = 1 / std::sin(top);
            continue;
        case SEC:
            if (std::cos(top) == 0) {
                return 0;
            }
            top = 1 / std::cos(top);
            continue;
        case COT:
            if (std::sin(top) == 0) {
                return 0;
            }
            top = std::cos(top) / std::sin(top);
            continue;
        case LN:
            if (top <= 0) {
                return 0;
            }
            top = std::log(top);
            continue;
        case LOG10:
            if (top <= 0) {
                return 0;
            }
            top = std::log10(top);
            continue;
        case ABS:
            top = std::fabs(top);
            continue;
        case FLOOR:
            top = std::floor(top);
            continue;
        default:
            break;
        }

        // Binary operations: the left operand is below the right one.
        const double b = stack.back();
        stack.pop_back();
        double &a = stack.back();

        switch (token) {
        case PLU:
            a += b;
            break;
        case MIN:
            a -= b;
            break;
        case MUL:
            a *= b;
            break;
        case MOD:
            if (b == 0) {
                return 0;
            }
            a = std::fmod(a, b);
            break;
        case DIV:
            if (b == 0) {
                return 0;
            }
            a /= b;
            break;
        case POW:
            a = std::pow(a, b);
            break;
        case LOG: // log(base, x)
            if (b <= 0 || a <= 0 || a == 1) {
                return 0;
            }
            a = std::log(b) / std::log(a);
            break;
        case IRAND:
            a = irandom(a, b);
            break;
        case FRAND:
            a = frandom(a, b);
            break;
        case MINF:
            a = a < b ? a : b;
            break;
        case MAXF:
            a = a > b ? a : b;
            break;
        default:
            return 0;
        }
    }

    return stack.back();
}

auto expression::analyze(const char *str) -> bool {
    if (str) {
        data_ = str;
    }

    token_base_.clear();
    num_base_.clear();

    // An empty formula is accepted and evaluates to 0.
    if (data_.empty()) {
        error_occur_ = true;
        return true;
    }

    error_occur_ = false;
    ui_look_pos_ = 0;
    look_ahead_ = lexan();

    expr();

    return !token_base_.empty() && !error_occur_;
}

void expression::expr() {
    // A leading sign reads as 0 followed by that operator.
    if (look_ahead_ == '+' || look_ahead_ == '-') {
        ui_look_pos_--;
        look_ahead_ = NUM;
        num_token_ = token_ = 0;
    }

    term();

    while (!error_occur_) {
        switch (look_ahead_) {
        case '+':
        case '-':
            {
                const int t = look_ahead_;
                match(t);
                term();
                emit(t, NONE);
                continue;
            }

        case EOS:
        case ')':
        case ',':
            return;

        default:
            error();
            return;
        }
    }
}

void expression::error() {
    error_occur_ = true;
}

auto expression::lexan() -> int {
    while (ui_look_pos_ < data_.size()) {
        const auto c = static_cast<unsigned char>(data_[ui_look_pos_]);

        if (c == ' ' || c == '\t') {
            ui_look_pos_++;
            continue;
        }

        if (std::isdigit(c)) {
            int t = 0;
            for (; ui_look_pos_ < data_.size() && std::isdigit(static_cast<unsigned char>(data_[ui_look_pos_])); ui_look_pos_++) {
                t = t * 10 + data_[ui_look_pos_] - '0';
            }

            token_ = t;
            num_token_ = 0;
            if (ui_look_pos_ < data_.size() && data_[ui_look_pos_] == '.') {
                ui_look_pos_++;
                for (double scale = 0.1; ui_look_pos_ < data_.size() && std::isdigit(static_cast<unsigned char>(data_[ui_look_pos_])); ui_look_pos_++, scale *= 0.1) {
                    num_token_ += scale * (data_[ui_look_pos_] - '0');
                }
            }

            num_token_ += token_;
            return NUM;
        }

        if (std::isalpha(c)) {
            std::string name;
            while (ui_look_pos_ < data_.size() &&
                   std::isalpha(static_cast<unsigned char>(data_[ui_look_pos_]))) {
                name += data_[ui_look_pos_++];
            }

            token_ = find(name);
            if (token_ == -1) {
                token_ = insert(name, ID);
            }
            return symbols_[token_].token;
        }

        token_ = 0;
        ui_look_pos_++;
        return c;
    }

    return EOS;
}

void expression::term() {
    factor();
    while (!error_occur_) {
        switch (look_ahead_) {
        case '*':
        case '/':
        case '%':
            {
                const int t = look_ahead_;
                match(t);
                factor();
                emit(t, NONE);
                continue;
            }

        default:
            return;
        }
    }
}

void expression::factor() {
    expo();
    while (!error_occur_ && look_ahead_ == '^') {
        match('^');
        expo();
        emit('^', NONE);
    }
}

void expression::expo() {
    const int t = look_ahead_;

    switch (t) {
    case '(':
        match('(');
        expr();
        match(')');
        break;
    case NUM:
        emit(NUM, token_);
        match(NUM);
        break;
    case ID:
        emit(ID, token_);
        match(ID);
        break;
    case ROOT:
    case SIN:
    case COT:
    case TAN:
    case CSC:
    case SEC:
    case LN:
    case LOG10:
    case COS:
    case ABS:
    case FLOOR:
    case SIGN:
        match(t);
        match('(');
        expr();
        match(')');
        emit(t, token_);
        break;
    case LOG:
    case MINF:
    case MAXF:
    case IRAND:
    case FRAND:
    case MOD:
        match(t);
        match('(');
        expr();
        match(',');
        expr();
        match(')');
        emit(t, token_);
        break;
    default: // includes the end of the formula where an operand is due, e.g. "1+"
        error();
    }
}

void expression::match(int t) {
    if (look_ahead_ == t) {
        look_ahead_ = lexan();
    } else {
        error();
    }
}

void expression::emit(int t, int tval) {
    switch (t) {
    case '+': token_base_.push_back(PLU); break;
    case '-': token_base_.push_back(MIN); break;
    case '*': token_base_.push_back(MUL); break;
    case '/': token_base_.push_back(DIV); break;
    case '%': token_base_.push_back(MOD); break;
    case '^': token_base_.push_back(POW); break;

    case NUM:
        token_base_.push_back(NUM);
        num_base_.push_back(num_token_);
        break;

    case ID:
        token_base_.push_back(ID);
        token_base_.push_back(tval);
        break;

    default: // a function
        token_base_.push_back(t);
        break;
    }
}

auto expression::find(const std::string &name) const -> int {
    const auto it = symbol_index_.find(name);

    return it == symbol_index_.end() ? -1 : it->second;
}

auto expression::insert(const std::string &name, int token) -> int {
    const int index = static_cast<int>(symbols_.size());

    symbols_.push_back({token, 0});
    symbol_index_.emplace(name, index);

    return index;
}

auto expression::set_var(const std::string &name, double value) -> bool {
    const int index = find(name);
    if (error_occur_ || index == -1) {
        return false;
    }

    symbols_[index].value = value;
    return true;
}

auto expression::var(const std::string &name) const -> double {
    const int index = find(name);
    if (error_occur_ || index == -1) {
        return 0;
    }

    return symbols_[index].value;
}
}
