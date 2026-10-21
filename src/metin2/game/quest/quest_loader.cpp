#include "quest_loader.h"

#include <metin2/game/app/crc32.h>

// Lua 5.0 and its lexer internals are C. IWYU pragma: begin_keep
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "lzio.h"
}
// IWYU pragma: end_keep

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::quest {
    void quest_scripts::add(const std::string &dir, const std::string &name, std::string content) {
        dirs_[dir][name] = std::move(content);
    }

    auto quest_scripts::list(std::string_view dir) const -> std::vector<std::string> {
        std::vector<std::string> names;
        if (const auto it = dirs_.find(dir); it != dirs_.end()) {
            for (const auto &[name, content] : it->second) {
                names.push_back(name);
            }
        }
        return names;
    }

    auto quest_scripts::find(std::string_view dir, std::string_view name) const -> const std::string * {
        const auto it = dirs_.find(dir);
        if (it == dirs_.end()) {
            return nullptr;
        }

        const auto file = it->second.find(std::string(name));
        return file == it->second.end() ? nullptr : &file->second;
    }

    auto quest_scripts::has_npc(std::string_view npc) const -> bool {
        const std::string prefix = std::format("{}/", npc);
        const auto it = dirs_.lower_bound(prefix);
        return it != dirs_.end() && it->first.starts_with(prefix);
    }

    namespace {
        struct token {
            int type = TK_EOS;
            std::string text;
            double number = 0;
            int line = 0;
        };

        struct quest_error {
            int line;
            std::string message;
        };

        struct lex_job {
            const char *data;
            std::size_t size;
            bool consumed = false;
            Mbuffer buffer;
            std::vector<token> *tokens;
        };

        auto read_source(lua_State *, void *data, std::size_t *size) -> const char * {
            auto *job = static_cast<lex_job *>(data);
            if (job->consumed) {
                return nullptr;
            }

            job->consumed = true;
            *size = job->size;
            return job->data;
        }

        // Runs under lua_cpcall: the lexer reports a malformed token with a Lua error, which unwinds to lua_cpcall.
        // Nothing here may own resources across a luaX_lex call.
        auto lex_all(lua_State *lua) -> int {
            auto *job = static_cast<lex_job *>(lua_touserdata(lua, 1));

            ZIO zio;
            luaZ_init(&zio, read_source, job, "quest");

            LexState lex;
            lex.buff = &job->buffer;
            luaX_setinput(lua, &lex, &zio, luaS_new(lua, zname(&zio)));

            for (;;) {
                SemInfo info;
                const int type = luaX_lex(&lex, &info);
                if (type == TK_EOS) {
                    break;
                }

                token t;
                t.type = type;
                t.line = lex.linenumber;
                if (type == TK_NAME || type == TK_STRING) {
                    t.text = getstr(info.ts);
                } else if (type == TK_NUMBER) {
                    t.number = info.r;
                } else if (type < FIRST_RESERVED) {
                    t.text = std::string(1, static_cast<char>(type));
                } else {
                    t.text = luaX_token2str(&lex, type);
                }
                job->tokens->push_back(std::move(t));
            }
            return 0;
        }

        auto tokenize(lua_State *lua, std::string_view source, std::vector<token> &tokens) -> std::expected<void, std::string> {
            lex_job job{};
            job.data = source.data();
            job.size = source.size();
            job.tokens = &tokens;
            luaZ_initbuffer(lua, &job.buffer);

            const int result = lua_cpcall(lua, lex_all, &job);
            luaZ_freebuffer(lua, &job.buffer);

            if (result != 0) {
                std::string message = lua_isstring(lua, -1) ? lua_tostring(lua, -1) : "lexer error";
                lua_pop(lua, 1);
                return std::unexpected(std::move(message));
            }
            return {};
        }

        // Numbers are printed exactly: the old compiler kept six significant digits, so 1066206 became 1066210.
        auto format_number(double value) -> std::string {
            if (std::trunc(value) == value && std::fabs(value) < 1e15) {
                return std::format("{}", static_cast<long long>(value));
            }
            return std::format("{}", value);
        }

        auto quote(std::string_view text) -> std::string {
            std::string out = "\"";
            for (const char c : text) {
                switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\0': out += "\\0"; break;
                default:   out += c;
                }
            }
            return out + '"';
        }

        auto text_of(const token &t) -> std::string {
            switch (t.type) {
            case TK_NUMBER: return format_number(t.number);
            case TK_STRING: return quote(t.text);
            default:        return t.text;
            }
        }

        auto lower(std::string s) -> std::string {
            std::ranges::transform(s, s.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        // Reads tokens the way the old compiler's lexer loop did, including where it put line breaks: a look-ahead
        // moves the lexer's line, so the token taken from it never starts a new output line.
        class token_stream {
          public:
            explicit token_stream(std::vector<token> tokens) : tokens_(std::move(tokens)) {}

            void next() {
                last_line_ = line_;
                if (lookahead_.type != TK_EOS) {
                    current_ = std::exchange(lookahead_, token{});
                } else {
                    current_ = read();
                }
            }

            void peek() { lookahead_ = read(); }
            void set_lookahead(token t) { lookahead_ = std::move(t); }

            auto t() -> token & { return current_; }
            [[nodiscard]] auto ahead() const -> const token & { return lookahead_; }
            [[nodiscard]] auto line_changed() const -> bool { return line_ != last_line_; }
            [[nodiscard]] auto line() const -> int { return line_; }

          private:
            auto read() -> token {
                if (position_ >= tokens_.size()) {
                    token end;
                    end.line = line_;
                    return end;
                }
                token t = tokens_[position_++];
                line_ = t.line;
                return t;
            }

            std::vector<token> tokens_;
            std::size_t position_ = 0;
            token current_;
            token lookahead_;
            int line_ = 1;
            int last_line_ = 1;
        };

        struct arg_script {
            std::string condition;
            std::string argument;
            std::string script;
        };

        // define NAME [=] VALUE lines before "quest" (what pre_qc.py did): later uses of NAME become VALUE.
        auto apply_defines(std::vector<token> &tokens) -> std::vector<token> {
            std::map<std::string, std::vector<token>> defines;
            std::size_t i = 0;

            while (i < tokens.size() && tokens[i].type == TK_NAME && tokens[i].text == "define") {
                if (i + 2 >= tokens.size() || tokens[i + 1].type != TK_NAME) {
                    throw quest_error{tokens[i].line, "define needs a name and a value"};
                }

                const std::string name = tokens[i + 1].text;
                i += 2;
                if (tokens[i].type == '=') {
                    ++i;
                }

                std::vector<token> value;
                if (i < tokens.size() && tokens[i].type == '-') {
                    value.push_back(tokens[i++]);
                }
                if (i >= tokens.size() || (tokens[i].type != TK_NUMBER && tokens[i].type != TK_STRING && tokens[i].type != TK_NAME)) {
                    throw quest_error{tokens[i - 1].line, std::format("define {} needs a value", name)};
                }
                value.push_back(tokens[i++]);
                defines[name] = std::move(value);
            }

            std::vector<token> out;
            for (; i < tokens.size(); ++i) {
                const auto it = tokens[i].type == TK_NAME ? defines.find(tokens[i].text) : defines.end();
                if (it == defines.end()) {
                    out.push_back(std::move(tokens[i]));
                    continue;
                }

                for (token value : it->second) {
                    value.line = tokens[i].line;
                    out.push_back(std::move(value));
                }

                // pre_qc.py replaced text, and the lexer then read "20126.chat" as the number "20126." and "chat".
                if (out.back().type == TK_NUMBER && i + 1 < tokens.size() && tokens[i + 1].type == '.') {
                    ++i;
                }
            }
            return out;
        }

        // The quest state machine of the old compiler (qc.cc), producing the same fragments.
        class translator {
          public:
            translator(std::vector<token> tokens, quest_scripts &out) : lex_(std::move(tokens)), out_(out) {}

            void run() {
                enum class step { start,
                    quest,
                    quest_with_or_begin,
                    state_list,
                    state_name,
                    state_begin,
                    when_list_or_function,
                    when_name,
                    when_with_or_begin,
                    when_body,
                    function_name,
                    function_arg,
                    function_body };

                step ps = step::start;

                while (true) {
                    lex_.next();
                    if (lex_.t().type == TK_EOS) {
                        break;
                    }

                    token &t = lex_.t();

                    switch (ps) {
                    case step::start:
                        if (t.type != TK_QUEST) {
                            fail("must start with 'quest'");
                        }
                        ps = step::quest;
                        break;

                    case step::quest:
                        if (t.type != TK_NAME && t.type != TK_STRING) {
                            fail("quest name must be given");
                        }
                        quest_name_ = t.text;
                        ps = step::quest_with_or_begin;
                        break;

                    case step::quest_with_or_begin:
                        if (t.type == TK_WITH) {
                            start_condition_ = read_condition();
                        }
                        if (lex_.t().type != TK_DO) {
                            fail(std::format("quest doesn't have begin-end clause. ({})", text_of(lex_.t())));
                        }
                        ps = step::state_list;
                        ++nested_;
                        break;

                    case step::state_list:
                        if (t.type == TK_STATE) {
                            ps = step::state_name;
                        } else if (t.type == TK_END) {
                            --nested_;
                            ps = step::start;
                        } else {
                            fail("expecting 'state'");
                        }
                        break;

                    case step::state_name:
                        if (t.type != TK_NAME && t.type != TK_STRING) {
                            fail("state name must be given");
                        }
                        state_name_ = t.text;
                        defined_states_.insert(state_name_);
                        ps = step::state_begin;
                        break;

                    case step::state_begin:
                        if (t.type != TK_DO) {
                            fail("state doesn't have begin-end clause.");
                        }
                        ++nested_;
                        ps = step::when_list_or_function;
                        break;

                    case step::when_list_or_function:
                        if (t.type == TK_WHEN) {
                            ps = step::when_name;
                            when_names_.clear();
                        } else if (t.type == TK_END) {
                            --nested_;
                            ps = step::state_list;
                        } else if (t.type == TK_FUNCTION) {
                            ps = step::function_name;
                        } else {
                            fail("expecting 'when' or 'function'");
                        }
                        break;

                    case step::when_name:
                        read_when_name();
                        ps = lex_.ahead().type == TK_OR ? step::when_name : step::when_with_or_begin;
                        if (ps == step::when_name) {
                            when_names_.emplace_back(when_name_, when_argument_);
                            lex_.next();
                        }
                        break;

                    case step::when_with_or_begin:
                        when_condition_.clear();
                        if (t.type == TK_WITH) {
                            when_condition_ = read_condition();
                        }
                        if (lex_.t().type != TK_DO) {
                            fail(std::format("when doesn't have begin-end clause. ({})", text_of(lex_.t())));
                        }
                        ps = step::when_body;
                        ++nested_;
                        break;

                    case step::when_body:
                        read_when_body();
                        ps = step::when_list_or_function;
                        break;

                    case step::function_name:
                        if (t.type == TK_NAME) {
                            function_name_ = t.text;
                            ps = step::function_arg;
                        }
                        break;

                    case step::function_arg:
                        read_function_args();
                        ps = step::function_body;
                        ++nested_;
                        break;

                    case step::function_body:
                        read_function_body();
                        ps = step::when_list_or_function;
                        break;
                    }
                }

                if (nested_ != 0) {
                    fail("unbalanced begin/end");
                }

                for (const auto &[line, name] : used_states_) {
                    if (!defined_states_.contains(name)) {
                        throw quest_error{line, std::format("state name not found : {}", name)};
                    }
                }

                emit();
            }

          private:
            [[noreturn]] void fail(const std::string &message) { throw quest_error{lex_.line(), message}; }

            // "with" <expression> up to "begin".
            auto read_condition() -> std::string {
                lex_.next();
                std::string condition = text_of(lex_.t());
                lex_.next();
                while (lex_.t().type != TK_DO) {
                    if (lex_.t().type == TK_EOS) {
                        fail("condition without begin");
                    }
                    condition += " " + text_of(lex_.t());
                    lex_.next();
                }
                return condition;
            }

            void read_when_name() {
                token &t = lex_.t();
                if (t.type != TK_NAME && t.type != TK_STRING && t.type != TK_NUMBER) {
                    fail("when name must be given");
                }

                if (t.type == TK_NUMBER) {
                    // The lexer reads "20011.chat" as the number "20011." followed by "chat".
                    when_name_ = std::to_string(static_cast<unsigned int>(t.number));
                    lex_.set_lookahead(token{.type = '.', .text = ".", .line = t.line});
                } else {
                    when_name_ = t.text;
                    lex_.peek();
                }

                when_argument_.clear();
                if (lex_.ahead().type == '.') {
                    lex_.next();
                    when_name_ += '.';
                    lex_.next();
                    const std::string part = text_of(lex_.t());
                    if (part == "target") {
                        when_argument_ = "." + when_name_;
                        when_argument_.pop_back();
                        when_name_ = "target";
                    } else {
                        when_name_ += part;
                    }
                    lex_.peek();
                }

                while (lex_.ahead().type == '.') {
                    lex_.next();
                    lex_.next();
                    when_argument_ += "." + text_of(lex_.t());
                    lex_.peek();
                }
            }

            // Tracks block depth and the functions a body calls, as the old compiler did.
            void track(std::string &call_name, token &previous, bool &registered) {
                const token &t = lex_.t();
                if (t.type == TK_DO || t.type == TK_IF || t.type == TK_BEGIN || t.type == TK_FUNCTION) {
                    ++nested_;
                } else if (t.type == TK_END) {
                    --nested_;
                }

                if (!call_name.empty()) {
                    lex_.peek();
                    if (lex_.ahead().type == '(') {
                        out_.called_functions.insert(call_name);
                        registered = true;
                    }
                    call_name.clear();
                } else if (t.type == '(') {
                    if (!registered && previous.type == TK_NAME) {
                        out_.called_functions.insert(previous.text);
                    }
                    registered = false;
                }

                if (t.type == '.') {
                    lex_.peek();
                    call_name = text_of(previous) + "." + text_of(lex_.ahead());
                }
            }

            void read_when_body() {
                std::ostringstream os;
                int state_check = 0;
                token previous = lex_.t();
                std::string call_name;
                bool registered = false;

                while (true) {
                    if (lex_.t().type == TK_EOS) {
                        fail("when without end");
                    }

                    track(call_name, previous, registered);

                    token &t = lex_.t();
                    if (state_check && --state_check == 0 && (t.type == TK_NAME || t.type == TK_STRING)) {
                        used_states_.emplace(t.line, t.text);
                        t.type = TK_STRING;
                    }

                    if (t.type == TK_NAME && (t.text == "set_state" || t.text == "newstate" || t.text == "setstate")) {
                        state_check = 2;
                    }

                    if (nested_ == 2) {
                        break;
                    }

                    os << text_of(t) << ' ';
                    previous = t;
                    lex_.next();
                    if (lex_.line_changed()) {
                        os << '\n';
                    }
                }

                const std::string body = os.str();
                std::ranges::reverse(when_names_);
                while (true) {
                    if (when_argument_.empty()) {
                        if (when_condition_.empty()) {
                            scripts_[when_name_][state_name_] += body;
                        } else {
                            scripts_[when_name_][state_name_] += "if " + when_condition_ + " then " + body + " return end ";
                        }
                    } else {
                        arg_scripts_[when_name_][state_name_].push_back(arg_script{when_condition_, when_argument_, body});
                    }

                    if (when_names_.empty()) {
                        break;
                    }
                    std::tie(when_name_, when_argument_) = when_names_.back();
                    when_names_.pop_back();
                }
            }

            void read_function_args() {
                if (lex_.t().type != '(') {
                    fail(std::format("function {} needs an argument list", function_name_));
                }

                lex_.next();
                function_args_ = "(";
                if (lex_.t().type != ')') {
                    do {
                        if (lex_.t().type != TK_NAME) {
                            fail(std::format("invalid argument name {} for function {}", text_of(lex_.t()), function_name_));
                        }
                        function_args_ += lex_.t().text;
                        lex_.next();
                        if (lex_.t().type != ')') {
                            function_args_ += ',';
                        }
                    } while (lex_.t().type == ',' && (lex_.next(), true));
                }
                function_args_ += ')';
            }

            void read_function_body() {
                std::ostringstream os;
                token previous = lex_.t();
                if (previous.type == '.') {
                    previous.type = TK_DO;
                }
                std::string call_name;
                bool registered = false;

                while (nested_ >= 3) {
                    if (lex_.t().type == TK_EOS) {
                        fail("function without end");
                    }

                    track(call_name, previous, registered);
                    os << text_of(lex_.t()) << ' ';
                    if (nested_ == 2) {
                        break;
                    }

                    previous = lex_.t();
                    lex_.next();
                    if (lex_.line_changed()) {
                        os << '\n';
                    }
                }

                functions_ += "," + function_name_ + "= function " + function_args_ + os.str();
            }

            // Where the old compiler put a trigger's files: "<npc>/<event>" or "notarget/<event>".
            static auto directory(const std::string &when_name) -> std::string {
                const std::string lowered = lower(when_name);
                const auto dot = when_name.find('.');
                if (dot == std::string::npos) {
                    return "notarget/" + lowered;
                }
                return when_name.substr(0, dot) + "/" + lowered.substr(dot + 1);
            }

            void emit() {
                if (!defined_states_.empty()) {
                    // State numbers are stored with the player's quest progress, so they must not change: the CRC-32 of
                    // the name, moved up past collisions in name order ("start" is always 0 but still takes its slot).
                    std::map<std::string, int> numbers{
                        {"start", 0}
                    };
                    std::set<int> taken;
                    for (const std::string &name : defined_states_) {
                        int number = static_cast<int>(get_cr_c32(name.data(), name.size()));
                        while (taken.contains(number)) {
                            ++number;
                        }
                        taken.insert(number);
                        numbers.emplace(name, number);
                    }

                    std::string table = quest_name_ + "={[\"start\"]=0";
                    for (const std::string &name : defined_states_) {
                        if (name != "start") {
                            table += std::format(",[\"{}\"]={}", name, numbers[name]);
                        }
                    }
                    out_.add("state", quest_name_, table + functions_ + "}");
                }

                if (!start_condition_.empty()) {
                    out_.add("begin_condition", quest_name_, "return " + start_condition_);
                }

                for (const auto &[when_name, states] : arg_scripts_) {
                    const std::string dir = directory(when_name);
                    for (const auto &[state, scripts] : states) {
                        for (std::size_t i = 0; i < scripts.size(); ++i) {
                            const std::string base = std::format("{}.{}.{}.", quest_name_, state, i);
                            out_.add(dir, base + "script", scripts[i].script);
                            out_.add(dir, base + "when", scripts[i].condition.empty() ? "" : "return " + scripts[i].condition);
                            out_.add(dir, base + "arg", scripts[i].argument.substr(1));
                        }
                    }
                }

                for (const auto &[when_name, states] : scripts_) {
                    const std::string dir = directory(when_name);
                    for (const auto &[state, code] : states) {
                        out_.add(dir, quest_name_ + "." + state, code);
                    }
                }
            }

            token_stream lex_;
            quest_scripts &out_;
            int nested_ = 0;

            std::string quest_name_;
            std::string start_condition_;
            std::string state_name_;
            std::string when_name_;
            std::string when_condition_;
            std::string when_argument_;
            std::string function_name_;
            std::string function_args_;
            std::string functions_;

            std::set<std::string> defined_states_;
            std::multimap<int, std::string> used_states_;
            std::vector<std::pair<std::string, std::string>> when_names_;
            std::map<std::string, std::map<std::string, std::string>> scripts_;
            std::map<std::string, std::map<std::string, std::vector<arg_script>>> arg_scripts_;
        };

        struct lua_closer {
            void operator()(lua_State *lua) const { lua_close(lua); }
        };

        auto check_syntax(lua_State *lua, const quest_scripts &scripts, const std::string &file) -> std::expected<void, std::string> {
            for (const auto &[dir, files] : scripts.entries()) {
                for (const auto &[name, code] : files) {
                    if (name.ends_with(".arg")) {
                        continue;
                    }

                    const std::string chunk = std::format("{} ({}/{})", file, dir, name);
                    if (luaL_loadbuffer(lua, code.data(), code.size(), chunk.c_str()) != 0) {
                        std::string message = lua_tostring(lua, -1);
                        lua_pop(lua, 1);
                        return std::unexpected(std::format("syntax error: {}", message));
                    }
                    lua_pop(lua, 1);
                }
            }
            return {};
        }
    }

    auto translate_quest(std::string_view source, const std::string &file, quest_scripts &out) -> std::expected<void, std::string> {
        const std::unique_ptr<lua_State, lua_closer> lua(lua_open());

        std::vector<token> tokens;
        if (auto lexed = tokenize(lua.get(), source, tokens); !lexed) {
            return std::unexpected(std::format("{}: {}", file, lexed.error()));
        }

        quest_scripts translated;
        try {
            translator(apply_defines(tokens), translated).run();
        } catch (const quest_error &error) {
            return std::unexpected(std::format("{}:{}: {}", file, error.line, error.message));
        }

        if (auto checked = check_syntax(lua.get(), translated, file); !checked) {
            return std::unexpected(checked.error());
        }

        for (const auto &[dir, files] : translated.entries()) {
            for (const auto &[name, code] : files) {
                out.add(dir, name, code);
            }
        }
        out.called_functions.insert(translated.called_functions.begin(), translated.called_functions.end());
        return {};
    }

    auto load_quest_scripts(const std::filesystem::path &dir) -> std::expected<quest_scripts, std::string> {
        std::vector<std::filesystem::path> files;
        std::error_code error;
        for (std::filesystem::directory_iterator it(dir, error), end; !error && it != end; it.increment(error)) {
            if (it->is_regular_file() && it->path().extension() == ".quest") {
                files.push_back(it->path());
            }
        }
        if (error) {
            return std::unexpected(std::format("cannot read {}: {}", dir.string(), error.message()));
        }
        std::ranges::sort(files);

        quest_scripts scripts;
        std::string errors;
        for (const auto &path : files) {
            std::ifstream in(path, std::ios::binary);
            const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (auto result = translate_quest(source, path.filename().string(), scripts); !result) {
                errors += result.error() + "\n";
            }
        }

        if (!errors.empty()) {
            return std::unexpected(errors);
        }
        return scripts;
    }
}
