#include "check.h"

#include <metin2/game/app/crc32.h>
#include <metin2/game/quest/quest_loader.h>

#include <format>
#include <string>
#include <string_view>

namespace {
    using metin2::game::quest::quest_scripts;

    auto contains(std::string_view text, std::string_view part) -> bool {
        return text.find(part) != std::string_view::npos;
    }

    auto translate(std::string_view source, quest_scripts &out) -> std::string {
        const auto result = metin2::game::quest::translate_quest(source, "test.quest", out);
        return result ? std::string() : result.error();
    }

    auto file(const quest_scripts &scripts, std::string_view dir, std::string_view name) -> std::string {
        const std::string *code = scripts.find(dir, name);
        return code ? *code : std::string("<missing>");
    }

    auto state_number(std::string_view name) -> int {
        return static_cast<int>(metin2::game::get_cr_c32(name.data(), name.size()));
    }
}

TEST(quest_state_table_numbers_states_by_crc) {
    quest_scripts scripts;
    CHECK_EQ(translate(R"(
quest q begin
    state start begin
        when login begin set_state(done) end
    end
    state done begin
    end
end
)",
                 scripts),
        std::string());

    CHECK_EQ(file(scripts, "state", "q"), std::format("q={{[\"start\"]=0,[\"done\"]={}}}", state_number("done")));
    CHECK_EQ(file(scripts, "notarget/login", "q.start"), std::string("set_state ( \"done\" ) "));
}

TEST(quest_when_condition_wraps_the_handler) {
    quest_scripts scripts;
    CHECK_EQ(translate("quest q begin state start begin when 101.kill with pc.level > 10 begin say(1) end end end", scripts), std::string());
    CHECK_EQ(file(scripts, "101/kill", "q.start"), std::string("if pc . level > 10 then say ( 1 )  return end "));
}

TEST(quest_chat_handler_gets_argument_files) {
    quest_scripts scripts;
    CHECK_EQ(translate(R"(quest q begin state start begin
when 20011.chat."Hello" with pc.is_gm() begin say(1) end
end end)",
                 scripts),
        std::string());

    CHECK_EQ(file(scripts, "20011/chat", "q.start.0.arg"), std::string("\"Hello\""));
    CHECK_EQ(file(scripts, "20011/chat", "q.start.0.when"), std::string("return pc . is_gm ( )"));
    CHECK_EQ(file(scripts, "20011/chat", "q.start.0.script"), std::string("say ( 1 ) "));
    CHECK(scripts.has_npc("20011"));
    CHECK(!scripts.has_npc("2001"));
}

TEST(quest_or_shares_a_handler) {
    quest_scripts scripts;
    CHECK_EQ(translate("quest q begin state start begin when login or enter begin say(1) end end end", scripts), std::string());
    CHECK_EQ(file(scripts, "notarget/login", "q.start"), std::string("say ( 1 ) "));
    CHECK_EQ(file(scripts, "notarget/enter", "q.start"), std::string("say ( 1 ) "));
}

TEST(quest_defines_are_substituted) {
    quest_scripts scripts;
    CHECK_EQ(translate(R"(define keeper 20011
define reward = 1066206
quest q begin state start begin
when keeper.chat."Reward" begin pc.give_exp2(reward) end
end end)",
                 scripts),
        std::string());

    CHECK_EQ(file(scripts, "20011/chat", "q.start.0.script"), std::string("pc . give_exp2 ( 1066206 ) "));
}

TEST(quest_functions_and_begin_condition) {
    quest_scripts scripts;
    CHECK_EQ(translate(R"(quest q with pc.level > 5 begin
state start begin
    function twice(a, b) return a + b end
    when login begin q.twice(1, 2) end
end end)",
                 scripts),
        std::string());

    CHECK(contains(file(scripts, "state", "q"), ",twice= function (a,b)return a + b end "));
    CHECK_EQ(file(scripts, "begin_condition", "q"), std::string("return pc . level > 5"));
    CHECK(scripts.called_functions.contains("q.twice"));
}

TEST(quest_strings_keep_their_meaning) {
    quest_scripts scripts;
    CHECK_EQ(translate(R"(quest q begin state start begin when login begin say("a\\b \"c\"") end end end)", scripts), std::string());
    CHECK_EQ(file(scripts, "notarget/login", "q.start"), std::string(R"(say ( "a\\b \"c\"" ) )"));
}

TEST(quest_errors_name_file_and_line) {
    quest_scripts scripts;
    const std::string unknown_state = translate("quest q begin\nstate start begin\nwhen login begin\nset_state(gone)\nend end end", scripts);
    CHECK(contains(unknown_state, "test.quest:4: state name not found : gone"));

    const std::string no_begin = translate("quest q begin\nstate start\nend", scripts);
    CHECK(contains(no_begin, "test.quest:3:"));

    const std::string bad_lua = translate("quest q begin state start begin when login begin x = = 1 end end end", scripts);
    CHECK(contains(bad_lua, "syntax error"));

    const std::string bad_token = translate("quest q begin state start begin when login begin say(\"open) end end end", scripts);
    CHECK(contains(bad_token, "test.quest"));
}
