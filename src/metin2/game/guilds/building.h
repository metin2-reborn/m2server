#pragma once

#include <metin2/game/world/entity.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/building.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace metin2::game::inline guilds {

namespace building
{
	using namespace metin2::proto::building;

	class land;

	class object : public entity
	{
		public:
			object(object_table * data, object_proto * proto);
			~object() override;

			void	destroy();

			void encode_insert_packet(entity * entity) override;
			void encode_remove_packet(entity * entity) override;

			auto	id() -> uint32_t { return data_.id; }

			void	set_vid(uint32_t vid);
			auto	get_vid() -> uint32_t { return vid_; }

			auto	show(int32_t map_index_value, int32_t x, int32_t y) -> bool;

			void	save();

			void	set_land(land * land_value) { land_ = land_value; }
			auto	get_land() -> land *		{ return land_; }

			auto	vnum() -> uint32_t		{ return proto_ ? proto_->vnum : 0; }
			auto	group() -> uint32_t		{ return proto_ ? proto_->group_vnum : 0; }

			void	regen_npc();

			void	apply_special_effect();
			void	remove_special_effect();

			void	reconstruct(uint32_t vnum);

			auto npc() -> character * { return ch_npc_; }

		protected:
			object_proto *	proto_;
			object_table		data_;
			uint32_t		vid_;
			land *		land_;

			character *		ch_npc_;
	};

	class land
	{
		public:
			land(land_table * data);
			~land();

			void	destroy();

			auto data() -> const land_table &;
			void	put_data(const land_table * data);

			auto	id() const -> uint32_t { return data_.id; }
			void	set_owner(uint32_t guild);
			auto	owner() const -> uint32_t { return data_.guild_id; }

			void	insert_object(object * obj);
			auto	find_object(uint32_t id) -> object *;
			auto	find_object_by_vid(uint32_t vid) -> object *;
			auto	find_object_by_group(uint32_t group_vnum) -> object *;
			auto	find_object_by_npc(character * npc) -> object *;
			void delete_object(uint32_t id);

			auto	request_create_object(uint32_t vnum_value, int32_t map_index_value, int32_t x, int32_t y, float x_rot_value, float y_rot_value, float z_rot_value, bool check_another) -> bool;
			void	request_delete_object(uint32_t id);
			void	request_delete_object_by_vid(uint32_t vid);

			void	request_update(uint32_t guild);

			void	clear_land();

			auto request_create_wall(int32_t map_index, float rot) -> bool;
			void request_delete_wall();

			auto request_create_wall_blocks(uint32_t vnum, int32_t map_index, char wall_size, bool door_east, bool door_west, bool door_south, bool door_north) -> bool;
			void request_delete_wall_blocks(uint32_t id_value);

			auto map_index() -> uint32_t { return data_.map_index; }

		protected:
			land_table			data_;
			std::map<uint32_t, object *>	map_pk_object_;
			std::map<uint32_t, object *>	map_pk_object_by_vid_;

		private :
			void draw_wall(uint32_t vnum, int32_t map_index, int32_t& x, int32_t& y, char length, float z_rot);
	};

	class manager : public singleton<manager>
	{
		public:
			manager();
			~manager() override;

			void	destroy();

			void	finalize_boot();

			auto	load_object_proto(const proto::building::object_proto * proto, int size) -> bool;
			auto object_proto(uint32_t vnum) -> proto::building::object_proto *;

			auto	load_land(land_table * table) -> bool;
			auto	find_land(uint32_t id) -> land *;
			auto	find_land(int32_t map_index_value, int32_t x, int32_t y) -> land *;
			auto	find_land_by_guild(uint32_t gid) -> land *;
			void	update_land(land_table * table);

			auto	load_object(object_table * table, bool is_boot=false) -> bool;
			void	delete_object(uint32_t id);
			void	unregister_object(object * obj);

			auto	find_object_by_vid(uint32_t vid) -> object *;

			void	send_land_list(desc * d, int32_t map_index_value);

			void	clear_land(uint32_t land_id);
			void	clear_land_by_guild_id(uint32_t guild_id);

		protected:
			std::vector<proto::building::object_proto>		vec_k_object_proto_;
			std::map<uint32_t, proto::building::object_proto *>	map_pk_object_proto_;

			std::map<uint32_t, std::unique_ptr<land>>	map_pk_land_;
			std::map<uint32_t, object *>		map_pk_obj_by_id_;
			std::map<uint32_t, object *>		map_pk_obj_by_vid_;
	};
}
}
