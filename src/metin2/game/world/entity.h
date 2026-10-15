#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline world {

class sectree;

class entity
{
	public:
		typedef std::unordered_map<entity *, int> entity_map;

	public:
		entity();
		virtual	~entity();

		virtual void	encode_insert_packet(entity * entity) = 0;
		virtual	void	encode_remove_packet(entity * entity) = 0;

	protected:
		void			initialize(int type = -1);
		void			destroy();

	public:
		int				type() const;
		bool			is_type(int type) const;

		void			view_cleanup();
		void			view_insert(entity * entity, bool recursive = true);
		void			view_remove(entity * entity, bool recursive = true);
		void			view_reencode();	// Resend packets to surrounding entities.

		int32_t			x() const		{ return pos_.x; }
		int32_t			y() const		{ return pos_.y; }
		int32_t			z() const		{ return pos_.z; }
		const pixel_position &	xyz() const		{ return pos_; }

		void			set_xyz(int32_t x, int32_t y, int32_t z)		{ pos_.x = x, pos_.y = y, pos_.z = z; }

		sectree *		get_sectree() const			{ return sectree_;	}
		void			set_sectree(sectree * tree)	{ sectree_ = tree;	}

		void			update_sectree();
		void			packet_around(const void * data, int bytes, entity * except = nullptr);
		void			packet_view(const void * data, int bytes, entity * except = nullptr);

		void			bind_desc(desc * d)     { desc_ = d; }
		desc *			get_desc() const			{ return desc_; }

		void			set_map_index(int32_t l)	{ map_index_ = l; }
		int32_t			map_index() const	{ return map_index_; }

		void			set_observer_mode(bool flag);
		bool			is_observer_mode() const	{ return is_observer_; }

	protected:
		bool			is_observer_;
		bool			observer_mode_change_;
		entity_map		map_view_;
		int32_t			map_index_;

	private:
		desc *			desc_;

		int			type_;
		bool			is_destroyed_;

		pixel_position		pos_;

		int			view_age_;

		sectree *		sectree_;
};
}
