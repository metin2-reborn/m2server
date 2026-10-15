#include <metin2/config/config.h>
#include <metin2/game/guilds/building.h>
#include <metin2/proto/building.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <numbers>
#include <utility>

namespace metin2::game::inline guilds {

enum
{
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL = 14061,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM = 14062,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE = 14063,

	FLAG_VNUM = 14200,
	WALL_DOOR_VNUM	= 14201,
	WALL_BACK_VNUM	= 14202,
	WALL_LEFT_VNUM	= 14203,
	WALL_RIGHT_VNUM	= 14204,
};

using namespace building;

object::object(object_table * data, object_proto * proto)
	: proto_(proto), vid_(0), ch_npc_(nullptr)
{
	entity::initialize(ENTITY_OBJECT);

	std::memcpy(&data_, data, sizeof(object_table));
}

object::~object()
{
	destroy();
}

void object::destroy()
{
	if (proto_)
	{
		sectree_manager::instance().for_attr_region(map_index(),
				x() + proto_->region[0],
				y() + proto_->region[1],
				x() + proto_->region[2],
				y() + proto_->region[3],
				(int32_t)data_.z_rot,
				ATTR_OBJECT,
				ATTR_REGION_MODE_REMOVE);
	}

	entity::destroy();

	if (get_sectree())
		get_sectree()->remove_entity(this);


	remove_special_effect();
}

void object::reconstruct(uint32_t vnum)
{
	const map_region * r = sectree_manager::instance().get_map_region(data_.map_index);
	if (!r)
		return;

	land* pLand = get_land();
	pLand->request_delete_object(id());
	pLand->request_create_object(vnum, data_.map_index, data_.x - r->sx, data_.y - r->sy, data_.x_rot, data_.y_rot, data_.z_rot, false);
}

void object::encode_insert_packet(entity * entity)
{
	desc * d;

	if (!(d = entity->get_desc()))
		return;

	metin2::log::info( "ObjectInsertPacket vid {:d} vnum {:d} rot {:f} {:f} {:f}", 
			vid_, data_.vnum, data_.x_rot, data_.y_rot, data_.z_rot);

	packet_gc_character_add pack;

	memset(&pack, 0, sizeof(packet_gc_character_add));

	pack.header         = HEADER_GC_CHARACTER_ADD;
	pack.vid          = vid_;
	pack.type          = CHAR_TYPE_BUILDING;
	pack.angle          = data_.z_rot;
	pack.x              = x();
	pack.y              = y();
	pack.z              = z();
	pack.race_num       = data_.vnum;

	pack.affect_flag[0] = unsigned(data_.x_rot);
	pack.affect_flag[1] = unsigned(data_.y_rot);


	if (get_land())
	{
	}

	d->packet(&pack, sizeof(pack));
}

void object::encode_remove_packet(entity * entity)
{
	desc * d;

	if (!(d = entity->get_desc()))
		return;

	metin2::log::info( "ObjectRemovePacket vid {:d}", vid_);

	packet_gc_character_delete pack;

	pack.header = HEADER_GC_CHARACTER_DEL;
	pack.id     = vid_;

	d->packet(&pack, sizeof(packet_gc_character_delete));
}

void object::set_vid(uint32_t vid)
{
	vid_ = vid;
}

auto object::show(int32_t map_index_value, int32_t x, int32_t y) -> bool
{
	sectree * tree = sectree_manager::instance().get(map_index_value, x, y);

	if (!tree)
	{
		metin2::log::error("cannot find sectree by {:d}x{:d} mapindex {:d}", x, y, map_index_value);
		return false;
	}

	if (get_sectree())
	{
		get_sectree()->remove_entity(this);
		view_cleanup();
	}

	data_.map_index = map_index_value;
	data_.x = x;
	data_.y = y;

	save();

	set_map_index(map_index_value);
	set_xyz(x, y, 0);

	tree->insert_entity(this);
	update_sectree();

	sectree_manager::instance().for_attr_region(map_index_value,
			x + proto_->region[0],
			y + proto_->region[1],
			x + proto_->region[2],
			y + proto_->region[3],
			(int32_t)data_.z_rot,
			ATTR_OBJECT,
			ATTR_REGION_MODE_SET);

	return true;
}

void object::save()
{
}

void object::apply_special_effect()
{
	if (proto_)
	{
		if (proto_->vnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL ||
				proto_->vnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM ||
				proto_->vnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE)
		{
			land* pLand = get_land();
			uint32_t guild_id = 0;
			if (pLand)
				guild_id = pLand->owner();
			guild* pGuild = guild_manager::instance().find_guild(guild_id);
			if (pGuild)
			{
				switch (proto_->vnum)
				{
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL:
						pGuild->set_member_count_bonus(6);
						break;
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM:
						pGuild->set_member_count_bonus(12);
						break;
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE:
						pGuild->set_member_count_bonus(18);
						break;
				}
				if (map_allow_find(pLand->map_index()))
				{
					pGuild->broadcast_member_count_bonus();
				}
			}
		}
	}
}

void object::remove_special_effect()
{
	if (proto_)
	{
		if (proto_->vnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL ||
				proto_->vnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM ||
				proto_->vnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE)
		{
			land* pLand = get_land();
			uint32_t guild_id = 0;
			if (pLand)
				guild_id = pLand->owner();
			guild* pGuild = guild_manager::instance().find_guild(guild_id);
			if (pGuild)
			{
				pGuild->set_member_count_bonus(0);
				if (map_allow_find(pLand->map_index()))
					pGuild->broadcast_member_count_bonus();
			}
		}
	}
}

void object::regen_npc()
{
	if (!proto_)
		return;

	if (!proto_->npc_vnum)
		return;

	if (!land_)
		return;

	uint32_t dwGuildID = land_->owner();
	guild* pGuild = guild_manager::instance().find_guild(dwGuildID);

	if (!pGuild)
		return;

	int x_value = proto_->npcx;
	int y_value = proto_->npcy;
	int newX, newY;

	float rot = data_.z_rot * 2.0f * std::numbers::pi / 360.0f;

	newX = (int)(( x_value * cosf(rot)) + ( y_value * sinf(rot)));
	newY = (int)(( y_value * cosf(rot)) - ( x_value * sinf(rot)));

	ch_npc_ = character_manager::instance().spawn_mob(proto_->npc_vnum,
			map_index(),
			x() + newX,
			y() + newY,
			z(),
			false,
			(int)data_.z_rot);


	if (!ch_npc_)
	{
		metin2::log::error("Cannot create guild npc");
		return;
	}

	ch_npc_->set_guild(pGuild);

	if ( proto_->vnum == 14061 || proto_->vnum == 14062 || proto_->vnum == 14063 )
	{
		quest::pc* pPC = quest::quest_manager::instance().get_pc(pGuild->master_pid());

		if ( pPC != nullptr )
		{
			pPC->set_flag("alter_of_power.build_level", pGuild->level());
		}
	}
}


land::land(land_table * data)
{
	std::memcpy(&data_, data, sizeof(land_table));
}

land::~land()
{
	destroy();
}

void land::destroy()
{
	auto it = map_pk_object_.begin();

	while (it != map_pk_object_.end())
	{
		object * pkObj = (it++)->second;
		manager::instance().unregister_object(pkObj);
		delete pkObj;
	}

	map_pk_object_.clear();
	map_pk_object_by_vid_.clear();
}

auto land::data() -> const land_table &
{
	return data_;
}

void land::put_data(const land_table * data)
{
	memcpy(&data_, data, sizeof(land_table));

	if (data_.guild_id)
	{
		const map_region * r = sectree_manager::instance().get_map_region(data_.map_index);

		if (r)
		{
			character_vector_interactor i;

			if (character_manager::instance().characters_by_race_num(20040, i))
			{
				character_vector_interactor::iterator it = i.begin();

				while (it != i.end())
				{
					character * ch = *(it++);

					if (ch->map_index() != data_.map_index)
						continue;

					int x = ch->x() - r->sx;
					int y = ch->y() - r->sy;

					if (x > data_.x + data_.width || x < data_.x)
						continue;

					if (y > data_.y + data_.height || y < data_.y)
						continue;

					character_manager::instance().destroy_character(ch);
				}
			}
		}
	}
}

void land::insert_object(object * obj)
{
	map_pk_object_.insert(std::make_pair(obj->id(), obj));
	map_pk_object_by_vid_.insert(std::make_pair(obj->get_vid(), obj));

	obj->set_land(this);
}

auto land::find_object(uint32_t id) -> object *
{
	std::map<uint32_t, object *>::iterator it = map_pk_object_.find(id);

	if (it == map_pk_object_.end())
		return nullptr;

	return it->second;
}

auto land::find_object_by_group(uint32_t group_vnum) -> object *
{
	std::map<uint32_t, object *>::iterator it;
	for (it = map_pk_object_.begin(); it != map_pk_object_.end(); ++it)
	{
		object * pObj = it->second;
		if (pObj->group() == group_vnum)
			return pObj;
	}

	return nullptr;
}

auto land::find_object_by_npc(character * npc) -> object *
{
	if (!npc)
		return nullptr;

	std::map<uint32_t, object *>::iterator it;
	for (it = map_pk_object_.begin(); it != map_pk_object_.end(); ++it)
	{
		object * pObj = it->second;
		if (pObj->npc() == npc)
			return pObj;
	}

	return nullptr;
}

auto land::find_object_by_vid(uint32_t vid) -> object *
{
	std::map<uint32_t, object *>::iterator it = map_pk_object_by_vid_.find(vid);

	if (it == map_pk_object_by_vid_.end())
		return nullptr;

	return it->second;
}

void land::delete_object(uint32_t id)
{
	object * pkObj;

	if (!(pkObj = find_object(id)))
		return;

	metin2::log::info( "Land::DeleteObject {:d}", id);
	manager::instance().unregister_object(pkObj);
	character_manager::instance().destroy_character(pkObj->npc());

	map_pk_object_.erase(id);
	map_pk_object_by_vid_.erase(id);

	delete pkObj;
}

struct f_is_in
{
	int32_t sx, sy;
	int32_t ex, ey;
	
	bool in;
	f_is_in (	int32_t sx_value, int32_t sy_value, int32_t ex_value, int32_t ey_value)
		: sx(sx_value), sy(sy_value), ex(ex_value), ey(ey_value), in(false) 
	{}

	void operator () (entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch = (character *) ent;
			if (ch->is_monster())
			{
				return;
			}
			if (sx <= ch->x() && ch->x() <= ex
				&& sy <= ch->y() && ch->y() <= ey)
			{
				in = true;
			}
		}
	}
};

auto land::request_create_object(uint32_t vnum_value, int32_t map_index_value, int32_t x, int32_t y, float x_rot_value, float y_rot_value, float z_rot_value, bool check_another) -> bool
{
	sectree_manager& rkSecTreeMgr = sectree_manager::instance();
	object_proto * pkProto = manager::instance().object_proto(vnum_value);

	if (!pkProto)
	{
		metin2::log::error("Invalid Object vnum {:d}", vnum_value);
		return false;
	}
	const map_region * r = rkSecTreeMgr.get_map_region(map_index_value);
	if (!r)
		return false;

	metin2::log::info( "RequestCreateObject(vnum={:d}, map={:d}, pos=({:d},{:d}), rot=({:.1f},{:.1f},{:.1f}) region({:d},{:d} ~ {:d},{:d})", 
			vnum_value, map_index_value, x, y, x_rot_value, y_rot_value, z_rot_value, r->sx, r->sy, r->ex, r->ey);

	x += r->sx;
	y += r->sy;

	int sx = r->sx + data_.x;
	int ex = sx + data_.width;
	int sy = r->sy + data_.y;
	int ey = sy + data_.height;

	int osx = x + pkProto->region[0];
	int osy = y + pkProto->region[1];
	int oex = x + pkProto->region[2];
	int oey = y + pkProto->region[3];

	float rad = z_rot_value * 2.0f * std::numbers::pi / 360.0f;

	int tsx = (int)(pkProto->region[0] * cosf(rad) + pkProto->region[1] * sinf(rad) + x);
	int tsy = (int)(pkProto->region[0] * -sinf(rad) + pkProto->region[1] * cosf(rad) + y);

	int tex = (int)(pkProto->region[2] * cosf(rad) + pkProto->region[3] * sinf(rad) + x);
	int tey = (int)(pkProto->region[2] * -sinf(rad) + pkProto->region[3] * cosf(rad) + y);

	if (tsx < sx || tex > ex || tsy < sy || tey > ey)
	{
		metin2::log::error("invalid position: object is outside of land region\nLAND: {:d} {:d} ~ {:d} {:d}\nOBJ: {:d} {:d} ~ {:d} {:d}", sx, sy, ex, ey, osx, osy, oex, oey);
		return false;
	}

	if ( check_another )
	{
		if (rkSecTreeMgr.for_attr_region(map_index_value, osx, osy, oex, oey, (int32_t)z_rot_value, ATTR_OBJECT, ATTR_REGION_MODE_CHECK))
		{
			metin2::log::error("another object already exist");
			return false;
		}
		f_is_in f (osx, osy, oex, oey);
		rkSecTreeMgr.map(map_index_value)->for_each(f);
		
		if (f.in)
		{
			metin2::log::error("another object already exist");
			return false;
		}
	}

	packet_gd_create_object p;

	p.vnum = vnum_value;
	p.land_id = data_.id;
	p.map_index = map_index_value;
	p.x = x;
	p.y = y;
	p.x_rot = x_rot_value;
	p.y_rot = y_rot_value;
	p.z_rot = z_rot_value;

	db_clientdesc->db_packet(HEADER_GD_CREATE_OBJECT, 0, &p, sizeof(packet_gd_create_object));
	return true;
}

void land::request_delete_object(uint32_t id)
{
	if (!find_object(id))
	{
		metin2::log::error("no object by id {:d}", id);
		return;
	}

	db_clientdesc->db_packet(HEADER_GD_DELETE_OBJECT, 0, &id, sizeof(uint32_t));
	metin2::log::info( "RequestDeleteObject id {:d}", id);
}

void land::request_delete_object_by_vid(uint32_t vid)
{
	object * pkObj;

	if (!(pkObj = find_object_by_vid(vid)))
	{
		metin2::log::error("no object by vid {:d}", vid);
		return;
	}

	uint32_t dwID = pkObj->id();
	db_clientdesc->db_packet(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(uint32_t));
	metin2::log::info( "RequestDeleteObject vid {:d} id {:d}", vid, dwID);
}

void land::set_owner(uint32_t guild)
{
	if (data_.guild_id != guild)
	{
		data_.guild_id = guild;
		request_update(guild);
	}
}

void land::request_update(uint32_t guild)
{
	uint32_t a[2];

	a[0] = id();
	a[1] = guild;

	db_clientdesc->db_packet(HEADER_GD_UPDATE_LAND, 0, &a[0], sizeof(uint32_t) * 2);
	metin2::log::info( "RequestUpdate id {:d} guild {:d}", a[0], a[1]);
}


manager::manager()
{
}

manager::~manager()
{
	destroy();
}

void manager::destroy()
{
	map_pk_land_.clear();
}

auto manager::load_object_proto(const proto::building::object_proto * proto, int size) -> bool
{
	vec_k_object_proto_.resize(size);
	std::memcpy(&vec_k_object_proto_[0], proto, sizeof(proto::building::object_proto) * size);

	for (int i = 0; i < size; ++i)
	{
		proto::building::object_proto & r = vec_k_object_proto_[i];

		metin2::log::info( "ObjectProto {:d} price {:d} upgrade {:d} upg_limit {:d} life {:d} NPC {:d}",
				r.vnum, r.price, r.upgrade_vnum, r.upgrade_limit_time, r.life, r.npc_vnum);

		for (int j = 0; j < OBJECT_MATERIAL_MAX_NUM; ++j)
		{
			if (!r.materials[j].item_vnum)
				break;

			if (nullptr == item_manager::instance().table(r.materials[j].item_vnum))
			{
				metin2::log::error("          mat: ERROR!! no item by vnum {:d}", r.materials[j].item_vnum);
				return false;
			}

			metin2::log::info( "          mat: {:d} {:d}", r.materials[j].item_vnum, r.materials[j].count);
		}

		map_pk_object_proto_.insert(std::make_pair(r.vnum, &vec_k_object_proto_[i]));
	}

	return true;
}

auto manager::object_proto(uint32_t vnum) -> proto::building::object_proto *
{
	auto it = map_pk_object_proto_.find(vnum);

	if (it == map_pk_object_proto_.end())
		return nullptr;

	return it->second;
}

auto manager::load_land(land_table * table) -> bool
{

	auto pkLand = std::make_unique<land>(table);
	uint32_t dwID = pkLand->id();
	map_pk_land_.insert(std::make_pair(dwID, std::move(pkLand)));

	metin2::log::info( "LAND: {:d} map {:d} {:d}x{:d} w {:d} h {:d}", 
			table->id, table->map_index, table->x, table->y, table->width, table->height);

	return true;
}

void manager::update_land(land_table * table)
{
	land * pkLand = find_land(table->id);

	if (!pkLand)
	{
		metin2::log::error("cannot find land by id {:d}", table->id);
		return;
	}

	pkLand->put_data(table);

	const desc_manager::desc_set & cont = desc_manager::instance().client_set();

	auto it = cont.begin();

	packet_gc_land_list p;

	p.header = HEADER_GC_LAND_LIST;
	p.size = sizeof(packet_gc_land_list) + sizeof(land_packet_element);

	land_packet_element e;

	e.id = table->id;
	e.x = table->x;
	e.y = table->y;
	e.width = table->width;
	e.height = table->height;
	e.guild_id = table->guild_id;

	metin2::log::info( "BUILDING: UpdateLand {:d} pos {:d}x{:d} guild {:d}", e.id, e.x, e.y, e.guild_id);

	guild *guild = guild_manager::instance().find_guild(table->guild_id);
	while (it != cont.end())
	{
		desc * d = *(it++);

		if (d->get_character() && d->get_character()->map_index() == table->map_index)
		{
			// The guild name must be sent first.
			d->get_character()->send_guild_name(guild);

			d->buffered_packet(&p, sizeof(packet_gc_land_list));
			d->packet(&e, sizeof(land_packet_element));
		}
	}
}

auto manager::find_land(uint32_t id) -> land *
{
	auto it = map_pk_land_.find(id);

	if (it == map_pk_land_.end())
		return nullptr;

	return it->second.get();
}

auto manager::find_land(int32_t map_index_value, int32_t x, int32_t y) -> land *
{
	metin2::log::info( "BUILDING: FindLand {:d} {:d} {:d}", map_index_value, x, y);

	const map_region * r = sectree_manager::instance().get_map_region(map_index_value);

	if (!r)
		return nullptr;

	x -= r->sx;
	y -= r->sy;

	auto it = map_pk_land_.begin();

	while (it != map_pk_land_.end())
	{
		land * pkLand = (it++)->second.get();
		const land_table & r = pkLand->data();

		if (r.map_index != map_index_value)
			continue;

		if (x < r.x || y < r.y)
			continue;

		if (x > r.x + r.width || y > r.y + r.height)
			continue;

		return pkLand;
	}

	return nullptr;
}

auto manager::find_land_by_guild(uint32_t gid) -> land *
{
	auto it = map_pk_land_.begin();

	while (it != map_pk_land_.end())
	{
		land * pkLand = (it++)->second.get();

		if (pkLand->data().guild_id == gid)
			return pkLand;
	}

	return nullptr;
}

auto manager::load_object(object_table * table, bool is_boot) -> bool
{
	if (!map_allow_find(table->map_index))
		return false;

	land * pkLand = find_land(table->land_id);

	if (!pkLand)
	{
		metin2::log::info( "Cannot find land by id {:d}", table->land_id);
		return false;
	}

	proto::building::object_proto * pkProto = object_proto(table->vnum);

	if (!pkProto)
	{
		metin2::log::error("Cannot find object {:d} in prototype (id {:d})", table->vnum, table->id);
		return false;
	}

	metin2::log::info( "OBJ: id {:d} vnum {:d} map {:d} pos {:d}x{:d}", table->id, table->vnum, table->map_index, table->x, table->y);

	object * pkObj = new object(table, pkProto);

	uint32_t dwVID = character_manager::instance().alloc_vid();
	pkObj->set_vid(dwVID);

	map_pk_obj_by_vid_.insert(std::make_pair(dwVID, pkObj));
	map_pk_obj_by_id_.insert(std::make_pair(table->id, pkObj));

	pkLand->insert_object(pkObj);

	if (!is_boot)
		pkObj->show(table->map_index, table->x, table->y);
	else
	{
		pkObj->set_map_index(table->map_index);
		pkObj->set_xyz(table->x, table->y, 0);
	}

	if (!is_boot)
	{ 
		if (pkProto->npc_vnum)
			pkObj->regen_npc();

		pkObj->apply_special_effect();
	}

	return true;
}

void manager::finalize_boot()
{
	auto it = map_pk_obj_by_id_.begin();

	while (it != map_pk_obj_by_id_.end())
	{
		object * pkObj = (it++)->second;

		pkObj->show(pkObj->map_index(), pkObj->x(), pkObj->y());
		pkObj->regen_npc();
		pkObj->apply_special_effect();
	}

	metin2::log::info( "FinalizeBoot");

	auto it2 = map_pk_land_.begin();

	while (it2 != map_pk_land_.end())
	{
		land * pkLand = (it2++)->second.get();

		const land_table & r = pkLand->data();

		metin2::log::info( "LandMaster map_index={:d} pos=({:d}, {:d})", r.map_index, r.x, r.y);

		if (r.guild_id != 0)
			continue;

		if (!map_allow_find(r.map_index))
			continue;

		const map_region * region = sectree_manager::instance().get_map_region(r.map_index);
		if (!region)
			continue;

		character_manager::instance().spawn_mob(20040, r.map_index, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
	}
}

void manager::delete_object(uint32_t id)
{
	metin2::log::info( "OBJ_DEL: {:d}", id);

	auto it = map_pk_obj_by_id_.find(id);

	if (it == map_pk_obj_by_id_.end())
		return;

	it->second->get_land()->delete_object(id);
}

auto manager::find_object_by_vid(uint32_t vid) -> object *
{
	auto it = map_pk_obj_by_vid_.find(vid);

	if (it == map_pk_obj_by_vid_.end())
		return nullptr;

	return it->second;
}

void manager::unregister_object(object * obj)
{
	map_pk_obj_by_id_.erase(obj->id());
	map_pk_obj_by_vid_.erase(obj->get_vid());
}

void manager::send_land_list(desc * d, int32_t map_index_value)
{
	land_packet_element e;

	temp_buffer buf;

	uint16_t wCount = 0;

	auto it = map_pk_land_.begin();

	while (it != map_pk_land_.end())
	{
		land * pkLand = (it++)->second.get();
		const land_table & r = pkLand->data();

		if (r.map_index != map_index_value)
			continue;

		character * ch  = d->get_character();
		if (ch)
		{
			guild *guild = guild_manager::instance().find_guild(r.guild_id);
			ch->send_guild_name(guild);
		}

		e.id = r.id;
		e.x = r.x;
		e.y = r.y;
		e.width = r.width;
		e.height = r.height;
		e.guild_id = r.guild_id;

		buf.write(&e, sizeof(land_packet_element));
		++wCount;
	}

	metin2::log::info( "SendLandList map {:d} count {:d} elem_size: {:d}", map_index_value, wCount, buf.size());

	if (wCount != 0)
	{
		packet_gc_land_list p;

		p.header = HEADER_GC_LAND_LIST;
		p.size = sizeof(packet_gc_land_list) + buf.size();

		d->buffered_packet(&p, sizeof(packet_gc_land_list));
		d->packet(buf.read_peek(), buf.size());
	}
}

void manager::clear_land(uint32_t land_id)
{
	land* pLand = find_land(land_id);

	if ( pLand == nullptr )
	{
		metin2::log::info( "LAND_CLEAR: there is no LAND id like {:d}", land_id);
		return;
	}

	pLand->clear_land();

	metin2::log::info( "LAND_CLEAR: request Land Clear. LandID: {:d}", pLand->id());
}

void manager::clear_land_by_guild_id(uint32_t guild_id)
{
	land* pLand = find_land_by_guild(guild_id);

	if ( pLand == nullptr )
	{
		metin2::log::info( "LAND_CLEAR: there is no GUILD id like {:d}", guild_id);
		return;
	}

	pLand->clear_land();

	metin2::log::info( "LAND_CLEAR: request Land Clear. LandID: {:d}", pLand->id());
}

void land::clear_land()
{
	auto iter = map_pk_object_.begin();

	while ( iter != map_pk_object_.end() )
	{
		request_delete_object(iter->second->id());
		iter++;
	}

	set_owner(0);

	const land_table & r = data();
	const map_region * region = sectree_manager::instance().get_map_region(r.map_index);

	character_manager::instance().spawn_mob(20040, r.map_index, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
}

void land::draw_wall(uint32_t vnum, int32_t map_index, int32_t& x, int32_t& y, char length, float z_rot)
{
	int rot = (int)z_rot;
	rot = ((rot%360) / 90) * 90;

	int dx=0, dy=0;

	switch ( rot )
	{
		case 0 :
			dx = -500;
			dy = 0;
			break;

		case 90 :
			dx = 0;
			dy = 500;
			break;

		case 180 :
			dx = 500;
			dy = 0;
			break;

		case 270 :
			dx = 0;
			dy = -500;
			break;
	}

	for ( int i=0; i < length; i++ )
	{
		this->request_create_object(vnum, map_index, x, y, 0, 0, rot, false);
		x += dx;
		y += dy;
	}
}


auto land::request_create_wall(int32_t map_index, float rot) -> bool
{
	const bool 	WALL_ANOTHER_CHECKING_ENABLE = false;

	const land_table& land = data();

	int center_x = land.x + land.width  / 2;
	int center_y = land.y + land.height / 2;

	int wall_x = center_x;
	int wall_y = center_y;
	int wall_half_w = 1000;
	int wall_half_h = 1362;

	if (rot == 0.0f)
	{
		int door_x = wall_x;
		int door_y = wall_y + wall_half_h;
		request_create_object(WALL_DOOR_VNUM,	map_index, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_BACK_VNUM,	map_index, wall_x, wall_y - wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_LEFT_VNUM,	map_index, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_RIGHT_VNUM,	map_index, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}	
	else if (rot == 180.0f)
	{
		int door_x = wall_x;
		int door_y = wall_y - wall_half_h;
		request_create_object(WALL_DOOR_VNUM,	map_index, wall_x, wall_y - wall_half_h, door_x, door_y, 180.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_BACK_VNUM,	map_index, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_LEFT_VNUM,	map_index, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_RIGHT_VNUM,	map_index, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 90.0f)
	{
		int door_x = wall_x + wall_half_h;
		int door_y = wall_y;
		request_create_object(WALL_DOOR_VNUM,	map_index, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_BACK_VNUM,	map_index, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_LEFT_VNUM,	map_index, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_RIGHT_VNUM,	map_index, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 270.0f)
	{
		int door_x = wall_x - wall_half_h;
		int door_y = wall_y;
		request_create_object(WALL_DOOR_VNUM,	map_index, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_BACK_VNUM,	map_index, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_LEFT_VNUM,	map_index, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(WALL_RIGHT_VNUM,	map_index, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}

	if (metin2::config::test_server())
	{
		request_create_object(FLAG_VNUM, map_index, land.x + 50, 			land.y + 50, 0, 0, 0.0, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(FLAG_VNUM, map_index, land.x + land.width - 50,	land.y + 50, 0, 0, 90.0, WALL_ANOTHER_CHECKING_ENABLE);
		request_create_object(FLAG_VNUM, map_index, land.x + land.width - 50,	land.y + land.height - 50, 0, 0, 180.0, WALL_ANOTHER_CHECKING_ENABLE); 
		request_create_object(FLAG_VNUM, map_index, land.x + 50, 			land.y + land.height - 50, 0, 0, 270.0, WALL_ANOTHER_CHECKING_ENABLE);
	}
	return true;
}

void land::request_delete_wall()
{
	auto iter = map_pk_object_.begin();

	while (iter != map_pk_object_.end())
	{
		unsigned id   = iter->second->id();
		unsigned vnum = iter->second->vnum();

		switch (vnum)
		{
			case WALL_DOOR_VNUM:
			case WALL_BACK_VNUM:
			case WALL_LEFT_VNUM:
			case WALL_RIGHT_VNUM:
				request_delete_object(id);
				break;
		}


		if (metin2::config::test_server())
		{
			if (FLAG_VNUM == vnum)
				request_delete_object(id);

		}

		iter++;
	}
}

auto land::request_create_wall_blocks(uint32_t vnum, int32_t map_index, char wall_size, bool door_east, bool door_west, bool door_south, bool door_north) -> bool
{
	const land_table & r = data();

	int32_t startX = r.x + (r.width  / 2) - (1300 + wall_size*500);
	int32_t startY = r.y + (r.height / 2) + (1300 + wall_size*500);

	uint32_t corner = vnum - 4;
	uint32_t wall   = vnum - 3;
	uint32_t door   = vnum - 1;

	bool checkAnother = false;
	int32_t* ptr = nullptr;
	int delta = 1;
	int rot = 270;

	bool doorOpen[4];
	doorOpen[0] = door_west;
	doorOpen[1] = door_north;
	doorOpen[2] = door_east;
	doorOpen[3] = door_south;

	if ( wall_size > 3 ) wall_size = 3;
	else if ( wall_size < 0 ) wall_size = 0;

	for ( int i=0; i < 4; i++, rot -= 90 )
	{
		switch ( i )
		{
			case 0 :
				delta = -1;
				ptr = &startY;
				break;
			case 1 :
				delta = 1;
				ptr = &startX;
				break;
			case 2 :
				ptr = &startY;
				delta = 1;
				break;
			case 3 :
				ptr = &startX;
				delta = -1;
				break;
		}

		this->request_create_object(corner, map_index, startX, startY, 0, 0, rot, checkAnother);

		*ptr = *ptr + ( 700 * delta );

		if ( doorOpen[i] )
		{
			this->draw_wall(wall, map_index, startX, startY, wall_size, rot);

			*ptr = *ptr + ( 700 * delta );

			this->request_create_object(door, map_index, startX, startY, 0, 0, rot, checkAnother);

			*ptr = *ptr + ( 1300 * delta );

			this->draw_wall(wall, map_index, startX, startY, wall_size, rot);
		}
		else
		{
			this->draw_wall(wall, map_index, startX, startY, wall_size*2 + 4, rot);
		}

		*ptr = *ptr + ( 100 * delta );
	}

	return true;
}

void land::request_delete_wall_blocks(uint32_t id_value)
{
	auto iter = map_pk_object_.begin();

	uint32_t corner = id_value - 4;
	uint32_t wall = id_value - 3;
	uint32_t door = id_value - 1;
	uint32_t dwVnum = 0;

	while ( iter != map_pk_object_.end() )
	{
		dwVnum = iter->second->vnum();

		if ( dwVnum == corner || dwVnum == wall || dwVnum == door )
		{
			request_delete_object(iter->second->id());
		}
		iter++;
	}
}
}
