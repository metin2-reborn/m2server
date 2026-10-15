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
	// The shared land and object tables (proto).
	using namespace metin2::proto::building;

	class land;

	class object : public entity
	{
		public:
			object(object_table * pData, object_proto * pProto);
			~object() override;

			void	destroy();

			void encode_insert_packet(entity * entity) override;
			void encode_remove_packet(entity * entity) override;

			uint32_t	id() { return data_.id; }

			void	set_vid(uint32_t dwVID);
			uint32_t	get_vid() { return vid_; }

			bool	show(int32_t lMapIndex, int32_t x, int32_t y);

			void	save();

			void	set_land(land * pkLand) { land_ = pkLand; }
			land *	get_land()		{ return land_; }

			uint32_t	vnum()		{ return proto_ ? proto_->vnum : 0; }
			uint32_t	group()		{ return proto_ ? proto_->group_vnum : 0; }

			void	regen_npc();

			// BUILDING_NPC
			void	apply_special_effect();
			void	remove_special_effect();

			void	reconstruct(uint32_t dwVnum);

			character * npc() { return ch_npc_; }
			// END_OF_BUILDING_NPC

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
			land(land_table * pData);
			~land();

			void	destroy();

			const land_table & data();
			void	put_data(const land_table * data);

			uint32_t	id() const { return data_.id; }
			void	set_owner(uint32_t dwGID);
			uint32_t	owner() const { return data_.guild_id; }

			void	insert_object(object * pkObj);
			object *	find_object(uint32_t dwID);
			object *	find_object_by_vid(uint32_t dwVID);
			object *	find_object_by_group(uint32_t dwGroupVnum);
			object *	find_object_by_npc(character * npc);
			void delete_object(uint32_t dwID);

			bool	request_create_object(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, float xRot, float yRot, float zRot, bool checkAnother);
			void	request_delete_object(uint32_t dwID);
			void	request_delete_object_by_vid(uint32_t dwVID);

			void	request_update(uint32_t dwGuild);

			// LAND_CLEAR
			void	clear_land();
			// END_LAND_CLEAR

			// BUILD_WALL
			bool request_create_wall(int32_t nMapIndex, float rot);
			void request_delete_wall();

			bool request_create_wall_blocks(uint32_t dwVnum, int32_t nMapIndex, char wallSize, bool doorEast, bool doorWest, bool doorSouth, bool doorNorth);
			void request_delete_wall_blocks(uint32_t dwVnum);
			// END_BUILD_WALL

			uint32_t map_index() { return data_.map_index; }

		protected:
			land_table			data_;
			std::map<uint32_t, object *>	map_pk_object_;
			std::map<uint32_t, object *>	map_pk_object_by_vid_;

			// BUILD_WALL
		private :
			void draw_wall(uint32_t dwVnum, int32_t nMapIndex, int32_t& centerX, int32_t& centerY, char length, float zRot);
			// END_BUILD_WALL
	};

	class manager : public singleton<manager>
	{
		public:
			manager();
			~manager() override;

			void	destroy();

			void	finalize_boot();

			bool	load_object_proto(const proto::building::object_proto * pProto, int size);
			proto::building::object_proto * object_proto(uint32_t dwVnum);

			bool	load_land(land_table * pTable);
			land *	find_land(uint32_t dwID);
			land *	find_land(int32_t lMapIndex, int32_t x, int32_t y);
			land *	find_land_by_guild(uint32_t GID);
			void	update_land(land_table * pTable);

			bool	load_object(object_table * pTable, bool isBoot=false);
			void	delete_object(uint32_t dwID);
			void	unregister_object(object * pkObj);

			object *	find_object_by_vid(uint32_t dwVID);

			void	send_land_list(desc * d, int32_t lMapIndex);

			// LAND_CLEAR
			void	clear_land(uint32_t dwLandID);
			void	clear_land_by_guild_id(uint32_t dwGuildID);
			// END_LAND_CLEAR

		protected:
			std::vector<proto::building::object_proto>		vec_k_object_proto_;
			std::map<uint32_t, proto::building::object_proto *>	map_pk_object_proto_;

			std::map<uint32_t, std::unique_ptr<land>>	map_pk_land_;
			std::map<uint32_t, object *>		map_pk_obj_by_id_;
			std::map<uint32_t, object *>		map_pk_obj_by_vid_;
	};
}
}
