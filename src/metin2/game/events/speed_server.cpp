#include <metin2/game/events/speed_server.h>

#include <metin2/game/app/utils.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <string>
#include <utility>

namespace metin2::game::inline events {

    speed_server_manager::speed_server_manager() {
    }

    speed_server_manager::~speed_server_manager() {
    }

    speed_server_empire_exp::speed_server_empire_exp() {
    }

    speed_server_empire_exp::~speed_server_empire_exp() {
    }

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

    auto speed_server_empire_exp::load_wday_exp_table(int wday, char *str) -> bool {
        std::list<hme> &lst = wday_exp_table_[wday];
        lst.clear();
        char *p, *n;
        const char *delim = " \t\r\n";
        char *t;
        char *h, *m, *e;
        int hour, min, exp;
        metin2::log::debug("Speed server: parsing exp bonus entry {}", str);
        strtok(str, delim);
        p = strtok(nullptr, ";");
        n = strtok(nullptr, ";");
        while (p != nullptr) {
            t = strtok(p, delim);
            e = strtok(nullptr, delim);
            h = strtok(t, ":");
            m = strtok(nullptr, delim);
            if (!str_to_number(hour, h) || !str_to_number(min, m) || !str_to_number(exp, e)) {
                metin2::log::error("Invalid exp bonus entry '{}:{} {}', expected hh:mm exp", h, m, e);
                return false;
            }
            metin2::log::debug("Speed server: exp bonus {} at {}:{}", e, h, m);

            lst.push_back(hme(hour, min, exp));
            p = strtok(n, ";");
            n = strtok(nullptr, ";");
        }
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

        char wday_name[7][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        for (int i = 0; i < 7; i++) {
            fprintf(fp, "%s", wday_name[i]);
            for (std::list<hme>::iterator it = wday_exp_table_[i].begin(); it != wday_exp_table_[i].end(); it++) {
                fprintf(fp, " %d:%d %d;", it->hour, it->min, it->exp);
            }
            fprintf(fp, "\n");
        }

        for (std::map<date, std::list<hme>>::iterator holi_it = holiday_map_.begin(); holi_it != holiday_map_.end(); holi_it++) {
            fprintf(fp, "HOLIDAY %d.%d.%d", holi_it->first.year + 1900, holi_it->first.mon + 1, holi_it->first.day);
            for (std::list<hme>::iterator it = holi_it->second.begin(); it != holi_it->second.end(); it++) {
                fprintf(fp, " %d:%d %d;", it->hour, it->min, it->exp);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
        return true;
    }

    auto speed_server_empire_exp::load_exp_table() -> bool {
        FILE *fp;
        char one_line[256];
        char temp[256];
        const char *delim = " \t\r\n";

        metin2::log::debug("Speed server: loading exp bonus table");
        if (file_name_.empty())
            return false;

        if ((fp = fopen(file_name_.c_str(), "r")) == nullptr)
            return false;

        while (fgets(one_line, 256, fp)) {
            if (one_line[0] == '#')
                continue;

            strlcpy(temp, one_line, sizeof(temp));

            const char *token_string = strtok(one_line, delim);

            if (nullptr == token_string)
                continue;

            TOKEN("SUN") {
                load_wday_exp_table(0, temp);
            }
            else TOKEN("MON") {
                load_wday_exp_table(1, temp);
            }
            else TOKEN("TUE") {
                load_wday_exp_table(2, temp);
            }
            else TOKEN("WED") {
                load_wday_exp_table(3, temp);
            }
            else TOKEN("THU") {
                load_wday_exp_table(4, temp);
            }
            else TOKEN("FRI") {
                load_wday_exp_table(5, temp);
            }
            else TOKEN("SAT") {
                load_wday_exp_table(6, temp);
            }
            else TOKEN("HOLIDAY") {
                std::list<hme> lst;
                lst.clear();
                char *p, *n;
                char *t, *v;
                char *h, *m, *e;
                int hour, min, exp;

                v = strtok(temp, delim);
                v = strtok(nullptr, delim);
                metin2::log::debug("Speed server: holiday {}", v);

                p = strtok(nullptr, ";");
                n = strtok(nullptr, ";");
                while (p != nullptr) {
                    t = strtok(p, delim);
                    e = strtok(nullptr, delim);
                    h = strtok(t, ":");
                    m = strtok(nullptr, delim);
                    if (!str_to_number(hour, h) || !str_to_number(min, m) || !str_to_number(exp, e)) {
                        metin2::log::error("Invalid exp bonus entry '{}:{} {}', expected hh:mm exp", h, m, e);
                        return false;
                    }
                    metin2::log::debug("Speed server: exp bonus {} at {}:{}", e, h, m);

                    lst.push_back(hme(hour, min, exp));
                    p = strtok(n, ";");
                    n = strtok(nullptr, ";");
                }
                int year, mon, day;
                if (!str_to_number(year, strtok(v, ".")) || !str_to_number(mon, strtok(nullptr, ".")) || !str_to_number(day, strtok(nullptr, "."))) {
                    metin2::log::error("Invalid holiday date in exp bonus table");
                    return false;
                }

                metin2::log::debug("Speed server: holiday date {:d}-{:d}-{:d}", year, mon, day);

                holiday_map_.insert(std::pair<date, std::list<hme>>(date(year - 1900, mon - 1, day), lst));
            }
        }

        fclose(fp);

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

    auto speed_server_manager::holiday_exp_table_of_empire(uint8_t empire, date date, bool &is_exist) -> std::list<hme> & {
        return empire_[empire].holiday_exp_table(date, is_exist);
    }

    auto speed_server_empire_exp::holiday_exp_table(date date, bool &is_exist) -> std::list<hme> & {
        std::map<game::events::date, std::list<hme>>::iterator it = holiday_map_.find(date);
        if (it != holiday_map_.end()) {
            is_exist = true;
            return it->second;
        } else {
            is_exist = false;
            metin2::log::debug("Speed server: no holiday entry for {:d}-{:d}-{:d}", date.year, date.mon, date.day);
        }
        return it->second;
    }

    void speed_server_manager::set_holiday_exp_table_of_empire(uint8_t empire, date date, hme hme) {
        empire_[empire].set_holiday_exp_table(date, hme);
    }

    void speed_server_empire_exp::set_holiday_exp_table(date date, hme hme) {
        std::map<game::events::date, std::list<game::events::hme>>::iterator it = holiday_map_.find(date);
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
        std::map<game::events::date, std::list<hme>>::iterator it = holiday_map_.find(date);
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

        std::map<game::events::date, std::list<hme>>::iterator holi_it = holiday_map_.find(date);

        int total_sec = datetime->tm_hour * 3600 + datetime->tm_min * 60 + datetime->tm_sec;

        hme hme;

        if (holi_it != holiday_map_.end()) {
            for (std::list<game::events::hme>::iterator it = holi_it->second.begin();
                it != wday_exp_table_[datetime->tm_wday].end(); it++) {
                if (total_sec < (it->hour * 3600 + it->min * 60)) {
                    hme = *it;
                    break;
                }
            }
        } else {
            for (std::list<game::events::hme>::iterator it = wday_exp_table_[datetime->tm_wday].begin();
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
