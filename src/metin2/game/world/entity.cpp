#include <metin2/game/world/entity.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace metin2::game::inline world {

entity::entity()
{
	initialize();
}

entity::~entity()
{
	if (!is_destroyed_)
		assert(!"You must call CEntity::destroy() method in your derived class destructor");
}

void entity::initialize(int type)
{
	is_destroyed_ = false;

	type_ = type;
	view_age_ = 0;
	pos_.x = pos_.y = pos_.z = 0;
	map_view_.clear();

	sectree_ = nullptr;
	desc_ = nullptr;
	map_index_ = 0;
	is_observer_ = false;
	observer_mode_change_ = false;
}

void entity::destroy()
{
	if (is_destroyed_) {
		return;
	}
	view_cleanup();
	is_destroyed_ = true;
}

int entity::type() const
{
	return type_;
}

bool entity::is_type(int type) const
{
	return (type_ == type ? true : false);
}

struct func_packet_around
{
	const void *        data;
	int                 bytes;
	entity *            except;

	func_packet_around(const void * data_value, int bytes_value, entity * except_value = nullptr) :data(data_value), bytes(bytes_value), except(except_value)
	{
	}

	void operator () (entity * ent)
	{
		if (ent == except)
			return;

		if (ent->get_desc())
			ent->get_desc()->packet(data, bytes);
	}
};

struct func_packet_view : public func_packet_around
{
	func_packet_view(const void * data, int bytes, entity * except = nullptr) : func_packet_around(data, bytes, except)
	{}

	void operator() (const entity::entity_map::value_type& v)
	{
		func_packet_around::operator() (v.first);
	}
};

void entity::packet_around(const void * data, int bytes, entity * except)
{
	packet_view(data, bytes, except);
}

void entity::packet_view(const void * data, int bytes, entity * except)
{
	if (!get_sectree())
		return;

	func_packet_view f(data, bytes, except);

	// In observer mode, only I receive my packets.
	if (!is_observer_)
		for_each(map_view_.begin(), map_view_.end(), f);

	f(std::make_pair(this, 0));
}

void entity::set_observer_mode(bool flag)
{
	if (is_observer_ == flag)
		return;

	is_observer_ = flag;
	observer_mode_change_ = true;
	update_sectree();

	if (is_type(ENTITY_CHARACTER))
	{
		character * ch = (character *) this;
		ch->chat_packet(CHAT_TYPE_COMMAND, "ObserverMode %d", is_observer_ ? 1 : 0);
	}
}
}
