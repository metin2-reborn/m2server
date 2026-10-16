#include <metin2/game/events/speed_server.h>

#include <metin2/game/text/locale_service.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/limits.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace metin2::game::inline events {

    namespace {

        constexpr std::array<std::string_view, 7> wday_names = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

        auto iequals(std::string_view a, std::string_view b) -> bool {
            return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
        }

        auto next_word(std::string_view &text) -> std::string_view {
            constexpr std::string_view whitespace = " \t\r\n";
            const auto begin = text.find_first_not_of(whitespace);
            if (begin == std::string_view::npos) {
                text = {};
                return {};
            }
            text.remove_prefix(begin);
            const auto word = text.substr(0, text.find_first_of(whitespace));
            text.remove_prefix(word.size());
            return word;
        }

        auto parse_hme_list(std::string_view text, std::list<hme> &lst) -> bool {
            for (const auto part : text | std::views::split(';')) {
                std::string_view entry(part.begin(), part.end());
                const auto time = next_word(entry);
                if (time.empty())
                    continue;

                const auto exp_text = next_word(entry);
                const auto colon = time.find(':');
                const auto hour_text = time.substr(0, colon);
                const auto min_text = colon == std::string_view::npos ? std::string_view{} : time.substr(colon + 1);

                const auto hour = parse<int>(hour_text);
                const auto min = parse<int>(min_text);
                const auto exp = parse<int>(exp_text);
                if (!hour || !min || !exp) {
                    metin2::log::error("Invalid exp bonus entry '{}:{} {}', expected hh:mm exp", hour_text, min_text, exp_text);
                    return false;
                }
                metin2::log::debug("Speed server: exp bonus {} at {}:{}", exp_text, hour_text, min_text);

                lst.emplace_back(*hour, *min, *exp);
            }
            return true;
        }

        auto parse_date(std::string_view text) -> std::optional<date> {
            std::array<int, 3> fields{};
            std::size_t count = 0;
            for (const auto part : text | std::views::split('.')) {
                if (part.empty())
                    continue;
                if (count == fields.size())
                    break;
                const auto value = parse<int>(std::string_view(part.begin(), part.end()));
                if (!value)
                    return std::nullopt;
                fields[count++] = *value;
            }
            if (count != fields.size())
                return std::nullopt;
            return date(fields[0], fields[1], fields[2]);
        }
    }

    speed_server_manager::speed_server_manager() = default;

    speed_server_manager::~speed_server_manager() = default;

    speed_server_empire_exp::speed_server_empire_exp() = default;

    speed_server_empire_exp::~speed_server_empire_exp() = default;

    auto speed_server_manager::initialize() -> bool {
        for (int i = 1; i < EMPIRE_MAX_NUM; i++) {
            if (!empire_[i].initialize(i)) {
                metin2::log::error("Failed to initialize exp bonus table for empire {:d}", i);
                return false;
            }
        }
        return true;
    }

    auto speed_server_empire_exp::initialize(uint8_t e) -> bool {
        empire_ = e;
        metin2::log::info("Speed server: loading exp bonus table for empire {:d}", empire_);
        file_name_ = locale_service_get_base_path() + "/exp_bonus_table_" + std::to_string(empire_) + ".txt";

        for (int i = 1; i < 6; i++) {
            wday_exp_table_[i].push_back(hme(18, 0, 50));
            wday_exp_table_[i].push_back(hme(24, 0, 100));
        }

        wday_exp_table_[0].push_back(hme(18, 0, 100));
        wday_exp_table_[0].push_back(hme(24, 0, 150));
        wday_exp_table_[6].push_back(hme(18, 0, 100));
        wday_exp_table_[6].push_back(hme(24, 0, 150));

        load_exp_table();
        return true;
    }

    auto speed_server_empire_exp::write_exp_table() -> bool {
        FILE *fp;

        metin2::log::debug("Speed server: writing exp bonus table");
        if (file_name_.empty())
            return false;

        if ((fp = fopen(file_name_.c_str(), "w")) == nullptr) {
            return false;
        }

        for (int i = 0; i < 7; i++) {
            fprintf(fp, "%s", wday_names[i].data());
            for (auto it = wday_exp_table_[i].begin(); it != wday_exp_table_[i].end(); it++) {
                fprintf(fp, " %d:%d %d;", it->hour, it->min, it->exp);
            }
            fprintf(fp, "\n");
        }

        for (auto holi_it = holiday_map_.begin(); holi_it != holiday_map_.end(); holi_it++) {
            fprintf(fp, "HOLIDAY %d.%d.%d", holi_it->first.year + 1900, holi_it->first.mon + 1, holi_it->first.day);
            for (auto it = holi_it->second.begin(); it != holi_it->second.end(); it++) {
                fprintf(fp, " %d:%d %d;", it->hour, it->min, it->exp);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
        return true;
    }

    auto speed_server_empire_exp::load_exp_table() -> bool {
        metin2::log::debug("Speed server: loading exp bonus table");
        if (file_name_.empty())
            return false;

        std::ifstream file(file_name_);
        if (!file)
            return false;

        std::string line;
        while (std::getline(file, line)) {
            if (line.starts_with('#'))
                continue;

            std::string_view rest = line;
            const auto keyword = next_word(rest);
            if (keyword.empty())
                continue;

            if (iequals(keyword, "HOLIDAY")) {
                const auto date_text = next_word(rest);
                metin2::log::debug("Speed server: holiday {}", date_text);

                std::list<hme> lst;
                if (!parse_hme_list(rest, lst))
                    return false;

                const auto holiday = parse_date(date_text);
                if (!holiday) {
                    metin2::log::error("Invalid holiday date in exp bonus table");
                    return false;
                }

                metin2::log::debug("Speed server: holiday date {:d}-{:d}-{:d}", holiday->year, holiday->mon, holiday->day);

                holiday_map_.emplace(date(holiday->year - 1900, holiday->mon - 1, holiday->day), std::move(lst));
            } else if (const auto it = std::ranges::find_if(wday_names, [&](std::string_view name) { return iequals(keyword, name); });
                it != wday_names.end()) {
                auto &lst = wday_exp_table_[it - wday_names.begin()];
                lst.clear();
                parse_hme_list(rest, lst);
            }
        }

        return true;
    }

    auto speed_server_manager::wday_exp_table_of_empire(uint8_t empire, int wday) -> std::list<hme> & {
        return empire_[empire].wday_exp_table(wday);
    }

    auto speed_server_empire_exp::wday_exp_table(int wday) -> std::list<hme> & {
        return wday_exp_table_[wday];
    }

    void speed_server_manager::set_wday_exp_table_of_empire(uint8_t empire, int wday, hme hme) {
        empire_[empire].set_wday_exp_table(wday, hme);
    }

    void speed_server_empire_exp::set_wday_exp_table(int wday, hme hme) {
        wday_exp_table_[wday].push_back(hme);
        write_exp_table();
    }

    void speed_server_manager::init_wday_exp_table_of_empire(uint8_t empire, int wday) {
        if (empire > EMPIRE_MAX_NUM) {
            metin2::log::error("Invalid empire {:d}", empire);
            return;
        }

        empire_[empire].init_wday_exp_table(wday);
    }

    void speed_server_empire_exp::init_wday_exp_table(int wday) {
        wday_exp_table_[wday].clear();
    }

    auto speed_server_manager::holiday_exp_table_of_empire(uint8_t empire, date date, bool &is_exist) -> std::list<hme> {
        return empire_[empire].holiday_exp_table(date, is_exist);
    }

    auto speed_server_empire_exp::holiday_exp_table(date date, bool &is_exist) -> std::list<hme> {
        auto it = holiday_map_.find(date);
        is_exist = it != holiday_map_.end();
        if (!is_exist) {
            metin2::log::debug("Speed server: no holiday entry for {:d}-{:d}-{:d}", date.year, date.mon, date.day);
            return {};
        }
        return it->second;
    }

    void speed_server_manager::set_holiday_exp_table_of_empire(uint8_t empire, date date, hme hme) {
        empire_[empire].set_holiday_exp_table(date, hme);
    }

    void speed_server_empire_exp::set_holiday_exp_table(date date, hme hme) {
        auto it = holiday_map_.find(date);
        if (it != holiday_map_.end()) {
            it->second.push_back(hme);
        }
        write_exp_table();
    }

    void speed_server_manager::init_holiday_exp_table_of_empire(uint8_t empire, date date) {
        if (empire > EMPIRE_MAX_NUM) {
            metin2::log::error("Invalid empire {:d}", empire);
            return;
        }

        empire_[empire].init_holiday_exp_table(date);
    }

    void speed_server_empire_exp::init_holiday_exp_table(date date) {
        metin2::log::debug("Speed server: initializing holiday exp table");
        auto it = holiday_map_.find(date);
        if (it == holiday_map_.end()) {
            std::list<hme> lst;
            holiday_map_.insert(std::pair<game::events::date, std::list<hme>>(date, lst));
        } else {
            it->second.clear();
        }
    }

    auto speed_server_manager::current_exp_priv_of_empire(uint8_t empire, int &duration, bool &is_change) -> hme {
        return empire_[empire].current_exp_priv(duration, is_change);
    }

    auto speed_server_empire_exp::current_exp_priv(int &duration, bool &is_change) -> hme {
        struct tm *datetime;
        time_t t;
        t = time(nullptr);
        datetime = localtime(&t);

        date date(datetime->tm_year, datetime->tm_mon,
            datetime->tm_mday);

        auto holi_it = holiday_map_.find(date);

        int total_sec = datetime->tm_hour * 3600 + datetime->tm_min * 60 + datetime->tm_sec;

        hme hme;

        if (holi_it != holiday_map_.end()) {
            for (auto it = holi_it->second.begin();
                it != holi_it->second.end(); it++) {
                if (total_sec < (it->hour * 3600 + it->min * 60)) {
                    hme = *it;
                    break;
                }
            }
        } else {
            for (auto it = wday_exp_table_[datetime->tm_wday].begin();
                it != wday_exp_table_[datetime->tm_wday].end(); it++) {
                if (total_sec < (it->hour * 3600 + it->min * 60)) {
                    hme = *it;
                    break;
                }
            }
        }

        duration = hme.hour * 3600 + hme.min * 60 - total_sec;

        is_change = !(hme == current_hme_);
        current_hme_ = hme;

        return hme;
    }
}
