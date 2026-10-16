#include <metin2/db/app/main.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/empire/monarch.h>
#include <metin2/db/storage/db_manager.h>
#include <metin2/db/tables/csv_reader.h>
#include <metin2/db/tables/proto_reader.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/building.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace metin2::db::inline clients {

    namespace {
        auto load_locale_names(const char *file_name) -> std::map<int, std::string> {
            std::map<int, std::string> names;
            c_csv_table table;
            if (!table.load(file_name, '\t')) {
                metin2::log::warn("Cannot read {}", file_name);
                return names;
            }
            table.next();
            while (table.next())
                names[atoi(table.as_string_by_index(0))] = table.as_string_by_index(1);
            return names;
        }

        struct proto_files {
            const char *names;
            const char *proto;
            const char *test;
            const char *missing_test_warning;
            const char *parse_error;
            const char *reloading;
        };

        template <typename Table>
        auto load_proto(const proto_files &files, auto (*parse)(Table *, c_csv_table &, const std::map<int, std::string> &)->bool,
            void (*reset_untaken)(Table &), std::vector<Table> &tables) -> std::optional<size_t> {
            const auto names = load_locale_names(files.names);

            std::vector<Table> test_tables;
            c_csv_table test_data;
            if (!test_data.load(files.test, '\t')) {
                metin2::log::warn("{}", files.missing_test_warning);
            } else {
                test_data.next();
                while (test_data.next()) {
                    if (!parse(&test_tables.emplace_back(), test_data, names))
                        metin2::log::error("{}", files.parse_error);
                }
            }

            std::map<uint32_t, const Table *> test_by_vnum;
            for (const auto &table : test_tables)
                test_by_vnum.try_emplace(table.vnum, &table);

            c_csv_table data;
            if (!data.load(files.proto, '\t')) {
                metin2::log::error("Cannot read {}", files.proto);
                return std::nullopt;
            }

            if (!tables.empty()) {
                metin2::log::info("{}", files.reloading);
                tables.clear();
            }
            tables.reserve(data.row_count() + test_tables.size());

            std::set<uint32_t> proto_vnums;
            data.next();
            while (data.next()) {
                Table &table = tables.emplace_back();
                if (const auto it = test_by_vnum.find(atoi(data.as_string_by_index(0))); it != test_by_vnum.end()) {
                    table = *it->second;
                    reset_untaken(table);
                } else if (!parse(&table, data, names)) {
                    metin2::log::error("{}", files.parse_error);
                }
                proto_vnums.insert(table.vnum);
            }

            const size_t proto_count = tables.size();
            for (const auto &table : test_tables) {
                if (!proto_vnums.contains(table.vnum))
                    tables.push_back(table);
            }
            return proto_count;
        }

        auto load_item_attr_table(const char *table_name, std::vector<item_attr_table> &tables) -> bool {
            const std::string query = sql::format(
                "SELECT apply, apply+0, prob, lv1, lv2, lv3, lv4, lv5, weapon, body, wrist, foots, neck, head, shield, ear FROM {}{} ORDER BY apply",
                sql::raw(table_name), sql::raw(get_table_postfix()));

            auto msg = db_manager::instance().direct_query(query);
            sql_result *res = msg->get();

            if (!res->num_rows) {
                metin2::log::error("{} is empty", table_name);
                return false;
            }

            if (!tables.empty()) {
                metin2::log::info("Reloading {}", table_name);
                tables.clear();
            }

            tables.reserve(res->num_rows);

            static constexpr int sets[] = {ATTRIBUTE_SET_WEAPON, ATTRIBUTE_SET_BODY, ATTRIBUTE_SET_WRIST, ATTRIBUTE_SET_FOOTS,
                ATTRIBUTE_SET_NECK, ATTRIBUTE_SET_HEAD, ATTRIBUTE_SET_SHIELD, ATTRIBUTE_SET_EAR};

            MYSQL_ROW data;
            while ((data = mysql_fetch_row(res->res.get()))) {
                item_attr_table t;
                int col = 0;

                strlcpy(t.apply, data[col++], sizeof(t.apply));
                str_to_number(t.apply_index, data[col++]);
                str_to_number(t.prob, data[col++]);
                for (auto &value : t.values)
                    str_to_number(value, data[col++]);
                for (const int set : sets)
                    str_to_number(t.max_level_by_set[set], data[col++]);

                metin2::log::debug("{}: apply {} prob {:d}, values {:d} {:d} {:d} {:d} {:d}, max level weapon {:d} body {:d} wrist {:d} foots {:d} neck {:d} head {:d} shield {:d} ear {:d}",
                    table_name,
                    t.apply,
                    t.prob,
                    t.values[0],
                    t.values[1],
                    t.values[2],
                    t.values[3],
                    t.values[4],
                    t.max_level_by_set[ATTRIBUTE_SET_WEAPON],
                    t.max_level_by_set[ATTRIBUTE_SET_BODY],
                    t.max_level_by_set[ATTRIBUTE_SET_WRIST],
                    t.max_level_by_set[ATTRIBUTE_SET_FOOTS],
                    t.max_level_by_set[ATTRIBUTE_SET_NECK],
                    t.max_level_by_set[ATTRIBUTE_SET_HEAD],
                    t.max_level_by_set[ATTRIBUTE_SET_SHIELD],
                    t.max_level_by_set[ATTRIBUTE_SET_EAR]);

                tables.push_back(t);
            }

            return true;
        }

        auto locale_name_fragments(const char *locale_name) -> std::pair<std::string, std::string> {
            if (locale_name_column == "name")
                return {};
            return {", " + locale_name_column, sql::format(", {}", locale_name)};
        }
    }

    auto client_manager::initialize_tables() -> bool {
        if (!initialize_mob_table()) {
            metin2::log::error("Cannot load mob table");
            return false;
        }
        if (!mirror_mob_table_into_db()) {
            metin2::log::error("Cannot mirror mob table into the database");
            return false;
        }

        if (!initialize_item_table()) {
            metin2::log::error("Cannot load item table");
            return false;
        }

        if (!mirror_item_table_into_db()) {
            metin2::log::error("Cannot mirror item table into the database");
            return false;
        }

        if (!initialize_shop_table()) {
            metin2::log::error("Cannot load shop table");
            return false;
        }

        if (!initialize_skill_table()) {
            metin2::log::error("Cannot load skill table");
            return false;
        }

        if (!initialize_refine_table()) {
            metin2::log::error("Cannot load refine table");
            return false;
        }

        if (!initialize_item_attr_table()) {
            metin2::log::error("Cannot load item attribute table");
            return false;
        }

        if (!initialize_item_rare_table()) {
            metin2::log::error("Cannot load rare item attribute table");
            return false;
        }

        if (!initialize_banword_table()) {
            metin2::log::error("Cannot load banword table");
            return false;
        }

        if (!initialize_land_table()) {
            metin2::log::error("Cannot load land table");
            return false;
        }

        if (!initialize_object_proto()) {
            metin2::log::error("Cannot load object proto");
            return false;
        }

        if (!initialize_object_table()) {
            metin2::log::error("Cannot load object table");
            return false;
        }

        if (!initialize_monarch()) {
            metin2::log::error("Cannot load monarch data");
            return false;
        }

        return true;
    }

    auto client_manager::initialize_refine_table() -> bool {
        const std::string query = sql::format(
            "SELECT id, cost, prob, vnum0, count0, vnum1, count1, vnum2, count2,  vnum3, count3, vnum4, count4 FROM refine_proto{}",
            sql::raw(get_table_postfix()));

        auto msg = db_manager::instance().direct_query(query);
        sql_result *res = msg->get();

        if (!res->num_rows)
            return true;

        if (refine_table_) {
            metin2::log::info("Reloading refine_proto");
            refine_table_.reset();
        }

        refine_table_size_ = res->num_rows;

        refine_table_ = std::make_unique<refine_table[]>(refine_table_size_);
        memset(refine_table_.get(), 0, sizeof(refine_table) * refine_table_size_);

        refine_table *prt = refine_table_.get();
        MYSQL_ROW data;

        while ((data = mysql_fetch_row(res->res.get()))) {

            int col = 0;
            str_to_number(prt->id, data[col++]);
            str_to_number(prt->cost, data[col++]);
            str_to_number(prt->prob, data[col++]);

            for (int i = 0; i < REFINE_MATERIAL_MAX_NUM; i++) {
                str_to_number(prt->materials[i].vnum, data[col++]);
                str_to_number(prt->materials[i].count, data[col++]);
                if (prt->materials[i].vnum == 0) {
                    prt->material_count = i;
                    break;
                }
            }

            metin2::log::debug("Refine {:d}: cost {:d}, probability {:d}, first material vnum {:d} x{:d}", prt->id, prt->cost, prt->prob, prt->materials[0].vnum, prt->materials[0].count);

            prt++;
        }
        return true;
    }

    auto client_manager::initialize_mob_table() -> bool {
        static constexpr proto_files files = {
            "mob_names.txt", "mob_proto.txt", "mob_proto_test.txt",
            "No mob_proto_test.txt, continuing without it", "Cannot parse a mob_proto row", "Reloading mob_proto"};

        // The test override has never carried the mob colour.
        const auto proto_count = load_proto<mob_table>(files, set_proto_mob_table, [](mob_table &t) { t.mob_color = 0; }, vec_mob_table_);
        if (!proto_count)
            return false;

        for (const auto &t : std::span(vec_mob_table_).first(*proto_count))
            metin2::log::debug("Mob {1} / {2} (vnum {0:d}) level {3:d} rank {4:d} empire {5:d}", t.vnum, t.name, t.locale_name, t.level, t.rank, t.empire);

        std::ranges::sort(vec_mob_table_, {}, &mob_table::vnum);
        return true;
    }

    auto client_manager::initialize_shop_table() -> bool {
        MYSQL_ROW data;
        int col;

        static const char *query =
            "SELECT "
            "shop.vnum, "
            "shop.npc_vnum, "
            "shop_item.item_vnum, "
            "shop_item.count "
            "FROM shop LEFT JOIN shop_item "
            "ON shop.vnum = shop_item.shop_vnum ORDER BY shop.vnum, shop_item.item_vnum";

        auto msg2 = db_manager::instance().direct_query(query);

        sql_result *res2 = msg2->get();

        if (!res2->num_rows) {
            metin2::log::error("Shop table is empty");
            return false;
        }

        std::map<int, std::unique_ptr<shop_table>> map_shop;

        if (shop_table_) {
            shop_table_.reset();
        }

        shop_table *shop_table = shop_table_.get();

        while ((data = mysql_fetch_row(res2->res.get()))) {
            col = 0;

            int shop_vnum = 0;
            str_to_number(shop_vnum, data[col++]);

            if (map_shop.end() == map_shop.find(shop_vnum)) {
                auto shop_table_value = std::make_unique<proto::shop_table>();
                shop_table = shop_table_value.get();
                *shop_table = proto::shop_table();
                shop_table->vnum = shop_vnum;

                map_shop[shop_vnum] = std::move(shop_table_value);
            } else
                shop_table = map_shop[shop_vnum].get();

            str_to_number(shop_table->npc_vnum, data[col++]);

            if (!data[col])
                continue;

            if (shop_table->item_count >= SHOP_HOST_ITEM_MAX_NUM) {
                metin2::log::error("Shop {:d} has more than {:d} items; the rest are ignored", shop_vnum, SHOP_HOST_ITEM_MAX_NUM);
                continue;
            }

            shop_item_table *item = &shop_table->items[shop_table->item_count];

            str_to_number(item->vnum, data[col++]);
            str_to_number(item->count, data[col++]);

            ++shop_table->item_count;
        }

        shop_table_ = std::make_unique<proto::shop_table[]>(map_shop.size());
        shop_table_size_ = map_shop.size();

        int i = 0;
        for (const auto &[vnum, table] : map_shop) {
            shop_table_[i] = *table;
            metin2::log::debug("Shop {:d}: {:d} items", shop_table_[i].vnum, shop_table_[i].item_count);
            ++i;
        }

        return true;
    }

    auto client_manager::initialize_item_table() -> bool {
        static constexpr proto_files files = {
            "item_names.txt", "item_proto.txt", "item_proto_test.txt",
            "Cannot read item_proto_test.txt", "Cannot parse an item_proto row", "Reloading item_proto"};

        // The test override has never carried the vnum range or the specular value.
        const auto reset_untaken = [](item_table &t) {
            t.vnum_range = 0;
            t.specular = 0;
        };
        if (!load_proto<item_table>(files, set_proto_item_table, reset_untaken, vec_item_table_))
            return false;

        std::ranges::sort(vec_item_table_, {}, &proto::item_table::vnum);
        map_item_table_by_vnum_.clear();

        for (auto &t : vec_item_table_) {
            const proto::item_table *item_table = &t;

            metin2::log::debug("Item {1} / {2} (vnum {0:d}) values {3:d} {4:d} {5:d} {6:d} {7:d} {8:d}, wear flags {9:d}, anti flags {10:d}, immune {11:d}, refined vnum {12:d}, refine set {13:d}, magic pct {14:d}",
                item_table->vnum,
                item_table->name,
                item_table->locale_name,
                item_table->values[0],
                item_table->values[1],
                item_table->values[2],
                item_table->values[3],
                item_table->values[4],
                item_table->values[5],
                item_table->wear_flags,
                item_table->anti_flags,
                item_table->immune_flag,
                item_table->refined_vnum,
                item_table->refine_set,
                item_table->alter_to_magic_item_pct);

            map_item_table_by_vnum_.emplace(t.vnum, &t);
        }
        return true;
    }

    auto client_manager::initialize_skill_table() -> bool {
        const std::string query = sql::format(
            "SELECT dwVnum, szName, bType, bMaxLevel, dwSplashRange, "
            "szPointOn, szPointPoly, szSPCostPoly, szDurationPoly, szDurationSPCostPoly, "
            "szCooldownPoly, szMasterBonusPoly, setFlag+0, setAffectFlag+0, "
            "szPointOn2, szPointPoly2, szDurationPoly2, setAffectFlag2+0, "
            "szPointOn3, szPointPoly3, szDurationPoly3, szGrandMasterAddSPCostPoly, "
            "bLevelStep, bLevelLimit, prerequisiteSkillVnum, prerequisiteSkillLevel, iMaxHit, szSplashAroundDamageAdjustPoly, eSkillType+0, dwTargetRange "
            "FROM skill_proto{} ORDER BY dwVnum",
            sql::raw(get_table_postfix()));

        auto msg = db_manager::instance().direct_query(query);
        sql_result *res = msg->get();

        if (!res->num_rows) {
            metin2::log::error("skill_proto is empty");
            return false;
        }

        if (!vec_skill_table_.empty()) {
            metin2::log::info("Reloading skill_proto");
            vec_skill_table_.clear();
        }

        vec_skill_table_.reserve(res->num_rows);

        MYSQL_ROW data;
        int col;

        while ((data = mysql_fetch_row(res->res.get()))) {
            skill_table t;
            memset(&t, 0, sizeof(t));

            col = 0;

            str_to_number(t.vnum, data[col++]);
            strlcpy(t.name, data[col++], sizeof(t.name));
            str_to_number(t.type, data[col++]);
            str_to_number(t.max_level, data[col++]);
            str_to_number(t.splash_range, data[col++]);

            strlcpy(t.point_on, data[col++], sizeof(t.point_on));
            strlcpy(t.point_poly, data[col++], sizeof(t.point_poly));
            strlcpy(t.sp_cost_poly, data[col++], sizeof(t.sp_cost_poly));
            strlcpy(t.duration_poly, data[col++], sizeof(t.duration_poly));
            strlcpy(t.duration_sp_cost_poly, data[col++], sizeof(t.duration_sp_cost_poly));
            strlcpy(t.cooldown_poly, data[col++], sizeof(t.cooldown_poly));
            strlcpy(t.master_bonus_poly, data[col++], sizeof(t.master_bonus_poly));

            str_to_number(t.flag, data[col++]);
            str_to_number(t.affect_flag, data[col++]);

            strlcpy(t.point_on2, data[col++], sizeof(t.point_on2));
            strlcpy(t.point_poly2, data[col++], sizeof(t.point_poly2));
            strlcpy(t.duration_poly2, data[col++], sizeof(t.duration_poly2));
            str_to_number(t.affect_flag2, data[col++]);

            strlcpy(t.point_on3, data[col++], sizeof(t.point_on3));
            strlcpy(t.point_poly3, data[col++], sizeof(t.point_poly3));
            strlcpy(t.duration_poly3, data[col++], sizeof(t.duration_poly3));

            strlcpy(t.grand_master_add_sp_cost_poly, data[col++], sizeof(t.grand_master_add_sp_cost_poly));

            str_to_number(t.level_step, data[col++]);
            str_to_number(t.level_limit, data[col++]);
            str_to_number(t.pre_skill_vnum, data[col++]);
            str_to_number(t.pre_skill_level, data[col++]);

            str_to_number(t.max_hit, data[col++]);

            strlcpy(t.splash_around_damage_adjust_poly, data[col++], sizeof(t.splash_around_damage_adjust_poly));

            str_to_number(t.skill_attr_type, data[col++]);
            str_to_number(t.target_range, data[col++]);

            metin2::log::debug("Skill {1} (vnum {0:d}) flag {2:d}, point {3}, affect {4:d}, cooldown {5}", t.vnum, t.name, t.flag, t.point_on, t.affect_flag, t.cooldown_poly);

            vec_skill_table_.push_back(t);
        }

        return true;
    }

    auto client_manager::initialize_banword_table() -> bool {
        vec_banword_table_.clear();

        auto msg = db_manager::instance().direct_query("SELECT word FROM banword");

        sql_result *res = msg->get();

        if (res->num_rows == 0)
            return true;

        MYSQL_ROW data;

        while ((data = mysql_fetch_row(res->res.get()))) {
            banword_table t;

            if (data[0]) {
                strlcpy(t.word, data[0], sizeof(t.word));
                vec_banword_table_.push_back(t);
            }
        }

        metin2::log::info("Loaded {:d} banwords", vec_banword_table_.size());
        return true;
    }

    auto client_manager::initialize_item_attr_table() -> bool {
        return load_item_attr_table("item_attr", vec_item_attr_table_);
    }

    auto client_manager::initialize_item_rare_table() -> bool {
        return load_item_attr_table("item_attr_rare", vec_item_rare_table_);
    }

    auto client_manager::initialize_land_table() -> bool {
        using namespace building;

        const std::string query = sql::format(
            "SELECT id, map_index, x, y, width, height, guild_id, guild_level_limit, price "
            "FROM land{} WHERE enable='YES' ORDER BY id",
            sql::raw(get_table_postfix()));

        auto msg = db_manager::instance().direct_query(query);
        sql_result *res = msg->get();

        if (!vec_land_table_.empty()) {
            metin2::log::info("Reloading land");
            vec_land_table_.clear();
        }

        vec_land_table_.reserve(res->num_rows);

        MYSQL_ROW data;

        if (res->num_rows > 0)
            while ((data = mysql_fetch_row(res->res.get()))) {
                land_table t;

                memset(&t, 0, sizeof(t));

                int col = 0;

                str_to_number(t.id, data[col++]);
                str_to_number(t.map_index, data[col++]);
                str_to_number(t.x, data[col++]);
                str_to_number(t.y, data[col++]);
                str_to_number(t.width, data[col++]);
                str_to_number(t.height, data[col++]);
                str_to_number(t.guild_id, data[col++]);
                str_to_number(t.guild_level_limit, data[col++]);
                str_to_number(t.price, data[col++]);

                metin2::log::debug("Land {:d}: map {:d} at ({:d}, {:d}), {:d} x {:d}", t.id, t.map_index, t.x, t.y, t.width, t.height);

                vec_land_table_.push_back(t);
            }

        return true;
    }

    void parse_pair_number_string(const char *string, std::vector<std::pair<int, int>> &vec) {
        const char *t = string;
        const char *p = strchr(t, '/');
        std::pair<int, int> k;

        char num[32 + 1];
        char *comma;

        while (p) {
            if (is_digit(*t)) {
                strlcpy(num, t, std::min<int>(sizeof(num), (p - t) + 1));

                comma = strchr(num, ',');

                if (comma) {
                    *comma = '\0';
                    str_to_number(k.second, comma + 1);
                } else
                    k.second = 0;

                str_to_number(k.first, num);
                vec.push_back(k);
            }

            t = p + 1;
            p = strchr(t, '/');
        }

        if (is_digit(*t)) {
            strlcpy(num, t, sizeof(num));

            comma = strchr(const_cast<char *>(t), ',');

            if (comma) {
                *comma = '\0';
                str_to_number(k.second, comma + 1);
            } else
                k.second = 0;

            str_to_number(k.first, num);
            vec.push_back(k);
        }
    }

    auto client_manager::initialize_object_proto() -> bool {
        using namespace building;

        const std::string query = sql::format(
            "SELECT vnum, price, materials, upgrade_vnum, upgrade_limit_time, life, reg_1, reg_2, reg_3, reg_4, npc, group_vnum, dependent_group "
            "FROM object_proto{} ORDER BY vnum",
            sql::raw(get_table_postfix()));

        auto msg = db_manager::instance().direct_query(query);
        sql_result *res = msg->get();

        if (!vec_object_proto_.empty()) {
            metin2::log::info("Reloading object_proto");
            vec_object_proto_.clear();
        }

        vec_object_proto_.reserve(std::max<int>(0, res->num_rows));

        MYSQL_ROW data;

        if (res->num_rows > 0)
            while ((data = mysql_fetch_row(res->res.get()))) {
                object_proto t;

                memset(&t, 0, sizeof(t));

                int col = 0;

                str_to_number(t.vnum, data[col++]);
                str_to_number(t.price, data[col++]);

                std::vector<std::pair<int, int>> vec;
                parse_pair_number_string(data[col++], vec);

                for (unsigned int i = 0; i < OBJECT_MATERIAL_MAX_NUM && i < vec.size(); ++i) {
                    std::pair<int, int> &r = vec[i];

                    t.materials[i].item_vnum = r.first;
                    t.materials[i].count = r.second;
                }

                str_to_number(t.upgrade_vnum, data[col++]);
                str_to_number(t.upgrade_limit_time, data[col++]);
                str_to_number(t.life, data[col++]);
                str_to_number(t.region[0], data[col++]);
                str_to_number(t.region[1], data[col++]);
                str_to_number(t.region[2], data[col++]);
                str_to_number(t.region[3], data[col++]);

                str_to_number(t.npc_vnum, data[col++]);
                str_to_number(t.group_vnum, data[col++]);
                str_to_number(t.depend_on_group_vnum, data[col++]);

                t.npcx = 0;
                t.npcy = std::max(t.region[1], t.region[3]) + 300;

                metin2::log::debug("Object proto {:d}: price {:d}, first material vnum {:d} x{:d}",
                    t.vnum, t.price, t.materials[0].item_vnum, t.materials[0].count);

                vec_object_proto_.push_back(t);
            }

        return true;
    }

    auto client_manager::initialize_object_table() -> bool {
        using namespace building;

        const std::string query = sql::format(
            "SELECT id, land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot, life FROM object{} ORDER BY id",
            sql::raw(get_table_postfix()));

        auto msg = db_manager::instance().direct_query(query);
        sql_result *res = msg->get();

        if (!map_object_table_.empty()) {
            metin2::log::info("Reloading object");
            map_object_table_.clear();
        }

        MYSQL_ROW data;

        if (res->num_rows > 0)
            while ((data = mysql_fetch_row(res->res.get()))) {
                auto k = std::make_unique<object_table>();

                memset(k.get(), 0, sizeof(object_table));

                int col = 0;

                str_to_number(k->id, data[col++]);
                str_to_number(k->land_id, data[col++]);
                str_to_number(k->vnum, data[col++]);
                str_to_number(k->map_index, data[col++]);
                str_to_number(k->x, data[col++]);
                str_to_number(k->y, data[col++]);
                str_to_number(k->x_rot, data[col++]);
                str_to_number(k->y_rot, data[col++]);
                str_to_number(k->z_rot, data[col++]);
                str_to_number(k->life, data[col++]);

                metin2::log::debug("Object {:d} (vnum {:d}) on map {:d} at ({:d}, {:d}), life {:d}",
                    k->id, k->vnum, k->map_index, k->x, k->y, k->life);

                map_object_table_.insert(std::make_pair(k->id, std::move(k)));
            }

        return true;
    }

    auto client_manager::initialize_monarch() -> bool {
        monarch::instance().load_monarch();

        return true;
    }

    auto client_manager::mirror_mob_table_into_db() -> bool {
        for (const mob_table &t : vec_mob_table_) {
            const auto [locale_column, locale_value] = locale_name_fragments(t.locale_name);
            const std::string query = sql::format(
                "replace into mob_proto{} "
                "("
                "vnum, name{}, type, rank, battle_type, level, size, ai_flag, setRaceFlag, setImmuneFlag, "
                "on_click, empire, drop_item, resurrection_vnum, folder, "
                "st, dx, ht, iq, damage_min, damage_max, max_hp, regen_cycle, regen_percent, exp, "
                "gold_min, gold_max, def, attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item, "

                "enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate, "
                "resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow, "
                "resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, "
                "dam_multiply, summon, drain_sp, mob_color, "

                "skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, "
                "skill_vnum3, skill_level3, skill_vnum4, skill_level4, "
                "sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive"
                ") "
                "values ("

                "{}, {}{}, {}, {}, {}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, {}, {}, {}, {}, "

                "{}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, "
                "{:f}, {}, {}, {}, "

                "{}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, "
                "{}, {}, {}, {}, {}"
                ")",
                sql::raw(get_table_postfix()), sql::raw(locale_column),

                t.vnum, t.name, sql::raw(locale_value), t.type, t.rank, t.battle_type, t.level, t.size, t.ai_flag, t.race_flag, t.immune_flag,
                t.on_click_type, t.empire, t.drop_item_vnum, t.resurrection_vnum, t.folder,
                t.str, t.dex, t.con, t.iq, t.damage_range[0], t.damage_range[1], t.max_hp, t.regen_cycle, t.regen_percent, t.exp,

                t.gold_min, t.gold_max, t.def, t.s_attack_speed, t.s_moving_speed, t.aggresive_hp_pct, t.aggressive_sight, t.attack_range, t.polymorph_item_vnum,
                t.c_enchants[0], t.c_enchants[1], t.c_enchants[2], t.c_enchants[3], t.c_enchants[4], t.c_enchants[5],
                t.c_resists[0], t.c_resists[1], t.c_resists[2], t.c_resists[3], t.c_resists[4], t.c_resists[5],
                t.c_resists[6], t.c_resists[7], t.c_resists[8], t.c_resists[9], t.c_resists[10],
                t.dam_multiply, t.summon_vnum, t.drain_sp, t.mob_color,

                t.skills[0].vnum, t.skills[0].level, t.skills[1].vnum, t.skills[1].level, t.skills[2].vnum, t.skills[2].level,
                t.skills[3].vnum, t.skills[3].level, t.skills[4].vnum, t.skills[4].level,
                t.berserk_point, t.stone_skin_point, t.god_speed_point, t.death_blow_point, t.revive_point);

            db_manager::instance().async_query(query);
        }
        return true;
    }

    auto client_manager::mirror_item_table_into_db() -> bool {
        for (const item_table &t : vec_item_table_) {
            const auto [locale_column, locale_value] = locale_name_fragments(t.locale_name);
            const std::string query = sql::format(
                "replace into item_proto{} ("
                "vnum, type, subtype, name{}, gold, shop_buy_price, weight, size, "
                "flag, wearflag, antiflag, immuneflag, "
                "refined_vnum, refine_set, magic_pct, socket_pct, addon_type, specular, "
                "limittype0, limitvalue0, limittype1, limitvalue1, "
                "applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, "
                "value0, value1, value2, value3, value4, value5 ) "
                "values ("
                "{}, {}, {}, {}{}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, {}, "
                "{}, {}, {}, {}, {}, {} )",
                sql::raw(get_table_postfix()), sql::raw(locale_column),
                t.vnum, t.type, t.sub_type, t.name, sql::raw(locale_value), static_cast<int32_t>(t.gold), t.shop_buy_price, t.weight, t.size,
                // Signed columns: keep the bit pattern the old printf %d wrote.
                static_cast<int32_t>(t.flags), static_cast<int32_t>(t.wear_flags), static_cast<int32_t>(t.anti_flags), t.immune_flag,
                t.refined_vnum, t.refine_set, t.alter_to_magic_item_pct, t.gain_socket_pct, t.s_addon_type, t.specular,
                t.limits[0].type, t.limits[0].value, t.limits[1].type, t.limits[1].value,
                t.applies[0].type, t.applies[0].value, t.applies[1].type, t.applies[1].value, t.applies[2].type, t.applies[2].value,
                t.values[0], t.values[1], t.values[2], t.values[3], t.values[4], t.values[5]);
            db_manager::instance().async_query(query);
        }
        return true;
    }
}
