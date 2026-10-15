
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/log/log.h>

namespace metin2::game::inline world {

void entity::view_cleanup()
{
	entity_map::iterator it = map_view_.begin();

	while (it != map_view_.end())
	{
		entity * entity = it->first;
		++it;

		entity->view_remove(this, false);
	}

	map_view_.clear();
}

void entity::view_reencode()
{
	if (is_observer_)
		return;

	encode_remove_packet(this);
	encode_insert_packet(this);

	entity_map::iterator it = map_view_.begin();

	while (it != map_view_.end())
	{
		entity * entity = (it++)->first;

		encode_remove_packet(entity);
		if (!is_observer_)
			encode_insert_packet(entity);

		if (!entity->is_observer_)
			entity->encode_insert_packet(this);
	}

}

void entity::view_insert(entity * entity, bool recursive)
{
	if (this == entity)
		return;

	entity_map::iterator it = map_view_.find(entity);

	if (map_view_.end() != it)
	{
		it->second = view_age_;
		return;
	}

	map_view_.insert(entity_map::value_type(entity, view_age_));

	if (!entity->is_observer_)
		entity->encode_insert_packet(this);

	if (recursive)
		entity->view_insert(this, false);
}

void entity::view_remove(entity * entity, bool recursive)
{
	entity_map::iterator it = map_view_.find(entity);

	if (it == map_view_.end())
		return;

	map_view_.erase(it);

	if (!entity->is_observer_)
		entity->encode_remove_packet(this);

	if (recursive)
		entity->view_remove(this, false);
}

class func_view_insert
{
	private:
		int view_range_;

	public:
		entity * me;

		func_view_insert(entity * ent) :
			view_range_(metin2::config::game().view_range + VIEW_BONUS_RANGE),
			me(ent)
		{
		}

		void operator () (entity * ent)
		{
			if (!ent->is_type(ENTITY_OBJECT))
				if (distance_approx(ent->x() - me->x(), ent->y() - me->y()) > view_range_)
					return;

			me->view_insert(ent);

			if (ent->is_type(ENTITY_CHARACTER) && me->is_type(ENTITY_CHARACTER))
			{
				character * chMe = (character *) me;
				character * chEnt = (character *) ent;

				if (chMe->is_pc() && !chEnt->is_pc() && !chEnt->is_warp() && !chEnt->is_goto())
					chEnt->start_state_machine();
			}
		}
};

void entity::update_sectree()
{
	if (!get_sectree())
	{
		if (is_type(ENTITY_CHARACTER))
		{
			character * tch = (character *) this;
			metin2::log::error("null sectree name: {} {:d} {:d}",  tch->name(), x(), y());
		}

		return;
	}

	++view_age_;

	func_view_insert f(this);
	get_sectree()->for_each_around(f);

	entity_map::iterator it, this_it;

	if (observer_mode_change_)
	{
		if (is_observer_)
		{
			it = map_view_.begin();

			while (it != map_view_.end())
			{
				this_it = it++;
				if (this_it->second < view_age_)
				{
					entity * ent = this_it->first;

					ent->encode_remove_packet(this);
					map_view_.erase(this_it);

					ent->view_remove(this, false);
				}
				else
				{

					entity * ent = this_it->first;


					encode_remove_packet(ent);
				}
			}
		}
		else
		{
			it = map_view_.begin();

			while (it != map_view_.end())
			{
				this_it = it++;

				if (this_it->second < view_age_)
				{
					entity * ent = this_it->first;

					ent->encode_remove_packet(this);
					map_view_.erase(this_it);

					ent->view_remove(this, false);
				}
				else
				{
					entity * ent = this_it->first;
					ent->encode_insert_packet(this);
					encode_insert_packet(ent);

					ent->view_insert(this, true);
				}
			}
		}

		observer_mode_change_ = false;
	}
	else
	{
		if (!is_observer_)
		{
			it = map_view_.begin();

			while (it != map_view_.end())
			{
				this_it = it++;

				if (this_it->second < view_age_)
				{
					entity * ent = this_it->first;

					ent->encode_remove_packet(this);
					map_view_.erase(this_it);

					ent->view_remove(this, false);
				}
			}
		}
	}
}
}
