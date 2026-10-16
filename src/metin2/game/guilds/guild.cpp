#include <metin2/config/config.h>
#include <metin2/game/guilds/guild.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <sys/types.h>

namespace metin2::game::inline guilds {

    guild_member::guild_member(uint32_t pid, uint8_t grade, uint8_t is_general, uint8_t job, uint8_t level, uint32_t offer_exp, char *name)
        : pid(pid), grade(grade), is_general(is_general), job(job), level(level), offer_exp(offer_exp), name(name) {}

    void send_guild_name_to_all(uint32_t guild_id, const char *guild_name) {
        packet_gc_guild p;
        p.header = HEADER_GC_GUILD;
        p.subheader = GUILD_SUBHEADER_GC_GUILD_NAME;
        p.size = sizeof(p) + sizeof(uint32_t) + GUILD_NAME_MAX_LEN;

        character_manager::instance().for_each_pc([&p, guild_id, guild_name](character *ch) {
            desc *d = ch->get_desc();

            if (d) {
                d->buffered_packet(&p, sizeof(p));
                d->buffered_packet(&guild_id, sizeof(guild_id));
                d->packet(guild_name, GUILD_NAME_MAX_LEN);
            }
        });
    }

    guild::guild(guild_create_parameter &cp) {
        initialize();

        general_count_ = 0;

        member_count_bonus_ = 0;

        strlcpy(data_.name, cp.name, sizeof(data_.name));
        data_.master_pid = cp.master->player_id();
        strlcpy(data_.grade_array[0].grade_name, lc_text("길드장"), sizeof(data_.grade_array[0].grade_name));
        data_.grade_array[0].auth_flag = GUILD_AUTH_ADD_MEMBER | GUILD_AUTH_REMOVE_MEMBER | GUILD_AUTH_NOTICE | GUILD_AUTH_USE_SKILL;

        for (int i = 1; i < GUILD_GRADE_COUNT; ++i) {
            strlcpy(data_.grade_array[i].grade_name, lc_text("길드원"), sizeof(data_.grade_array[i].grade_name));
            data_.grade_array[i].auth_flag = 0;
        }

        std::unique_ptr<sql_msg> pmsg(db_manager::instance().direct_query(sql::format(
            "INSERT INTO guild{}(name, master, sp, level, exp, skill_point, skill) "
            "VALUES({}, {}, 1000, 1, 0, 0, '\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0')",
            sql::raw(get_table_postfix()), data_.name, data_.master_pid)));

        data_.guild_id = pmsg->get()->insert_id;

        for (int i = 0; i < GUILD_GRADE_COUNT; ++i) {
            db_manager::instance().query(sql::format("INSERT INTO guild_grade{} VALUES({}, {}, {}, {})",
                sql::raw(get_table_postfix()),
                data_.guild_id,
                i + 1,
                data_.grade_array[i].grade_name,
                data_.grade_array[i].auth_flag));
        }

        compute_guild_points();
        data_.power = data_.max_power;
        data_.ladder_point = 0;
        db_clientdesc->db_packet(HEADER_GD_GUILD_CREATE, 0, &data_.guild_id, sizeof(uint32_t));

        packet_guild_skill_update guild_skill;
        guild_skill.guild_id = data_.guild_id;
        guild_skill.amount = 0;
        guild_skill.skill_point = 0;
        memset(guild_skill.skill_levels, 0, GUILD_SKILL_COUNT);

        db_clientdesc->db_packet(HEADER_GD_GUILD_SKILL_UPDATE, 0, &guild_skill, sizeof(guild_skill));

        send_guild_name_to_all(id(), name());
        request_add_member(cp.master, GUILD_LEADER_GRADE);
    }

    void guild::initialize() {
        memset(&data_, 0, sizeof(data_));
        data_.level = 1;

        for (int i = 0; i < GUILD_SKILL_COUNT; ++i)
            skill_usable_[i] = true;

        member_count_bonus_ = 0;
    }

    guild::~guild() = default;

    void guild::request_add_member(character *ch, int grade) {
        if (ch->get_guild())
            return;

        packet_gd_guild_add_member gd;

        if (member_.find(ch->player_id()) != member_.end()) {
            metin2::log::error("Player {} (pid {:d}) is already a guild member", ch->name(), ch->player_id());
            return;
        }

        gd.pid = ch->player_id();
        gd.guild = id();
        gd.grade = grade;

        db_clientdesc->db_packet(HEADER_GD_GUILD_ADD_MEMBER, 0, &gd, sizeof(packet_gd_guild_add_member));
    }

    void guild::add_member(packet_dg_guild_member *p) {
        guild_member_container::iterator it;

        if ((it = member_.find(p->pid)) == member_.end())
            member_.insert(std::make_pair(p->pid, guild_member(p->pid, p->grade, p->is_general, p->job, p->level, p->offer, p->name)));
        else {
            guild_member &r_gm = it->second;
            r_gm.pid = p->pid;
            r_gm.grade = p->grade;
            r_gm.job = p->job;
            r_gm.offer_exp = p->offer;
            r_gm.is_general = p->is_general;
        }

        guild_manager::instance().link(p->pid, this);

        send_list_one_to_all(p->pid);

        character *ch = character_manager::instance().find_by_pid(p->pid);

        metin2::log::info("Guild: added member pid {:d}, grade {:d}, job {:d}, level {:d}, offer {:d}, name {} ({})",
            p->pid, p->grade, p->job, p->level, p->offer, p->name, static_cast<const void *>(ch));

        if (ch)
            login_member(ch);
        else
            p2p_login_member(p->pid);
    }

    auto guild::request_remove_member(uint32_t pid) -> bool {
        guild_member_container::iterator it;

        if ((it = member_.find(pid)) == member_.end())
            return false;

        if (it->second.grade == GUILD_LEADER_GRADE)
            return false;

        packet_guild gd_guild;

        gd_guild.guild = id();
        gd_guild.info = pid;

        db_clientdesc->db_packet(HEADER_GD_GUILD_REMOVE_MEMBER, 0, &gd_guild, sizeof(packet_guild));
        return true;
    }

    auto guild::remove_member(uint32_t pid) -> bool {
        guild_member_container::iterator it;

        if ((it = member_.find(pid)) == member_.end())
            return false;

        if (it->second.grade == GUILD_LEADER_GRADE)
            return false;

        if (it->second.is_general)
            general_count_--;

        member_.erase(it);
        send_online_remove_one_packet(pid);

        guild_manager::instance().unlink(pid);

        character *ch = character_manager::instance().find_by_pid(pid);

        if (ch) {
            member_online_.erase(ch);
            ch->set_guild(nullptr);
        }

        return true;
    }

    void guild::p2p_login_member(uint32_t pid) {
        if (member_.find(pid) == member_.end()) {
            metin2::log::warn("Member login for pid {:d}, who is not in the guild", pid);
            return;
        }

        member_p2p_online_.insert(pid);

        guild_member_online_container::iterator it;

        for (it = member_online_.begin(); it != member_online_.end(); ++it)
            send_login_packet(*it, pid);
    }

    void guild::login_member(character *ch) {
        if (member_.find(ch->player_id()) == member_.end()) {
            metin2::log::warn("Player {} (pid {:d}) logged in but is not in the guild", ch->name(), ch->player_id());
            return;
        }

        ch->set_guild(this);

        guild_member_online_container::iterator it;

        for (it = member_online_.begin(); it != member_online_.end(); ++it)
            send_login_packet(*it, ch);

        member_online_.insert(ch);

        send_all_grade_packet(ch);
        send_guild_info_packet(ch);
        send_list_packet(ch);
        send_skill_info_packet(ch);
        send_enemy_guild(ch);
    }

    void guild::p2p_logout_member(uint32_t pid) {
        if (member_.find(pid) == member_.end()) {
            metin2::log::warn("Member logout for pid {:d}, who is not in the guild", pid);
            return;
        }

        member_p2p_online_.erase(pid);

        guild_member_online_container::iterator it;
        for (it = member_online_.begin(); it != member_online_.end(); ++it) {
            send_logout_packet(*it, pid);
        }
    }

    void guild::logout_member(character *ch) {
        if (member_.find(ch->player_id()) == member_.end()) {
            metin2::log::warn("Player {} (pid {:d}) logged out but is not in the guild", ch->name(), ch->player_id());
            return;
        }

        member_online_.erase(ch);

        guild_member_online_container::iterator it;
        for (it = member_online_.begin(); it != member_online_.end(); ++it) {
            send_logout_packet(*it, ch);
        }
    }

    void guild::send_online_remove_one_packet(uint32_t pid) {
        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 4;
        pack.subheader = GUILD_SUBHEADER_GC_REMOVE;

        temp_buffer buf;
        buf.write(&pack, sizeof(pack));
        buf.write(&pid, sizeof(pid));

        guild_member_online_container::iterator it;

        for (it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();

            if (d)
                d->packet(buf.read_peek(), buf.size());
        }
    }

    void guild::send_all_grade_packet(character *ch) {
        desc *d = ch->get_desc();
        if (!d)
            return;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 1 + GUILD_GRADE_COUNT * (sizeof(guild_grade) + 1);
        pack.subheader = GUILD_SUBHEADER_GC_GRADE;

        temp_buffer buf;

        buf.write(&pack, sizeof(pack));
        uint8_t n = 15;
        buf.write(&n, 1);

        for (int i = 0; i < GUILD_GRADE_COUNT; i++) {
            uint8_t j = i + 1;
            buf.write(&j, 1);
            buf.write(&data_.grade_array[i], sizeof(guild_grade));
        }

        d->packet(buf.read_peek(), buf.size());
    }

    void guild::send_list_one_to_all(uint32_t pid) {

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(packet_gc_guild);
        pack.subheader = GUILD_SUBHEADER_GC_LIST;

        pack.size += sizeof(guild_member_packet_data);

        char c[CHARACTER_NAME_MAX_LEN + 1];
        memset(c, 0, sizeof(c));

        auto cit = member_.find(pid);
        if (cit == member_.end())
            return;

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();
            if (!d)
                continue;

            temp_buffer buf;

            buf.write(&pack, sizeof(pack));

            cit->second.dummy = 1;

            buf.write(&(cit->second), sizeof(uint32_t) * 3 + 1);
            buf.write(cit->second.name.c_str(), cit->second.name.length());
            buf.write(c, CHARACTER_NAME_MAX_LEN + 1 - cit->second.name.length());
            d->packet(buf.read_peek(), buf.size());
        }
    }

    void guild::send_list_packet(character *ch) {
        desc *d;
        if (!(d = ch->get_desc()))
            return;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(packet_gc_guild);
        pack.subheader = GUILD_SUBHEADER_GC_LIST;

        pack.size += sizeof(guild_member_packet_data) * member_.size();

        temp_buffer buf;

        buf.write(&pack, sizeof(pack));

        char c[CHARACTER_NAME_MAX_LEN + 1];

        for (auto it = member_.begin(); it != member_.end(); ++it) {
            it->second.dummy = 1;

            buf.write(&(it->second), sizeof(uint32_t) * 3 + 1);

            strlcpy(c, it->second.name.c_str(), std::min<int>(sizeof(c), it->second.name.length() + 1));

            buf.write(c, CHARACTER_NAME_MAX_LEN + 1);

            if (metin2::config::test_server())
                metin2::log::debug("Guild: sending member {} (job {:d})", it->second.name, it->second.job);
        }

        d->packet(buf.read_peek(), buf.size());

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            send_login_packet(ch, *it);
        }

        for (auto it = member_p2p_online_.begin(); it != member_p2p_online_.end(); ++it) {
            send_login_packet(ch, *it);
        }
    }

    void guild::send_login_packet(character *ch, character *ch_login) {
        send_login_packet(ch, ch_login->player_id());
    }

    void guild::send_login_packet(character *ch, uint32_t pid) {
        if (!ch->get_desc())
            return;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 4;
        pack.subheader = GUILD_SUBHEADER_GC_LOGIN;

        temp_buffer buf;

        buf.write(&pack, sizeof(pack));

        buf.write(&pid, 4);

        ch->get_desc()->packet(buf.read_peek(), buf.size());
    }

    void guild::send_logout_packet(character *ch, character *ch_logout) {
        send_logout_packet(ch, ch_logout->player_id());
    }

    void guild::send_logout_packet(character *ch, uint32_t pid) {
        if (!ch->get_desc())
            return;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 4;
        pack.subheader = GUILD_SUBHEADER_GC_LOGOUT;

        temp_buffer buf;

        buf.write(&pack, sizeof(pack));
        buf.write(&pid, 4);

        ch->get_desc()->packet(buf.read_peek(), buf.size());
    }

    void guild::load_guild_member_data(sql_msg &msg) {
        if (msg.get()->num_rows == 0)
            return;

        general_count_ = 0;

        member_.clear();

        for (uint i = 0; i < msg.get()->num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

            uint32_t pid = strtoul(row[0], (char **)nullptr, 10);
            auto grade = (uint8_t)strtoul(row[1], (char **)nullptr, 10);
            uint8_t is_general = 0;

            if (row[2] && *row[2] == '1')
                is_general = 1;

            uint32_t offer = strtoul(row[3], (char **)nullptr, 10);
            auto level = (uint8_t)strtoul(row[4], (char **)nullptr, 10);
            auto job = (uint8_t)strtoul(row[5], (char **)nullptr, 10);
            char *name = row[6];

            if (is_general)
                general_count_++;

            member_.insert(std::make_pair(pid, guild_member(pid, grade, is_general, job, level, offer, name)));
            guild_manager::instance().link(pid, this);
        }
    }

    void guild::load_guild_grade_data(sql_msg &msg) {
        for (uint i = 0; i < msg.get()->num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());
            uint8_t grade = 0;
            str_to_number(grade, row[0]);
            char *name = row[1];
            uint32_t auth = strtoul(row[2], nullptr, 10);

            if (grade >= 1 && grade <= 15) {
                strlcpy(data_.grade_array[grade - 1].grade_name, name, sizeof(data_.grade_array[grade - 1].grade_name));
                data_.grade_array[grade - 1].auth_flag = auth;
            }
        }
    }
    void guild::load_guild_data(sql_msg &msg) {
        if (msg.get()->num_rows == 0) {
            metin2::log::error("Guild data not found, query: {}", msg.query);
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());
        data_.master_pid = strtoul(row[0], (char **)nullptr, 10);
        data_.level = (uint8_t)strtoul(row[1], (char **)nullptr, 10);
        data_.exp = strtoul(row[2], (char **)nullptr, 10);
        strlcpy(data_.name, row[3], sizeof(data_.name));

        data_.skill_point = (uint8_t)strtoul(row[4], (char **)nullptr, 10);
        if (row[5])
            std::memcpy(data_.aby_skill, row[5], sizeof(uint8_t) * GUILD_SKILL_COUNT);
        else
            memset(data_.aby_skill, 0, sizeof(uint8_t) * GUILD_SKILL_COUNT);

        data_.power = std::max<int>(0, strtoul(row[6], (char **)nullptr, 10));

        str_to_number(data_.ladder_point, row[7]);

        if (data_.ladder_point < 0)
            data_.ladder_point = 0;

        str_to_number(data_.win, row[8]);
        str_to_number(data_.draw, row[9]);
        str_to_number(data_.loss, row[10]);
        str_to_number(data_.gold, row[11]);

        compute_guild_points();
    }

    void guild::load(uint32_t guild_id) {
        initialize();

        data_.guild_id = guild_id;

        db_manager::instance().func_query([guild_id](sql_msg &msg) {
            if (guild *g = guild_manager::instance().find_guild(guild_id))
                g->load_guild_data(msg);
        },
            sql::format("SELECT master, level, exp, name, skill_point, skill, sp, ladder_point, win, draw, loss, gold FROM guild{} WHERE id = {}", sql::raw(get_table_postfix()), data_.guild_id));

        metin2::log::info("Guild: loading guild {} (id {:d})", data_.name, guild_id);

        db_manager::instance().func_query([guild_id](sql_msg &msg) {
            if (guild *g = guild_manager::instance().find_guild(guild_id))
                g->load_guild_grade_data(msg);
        },
            sql::format("SELECT grade, name, auth+0 FROM guild_grade{} WHERE guild_id = {}", sql::raw(get_table_postfix()), data_.guild_id));

        db_manager::instance().func_query([guild_id](sql_msg &msg) {
            if (guild *g = guild_manager::instance().find_guild(guild_id))
                g->load_guild_member_data(msg);
        },
            sql::format("SELECT pid, grade, is_general, offer, level, job, name FROM guild_member{}, player{} WHERE guild_id = {} and pid = id", sql::raw(get_table_postfix()), sql::raw(get_table_postfix()), guild_id));
    }

    void guild::save_level() {
        db_manager::instance().query(sql::format("UPDATE guild{} SET level={}, exp={}, skill_point={} WHERE id = {}", sql::raw(get_table_postfix()), data_.level, data_.exp, data_.skill_point, data_.guild_id));
    }

    void guild::send_db_skill_update(int amount) {
        packet_guild_skill_update guild_skill;
        guild_skill.guild_id = data_.guild_id;
        guild_skill.amount = amount;
        guild_skill.skill_point = data_.skill_point;
        std::memcpy(guild_skill.skill_levels, data_.aby_skill, sizeof(uint8_t) * GUILD_SKILL_COUNT);

        db_clientdesc->db_packet(HEADER_GD_GUILD_SKILL_UPDATE, 0, &guild_skill, sizeof(guild_skill));
    }

    void guild::save_skill() {
        const std::string_view skill(reinterpret_cast<const char *>(data_.aby_skill), sizeof(data_.aby_skill));
        db_manager::instance().query(sql::format("UPDATE guild{} SET sp = {}, skill_point={}, skill={} WHERE id = {}",
            sql::raw(get_table_postfix()), data_.power, data_.skill_point, skill, data_.guild_id));
    }

    auto guild::member(uint32_t pid) -> guild_member * {
        auto it = member_.find(pid);
        if (it == member_.end())
            return nullptr;

        return &it->second;
    }

    auto guild::member_pid(const std::string &name_value) -> uint32_t {
        for (auto iter = member_.begin();
            iter != member_.end(); iter++) {
            if (iter->second.name == name_value) return iter->first;
        }

        return 0;
    }

    void guild::p2p_update_grade(sql_msg &msg) {
        if (msg.get()->num_rows) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

            int grade = 0;
            const char *name = row[1];
            int auth = 0;

            str_to_number(grade, row[0]);
            str_to_number(auth, row[2]);

            if (grade <= 0)
                return;

            grade--;

            if (0 != strcmp(data_.grade_array[grade].grade_name, name)) {
                strlcpy(data_.grade_array[grade].grade_name, name, sizeof(data_.grade_array[grade].grade_name));

                packet_gc_guild pack;

                pack.header = HEADER_GC_GUILD;
                pack.size = sizeof(pack);
                pack.subheader = GUILD_SUBHEADER_GC_GRADE_NAME;

                one_grade_name_packet pack2;

                pack.size += sizeof(pack2);
                pack2.grade = grade + 1;
                strlcpy(pack2.grade_name, name, sizeof(pack2.grade_name));

                temp_buffer buf;

                buf.write(&pack, sizeof(pack));
                buf.write(&pack2, sizeof(pack2));

                for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
                    desc *d = (*it)->get_desc();

                    if (d)
                        d->packet(buf.read_peek(), buf.size());
                }
            }

            if (data_.grade_array[grade].auth_flag != auth) {
                data_.grade_array[grade].auth_flag = auth;

                packet_gc_guild pack;
                pack.header = HEADER_GC_GUILD;
                pack.size = sizeof(pack);
                pack.subheader = GUILD_SUBHEADER_GC_GRADE_AUTH;

                one_grade_auth_packet pack2;
                pack.size += sizeof(pack2);
                pack2.grade = grade + 1;
                pack2.auth = auth;

                temp_buffer buf;
                buf.write(&pack, sizeof(pack));
                buf.write(&pack2, sizeof(pack2));

                for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
                    desc *d = (*it)->get_desc();
                    if (d) {
                        d->packet(buf.read_peek(), buf.size());
                    }
                }
            }
        }
    }

    void guild::p2p_change_grade(uint8_t grade) {
        db_manager::instance().func_query([guild_id = data_.guild_id](sql_msg &msg) {
            if (guild *g = guild_manager::instance().find_guild(guild_id))
                g->p2p_update_grade(msg);
        },
            sql::format("SELECT grade, name, auth+0 FROM guild_grade{} WHERE guild_id = {} and grade = {}", sql::raw(get_table_postfix()), data_.guild_id, grade));
    }

    namespace {
        void send_change_grade(uint32_t guild_id, uint8_t grade) {
            packet_guild p;
            p.guild = guild_id;
            p.info = grade;
            db_clientdesc->db_packet(HEADER_GD_GUILD_CHANGE_GRADE, 0, &p, sizeof(p));
        }
    }

    void guild::change_grade_name(uint8_t grade, const char *grade_name) {
        if (grade == 1)
            return;

        if (grade < 1 || grade > 15) {
            metin2::log::warn("Invalid guild grade {:d}", grade);
            return;
        }

        if (strlen(grade_name) > GUILD_NAME_MAX_LEN)
            return;

        if (!*grade_name)
            return;

        db_manager::instance().func_after_query([guild_id = id(), grade] { send_change_grade(guild_id, grade); }, sql::format("UPDATE guild_grade{} SET name = {} where guild_id = {} and grade = {}", sql::raw(get_table_postfix()), grade_name, data_.guild_id, grade));

        grade--;
        strlcpy(data_.grade_array[grade].grade_name, grade_name, sizeof(data_.grade_array[grade].grade_name));

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack);
        pack.subheader = GUILD_SUBHEADER_GC_GRADE_NAME;

        one_grade_name_packet pack2;
        pack.size += sizeof(pack2);
        pack2.grade = grade + 1;
        strlcpy(pack2.grade_name, grade_name, sizeof(pack2.grade_name));

        temp_buffer buf;
        buf.write(&pack, sizeof(pack));
        buf.write(&pack2, sizeof(pack2));

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();

            if (d)
                d->packet(buf.read_peek(), buf.size());
        }
    }

    void guild::change_grade_auth(uint8_t grade, uint8_t auth) {
        if (grade == 1)
            return;

        if (grade < 1 || grade > 15) {
            metin2::log::warn("Invalid guild grade {:d}", grade);
            return;
        }

        db_manager::instance().func_after_query([guild_id = id(), grade] { send_change_grade(guild_id, grade); }, sql::format("UPDATE guild_grade{} SET auth = {} where guild_id = {} and grade = {}", sql::raw(get_table_postfix()), auth, data_.guild_id, grade));

        grade--;

        data_.grade_array[grade].auth_flag = auth;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack);
        pack.subheader = GUILD_SUBHEADER_GC_GRADE_AUTH;

        one_grade_auth_packet pack2;
        pack.size += sizeof(pack2);
        pack2.grade = grade + 1;
        pack2.auth = auth;

        temp_buffer buf;
        buf.write(&pack, sizeof(pack));
        buf.write(&pack2, sizeof(pack2));

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();

            if (d)
                d->packet(buf.read_peek(), buf.size());
        }
    }

    void guild::send_guild_info_packet(character *ch) {
        desc *d = ch->get_desc();

        if (!d)
            return;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(packet_gc_guild) + sizeof(packet_gc_guild_info);
        pack.subheader = GUILD_SUBHEADER_GC_INFO;

        packet_gc_guild_info pack_sub;

        memset(&pack_sub, 0, sizeof(packet_gc_guild_info));
        pack_sub.member_count = member_count();
        pack_sub.max_member_count = max_member_count();
        pack_sub.guild_id = data_.guild_id;
        pack_sub.master_pid = data_.master_pid;
        pack_sub.exp = data_.exp;
        pack_sub.level = data_.level;
        strlcpy(pack_sub.name, data_.name, sizeof(pack_sub.name));
        pack_sub.gold = data_.gold;
        pack_sub.has_land = has_land();

        metin2::log::debug("Guild: sending guild info for {}", data_.name);
        metin2::log::debug("Guild: master pid {:d}", data_.master_pid);

        d->buffered_packet(&pack, sizeof(packet_gc_guild));
        d->packet(&pack_sub, sizeof(packet_gc_guild_info));
    }

    auto guild::offer_exp(character *ch, int amount) -> bool {
        auto cit = member_.find(ch->player_id());

        if (cit == member_.end())
            return false;

        if (data_.exp + amount < data_.exp)
            return false;

        if (amount < 0)
            return false;

        if (ch->get_exp() < (uint32_t)amount) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 제공하고자 하는 경험치가 남은 경험치보다 많습니다."));
            return false;
        }

        if (ch->get_exp() - (uint32_t)amount > ch->get_exp()) {
            metin2::log::warn("Invalid guild exp offer {:d} by player {} (pid {:d})", amount, ch->name(), ch->player_id());
            return false;
        }

        ch->point_change(POINT_EXP, -amount);

        packet_guild_exp_update guild_exp;
        guild_exp.guild_id = id();
        guild_exp.amount = amount / 100;
        db_clientdesc->db_packet(HEADER_GD_GUILD_EXP_UPDATE, 0, &guild_exp, sizeof(guild_exp));
        guild_point_change(POINT_EXP, amount / 100, true);

        cit->second.offer_exp += amount / 100;
        cit->second.dummy = 0;

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();
            if (d) {
                pack.subheader = GUILD_SUBHEADER_GC_LIST;
                pack.size = sizeof(pack) + 13;
                d->buffered_packet(&pack, sizeof(pack));
                d->packet(&(cit->second), sizeof(uint32_t) * 3 + 1);
            }
        }

        save_member(ch->player_id());

        packet_guild_change_member_data gd_guild;

        gd_guild.guild_id = id();
        gd_guild.pid = ch->player_id();
        gd_guild.offer = cit->second.offer_exp;
        gd_guild.level = ch->level();
        gd_guild.grade = cit->second.grade;

        db_clientdesc->db_packet(HEADER_GD_GUILD_CHANGE_MEMBER_DATA, 0, &gd_guild, sizeof(gd_guild));
        return true;
    }

    void guild::disband() {
        metin2::log::info("Guild: disbanding guild {} (id {:d})", name(), id());

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            character *ch = *it;
            ch->set_guild(nullptr);
            send_online_remove_one_packet(ch->player_id());
            ch->set_quest_flag("guild_building.new_withdraw_time", get_global_time());
        }

        for (auto it = member_.begin(); it != member_.end(); ++it) {
            guild_manager::instance().unlink(it->first);
        }
    }

    void guild::request_disband(uint32_t pid) {
        if (data_.master_pid != pid)
            return;

        packet_guild gd_guild;
        gd_guild.guild = id();
        gd_guild.info = 0;
        db_clientdesc->db_packet(HEADER_GD_GUILD_DISBAND, 0, &gd_guild, sizeof(packet_guild));

        building::manager::instance().clear_land_by_guild_id(id());
    }

    void guild::add_comment(character *ch, const std::string &str) {
        if (str.length() > GUILD_COMMENT_MAX_LEN || str.length() == 0)
            return;

        if (ch->increase_chat_counter() >= 2) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
            return;
        }

        db_manager::instance().func_after_query([guild_id = data_.guild_id, pid = ch->player_id()] {
            if (guild *g = guild_manager::instance().find_guild(guild_id))
                g->refresh_comment_force(pid);
        },
            sql::format("INSERT INTO guild_comment{}(guild_id, name, notice, content, time) VALUES({}, {}, {}, {}, NOW())", sql::raw(get_table_postfix()), data_.guild_id, ch->name(), (str[0] == '!') ? 1 : 0, str));
    }

    void guild::delete_comment(character *ch, uint32_t comment_id) {
        std::unique_ptr<sql_msg> pmsg;

        if (member(ch->player_id())->grade == GUILD_LEADER_GRADE)
            pmsg = db_manager::instance().direct_query(sql::format("DELETE FROM guild_comment{} WHERE id = {} AND guild_id = {}", sql::raw(get_table_postfix()), comment_id, data_.guild_id));
        else
            pmsg = db_manager::instance().direct_query(sql::format("DELETE FROM guild_comment{} WHERE id = {} AND guild_id = {} AND name = {}", sql::raw(get_table_postfix()), comment_id, data_.guild_id, ch->name()));

        if (pmsg->get()->affected_rows == 0 || pmsg->get()->affected_rows == (uint32_t)-1)
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 삭제할 수 없는 글입니다."));
        else
            refresh_comment_force(ch->player_id());
    }

    void guild::refresh_comment(character *ch) {
        refresh_comment_force(ch->player_id());
    }

    void guild::refresh_comment_force(uint32_t player_id) {
        character *ch = character_manager::instance().find_by_pid(player_id);
        if (ch == nullptr) {
            return;
        }

        auto pmsg = db_manager::instance().direct_query(sql::format("SELECT id, name, content FROM guild_comment{} WHERE guild_id = {} ORDER BY notice DESC, id DESC LIMIT {}", sql::raw(get_table_postfix()), data_.guild_id, GUILD_COMMENT_MAX_COUNT));

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 1;
        pack.subheader = GUILD_SUBHEADER_GC_COMMENTS;

        uint8_t count = pmsg->get()->num_rows;

        desc *d = ch->get_desc();

        if (!d)
            return;

        pack.size += (sizeof(uint32_t) + CHARACTER_NAME_MAX_LEN + 1 + GUILD_COMMENT_MAX_LEN + 1) * (uint16_t)count;
        d->buffered_packet(&pack, sizeof(pack));
        d->buffered_packet(&count, 1);
        char szName[CHARACTER_NAME_MAX_LEN + 1];
        char szContent[GUILD_COMMENT_MAX_LEN + 1];
        memset(szName, 0, sizeof(szName));
        memset(szContent, 0, sizeof(szContent));

        for (uint i = 0; i < pmsg->get()->num_rows; i++) {
            MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res.get());
            uint32_t id = strtoul(row[0], nullptr, 10);

            strlcpy(szName, row[1], sizeof(szName));
            strlcpy(szContent, row[2], sizeof(szContent));

            d->buffered_packet(&id, sizeof(id));
            d->buffered_packet(szName, sizeof(szName));

            if (i == pmsg->get()->num_rows - 1)
                d->packet(szContent, sizeof(szContent));
            else
                d->buffered_packet(szContent, sizeof(szContent));
        }
    }

    auto guild::change_member_general(uint32_t pid, uint8_t is_general) -> bool {
        if (is_general && general_count() >= max_general_count())
            return false;

        auto it = member_.find(pid);
        if (it == member_.end()) {
            return true;
        }

        is_general = is_general ? 1 : 0;

        if (it->second.is_general == is_general)
            return true;

        if (is_general)
            ++general_count_;
        else
            --general_count_;

        it->second.is_general = is_general;

        auto itOnline = member_online_.begin();

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 5;
        pack.subheader = GUILD_SUBHEADER_GC_CHANGE_MEMBER_GENERAL;

        while (itOnline != member_online_.end()) {
            desc *d = (*(itOnline++))->get_desc();

            if (!d)
                continue;

            d->buffered_packet(&pack, sizeof(pack));
            d->buffered_packet(&pid, sizeof(pid));
            d->packet(&is_general, sizeof(is_general));
        }

        save_member(pid);
        return true;
    }

    void guild::change_member_grade(uint32_t pid, uint8_t grade) {
        if (grade == 1)
            return;

        if (grade < 1 || grade > GUILD_GRADE_COUNT) {
            metin2::log::warn("Invalid guild grade {:d}", grade);
            return;
        }

        auto it = member_.find(pid);

        if (it == member_.end())
            return;

        it->second.grade = grade;

        auto itOnline = member_online_.begin();

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 5;
        pack.subheader = GUILD_SUBHEADER_GC_CHANGE_MEMBER_GRADE;

        while (itOnline != member_online_.end()) {
            desc *d = (*(itOnline++))->get_desc();

            if (!d)
                continue;

            d->buffered_packet(&pack, sizeof(pack));
            d->buffered_packet(&pid, sizeof(pid));
            d->packet(&grade, sizeof(grade));
        }

        save_member(pid);

        packet_guild_change_member_data gd_guild;

        gd_guild.guild_id = id();
        gd_guild.pid = pid;
        gd_guild.offer = it->second.offer_exp;
        gd_guild.level = it->second.level;
        gd_guild.grade = grade;

        db_clientdesc->db_packet(HEADER_GD_GUILD_CHANGE_MEMBER_DATA, 0, &gd_guild, sizeof(gd_guild));
    }

    void guild::skill_level_up(uint32_t vnum_value) {
        uint32_t dwRealVnum = vnum_value - GUILD_SKILL_START;

        if (dwRealVnum >= GUILD_SKILL_COUNT)
            return;

        skill_proto *pkSk = skill_manager::instance().get(vnum_value);

        if (!pkSk) {
            metin2::log::error("Unknown guild skill {:d}", vnum_value);
            return;
        }

        if (data_.aby_skill[dwRealVnum] >= pkSk->max_level)
            return;

        if (data_.skill_point <= 0)
            return;
        data_.skill_point--;

        data_.aby_skill[dwRealVnum]++;

        compute_guild_points();
        save_skill();
        send_db_skill_update();

        for (character *ch : member_online_)
            send_skill_info_packet(ch);

        metin2::log::info("Guild: guild {} raised skill {:d} to level {:d} (type {:d})", name(), pkSk->vnum, data_.aby_skill[dwRealVnum], pkSk->type);
    }

    void guild::use_skill(uint32_t vnum_value, character *ch, uint32_t pid) {
        character *victim = nullptr;

        if (!member(ch->player_id()) || !has_grade_auth(member(ch->player_id())->grade, GUILD_AUTH_USE_SKILL))
            return;

        metin2::log::info("Guild: player {} used guild skill {:d}", ch ? ch->name() : "", vnum_value);

        uint32_t dwRealVnum = vnum_value - GUILD_SKILL_START;

        if (!ch->can_move())
            return;

        if (dwRealVnum >= GUILD_SKILL_COUNT)
            return;

        skill_proto *pkSk = skill_manager::instance().get(vnum_value);

        if (!pkSk) {
            metin2::log::error("Unknown guild skill {:d}", vnum_value);
            return;
        }

        if (data_.aby_skill[dwRealVnum] == 0)
            return;

        if ((pkSk->flag & SKILL_FLAG_SELFONLY)) {
            if (ch->find_affect(pkSk->vnum))
                return;

            victim = ch;
        }

        if (ch->is_affect_flag(AFF_REVIVE_INVISIBLE))
            ch->remove_affect(AFFECT_REVIVE_INVISIBLE);

        if (ch->is_affect_flag(AFF_EUNHYUNG))
            ch->remove_affect(SKILL_EUNHYUNG);

        double k = 1.0 * data_.aby_skill[dwRealVnum] / pkSk->max_level;
        pkSk->sp_cost_poly.set_var("k", k);
        int iNeededSP = (int)pkSk->sp_cost_poly.eval();

        if (sp() < iNeededSP) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 용신력이 부족합니다. (%d, %d)"), sp(), iNeededSP);
            return;
        }

        pkSk->cooldown_poly.set_var("k", k);
        int iCooltime = (int)pkSk->cooldown_poly.eval();

        if (!skill_usable_[dwRealVnum]) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 쿨타임이 끝나지 않아 길드 스킬을 사용할 수 없습니다."));
            return;
        }

        {
            packet_guild_use_skill p;
            p.guild = id();
            p.skill_vnum = pkSk->vnum;
            p.cooltime = iCooltime;
            db_clientdesc->db_packet(HEADER_GD_GUILD_USE_SKILL, 0, &p, sizeof(p));
        }
        skill_usable_[dwRealVnum] = false;

        if (metin2::config::test_server())
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> %d 스킬을 사용함 (%d, %d) to %u"), vnum_value, sp(), iNeededSP, pid);

        switch (vnum_value) {
        case GUILD_SKILL_TELEPORT:
            send_db_skill_update(-iNeededSP);
            if ((victim = (character_manager::instance().find_by_pid(pid))))
                ch->warp_set(victim->x(), victim->y());
            else {
                if (member_p2p_online_.find(pid) != member_p2p_online_.end()) {
                    cci *pcci = p2p_manager::instance().find_by_pid(pid);

                    if (pcci->channel != metin2::config::server().channel) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대가 %d 채널에 있습니다. (현재 채널 %d)"), pcci->channel, metin2::config::server().channel);
                    } else {
                        packet_gg_find_position p;
                        p.header = HEADER_GG_FIND_POSITION;
                        p.from_pid = ch->player_id();
                        p.target_pid = pid;
                        pcci->desc->packet(&p, sizeof(packet_gg_find_position));
                        if (metin2::config::test_server()) ch->chat_packet(CHAT_TYPE_PARTY, "sent find position packet for guild teleport");
                    }
                } else
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대가 온라인 상태가 아닙니다."));
            }
            break;

        default:
            {

                if (!under_any_war()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드 스킬은 길드전 중에만 사용할 수 있습니다."));
                    return;
                }

                send_db_skill_update(-iNeededSP);

                for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
                    character *victim = *it;
                    victim->remove_affect(vnum_value);
                    ch->compute_skill(vnum_value, victim, data_.aby_skill[dwRealVnum]);
                }
            }
            break;
        }
    }

    void guild::send_skill_info_packet(character *ch) const {
        desc *d = ch->get_desc();

        if (!d)
            return;

        packet_gc_guild pack;

        pack.header = HEADER_GC_GUILD;
        pack.size = sizeof(pack) + 6 + GUILD_SKILL_COUNT;
        pack.subheader = GUILD_SUBHEADER_GC_SKILL_INFO;

        d->buffered_packet(&pack, sizeof(pack));
        d->buffered_packet(&data_.skill_point, 1);
        d->buffered_packet(&data_.aby_skill, GUILD_SKILL_COUNT);
        d->buffered_packet(&data_.power, 2);
        d->packet(&data_.max_power, 2);
    }

    void guild::compute_guild_points() {
        data_.max_power = GUILD_BASE_POWER + (data_.level - 1) * GUILD_POWER_PER_LEVEL;

        data_.power = std::clamp(data_.power, 0, data_.max_power);
    }

    auto guild::skill_level(uint32_t vnum) -> int {
        uint32_t dwRealVnum = vnum - GUILD_SKILL_START;

        if (dwRealVnum >= GUILD_SKILL_COUNT)
            return 0;

        return data_.aby_skill[dwRealVnum];
    }

    void guild::update_skill(uint8_t skill_point, uint8_t *skill_levels) {

        data_.skill_point = skill_point;

        std::memcpy(data_.aby_skill, skill_levels, sizeof(uint8_t) * GUILD_SKILL_COUNT);
        compute_guild_points();
    }

    static auto guild_levelup_exp(int level) -> uint32_t {
        return guild_exp_table2[level];
    }

    void guild::guild_point_change(uint8_t type, int amount, bool save) {
        switch (type) {
        case POINT_SP:
            data_.power += amount;

            data_.power = std::clamp(data_.power, 0, data_.max_power);

            if (save) {
                save_skill();
            }

            for (character *ch : member_online_)
                send_skill_info_packet(ch);
            break;

        case POINT_EXP:
            if (amount < 0 && data_.exp < (uint32_t)-amount) {
                data_.exp = 0;
            } else {
                data_.exp += amount;

                while (data_.exp >= guild_levelup_exp(data_.level)) {

                    if (data_.level < GUILD_MAX_LEVEL) {
                        data_.exp -= guild_levelup_exp(data_.level);
                        ++data_.level;
                        ++data_.skill_point;

                        if (data_.level > GUILD_MAX_LEVEL)
                            data_.level = GUILD_MAX_LEVEL;

                        compute_guild_points();
                        guild_point_change(POINT_SP, data_.max_power - data_.power);

                        if (save)
                            change_ladder_point(GUILD_LADDER_POINT_PER_LEVEL);

                        for (character *ch : member_online_)
                            send_guild_info_packet(ch);
                    }

                    if (data_.level == GUILD_MAX_LEVEL) {
                        data_.exp = 0;
                    }
                }
            }

            packet_gc_guild pack;
            pack.header = HEADER_GC_GUILD;
            pack.size = sizeof(pack) + 5;
            pack.subheader = GUILD_SUBHEADER_GC_CHANGE_EXP;

            temp_buffer buf;
            buf.write(&pack, sizeof(pack));
            buf.write(&data_.level, 1);
            buf.write(&data_.exp, 4);

            for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
                desc *d = (*it)->get_desc();

                if (d)
                    d->packet(buf.read_peek(), buf.size());
            }

            if (save)
                save_level();

            break;
        }
    }

    void guild::skill_recharge() {
    }

    void guild::save_member(uint32_t pid) {
        auto it = member_.find(pid);

        if (it == member_.end())
            return;

        db_manager::instance().query(sql::format(
            "UPDATE guild_member{} SET grade = {}, offer = {}, is_general = {} WHERE pid = {} and guild_id = {}",
            sql::raw(get_table_postfix()), it->second.grade, it->second.offer_exp, it->second.is_general, pid, data_.guild_id));
    }

    void guild::level_change(uint32_t pid, uint8_t level) {
        auto cit = member_.find(pid);

        if (cit == member_.end())
            return;

        cit->second.level = level;

        packet_guild_change_member_data gd_guild;

        gd_guild.guild_id = id();
        gd_guild.pid = pid;
        gd_guild.offer = cit->second.offer_exp;
        gd_guild.grade = cit->second.grade;
        gd_guild.level = level;

        db_clientdesc->db_packet(HEADER_GD_GUILD_CHANGE_MEMBER_DATA, 0, &gd_guild, sizeof(gd_guild));

        packet_gc_guild pack;
        pack.header = HEADER_GC_GUILD;
        cit->second.dummy = 0;

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();

            if (d) {
                pack.subheader = GUILD_SUBHEADER_GC_LIST;
                pack.size = sizeof(pack) + 13;
                d->buffered_packet(&pack, sizeof(pack));
                d->packet(&(cit->second), sizeof(uint32_t) * 3 + 1);
            }
        }
    }

    void guild::change_member_data(uint32_t pid, uint32_t offer, uint8_t level, uint8_t grade) {
        auto cit = member_.find(pid);

        if (cit == member_.end())
            return;

        cit->second.offer_exp = offer;
        cit->second.level = level;
        cit->second.grade = grade;
        cit->second.dummy = 0;

        packet_gc_guild pack;
        memset(&pack, 0, sizeof(pack));
        pack.header = HEADER_GC_GUILD;

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();
            if (d) {
                pack.subheader = GUILD_SUBHEADER_GC_LIST;
                pack.size = sizeof(pack) + 13;
                d->buffered_packet(&pack, sizeof(pack));
                d->packet(&(cit->second), sizeof(uint32_t) * 3 + 1);
            }
        }
    }

    void guild::p2p_chat(const char *text) {
        for (character *ch : member_online_)
            ch->chat_packet(CHAT_TYPE_GUILD, "%s", text);
    }

    void guild::chat(const char *text_value) {
        for (character *ch : member_online_)
            ch->chat_packet(CHAT_TYPE_GUILD, "%s", text_value);

        packet_gg_guild p1;
        packet_gg_guild_chat p2;

        p1.header = HEADER_GG_GUILD;
        p1.sub_header = GUILD_SUBHEADER_GG_CHAT;
        p1.guild = id();
        strlcpy(p2.text, text_value, sizeof(p2.text));

        p2p_manager::instance().send(&p1, sizeof(packet_gg_guild));
        p2p_manager::instance().send(&p2, sizeof(packet_gg_guild_chat));
    }

    auto guild::master_character() -> character * {
        return character_manager::instance().find_by_pid(master_pid());
    }

    void guild::packet(const void *buf, int size) {
        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            desc *d = (*it)->get_desc();

            if (d)
                d->packet(buf, size);
        }
    }

    auto guild::charge_sp(character *ch, int sp) -> bool {
        int gold = sp * 100;

        if (gold < sp || ch->gold() < gold)
            return false;

        int iRemainSP = data_.max_power - data_.power;

        if (sp > iRemainSP) {
            sp = iRemainSP;
            gold = sp * 100;
        }

        ch->point_change(POINT_GOLD, -gold);
        db_manager::instance().send_money_log(MONEY_LOG_GUILD, 1, -gold);

        send_db_skill_update(sp);
        {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> %u의 용신력을 회복하였습니다."), sp);
        }
        return true;
    }

    void guild::skill_usable_change(uint32_t skill_vnum, bool usable) {
        uint32_t dwRealVnum = skill_vnum - GUILD_SKILL_START;

        if (dwRealVnum >= GUILD_SKILL_COUNT)
            return;

        skill_usable_[dwRealVnum] = usable;

        metin2::log::debug("Guild: guild {} skill {:d} usable set to {:d}", name(), skill_vnum, usable);
    }

    void guild::set_member_count_bonus(int bonus) {
        member_count_bonus_ = bonus;
        metin2::log::info("Guild: member count bonus set to {:d}", bonus);
    }

    void guild::broadcast_member_count_bonus() {
        packet_gg_guild p1;

        p1.header = HEADER_GG_GUILD;
        p1.sub_header = GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS;
        p1.guild = id();

        p2p_manager::instance().send(&p1, sizeof(packet_gg_guild));
        p2p_manager::instance().send(&member_count_bonus_, sizeof(int));
    }

    auto guild::max_member_count() -> int {
        if (member_count_bonus_ < 0 || member_count_bonus_ > 18)
            member_count_bonus_ = 0;

        return 32 + 2 * (data_.level - 1) + member_count_bonus_;
    }

    void guild::request_deposit_money(character *ch, int gold_value) {
        if (false == ch->can_deposit()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 잠시후에 이용해주십시오"));
            return;
        }

        if (ch->gold() < gold_value)
            return;

        ch->point_change(POINT_GOLD, -gold_value);

        packet_gd_guild_money p;
        p.guild = id();
        p.gold = gold_value;
        db_clientdesc->db_packet(HEADER_GD_GUILD_DEPOSIT_MONEY, 0, &p, sizeof(p));

        const std::string hint = std::format("{} {}", id(), name());
        log_manager::instance().char_log(ch, gold_value, "GUILD_DEPOSIT", hint.c_str());

        ch->update_deposit_pulse();
        metin2::log::info("Guild: deposit to guild {} (id {:d}) by player {} (pid {:d}), {:d} gold", name(), id(), ch->name(), ch->player_id(), gold_value);
    }

    void guild::recv_money_change(int gold_value) {
        data_.gold = gold_value;

        packet_gc_guild p;
        p.header = HEADER_GC_GUILD;
        p.size = sizeof(p) + sizeof(int);
        p.subheader = GUILD_SUBHEADER_GC_MONEY_CHANGE;

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            character *ch = *it;
            desc *d = ch->get_desc();
            d->buffered_packet(&p, sizeof(p));
            d->packet(&gold_value, sizeof(int));
        }
    }

    void guild::recv_withdraw_money_give(int change_gold_value) {
        character *ch = master_character();

        if (ch) {
            ch->point_change(POINT_GOLD, change_gold_value);
            metin2::log::info("Guild: withdrawal from guild {} (id {:d}) by player {} (pid {:d}), {:d} gold", name(), id(), ch->name(), ch->player_id(), change_gold_value);
        }

        packet_gd_guild_money_withdraw_give_reply p;
        p.guild = id();
        p.change_gold = change_gold_value;
        p.give_success = ch ? 1 : 0;
        db_clientdesc->db_packet(HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY, 0, &p, sizeof(p));
    }

    auto guild::has_land() -> bool {
        return building::manager::instance().find_land_by_guild(id()) != nullptr;
    }

    EVENTINFO(invite_guild_event_info) {
        uint32_t invitee_pid;
        uint32_t guild_id;

        invite_guild_event_info()
            : invitee_pid(0), guild_id(0) {
        }
    };

    EVENTFUNC(guild_invite_event) {
        auto *pInfo = dynamic_cast<invite_guild_event_info *>(event->info.get());

        if (pInfo == nullptr) {
            metin2::log::error("Guild invite event has no event info");
            return 0;
        }

        guild *pGuild = guild_manager::instance().find_guild(pInfo->guild_id);

        if (pGuild) {
            metin2::log::debug("Guild: invite to guild {} expired", pGuild->name());
            pGuild->invite_deny(pInfo->invitee_pid);
        }

        return 0;
    }

    void guild::invite(character *pch_inviter, character *pch_invitee) {
        if (quest::quest_manager::instance().pc_force(pch_inviter->player_id())->is_running() == true) {
            pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방이 초대 신청을 받을 수 없는 상태입니다."));
            return;
        }

        if (quest::quest_manager::instance().pc_force(pch_invitee->player_id())->is_running() == true)
            return;

        if (pch_invitee->is_block_mode(BLOCK_GUILD_INVITE)) {
            pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방이 길드 초대 거부 상태입니다."));
            return;
        } else if (!has_grade_auth(member(pch_inviter->player_id())->grade, GUILD_AUTH_ADD_MEMBER)) {
            pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 초대할 권한이 없습니다."));
            return;
        } else if (pch_invitee->empire() != pch_inviter->empire()) {
            pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 다른 제국 사람을 길드에 초대할 수 없습니다."));
            return;
        }

        guild_join_err_code errcode = verify_guild_joinable_condition(pch_invitee);
        switch (errcode) {
        case GERR_NONE: break;
        case GERR_WITHDRAWPENALTY:
            pch_inviter->chat_packet(CHAT_TYPE_INFO,
                lc_text("<길드> 탈퇴한 후 %d일이 지나지 않은 사람은 길드에 초대할 수 없습니다."),
                quest::quest_manager::instance().event_flag("guild_withdraw_delay"));
            return;
        case GERR_COMMISSIONPENALTY:
            pch_inviter->chat_packet(CHAT_TYPE_INFO,
                lc_text("<길드> 길드를 해산한 지 %d일이 지나지 않은 사람은 길드에 초대할 수 없습니다."),
                quest::quest_manager::instance().event_flag("guild_disband_delay"));
            return;
        case GERR_ALREADYJOIN:     pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방이 이미 다른 길드에 속해있습니다.")); return;
        case GERR_GUILDISFULL:     pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 최대 길드원 수를 초과했습니다.")); return;
        case GERR_GUILD_IS_IN_WAR: pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 현재 길드가 전쟁 중 입니다.")); return;
        case GERR_INVITE_LIMIT:    pch_inviter->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 현재 신규 가입 제한 상태 입니다.")); return;

        default: metin2::log::error("Unhandled guild join error {:d}", errcode); return;
        }

        if (guild_invite_event_map_.end() != guild_invite_event_map_.find(pch_invitee->player_id()))
            return;

        auto *pInfo = alloc_event_info<invite_guild_event_info>();
        pInfo->invitee_pid = pch_invitee->player_id();
        pInfo->guild_id = id();

        guild_invite_event_map_.insert(event_map::value_type(pch_invitee->player_id(), event_create(guild_invite_event, pInfo, passes_per_sec(10))));

        uint32_t gid = id();

        packet_gc_guild p;
        p.header = HEADER_GC_GUILD;
        p.size = sizeof(p) + sizeof(uint32_t) + GUILD_NAME_MAX_LEN + 1;
        p.subheader = GUILD_SUBHEADER_GC_GUILD_INVITE;

        temp_buffer buf;
        buf.write(&p, sizeof(p));
        buf.write(&gid, sizeof(uint32_t));
        buf.write(name(), GUILD_NAME_MAX_LEN + 1);

        pch_invitee->get_desc()->packet(buf.read_peek(), buf.size());
    }

    void guild::invite_accept(character *pch_invitee) {
        auto itFind = guild_invite_event_map_.find(pch_invitee->player_id());
        if (itFind == guild_invite_event_map_.end()) {
            metin2::log::warn("Player {} accepted an invite to guild {} without being invited", pch_invitee->name(), name());
            return;
        }

        event_cancel(&itFind->second);
        guild_invite_event_map_.erase(itFind);

        guild_join_err_code errcode = verify_guild_joinable_condition(pch_invitee);
        switch (errcode) {
        case GERR_NONE: break;
        case GERR_WITHDRAWPENALTY:
            pch_invitee->chat_packet(CHAT_TYPE_INFO,
                lc_text("<길드> 탈퇴한 후 %d일이 지나지 않은 사람은 길드에 초대할 수 없습니다."),
                quest::quest_manager::instance().event_flag("guild_withdraw_delay"));
            return;
        case GERR_COMMISSIONPENALTY:
            pch_invitee->chat_packet(CHAT_TYPE_INFO,
                lc_text("<길드> 길드를 해산한 지 %d일이 지나지 않은 사람은 길드에 초대할 수 없습니다."),
                quest::quest_manager::instance().event_flag("guild_disband_delay"));
            return;
        case GERR_ALREADYJOIN:     pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방이 이미 다른 길드에 속해있습니다.")); return;
        case GERR_GUILDISFULL:     pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 최대 길드원 수를 초과했습니다.")); return;
        case GERR_GUILD_IS_IN_WAR: pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 현재 길드가 전쟁 중 입니다.")); return;
        case GERR_INVITE_LIMIT:    pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 현재 신규 가입 제한 상태 입니다.")); return;

        default: metin2::log::error("Unhandled guild join error {:d}", errcode); return;
        }

        request_add_member(pch_invitee, 15);
    }

    void guild::invite_deny(uint32_t pid) {
        auto itFind = guild_invite_event_map_.find(pid);
        if (itFind == guild_invite_event_map_.end()) {
            metin2::log::warn("Pid {:d} denied an invite to guild {} without being invited", pid, name());
            return;
        }

        event_cancel(&itFind->second);
        guild_invite_event_map_.erase(itFind);
    }

    auto guild::verify_guild_joinable_condition(character *const pch_invitee) -> guild::guild_join_err_code {
        if (get_global_time() - pch_invitee->quest_flag("guild_building.new_withdraw_time") < guild_manager::instance().withdraw_delay())
            return GERR_WITHDRAWPENALTY;
        else if (get_global_time() - pch_invitee->quest_flag("guild_building.new_disband_time") < guild_manager::instance().disband_delay())
            return GERR_COMMISSIONPENALTY;
        else if (pch_invitee->get_guild())
            return GERR_ALREADYJOIN;
        else if (member_count() >= max_member_count()) {
            metin2::log::debug("Guild: guild {} is full, {:d} of {:d} members (level {:d}, bonus {:d})",
                name(), member_count(), max_member_count(), data_.level, member_count_bonus_);
            return GERR_GUILDISFULL;
        } else if (under_any_war() != 0) {
            return GERR_GUILD_IS_IN_WAR;
        }

        return GERR_NONE;
    }

    auto guild::change_master_to(uint32_t pid) -> bool {
        if (member(pid) == nullptr) return false;

        packet_change_guild_master p;
        p.guild_id = id();
        p.id_from = master_pid();
        p.id_to = pid;

        db_clientdesc->db_packet(HEADER_GD_REQ_CHANGE_GUILD_MASTER, 0, &p, sizeof(p));

        return true;
    }

    void guild::send_guild_data_update_to_all_member(sql_msg &msg) {
        auto iter = member_online_.begin();

        for (; iter != member_online_.end(); iter++) {
            send_guild_info_packet(*iter);
            send_all_grade_packet(*iter);
        }
    }
}
