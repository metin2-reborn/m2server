// vim:ts=4 sw=4
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/tables/csv_reader.h>
#include <metin2/db/storage/db_manager.h>
#include <metin2/db/app/main.h>
#include <metin2/db/empire/monarch.h>
#include <metin2/db/tables/proto_reader.h>

#include <metin2/proto/building.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <mysql.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metin2::db::inline clients {


bool client_manager::initialize_tables()
{
	if (!initialize_mob_table())
	{
		metin2::log::error("InitializeMobTable FAILED");
		return false;
	}
	if (!mirror_mob_table_into_db())
	{
		metin2::log::error("MirrorMobTableIntoDB FAILED");
		return false; 
	}

	if (!initialize_item_table())
	{
		metin2::log::error("InitializeItemTable FAILED");
		return false; 
	}

	if (!mirror_item_table_into_db())
	{
		metin2::log::error("MirrorItemTableIntoDB FAILED");
		return false; 
	}

	if (!initialize_shop_table())
	{
		metin2::log::error("InitializeShopTable FAILED");
		return false;
	}

	if (!initialize_skill_table())
	{
		metin2::log::error("InitializeSkillTable FAILED");
		return false;
	}

	if (!initialize_refine_table())
	{
		metin2::log::error("InitializeRefineTable FAILED");
		return false;
	}

	if (!initialize_item_attr_table())
	{
		metin2::log::error("InitializeItemAttrTable FAILED");
		return false;
	}

	if (!initialize_item_rare_table())
	{
		metin2::log::error("InitializeItemRareTable FAILED");
		return false;
	}

	if (!initialize_banword_table())
	{
		metin2::log::error("InitializeBanwordTable FAILED");
		return false;
	}

	if (!initialize_land_table())
	{
		metin2::log::error("InitializeLandTable FAILED");
		return false;
	}

	if (!initialize_object_proto())
	{
		metin2::log::error("InitializeObjectProto FAILED");
		return false;
	}

	if (!initialize_object_table())
	{
		metin2::log::error("InitializeObjectTable FAILED");
		return false;
	}

	if (!initialize_monarch())
	{
		metin2::log::error("InitializeMonarch FAILED");
		return false;
	}


	return true;
}

bool client_manager::initialize_refine_table()
{
	char query[2048];

	snprintf(query, sizeof(query),
			"SELECT id, cost, prob, vnum0, count0, vnum1, count1, vnum2, count2,  vnum3, count3, vnum4, count4 FROM refine_proto%s",
			get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!pRes->num_rows)
		return true;

	if (refine_table_)
	{
		metin2::log::info( "RELOAD: refine_proto");
		refine_table_.reset();
	}

	refine_table_size_ = pRes->num_rows;

	refine_table_	= std::make_unique<refine_table[]>(refine_table_size_);
	memset(refine_table_.get(), 0, sizeof(refine_table) * refine_table_size_);

	refine_table* prt = refine_table_.get();
	MYSQL_ROW data;

	while ((data = mysql_fetch_row(pRes->sql_result)))
	{
		//const char* s_szQuery = "SELECT src_vnum, result_vnum, cost, prob, "
		//"vnum0, count0, vnum1, count1, vnum2, count2,  vnum3, count3, vnum4, count4 "

		int col = 0;
		str_to_number(prt->id, data[col++]);
		str_to_number(prt->cost, data[col++]);
		str_to_number(prt->prob, data[col++]);

		for (int i = 0; i < REFINE_MATERIAL_MAX_NUM; i++)
		{
			str_to_number(prt->materials[i].vnum, data[col++]);
			str_to_number(prt->materials[i].count, data[col++]);
			if (prt->materials[i].vnum == 0)
			{
				prt->material_count = i;
				break;
			}
		}

		metin2::log::info( "REFINE: id {:d} cost {:d} prob {:d} mat1 {:d} cnt1 {:d}", prt->id, prt->cost, prt->prob, prt->materials[0].vnum, prt->materials[0].count);

		prt++;
	}
	return true;
}

class f_compare_vnum
{
	public:
		bool operator () (const entity_table & a, const entity_table & b) const
		{
			return (a.vnum < b.vnum);
		}
};

bool client_manager::initialize_mob_table()
{
	//================== Function description ==================//
	//1. Summary: reads 'mob_proto.txt', 'mob_proto_test.txt' and 'mob_names.txt',
	//		and builds the (!)[mob_table] table object. (type: TMobTable)
	//2. Steps
	//	1) Read 'mob_names.txt' and build the (a)[localMap](vnum:name) map.
	//	2) From 'mob_proto_test.txt' and the (a)[localMap] map,
	//		build the (b)[test_map_mobTableByVnum](vnum:TMobTable) map.
	//	3) From 'mob_proto.txt' and the (a)[localMap] map,
	//		build the (!)[mob_table] table.
	//			<Note>
	//			For rows that exist in both 
	//			(b)[test_map_mobTableByVnum] and (!)[mob_table],
	//			the one from (b)[test_map_mobTableByVnum] is used.
	//	4) Add the rows of (b)[test_map_mobTableByVnum] that are not in (!)[mob_table].
	//3. Tests
	//	1) Whether the 'mob_proto.txt' data made it into mob_table correctly. -> done
	//	2) Whether the 'mob_names.txt' data made it into mob_table correctly.
	//	3) Whether the [overlapping] data from 'mob_proto_test.txt' made it into mob_table correctly.
	//	4) Whether the [new] data from 'mob_proto_test.txt' made it into mob_table correctly.
	//	5) (Final) Whether it works correctly in the game client.
	//_______________________________________________//


	//===============================================//
	//	1) Read 'mob_names.txt' and build the (a)[localMap] map.
	//<Build the (a)localMap map>
	std::map<int,const char*> localMap;
	//<Read file>
	c_csv_table nameData;
	if(!nameData.load("mob_names.txt",'\t'))
	{
		fprintf(stderr, "mob_names.txt 파일을 읽어오지 못했습니다\n");
	} else {
		nameData.next();	//Skip the description row.
		while(nameData.next()) {
			localMap[atoi(nameData.as_string_by_index(0))] = nameData.as_string_by_index(1);
		}
	}
	//________________________________________________//

	
	//===============================================//
	//	2) From 'mob_proto_test.txt' and the (a)localMap map,
	//		build the (b)[test_map_mobTableByVnum](vnum:TMobTable) map.
	//0. 
	std::set<int> vnumSet;	//Used to check which of the test file entries are new.
	//1. Read the file
	bool isTestFile = true;
	c_csv_table test_data;
	if(!test_data.load("mob_proto_test.txt",'\t'))
	{
		fprintf(stderr, "테스트 파일이 없습니다. 그대로 진행합니다.\n");
		isTestFile = false;
	}
	//2. Build the (c)[test_map_mobTableByVnum](vnum:TMobTable) map.
	std::map<uint32_t, mob_table *> test_map_mobTableByVnum;
	std::unique_ptr<mob_table[]> test_mob_tables;
	if (isTestFile) {
		test_data.next();	//Skip the description row.

		//a. Create the test monster table.
		mob_table * test_mob_table = nullptr;
		int test_MobTableSize = test_data.file.row_count()-1;
		test_mob_tables = std::make_unique<mob_table[]>(test_MobTableSize);
		test_mob_table = test_mob_tables.get();
		memset(test_mob_table, 0, sizeof(mob_table) * test_MobTableSize);

		//b. Fill the test monster table and also insert into the map.
		while(test_data.next()) {

			if (!set_proto_mob_table(test_mob_table, test_data, localMap))
			{
				fprintf(stderr, "몹 프로토 테이블 셋팅 실패.\n");			
			}

			test_map_mobTableByVnum.insert(std::map<uint32_t, mob_table *>::value_type(test_mob_table->vnum, test_mob_table));

			
			++test_mob_table;
			}

	}

	//	3) From 'mob_proto.txt' and the (a)[localMap] map,
	//		build the (!)[mob_table] table.
	//			<Note>
	//			For rows that exist in both 
	//			(b)[test_map_mobTableByVnum] and (!)[mob_table],
	//			the one from (b)[test_map_mobTableByVnum] is used.

	//1. Read the file.
	c_csv_table data;
	if(!data.load("mob_proto.txt",'\t')) {
		fprintf(stderr, "mob_proto.txt 파일을 읽어오지 못했습니다\n");
		return false;
	}
	data.next();					//Skip the description row
	//2. Build (!)[mob_table]
	//2.1 Count the newly added entries
	int addNumber = 0;
	while(data.next()) {
		int vnum = atoi(data.as_string_by_index(0));
		std::map<uint32_t, mob_table *>::iterator it_map_mobTable;
		it_map_mobTable = test_map_mobTableByVnum.find(vnum);
		if(it_map_mobTable != test_map_mobTableByVnum.end()) {
			addNumber++;
		}
	}
	//Move data back to the first line. (read it again;;)
	data.destroy();
	if(!data.load("mob_proto.txt",'\t'))
	{
		fprintf(stderr, "mob_proto.txt 파일을 읽어오지 못했습니다\n");
		return false;
	}
	data.next(); //Skip the top line (the item column descriptions)
	//2.2 Allocate mob_table to fit
	if (!vec_mob_table_.empty())
	{
		metin2::log::info( "RELOAD: mob_proto");
		vec_mob_table_.clear();
	}
	vec_mob_table_.resize(data.file.row_count()-1 + addNumber);
	memset(&vec_mob_table_[0], 0, sizeof(mob_table) * vec_mob_table_.size());
	mob_table * mob_table = &vec_mob_table_[0];
	//2.3 Fill in the data
	while (data.next())
	{
		int col = 0;
		//Check whether (b)[test_map_mobTableByVnum] has the same row.
		bool isSameRow = true;
		std::map<uint32_t, proto::mob_table *>::iterator it_map_mobTable;
		it_map_mobTable = test_map_mobTableByVnum.find(atoi(data.as_string_by_index(col)));
		if(it_map_mobTable == test_map_mobTableByVnum.end()) {
			isSameRow = false;
		}
		//If the same row exists, read it from (b).
		if(isSameRow) {
			proto::mob_table *tempTable = it_map_mobTable->second;

			mob_table->vnum = tempTable->vnum;
			strlcpy(mob_table->name, tempTable->name, sizeof(tempTable->name));
			strlcpy(mob_table->locale_name, tempTable->locale_name, sizeof(mob_table->locale_name));
			mob_table->rank = tempTable->rank;
			mob_table->type = tempTable->type;
			mob_table->battle_type = tempTable->battle_type;
			mob_table->level = tempTable->level;
			mob_table->size = tempTable->size;
			mob_table->ai_flag = tempTable->ai_flag;
			mob_table->race_flag = tempTable->race_flag;
			mob_table->immune_flag = tempTable->immune_flag;
			mob_table->empire = tempTable->empire;
			strlcpy(mob_table->folder, tempTable->folder, sizeof(mob_table->folder));
			mob_table->on_click_type = tempTable->on_click_type;
			mob_table->str = tempTable->str;
			mob_table->dex = tempTable->dex;
			mob_table->con = tempTable->con;
			mob_table->iq = tempTable->iq;
			mob_table->damage_range[0] = tempTable->damage_range[0];
			mob_table->damage_range[1] = tempTable->damage_range[1];
			mob_table->max_hp = tempTable->max_hp;
			mob_table->regen_cycle = tempTable->regen_cycle;
			mob_table->regen_percent = tempTable->regen_percent;
			mob_table->gold_min = tempTable->gold_min;
			mob_table->gold_max = tempTable->gold_max;
			mob_table->exp = tempTable->exp;
			mob_table->def = tempTable->def;
			mob_table->s_attack_speed = tempTable->s_attack_speed;
			mob_table->s_moving_speed = tempTable->s_moving_speed;
			mob_table->aggresive_hp_pct = tempTable->aggresive_hp_pct;
			mob_table->aggressive_sight = tempTable->aggressive_sight;
			mob_table->attack_range = tempTable->attack_range;
				
			mob_table->drop_item_vnum = tempTable->drop_item_vnum;
			mob_table->resurrection_vnum = tempTable->resurrection_vnum;
			for (int i = 0; i < MOB_ENCHANTS_MAX_NUM; ++i)
				mob_table->c_enchants[i] = tempTable->c_enchants[i];
				
			for (int i = 0; i < MOB_RESISTS_MAX_NUM; ++i)
				mob_table->c_resists[i] = tempTable->c_resists[i];
				
			mob_table->dam_multiply = tempTable->dam_multiply;
			mob_table->summon_vnum = tempTable->summon_vnum;
			mob_table->drain_sp = tempTable->drain_sp;
			mob_table->polymorph_item_vnum = tempTable->polymorph_item_vnum;
				
			
			mob_table->skills[0].level = tempTable->skills[0].level;
			mob_table->skills[0].vnum = tempTable->skills[0].vnum;
			mob_table->skills[1].level = tempTable->skills[1].level;
			mob_table->skills[1].vnum = tempTable->skills[1].vnum;
			mob_table->skills[2].level = tempTable->skills[2].level;
			mob_table->skills[2].vnum = tempTable->skills[2].vnum;
			mob_table->skills[3].level = tempTable->skills[3].level;
			mob_table->skills[3].vnum = tempTable->skills[3].vnum;
			mob_table->skills[4].level = tempTable->skills[4].level;
			mob_table->skills[4].vnum = tempTable->skills[4].vnum;
				
			mob_table->berserk_point = tempTable->berserk_point;
			mob_table->stone_skin_point = tempTable->stone_skin_point;
			mob_table->god_speed_point = tempTable->god_speed_point;
			mob_table->death_blow_point = tempTable->death_blow_point;
			mob_table->revive_point = tempTable->revive_point;
		} else {

			if (!set_proto_mob_table(mob_table, data, localMap))
			{
				fprintf(stderr, "몹 프로토 테이블 셋팅 실패.\n");			
			}

						
		}

		//Add the vnum to the set
		vnumSet.insert(mob_table->vnum);
		

		metin2::log::debug( "MOB #{:<5d} {:<24} {:<24} level: {:<3d} rank: {:d} empire: {:d}", mob_table->vnum, mob_table->name, mob_table->locale_name, mob_table->level, mob_table->rank, mob_table->empire);
		++mob_table;

	}
	//_____________________________________________________//


	//	4) Add the rows of (b)[test_map_mobTableByVnum] that are not in (!)[mob_table].
	//Read the file again.
	test_data.destroy();
	isTestFile = true;
	if(!test_data.load("mob_proto_test.txt",'\t'))
	{
		fprintf(stderr, "테스트 파일이 없습니다. 그대로 진행합니다.\n");
		isTestFile = false;
	}
	if(isTestFile) {
		test_data.next();	//Skip the description row.

		while (test_data.next())	//Go through each test entry and add the new ones.
		{
			//Skip duplicates.
			std::set<int>::iterator itVnum;
			itVnum=vnumSet.find(atoi(test_data.as_string_by_index(0)));
			if (itVnum != vnumSet.end()) {
				continue;
			}

			if (!set_proto_mob_table(mob_table, test_data, localMap))
			{
				fprintf(stderr, "몹 프로토 테이블 셋팅 실패.\n");			
			}

			metin2::log::info( "MOB #{:<5d} {:<24} {:<24} level: {:<3d} rank: {:d} empire: {:d}", mob_table->vnum, mob_table->name, mob_table->locale_name, mob_table->level, mob_table->rank, mob_table->empire);
			++mob_table;

		}
	}
	sort(vec_mob_table_.begin(), vec_mob_table_.end(), f_compare_vnum());
	return true;
}

bool client_manager::initialize_shop_table()
{
	MYSQL_ROW	data;
	int		col;

	static const char * s_szQuery = 
		"SELECT "
		"shop.vnum, "
		"shop.npc_vnum, "
		"shop_item.item_vnum, "
		"shop_item.count "
		"FROM shop LEFT JOIN shop_item "
		"ON shop.vnum = shop_item.shop_vnum ORDER BY shop.vnum, shop_item.item_vnum";

	std::unique_ptr<ql_msg> pkMsg2(db_manager::instance().direct_query(s_szQuery));

	// Careful: if a shop vnum exists but has no shop_item, it is treated as a failure.
	// Needs fixing
	ql_result * pRes2 = pkMsg2->get();

	if (!pRes2->num_rows)
	{
		metin2::log::error("InitializeShopTable : Table count is zero.");
		return false;
	}

	std::map<int, std::unique_ptr<shop_table>> map_shop;

	if (shop_table_)
	{
		shop_table_.reset();
	}

	shop_table * shop_table = shop_table_.get();

	while ((data = mysql_fetch_row(pRes2->sql_result)))
	{
		col = 0;

		int iShopVnum = 0;
		str_to_number(iShopVnum, data[col++]);

		if (map_shop.end() == map_shop.find(iShopVnum))
		{
			auto pShopTable = std::make_unique<proto::shop_table>();
			shop_table = pShopTable.get();
			*shop_table = proto::shop_table();
			shop_table->vnum	= iShopVnum;

			map_shop[iShopVnum] = std::move(pShopTable);
		}
		else
			shop_table = map_shop[iShopVnum].get();

		str_to_number(shop_table->npc_vnum, data[col++]);

		if (!data[col])	// Because NULL is returned when there are no items at all..
			continue;

		shop_item_table * pItem = &shop_table->items[shop_table->item_count];

		str_to_number(pItem->vnum, data[col++]);
		str_to_number(pItem->count, data[col++]);

		++shop_table->item_count;
	}

	shop_table_ = std::make_unique<proto::shop_table[]>(map_shop.size());
	shop_table_size_ = map_shop.size();

	__typeof(map_shop.begin()) it = map_shop.begin();

	int i = 0;

	while (it != map_shop.end())
	{
		std::memcpy((shop_table_.get() + i), (it++)->second.get(), sizeof(proto::shop_table));
		metin2::log::info( "SHOP: #{:d} items: {:d}", (shop_table_.get() + i)->vnum, (shop_table_.get() + i)->item_count);
		++i;
	}

	return true;
}

bool client_manager::initialize_item_table()
{
	//================== Function description ==================//
	//1. Summary: reads 'item_proto.txt', 'item_proto_test.txt' and 'item_names.txt',
	//		and builds the <item_table>(TItemTable) and <m_map_itemTableByVnum> objects.
	//2. Steps
	//	1) Read 'item_names.txt' and build the (a)[localMap](vnum:name) map.
	//	2) From 'item_proto_text.txt' and the (a)[localMap] map,
	//		build the (b)[test_map_itemTableByVnum](vnum:TItemTable) map.
	//	3) From 'item_proto.txt' and the (a)[localMap] map,
	//		build (!)[item_table] and <m_map_itemTableByVnum>.
	//			<Note>
	//			For rows that exist in both 
	//			(b)[test_map_itemTableByVnum] and (!)[mob_table],
	//			the one from (b)[test_map_itemTableByVnum] is used.
	//	4) Add the rows of (b)[test_map_itemTableByVnum] that are not in (!)[item_table].
	//3. Tests
	//	1) Whether the 'item_proto.txt' data made it into item_table correctly. -> done
	//	2) Whether the 'item_names.txt' data made it into item_table correctly.
	//	3) Whether the [overlapping] data from 'item_proto_test.txt' made it into item_table correctly.
	//	4) Whether the [new] data from 'item_proto_test.txt' made it into item_table correctly.
	//	5) (Final) Whether it works correctly in the game client.
	//_______________________________________________//



	//=================================================================================//
	//	1) Read 'item_names.txt' and build the (a)[localMap](vnum:name) map.
	//=================================================================================//
	std::map<int,const char*> localMap;
	c_csv_table nameData;
	if(!nameData.load("item_names.txt",'\t'))
	{
		fprintf(stderr, "item_names.txt 파일을 읽어오지 못했습니다\n");
	} else {
		nameData.next();
		while(nameData.next()) {
			localMap[atoi(nameData.as_string_by_index(0))] = nameData.as_string_by_index(1);
		}
	}
	//_________________________________________________________________//

	//=================================================================//
	//	2) From 'item_proto_text.txt' and the (a)[localMap] map,
	//		build the (b)[test_map_itemTableByVnum](vnum:TItemTable) map.
	//=================================================================//
	std::map<uint32_t, item_table *> test_map_itemTableByVnum;
	std::unique_ptr<item_table[]> test_item_tables;
	//1. Read the file.
	c_csv_table test_data;
	if(!test_data.load("item_proto_test.txt",'\t'))
	{
		fprintf(stderr, "item_proto_test.txt 파일을 읽어오지 못했습니다\n");
	} else {
		test_data.next();	//Skip the description row.

		//2. Create the test item table.
		item_table * test_item_table = nullptr;
		int test_itemTableSize = test_data.file.row_count()-1;
		test_item_tables = std::make_unique<item_table[]>(test_itemTableSize);
		test_item_table = test_item_tables.get();
		memset(test_item_table, 0, sizeof(item_table) * test_itemTableSize);

		//3. Fill the test item table and also insert into the map.
		while(test_data.next()) {


			if (!set_proto_item_table(test_item_table, test_data, localMap))
			{
				fprintf(stderr, "아이템 프로토 테이블 셋팅 실패.\n");			
			}

			test_map_itemTableByVnum.insert(std::map<uint32_t, item_table *>::value_type(test_item_table->vnum, test_item_table));
			test_item_table++;

		}
	}
	//______________________________________________________________________//


	//========================================================================//
	//	3) From 'item_proto.txt' and the (a)[localMap] map,
	//		build (!)[item_table] and <m_map_itemTableByVnum>.
	//			<Note>
	//			For rows that exist in both 
	//			(b)[test_map_itemTableByVnum] and (!)[mob_table],
	//			the one from (b)[test_map_itemTableByVnum] is used.
	//========================================================================//

	//Set of vnums. Used to identify new test items.
	std::set<int> vnumSet;

	//Read the file.
	c_csv_table data;
	if(!data.load("item_proto.txt",'\t'))
	{
		fprintf(stderr, "item_proto.txt 파일을 읽어오지 못했습니다\n");
		return false;
	}
	data.next(); //Skip the top line (the item column descriptions)

	if (!vec_item_table_.empty())
	{
		metin2::log::info( "RELOAD: item_proto");
		vec_item_table_.clear();
		map_item_table_by_vnum_.clear();
	}

	//===== Build the item table =====//
	//Count the newly added entries.
	int addNumber = 0;
	while(data.next()) {
		int vnum = atoi(data.as_string_by_index(0));
		std::map<uint32_t, item_table *>::iterator it_map_itemTable;
		it_map_itemTable = test_map_itemTableByVnum.find(vnum);
		if(it_map_itemTable != test_map_itemTableByVnum.end()) {
			addNumber++;
		}
	}
	//Move data back to the first line. (read it again;;)
	data.destroy();
	if(!data.load("item_proto.txt",'\t'))
	{
		fprintf(stderr, "item_proto.txt 파일을 읽어오지 못했습니다\n");
		return false;
	}
	data.next(); //Skip the top line (the item column descriptions)

	vec_item_table_.resize(data.file.row_count() - 1 + addNumber);
	memset(&vec_item_table_[0], 0, sizeof(item_table) * vec_item_table_.size());

	item_table * item_table = &vec_item_table_[0];

	while (data.next())
	{
		int col = 0;

		std::map<uint32_t, proto::item_table *>::iterator it_map_itemTable;
		it_map_itemTable = test_map_itemTableByVnum.find(atoi(data.as_string_by_index(col)));
		if(it_map_itemTable == test_map_itemTableByVnum.end()) {
			//Store each column's data
			
			if (!set_proto_item_table(item_table, data, localMap))
			{
				fprintf(stderr, "아이템 프로토 테이블 셋팅 실패.\n");			
			}


			
		} else {	//$$$$$$$$$$$$$$$$$$$$$$$ There is test item data!	
			proto::item_table *tempTable = it_map_itemTable->second;

			item_table->vnum = tempTable->vnum;
			strlcpy(item_table->name, tempTable->name, sizeof(item_table->name));
			strlcpy(item_table->locale_name, tempTable->locale_name, sizeof(item_table->locale_name));
			item_table->type = tempTable->type;
			item_table->sub_type = tempTable->sub_type;
			item_table->size = tempTable->size;
			item_table->anti_flags = tempTable->anti_flags;
			item_table->flags = tempTable->flags;
			item_table->wear_flags = tempTable->wear_flags;
			item_table->immune_flag = tempTable->immune_flag;
			item_table->gold = tempTable->gold;
			item_table->shop_buy_price = tempTable->shop_buy_price;
			item_table->refined_vnum =tempTable->refined_vnum;
			item_table->refine_set =tempTable->refine_set;
			item_table->alter_to_magic_item_pct = tempTable->alter_to_magic_item_pct;
			item_table->c_limit_real_time_first_use_index = -1;
			item_table->c_limit_timer_based_on_wear_index = -1;

			int i;

			for (i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			{
				item_table->limits[i].type = tempTable->limits[i].type;
				item_table->limits[i].value = tempTable->limits[i].value;

				if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->limits[i].type)
					item_table->c_limit_real_time_first_use_index = (char)i;

				if (LIMIT_TIMER_BASED_ON_WEAR == item_table->limits[i].type)
					item_table->c_limit_timer_based_on_wear_index = (char)i;
			}

			for (i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			{
				item_table->applies[i].type = tempTable->applies[i].type;
				item_table->applies[i].value = tempTable->applies[i].value;
			}

			for (i = 0; i < ITEM_VALUES_MAX_NUM; ++i)
				item_table->values[i] = tempTable->values[i];

			item_table->gain_socket_pct = tempTable->gain_socket_pct;
			item_table->s_addon_type = tempTable->s_addon_type;

			item_table->weight  = tempTable->weight;

		}
		vnumSet.insert(item_table->vnum);
		map_item_table_by_vnum_.insert(std::map<uint32_t, proto::item_table *>::value_type(item_table->vnum, item_table));
		++item_table;
	}
	//_______________________________________________________________________//

	//========================================================================//
	//	4) Add the rows of (b)[test_map_itemTableByVnum] that are not in (!)[item_table].
	//========================================================================//
	test_data.destroy();
	if(!test_data.load("item_proto_test.txt",'\t'))
	{
		fprintf(stderr, "item_proto_test.txt 파일을 읽어오지 못했습니다\n");
	} else {
		test_data.next();	//Skip the description row.

		while (test_data.next())	//Go through each test entry and add the new ones.
		{
			//Skip duplicates.
			std::set<int>::iterator itVnum;
			itVnum=vnumSet.find(atoi(test_data.as_string_by_index(0)));
			if (itVnum != vnumSet.end()) {
				continue;
			}
			
			if (!set_proto_item_table(item_table, test_data, localMap))
			{
				fprintf(stderr, "아이템 프로토 테이블 셋팅 실패.\n");			
			}


			map_item_table_by_vnum_.insert(std::map<uint32_t, proto::item_table *>::value_type(item_table->vnum, item_table));

			item_table++;

		}
	}



	// QUEST_ITEM_PROTO_DISABLE
	// END_OF_QUEST_ITEM_PROTO_DISABLE

	map_item_table_by_vnum_.clear();

	auto it = vec_item_table_.begin();

	while (it != vec_item_table_.end())
	{
		proto::item_table * item_table = &(*(it++));

		metin2::log::debug( "ITEM: #{:<5d} {:<24} {:<24} VAL: {:d} {:d} {:d} {:d} {:d} {:d} WEAR {:d} ANTI {:d} IMMUNE {:d} REFINE {:d} REFINE_SET {:d} MAGIC_PCT {:d}",
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

		map_item_table_by_vnum_.insert(std::map<uint32_t, proto::item_table *>::value_type(item_table->vnum, item_table));
	}
	sort(vec_item_table_.begin(), vec_item_table_.end(), f_compare_vnum());
	return true;
}


bool client_manager::initialize_skill_table()
{
	char query[4096];
	snprintf(query, sizeof(query),
		"SELECT dwVnum, szName, bType, bMaxLevel, dwSplashRange, "
		"szPointOn, szPointPoly, szSPCostPoly, szDurationPoly, szDurationSPCostPoly, "
		"szCooldownPoly, szMasterBonusPoly, setFlag+0, setAffectFlag+0, "
		"szPointOn2, szPointPoly2, szDurationPoly2, setAffectFlag2+0, "
		"szPointOn3, szPointPoly3, szDurationPoly3, szGrandMasterAddSPCostPoly, "
		"bLevelStep, bLevelLimit, prerequisiteSkillVnum, prerequisiteSkillLevel, iMaxHit, szSplashAroundDamageAdjustPoly, eSkillType+0, dwTargetRange "
		"FROM skill_proto%s ORDER BY dwVnum",
		get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!pRes->num_rows)
	{
		metin2::log::error("no result from skill_proto");
		return false;
	}

	if (!vec_skill_table_.empty())
	{
		metin2::log::info( "RELOAD: skill_proto");
		vec_skill_table_.clear();
	}

	vec_skill_table_.reserve(pRes->num_rows);

	MYSQL_ROW	data;
	int		col;

	while ((data = mysql_fetch_row(pRes->sql_result)))
	{
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

		// ADD_GRANDMASTER_SKILL
		strlcpy(t.point_on3, data[col++], sizeof(t.point_on3));
		strlcpy(t.point_poly3, data[col++], sizeof(t.point_poly3));
		strlcpy(t.duration_poly3, data[col++], sizeof(t.duration_poly3));

		strlcpy(t.grand_master_add_sp_cost_poly, data[col++], sizeof(t.grand_master_add_sp_cost_poly));
		// END_OF_ADD_GRANDMASTER_SKILL

		str_to_number(t.level_step, data[col++]);
		str_to_number(t.level_limit, data[col++]);
		str_to_number(t.pre_skill_vnum, data[col++]);
		str_to_number(t.pre_skill_level, data[col++]);

		str_to_number(t.max_hit, data[col++]);

		strlcpy(t.splash_around_damage_adjust_poly, data[col++], sizeof(t.splash_around_damage_adjust_poly));

		str_to_number(t.skill_attr_type, data[col++]);
		str_to_number(t.target_range, data[col++]);

		metin2::log::info( "SKILL: #{:d} {} flag {:d} point {} affect {:d} cooldown {}", t.vnum, t.name, t.flag, t.point_on, t.affect_flag, t.cooldown_poly);

		vec_skill_table_.push_back(t);
	}

	return true;
}

bool client_manager::initialize_banword_table()
{
	vec_banword_table_.clear();

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query("SELECT word FROM banword"));

	ql_result * pRes = pkMsg->get();

	if (pRes->num_rows == 0)
		return true;

	MYSQL_ROW data;

	while ((data = mysql_fetch_row(pRes->sql_result)))
	{
		banword_table t;

		if (data[0])
		{
			strlcpy(t.word, data[0], sizeof(t.word));
			vec_banword_table_.push_back(t);
		}
	}

	metin2::log::info( "BANWORD: total {:d}", vec_banword_table_.size());
	return true;
}

bool client_manager::initialize_item_attr_table()
{
	char query[4096];
	snprintf(query, sizeof(query),
			"SELECT apply, apply+0, prob, lv1, lv2, lv3, lv4, lv5, weapon, body, wrist, foots, neck, head, shield, ear FROM item_attr%s ORDER BY apply",
			get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!pRes->num_rows)
	{
		metin2::log::error("no result from item_attr");
		return false;
	}

	if (!vec_item_attr_table_.empty())
	{
		metin2::log::info( "RELOAD: item_attr");
		vec_item_attr_table_.clear();
	}

	vec_item_attr_table_.reserve(pRes->num_rows);

	MYSQL_ROW	data;

	while ((data = mysql_fetch_row(pRes->sql_result)))
	{
		item_attr_table t;

		t = item_attr_table();

		int col = 0;

		strlcpy(t.apply, data[col++], sizeof(t.apply));
		str_to_number(t.apply_index, data[col++]);
		str_to_number(t.prob, data[col++]);
		str_to_number(t.values[0], data[col++]);
		str_to_number(t.values[1], data[col++]);
		str_to_number(t.values[2], data[col++]);
		str_to_number(t.values[3], data[col++]);
		str_to_number(t.values[4], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_WEAPON], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_BODY], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_WRIST], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_FOOTS], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_NECK], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_HEAD], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_SHIELD], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_EAR], data[col++]);

		metin2::log::info( "ITEM_ATTR: {:<20} {:4d} {{ {:3d} {:3d} {:3d} {:3d} {:3d} }} {{ {:d} {:d} {:d} {:d} {:d} {:d} {:d} {:d} }}",
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

		vec_item_attr_table_.push_back(t);
	}

	return true;
}

bool client_manager::initialize_item_rare_table()
{
	char query[4096];
	snprintf(query, sizeof(query),
			"SELECT apply, apply+0, prob, lv1, lv2, lv3, lv4, lv5, weapon, body, wrist, foots, neck, head, shield, ear FROM item_attr_rare%s ORDER BY apply",
			get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!pRes->num_rows)
	{
		metin2::log::error("no result from item_attr_rare");
		return false;
	}

	if (!vec_item_rare_table_.empty())
	{
		metin2::log::info( "RELOAD: item_attr_rare");
		vec_item_rare_table_.clear();
	}

	vec_item_rare_table_.reserve(pRes->num_rows);

	MYSQL_ROW	data;

	while ((data = mysql_fetch_row(pRes->sql_result)))
	{
		item_attr_table t;

		t = item_attr_table();

		int col = 0;

		strlcpy(t.apply, data[col++], sizeof(t.apply));
		str_to_number(t.apply_index, data[col++]);
		str_to_number(t.prob, data[col++]);
		str_to_number(t.values[0], data[col++]);
		str_to_number(t.values[1], data[col++]);
		str_to_number(t.values[2], data[col++]);
		str_to_number(t.values[3], data[col++]);
		str_to_number(t.values[4], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_WEAPON], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_BODY], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_WRIST], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_FOOTS], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_NECK], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_HEAD], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_SHIELD], data[col++]);
		str_to_number(t.max_level_by_set[ATTRIBUTE_SET_EAR], data[col++]);

		metin2::log::info( "ITEM_RARE: {:<20} {:4d} {{ {:3d} {:3d} {:3d} {:3d} {:3d} }} {{ {:d} {:d} {:d} {:d} {:d} {:d} {:d} {:d} }}",
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

		vec_item_rare_table_.push_back(t);
	}

	return true;
}

bool client_manager::initialize_land_table()
{
	using namespace building;

	char query[4096];

	snprintf(query, sizeof(query),
		"SELECT id, map_index, x, y, width, height, guild_id, guild_level_limit, price "
		"FROM land%s WHERE enable='YES' ORDER BY id",
		get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!vec_k_land_table_.empty())
	{
		metin2::log::info( "RELOAD: land");
		vec_k_land_table_.clear();
	}

	vec_k_land_table_.reserve(pRes->num_rows);

	MYSQL_ROW	data;

	if (pRes->num_rows > 0)
		while ((data = mysql_fetch_row(pRes->sql_result)))
		{
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

			metin2::log::info( "LAND: {:d} map {:<4d} {:7d}x{:<7d} w {:<4d} h {:<4d}", t.id, t.map_index, t.x, t.y, t.width, t.height);

			vec_k_land_table_.push_back(t);
		}

	return true;
}

void parse_pair_number_string(const char * string, std::vector<std::pair<int, int> > & vec)
{
	// format: 10,1/20,3/300,50
	const char * t = string;
	const char * p = strchr(t, '/');
	std::pair<int, int> k;

	char szNum[32 + 1];
	char * comma;

	while (p)
	{
		if (is_digit(*t))
		{
			strlcpy(szNum, t, std::min<int>(sizeof(szNum), (p-t)+1));

			comma = strchr(szNum, ',');

			if (comma)
			{
				*comma = '\0';
				str_to_number(k.second, comma+1);
			}
			else
				k.second = 0;

			str_to_number(k.first, szNum);
			vec.push_back(k);
		}

		t = p + 1;
		p = strchr(t, '/');
	}

	if (is_digit(*t))
	{
		strlcpy(szNum, t, sizeof(szNum));

		comma = strchr(const_cast<char*>(t), ',');

		if (comma)
		{
			*comma = '\0';
			str_to_number(k.second, comma+1);
		}
		else
			k.second = 0;

		str_to_number(k.first, szNum);
		vec.push_back(k);
	}
}

bool client_manager::initialize_object_proto()
{
	using namespace building;

	char query[4096];
	snprintf(query, sizeof(query),
			"SELECT vnum, price, materials, upgrade_vnum, upgrade_limit_time, life, reg_1, reg_2, reg_3, reg_4, npc, group_vnum, dependent_group "
			"FROM object_proto%s ORDER BY vnum",
			get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!vec_k_object_proto_.empty())
	{
		metin2::log::info( "RELOAD: object_proto");
		vec_k_object_proto_.clear();
	}

	vec_k_object_proto_.reserve(std::max<int>(0, pRes->num_rows));

	MYSQL_ROW	data;

	if (pRes->num_rows > 0)
		while ((data = mysql_fetch_row(pRes->sql_result)))
		{
			object_proto t;

			memset(&t, 0, sizeof(t));

			int col = 0;

			str_to_number(t.vnum, data[col++]);
			str_to_number(t.price, data[col++]);

			std::vector<std::pair<int, int> > vec;
			parse_pair_number_string(data[col++], vec);

			for (unsigned int i = 0; i < OBJECT_MATERIAL_MAX_NUM && i < vec.size(); ++i)
			{
				std::pair<int, int> & r = vec[i];

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

			// ADD_BUILDING_NPC
			str_to_number(t.npc_vnum, data[col++]);
			str_to_number(t.group_vnum, data[col++]);
			str_to_number(t.depend_on_group_vnum, data[col++]);

			t.npcx = 0;
			t.npcy = std::max(t.region[1], t.region[3])+300;
			// END_OF_ADD_BUILDING_NPC

			metin2::log::info( "OBJ_PROTO: vnum {:d} price {:d} mat {:d} {:d}",
					t.vnum, t.price, t.materials[0].item_vnum, t.materials[0].count);

			vec_k_object_proto_.push_back(t);
		}

	return true;
}

bool client_manager::initialize_object_table()
{
	using namespace building;

	char query[4096];
	snprintf(query, sizeof(query), "SELECT id, land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot, life FROM object%s ORDER BY id", get_table_postfix());

	std::unique_ptr<ql_msg> pkMsg(db_manager::instance().direct_query(query));
	ql_result * pRes = pkMsg->get();

	if (!map_pk_object_table_.empty())
	{
		metin2::log::info( "RELOAD: object");
		map_pk_object_table_.clear();
	}

	MYSQL_ROW data;

	if (pRes->num_rows > 0)
		while ((data = mysql_fetch_row(pRes->sql_result)))
		{
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

			metin2::log::info( "OBJ: {:d} vnum {:d} map {:<4d} {:7d}x{:<7d} life {:d}",
					k->id, k->vnum, k->map_index, k->x, k->y, k->life);

			map_pk_object_table_.insert(std::make_pair(k->id, std::move(k)));
		}

	return true;
}

bool client_manager::initialize_monarch()
{
	monarch::instance().load_monarch();

	return true;
}

bool client_manager::mirror_mob_table_into_db()
{
	for (auto it = vec_mob_table_.begin(); it != vec_mob_table_.end(); it++)
	{
		const mob_table& t = *it;
		std::string query;
		if (locale_name_column == "name")
		{
			query = sql::format(
				"replace into mob_proto{} "
				"("
				"vnum, name, type, rank, battle_type, level, size, ai_flag, setRaceFlag, setImmuneFlag, "
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

				"{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, " 
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
				sql::raw(get_table_postfix()),

				t.vnum, t.name, /*t.szLocaleName, */t.type, t.rank, t.battle_type, t.level, t.size, t.ai_flag, t.race_flag, t.immune_flag,
				t.on_click_type, t.empire, t.drop_item_vnum, t.resurrection_vnum, t.folder,
				t.str, t.dex, t.con, t.iq, t.damage_range[0], t.damage_range[1], t.max_hp, t.regen_cycle, t.regen_percent, t.exp,

				t.gold_min, t.gold_max, t.def, t.s_attack_speed, t.s_moving_speed, t.aggresive_hp_pct, t.aggressive_sight, t.attack_range, t.polymorph_item_vnum,
				t.c_enchants[0], t.c_enchants[1], t.c_enchants[2], t.c_enchants[3], t.c_enchants[4], t.c_enchants[5],
				t.c_resists[0], t.c_resists[1], t.c_resists[2], t.c_resists[3], t.c_resists[4], t.c_resists[5],
				t.c_resists[6], t.c_resists[7], t.c_resists[8], t.c_resists[9], t.c_resists[10], 
				t.dam_multiply, t.summon_vnum, t.drain_sp, t.mob_color, 

				t.skills[0].vnum, t.skills[0].level, t.skills[1].vnum, t.skills[1].level, t.skills[2].vnum, t.skills[2].level, 
				t.skills[3].vnum, t.skills[3].level, t.skills[4].vnum, t.skills[4].level, 
				t.berserk_point, t.stone_skin_point, t.god_speed_point, t.death_blow_point, t.revive_point
				);
		}
		else
		{
			query = sql::format(
				"replace into mob_proto{} "
				"("
				"vnum, name, {}, type, rank, battle_type, level, size, ai_flag, setRaceFlag, setImmuneFlag, "
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

				"{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, " 
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
				sql::raw(get_table_postfix()), sql::raw(locale_name_column),

				t.vnum, t.name, t.locale_name, t.type, t.rank, t.battle_type, t.level, t.size, t.ai_flag, t.race_flag, t.immune_flag,
				t.on_click_type, t.empire, t.drop_item_vnum, t.resurrection_vnum, t.folder,
				t.str, t.dex, t.con, t.iq, t.damage_range[0], t.damage_range[1], t.max_hp, t.regen_cycle, t.regen_percent, t.exp,

				t.gold_min, t.gold_max, t.def, t.s_attack_speed, t.s_moving_speed, t.aggresive_hp_pct, t.aggressive_sight, t.attack_range, t.polymorph_item_vnum,
				t.c_enchants[0], t.c_enchants[1], t.c_enchants[2], t.c_enchants[3], t.c_enchants[4], t.c_enchants[5],
				t.c_resists[0], t.c_resists[1], t.c_resists[2], t.c_resists[3], t.c_resists[4], t.c_resists[5],
				t.c_resists[6], t.c_resists[7], t.c_resists[8], t.c_resists[9], t.c_resists[10], 
				t.dam_multiply, t.summon_vnum, t.drain_sp, t.mob_color, 

				t.skills[0].vnum, t.skills[0].level, t.skills[1].vnum, t.skills[1].level, t.skills[2].vnum, t.skills[2].level, 
				t.skills[3].vnum, t.skills[3].level, t.skills[4].vnum, t.skills[4].level, 
				t.berserk_point, t.stone_skin_point, t.god_speed_point, t.death_blow_point, t.revive_point
				);
		}

		db_manager::instance().async_query(query.c_str());
	}
	return true;
}

bool client_manager::mirror_item_table_into_db()
{
	for (auto it = vec_item_table_.begin(); it != vec_item_table_.end(); it++)
	{
		if (locale_name_column != "name")
		{
			const item_table& t = *it;
			const std::string query = sql::format(
				"replace into item_proto{} ("
				"vnum, type, subtype, name, {}, gold, shop_buy_price, weight, size, "
				"flag, wearflag, antiflag, immuneflag, "
				"refined_vnum, refine_set, magic_pct, socket_pct, addon_type, specular, "
				"limittype0, limitvalue0, limittype1, limitvalue1, "
				"applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, "
				"value0, value1, value2, value3, value4, value5 ) "
				"values ("
				"{}, {}, {}, {}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {} )",
				sql::raw(get_table_postfix()), sql::raw(locale_name_column), 
				t.vnum, t.type, t.sub_type, t.name, t.locale_name, static_cast<int32_t>(t.gold), t.shop_buy_price, t.weight, t.size,
				// signed int columns: keep the bit pattern as the old printf %d did
				static_cast<int32_t>(t.flags), static_cast<int32_t>(t.wear_flags), static_cast<int32_t>(t.anti_flags), t.immune_flag, 
				t.refined_vnum, t.refine_set, t.alter_to_magic_item_pct, t.gain_socket_pct, t.s_addon_type, t.specular,
				t.limits[0].type, t.limits[0].value, t.limits[1].type, t.limits[1].value,
				t.applies[0].type, t.applies[0].value, t.applies[1].type, t.applies[1].value, t.applies[2].type, t.applies[2].value,
				t.values[0], t.values[1], t.values[2], t.values[3], t.values[4], t.values[5]);
			db_manager::instance().async_query(query.c_str());
		}
		else
		{
			const item_table& t = *it;
			const std::string query = sql::format(
				"replace into item_proto{} ("
				"vnum, type, subtype, name, gold, shop_buy_price, weight, size, "
				"flag, wearflag, antiflag, immuneflag, "
				"refined_vnum, refine_set, magic_pct, socket_pct, addon_type, specular, "
				"limittype0, limitvalue0, limittype1, limitvalue1, "
				"applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, "
				"value0, value1, value2, value3, value4, value5 ) "
				"values ("
				"{}, {}, {}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {} )",
				sql::raw(get_table_postfix()), 
				t.vnum, t.type, t.sub_type, t.name, static_cast<int32_t>(t.gold), t.shop_buy_price, t.weight, t.size,
				// signed int columns: keep the bit pattern as the old printf %d did
				static_cast<int32_t>(t.flags), static_cast<int32_t>(t.wear_flags), static_cast<int32_t>(t.anti_flags), t.immune_flag, 
				t.refined_vnum, t.refine_set, t.alter_to_magic_item_pct, t.gain_socket_pct, t.s_addon_type, t.specular,
				t.limits[0].type, t.limits[0].value, t.limits[1].type, t.limits[1].value,
				t.applies[0].type, t.applies[0].value, t.applies[1].type, t.applies[1].value, t.applies[2].type, t.applies[2].value,
				t.values[0], t.values[1], t.values[2], t.values[3], t.values[4], t.values[5]);
			db_manager::instance().async_query(query.c_str());
		}
	}
	return true;
}
}
