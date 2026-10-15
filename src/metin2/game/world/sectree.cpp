#include <metin2/game/world/sectree.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/common/attribute.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>

namespace metin2::game::inline world {

sectree::sectree()
{
	initialize();
}

sectree::~sectree()
{
	destroy();
}

void sectree::initialize()
{
	id_.package = 0;
	attribute_ = nullptr;
	pc_count_ = 0;
	is_clone_ = false;
}

void sectree::destroy()
{
	if (!set_entity_.empty())
	{
		metin2::log::error("Sectree: entity set not empty!!");

		entity_set::iterator it = set_entity_.begin();

		for ( ; it != set_entity_.end(); ++it)
		{
			entity * ent = *it;

			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;

				metin2::log::error("Sectree: destroying character: {} is_pc {:d}", ch->name(), ch->is_pc() ? 1 : 0);

				if (ch->get_desc())
					desc_manager::instance().destroy_desc(ch->get_desc());
				else
					character_manager::instance().destroy_character(ch);
			}
			else if (ent->is_type(ENTITY_ITEM))
			{
				item * item = (game::items::item *) ent;

				metin2::log::error("Sectree: destroying Item: {}", item->name());

				item_manager::instance().destroy_item(item);
			}
			else
				metin2::log::error("Sectree: unknown type: {:d}", ent->type());
		}
	}
	set_entity_.clear();

	if (!is_clone_ && attribute_)
	{
		delete attribute_;
		attribute_ = nullptr;
	}
}

sectreeid sectree::id()
{
	return id_;
}

void sectree::increase_pc()
{
	sectree_list::iterator it_tree = neighbor_list_.begin();

	while (it_tree != neighbor_list_.end())
	{
		++(*it_tree)->pc_count_;
		++it_tree;
	}
}

void sectree::decrease_pc()
{
	sectree_list::iterator it_tree = neighbor_list_.begin();

	while (it_tree != neighbor_list_.end())
	{
		sectree * tree = *it_tree++;

		if (--tree->pc_count_ <= 0)
		{
			if (tree->pc_count_ < 0)
			{
				metin2::log::error("tree pc count lower than zero (value {:d} coord {:d} {:d})", tree->pc_count_, tree->id_.coord.x, tree->id_.coord.y);
				tree->pc_count_ = 0;
			}

			entity_set::iterator it_entity = tree->set_entity_.begin();

			while (it_entity != tree->set_entity_.end())
			{
				entity * pkEnt = *(it_entity++);

				if (pkEnt->is_type(ENTITY_CHARACTER))
				{
					character * ch = (character *) pkEnt;
					ch->stop_state_machine();
				}
			}
		}
	}
}

bool sectree::insert_entity(entity * pkEnt)
{
	sectree * pkCurTree;

	if ((pkCurTree = pkEnt->get_sectree()) == this)
		return false;

	if (set_entity_.find(pkEnt) != set_entity_.end()) {
		metin2::log::error("entity {} already exist in this sectree!", static_cast<const void *>(pkEnt));
		return false;
	}

	if (pkCurTree)
		pkCurTree->set_entity_.erase(pkEnt);

	pkEnt->set_sectree(this);

	set_entity_.insert(pkEnt);

	if (pkEnt->is_type(ENTITY_CHARACTER))
	{
		character * pkChr = (character *) pkEnt;

		if (pkChr->is_pc())
		{
			increase_pc();

			if (pkCurTree)
				pkCurTree->decrease_pc();
		}
		else if (pc_count_ > 0 && !pkChr->is_warp() && !pkChr->is_goto()) // If it's not a PC and there's a PC here, start the Idle event.
		{
			pkChr->start_state_machine();
		}
	}

	return true;
}

void sectree::remove_entity(entity * pkEnt)
{
	entity_set::iterator it = set_entity_.find(pkEnt);

	if (it == set_entity_.end()) {
		return;
	}
	set_entity_.erase(it);

	pkEnt->set_sectree(nullptr);

	if (pkEnt->is_type(ENTITY_CHARACTER))
	{
		if (((character *) pkEnt)->is_pc())
			decrease_pc();
	}
}

void sectree::bind_attribute(attribute * pkAttribute)
{
	attribute_ = pkAttribute;
}

void sectree::clone_attribute(sectree * tree)
{
	attribute_ = tree->attribute_;
	is_clone_ = true;
}

void sectree::set_attribute(uint32_t x, uint32_t y, uint32_t dwAttr)
{
	assert(attribute_ != nullptr);
	attribute_->set(x, y, dwAttr);
}

void sectree::remove_attribute(uint32_t x, uint32_t y, uint32_t dwAttr)
{
	assert(attribute_ != nullptr);
	attribute_->remove(x, y, dwAttr);
}

uint32_t sectree::get_attribute(int32_t x, int32_t y)
{
	assert(attribute_ != nullptr);
	return attribute_->get((x % SECTREE_SIZE) / CELL_SIZE, (y % SECTREE_SIZE) / CELL_SIZE);
}

bool sectree::is_attr(int32_t x, int32_t y, uint32_t dwFlag)
{
	if (is_set(get_attribute(x, y), dwFlag))
		return true;

	return false;
}

int sectree::event_attribute(int32_t x, int32_t y)
{
	return get_attribute(x, y) >> 8;
}
}
