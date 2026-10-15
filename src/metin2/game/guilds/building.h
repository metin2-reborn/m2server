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

			uint32_t	id() { return data_.id; }

			void	set_vid(uint32_t vid);
			uint32_t	get_vid() { return vid_; }

			bool	show(int32_t map_index_value, int32_t x, int32_t y);

			void	save();

			void	set_land(land * land_value) { land_ = land_value; }
			land *	get_land()		{ return land_; }

			uint32_t	vnum()		{ return proto_ ? proto_->vnum : 0; }
			uint32_t	group()		{ return proto_ ? proto_->group_vnum : 0; }

			void	regen_npc();

			void	apply_special_effect();
			void	remove_special_effect();

			void	reconstruct(uint32_t vnum);

			character * npc() { return ch_npc_; }

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

			const land_table & data();
			void	put_data(const land_table * data);

			uint32_t	id() const { return data_.id; }
			void	set_owner(uint32_t guild);
			uint32_t	owner() const { return data_.guild_id; }

			void	insert_object(object * obj);
			object *	find_object(uint32_t id);
			object *	find_object_by_vid(uint32_t vid);
			object *	find_object_by_group(uint32_t group_vnum);
			object *	find_object_by_npc(character * npc);
			void delete_object(uint32_t id);

			bool	request_create_object(uint32_t vnum_value, int32_t map_index_value, int32_t x, int32_t y, float x_rot_value, float y_rot_value, float z_rot_value, bool check_another);
			void	request_delete_object(uint32_t id);
			void	request_delete_object_by_vid(uint32_t vid);

			void	request_update(uint32_t guild);

			void	clear_land();

			bool request_create_wall(int32_t map_index, float rot);
			void request_delete_wall();

			bool request_create_wall_blocks(uint32_t vnum, int32_t map_index, char wall_size, bool door_east, bool door_west, bool door_south, bool door_north);
			void request_delete_wall_blocks(uint32_t id_value);

			uint32_t map_index() { return data_.map_index; }

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

			bool	load_object_proto(const proto::building::object_proto * proto, int size);
			proto::building::object_proto * object_proto(uint32_t vnum);

			bool	load_land(land_table * table);
			land *	find_land(uint32_t id);
			land *	find_land(int32_t map_index_value, int32_t x, int32_t y);
			land *	find_land_by_guild(uint32_t gid);
			void	update_land(land_table * table);

			bool	load_object(object_table * table, bool is_boot=false);
			void	delete_object(uint32_t id);
			void	unregister_object(object * obj);

			object *	find_object_by_vid(uint32_t vid);

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
