#include <metin2/game/items/DragonSoul.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/items/dragon_soul_table.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace metin2::game::inline items {

typedef std::vector <std::string> token_vector;
int gamble(std::vector<float>& vec_probs)
{
	float range = 0.f;
	for (size_t i = 0; i < vec_probs.size(); i++)
	{
		range += vec_probs[i];
	}
	float fProb = fnumber(0.f, range);
	float sum = 0.f;
	for (size_t idx = 0; idx < vec_probs.size(); idx++)
	{
		sum += vec_probs[idx];
		if (sum >= fProb)
			return idx;
	}
	return -1;
}

// Takes a weight table (prob_lst), selects random_set.size() indices and returns random_set
bool make_distinct_random_number_set(std::list <float> prob_lst, std::vector<int>& random_set)
{
	int size = prob_lst.size();
	int n = random_set.size();
	if (size < n)
		return false;

	std::vector <int> select_bit(size, 0);
	for (int i = 0; i < n; i++)
	{
		float range = 0.f;
		for (std::list <float>::iterator it = prob_lst.begin(); it != prob_lst.end(); it++)
		{
			range += *it;
		}
		float r = fnumber (0.f, range);
		float sum = 0.f;
		int idx = 0;
		for (std::list <float>::iterator it = prob_lst.begin(); it != prob_lst.end(); it++)
		{
			while (select_bit[idx++]);

			sum += *it;
			if (sum >= r)
			{
				select_bit[idx - 1] = 1;
				random_set[i] = idx - 1;
				prob_lst.erase(it);
				break;
			}
		}
	}
	return true;
}

/* Comment on dragon soul stone Vnum	
 * If the ITEM VNUM, from the 100,000s digit down, is FEDCBA
 * FE : dragon soul type.	D : grade
 * C : step			B : strength		
 * A : spare numbers... 	
 */

uint8_t get_type(uint32_t dwVnum)
{
	return (dwVnum / 10000);
}

uint8_t get_grade_idx(uint32_t dwVnum)
{
	return (dwVnum / 1000) % 10;
}

uint8_t get_step_idx(uint32_t dwVnum)
{
	return (dwVnum / 100) % 10;
}

uint8_t get_strength_idx(uint32_t dwVnum)
{
	return (dwVnum / 10) % 10;
}

bool ds_manager::read_dragon_soul_table_file(const char * c_pszFileName)
{
	table_ = std::make_unique<dragon_soul_table>();
	return table_->read_dragon_soul_table_file(c_pszFileName);
}

void ds_manager::dragon_soul_info(uint32_t dwVnum, uint8_t& bType, uint8_t& bGrade, uint8_t& bStep, uint8_t& bStrength) const
{
	bType = get_type(dwVnum);
	bGrade = get_grade_idx(dwVnum);
	bStep = get_step_idx(dwVnum);
	bStrength = get_strength_idx(dwVnum);
}

bool ds_manager::is_valid_cell_for_this_item(item * const pItem, const item_pos& Cell) const
{
	if (nullptr == pItem)
		return false;

	uint16_t wBaseCell = base_position(pItem);
	if (WORD_MAX == wBaseCell)
		return false;

	if (Cell.window_type != DRAGON_SOUL_INVENTORY
		|| (Cell.cell < wBaseCell || Cell.cell >= wBaseCell + DRAGON_SOUL_BOX_SIZE))
	{
		return false;
	}
	else
		return true;

}

uint16_t ds_manager::base_position(item * const pItem) const
{
	if (nullptr == pItem)
		return WORD_MAX;

	uint8_t type, grade_idx, step_idx, strength_idx;
	dragon_soul_info(pItem->vnum(), type, grade_idx, step_idx, strength_idx);

	uint8_t col_type = pItem->sub_type();
	uint8_t row_type = grade_idx;
	if (row_type > DRAGON_SOUL_GRADE_MAX)
		return WORD_MAX;

	return col_type * DRAGON_SOUL_STEP_MAX * DRAGON_SOUL_BOX_SIZE + row_type * DRAGON_SOUL_BOX_SIZE;
}

bool ds_manager::refresh_item_attributes(item * pDS)
{
	if (!pDS->is_dragon_soul())
	{
		metin2::log::error("This item(ID : {:d}) is not DragonSoul.", pDS->id());
		return false;
	}

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	dragon_soul_info(pDS->vnum(), ds_type, grade_idx, step_idx, strength_idx);

	dragon_soul_table::vec_applys vec_basic_applys;
	dragon_soul_table::vec_applys vec_addtional_applys;

	if (!table_->basic_applys(ds_type, vec_basic_applys))
	{
		metin2::log::error("There is no BasicApply about {:d} type dragon soul.", ds_type);
		return false;
	}

	if (!table_->additional_applys(ds_type, vec_addtional_applys))
	{
		metin2::log::error("There is no AdditionalApply about {:d} type dragon soul.", ds_type);
		return false;
	}

	// add_min and add_max are read as dummies.
	int basic_apply_num, add_min, add_max;
	if (!table_->apply_num_settings(ds_type, grade_idx, basic_apply_num, add_min, add_max))
	{
		metin2::log::error("In ApplyNumSettings, INVALID VALUES Group type({:d}), GRADE idx({:d})", ds_type, grade_idx);
		return false;
	}

	float fWeight = 0.f;
	if (!table_->weight(ds_type, grade_idx, step_idx, strength_idx, fWeight))
	{
		return false;
	}
	fWeight /= 100.f;

	int n = std::min<int>(basic_apply_num, vec_basic_applys.size());
	for (int i = 0; i < n; i++)
	{	
		const apply& basic_apply = vec_basic_applys[i];
		uint8_t bType = basic_apply.apply_type;
		short sValue = (short)(ceil((float)basic_apply.apply_value * fWeight - 0.01f));

		pDS->set_force_attribute(i, bType, sValue);
	}

	for (int i = DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
	{
		uint8_t bType = pDS->attribute_type(i);
		short sValue = 0;
		if (APPLY_NONE == bType)
			continue;
		for (size_t j = 0; j < vec_addtional_applys.size(); j++)
		{
			if (vec_addtional_applys[j].apply_type == bType)
			{
				sValue = vec_addtional_applys[j].apply_value;
				break;
			}
		}
		pDS->set_force_attribute(i, bType, (short)(ceil((float)sValue * fWeight - 0.01f)));
	}
	return true;
}

bool ds_manager::put_attributes(item * pDS)
{
	if (!pDS->is_dragon_soul())
	{
		metin2::log::error("This item(ID : {:d}) is not DragonSoul.", pDS->id());
		return false;
	}

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	dragon_soul_info(pDS->vnum(), ds_type, grade_idx, step_idx, strength_idx);

	dragon_soul_table::vec_applys vec_basic_applys;
	dragon_soul_table::vec_applys vec_addtional_applys;

	if (!table_->basic_applys(ds_type, vec_basic_applys))
	{
		metin2::log::error("There is no BasicApply about {:d} type dragon soul.", ds_type);
		return false;
	}
	if (!table_->additional_applys(ds_type, vec_addtional_applys))
	{
		metin2::log::error("There is no AdditionalApply about {:d} type dragon soul.", ds_type);
		return false;
	}

	
	int basic_apply_num, add_min, add_max;
	if (!table_->apply_num_settings(ds_type, grade_idx, basic_apply_num, add_min, add_max))
	{
		metin2::log::error("In ApplyNumSettings, INVALID VALUES Group type({:d}), GRADE idx({:d})", ds_type, grade_idx);
		return false;
	}

	float fWeight = 0.f;
	if (!table_->weight(ds_type, grade_idx, step_idx, strength_idx, fWeight))
	{
		return false;
	}
	fWeight /= 100.f;

	int n = std::min<int>(basic_apply_num, vec_basic_applys.size());
	for (int i = 0; i < n; i++)
	{	
		const apply& basic_apply = vec_basic_applys[i];
		uint8_t bType = basic_apply.apply_type;
		short sValue = (short)(ceil((float)basic_apply.apply_value * fWeight - 0.01f));

		pDS->set_force_attribute(i, bType, sValue);
	}

	uint8_t additional_attr_num = std::min(number (add_min, add_max), 3);

	std::vector <int> random_set;
	if (additional_attr_num > 0)
	{
		random_set.resize(additional_attr_num);
		std::list <float> list_probs;
		for (size_t i = 0; i < vec_addtional_applys.size(); i++)
		{
			list_probs.push_back(vec_addtional_applys[i].prob);
		}
		if (!make_distinct_random_number_set(list_probs, random_set))
		{
			metin2::log::error("MakeDistinctRandomNumberSet error.");
			return false;
		}

		for (int i = 0; i < additional_attr_num; i++)
		{
			int r = random_set[i];
			const apply& additional_attr = vec_addtional_applys[r];
			uint8_t bType = additional_attr.apply_type;
			short sValue = (short)(ceil((float)additional_attr.apply_value * fWeight - 0.01f));

			pDS->set_force_attribute(DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX + i, bType, sValue);
		}
	}

	return true;
}

bool ds_manager::dragon_soul_item_initialize(item * pItem)
{
	if (nullptr == pItem || !pItem->is_dragon_soul())
		return false;
	put_attributes(pItem);
	int time = ds_manager::instance().duration(pItem);
	if (time > 0)
		pItem->set_socket(ITEM_SOCKET_REMAIN_SEC, time);
	return true;
}

uint32_t ds_manager::make_dragon_soul_vnum(uint8_t bType, uint8_t grade, uint8_t step, uint8_t refine)
{
	return bType * 10000 + grade * 1000 + step * 100 + refine * 10;
}

int ds_manager::duration(item * const pItem) const
{
	return pItem->duration();
}

// Function that takes a dragon soul stone and extracts a dragon heart
bool ds_manager::extract_dragon_heart(character * ch, item * pItem, item * pExtractor)
{
	if (nullptr == ch || nullptr == pItem)
		return false;
	if (pItem->is_equipped())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("착용 중인 용혼석은 추출할 수 없습니다."));
		return false;
	}

	// The stone is consumed; one offered in a trade window would leave the exchange with a dangling pointer.
	if (pItem->is_exchanging())
		return false;

	uint32_t dwVnum = pItem->vnum();
	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	dragon_soul_info(dwVnum, ds_type, grade_idx, step_idx, strength_idx);

	int iBonus = 0;

	if (nullptr != pExtractor)
	{
		iBonus = pExtractor->value(0);
	}

	std::vector <float> vec_chargings;
	std::vector <float> vec_probs;

	if (!table_->dragon_heart_ext_values(ds_type, grade_idx, vec_chargings, vec_probs))
	{
		return false;
	}

	int idx = gamble(vec_probs);

	if (-1 == idx)
	{
		metin2::log::error("Gamble is failed. ds_type({:d}), grade_idx({:d})", ds_type, grade_idx);
		return false;
	}

	float fCharge = vec_chargings[idx] * (100 + iBonus) / 100.f;
	fCharge = std::clamp(fCharge, 0.f, 100.f);

	if (fCharge < FLT_EPSILON)
	{
		// Log first: SetCount(0) destroys the item.
		log_manager::instance().item_log(ch, pItem, "DS_HEART_EXTRACT_FAIL", "");
		pItem->set_count(pItem->count() - 1);
		if (nullptr != pExtractor)
		{
			pExtractor->set_count(pExtractor->count() - 1);
		}
	
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("용심 추출에 실패하였습니다."));
		return false;
	}
	else
	{
		item * pDH = item_manager::instance().create_item(DRAGON_HEART_VNUM);
		
		if (nullptr == pDH)
		{
			metin2::log::error("Cannot create DRAGON_HEART({:d}).", DRAGON_HEART_VNUM);
			return false;
		}

		int iCharge = (int)(fCharge + 0.5f);

		// Log first: SetCount(0) destroys the item.
		std::string s = std::to_string(iCharge);
		s += "%s";
		log_manager::instance().item_log(ch, pItem, "DS_HEART_EXTRACT_SUCCESS", s.c_str());

		pItem->set_count(pItem->count() - 1);
		if (nullptr != pExtractor)
		{
			pExtractor->set_count(pExtractor->count() - 1);
		}

		pDH->set_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX, iCharge);
		ch->auto_give_item(pDH, true);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("용심 추출에 성공하였습니다."));
		return true;
	}
}

// Function that decides success when removing a specific dragon soul stone from the equipment window, and gives a by-product on failure.
bool ds_manager::pull_out(character * ch, item_pos DestCell, item *& pItem, item * pExtractor)
{
	if (nullptr == ch || nullptr == pItem)
	{
		metin2::log::error("NULL POINTER. ch({}) or pItem({})", static_cast<const void *>(ch), static_cast<const void *>(pItem));
		return false;
	}

	// Check whether the target position is valid; if not, find an arbitrary empty slot.
	if (!is_valid_cell_for_this_item(pItem, DestCell))
	{
		int iEmptyCell = ch->empty_dragon_soul_inventory(pItem);
		if (iEmptyCell < 0)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
			return false;
		}
		else
		{
			DestCell.window_type = DRAGON_SOUL_INVENTORY;
			DestCell.cell = iEmptyCell;
		}
	}

	if (!pItem->is_equipped() || !pItem->remove_from_character())
		return false;

	bool bSuccess;
	uint32_t dwByProduct = 0;
	int iBonus = 0;
	float fProb;
	float fDice;
	// Decide dragon soul extraction success.
	{

		uint8_t ds_type, grade_idx, step_idx, strength_idx;
		dragon_soul_info(pItem->vnum(), ds_type, grade_idx, step_idx, strength_idx);

		// If there is no extraction info, assume it always succeeds for now.
		if (!table_->dragon_soul_ext_values(ds_type, grade_idx, fProb, dwByProduct))
		{
			pItem->add_to_character(ch, DestCell);
			return true;
		}


		if (nullptr != pExtractor)
		{
			iBonus = pExtractor->value(ITEM_VALUE_DRAGON_SOUL_POLL_OUT_BONUS_IDX);
			pExtractor->set_count(pExtractor->count() - 1);
		}
		fDice = fnumber(0.f, 100.f);
		bSuccess = fDice <= (fProb * (100 + iBonus) / 100.f);
	}

	// Extract and add or remove a character's dragon soul stone. Give by-products.
	{
		char buf[128];

		if (bSuccess)
		{
			if (pExtractor)
			{
				snprintf(buf, sizeof(buf), "dice(%d) prob(%d + %d) EXTR(VN:%d)", (int)fDice, (int)fProb, iBonus, pExtractor->vnum());
			}
			else
			{
				snprintf(buf, sizeof(buf), "dice(%g) prob(%g)", fDice, fProb);
			}
			log_manager::instance().item_log(ch, pItem, "DS_PULL_OUT_SUCCESS", buf);
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 성공하였습니다."));
			pItem->add_to_character(ch, DestCell);
			return true;
		}
		else
		{
			if (pExtractor)
			{
				snprintf(buf, sizeof(buf), "dice(%d) prob(%d + %d) EXTR(VN:%d) ByProd(VN:%d)", (int)fDice, (int)fProb, iBonus, pExtractor->vnum(), dwByProduct);
			}
			else
			{
				snprintf(buf, sizeof(buf), "dice(%d) prob(%d) ByProd(VNUM:%d)", (int)fDice, (int)fProb, dwByProduct);
			}
			log_manager::instance().item_log(ch, pItem, "DS_PULL_OUT_FAILED", buf);
			item_manager::instance().destroy_item(pItem);
			pItem = nullptr;
			if (dwByProduct)
			{
				item * pByProduct = ch->auto_give_item(dwByProduct, true);
				if (pByProduct)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하여 %s를 얻었습니다."), pByProduct->name());
				else
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하였습니다."));
			}
			else
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하였습니다."));
		}
	}

	return bSuccess;
}

bool ds_manager::do_refine_grade(character * ch, item_pos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (nullptr == ch)
		return false;

	if (!ch->dragon_soul_refine_window_can_refine())
	{
		metin2::log::error("{} do not activate DragonSoulRefineWindow. But how can he come here?", ch->name());
		ch->chat_packet(CHAT_TYPE_INFO, "[SYSTEM ERROR]You cannot upgrade dragon soul without refine window.");
		return false;
	}

	// Use a set to remove any possible duplicate item pointers
	// If a weird packet is sent, there may be duplicate or invalid TItemPos entries.
	std::set <item *> set_items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++)
	{
		if (aItemPoses[i].is_equip_position())
			return false;
		item * pItem = ch->get_item(aItemPoses[i]);
		if (nullptr != pItem)
		{
			// Materials are consumed: never take an equipped item or one offered in a trade window.
			if (pItem->is_equipped() || pItem->is_exchanging())
				return false;

			// Non-dragon-soul items cannot be in the refine window.
			if (!pItem->is_dragon_soul())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 필요한 재료가 아닙니다."));
				send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));

				return false;
			}

			set_items.insert(pItem);
		}
	}

	if (set_items.size() == 0)
	{
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	int count = set_items.size();
	int need_count = 0;
	int fee = 0;
	std::vector <float> vec_probs;

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	int result_grade;

	// Use the first one as the basis for the upgrade.
	std::set <item *>::iterator it = set_items.begin();
	{
		item * pItem = *it;

		dragon_soul_info(pItem->vnum(), ds_type, grade_idx, step_idx, strength_idx);
		
		if (!table_->refine_grade_values(ds_type, grade_idx, need_count, fee, vec_probs))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량할 수 없는 용혼석입니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));

			return false;
		}
	}
	while (++it != set_items.end())
	{
		item * pItem = *it;

		// Since the client UI prevents equipped items from being placed in the refine window,
		// no separate notification is done.
		if (pItem->is_equipped())
		{
			return false;
		}
		
		if (ds_type != get_type(pItem->vnum()) || grade_idx != get_grade_idx(pItem->vnum()))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 필요한 재료가 아닙니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));

			return false;
		}
	}

	// The client checks the count once, so if count != need_count it's very likely an invalid client.
	if (count != need_count)
	{
		metin2::log::error("Possiblity of invalid client. Name {}", ch->name());
		uint8_t bSubHeader = count < need_count? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
		send_refine_result_packet(ch, bSubHeader, NPOS);
		return false;
	}

	if (ch->gold() < fee)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}
	
	if (-1 == (result_grade = gamble(vec_probs)))
	{
		metin2::log::error("Gamble failed. See RefineGardeTables' probabilities");
		return false;
	}

	item * pResultItem = item_manager::instance().create_item(make_dragon_soul_vnum(ds_type, (uint8_t)result_grade, 0, 0));

	if (nullptr == pResultItem)
	{
		metin2::log::error("INVALID DRAGON SOUL({:d})", make_dragon_soul_vnum(ds_type, (uint8_t)result_grade, 0, 0));
		return false;
	}

	ch->point_change(POINT_GOLD, -fee);
	int left_count = need_count;
	
	for (std::set <item *>::iterator it = set_items.begin(); it != set_items.end(); it++)
	{
		item * pItem = *it;
		int n = pItem->count();
		if (left_count > n)
		{
			pItem->remove_from_character();
			item_manager::instance().destroy_item(pItem);
			left_count -= n;
		}
		else
		{
			pItem->set_count(n - left_count);
		}
	}

	ch->auto_give_item(pResultItem, true);

	if (result_grade > grade_idx)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "GRADE : %d -> %d", grade_idx, result_grade);
		log_manager::instance().item_log(ch, pResultItem, "DS_GRADE_REFINE_SUCCESS", buf);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 성공했습니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_SUCCEED, item_pos (pResultItem->window(), pResultItem->cell()));
		return true;
	}
	else
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "GRADE : %d -> %d", grade_idx, result_grade);
		log_manager::instance().item_log(ch, pResultItem, "DS_GRADE_REFINE_FAIL", buf);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 실패했습니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL, item_pos (pResultItem->window(), pResultItem->cell()));
		return false;
	}
}

bool ds_manager::do_refine_step(character * ch, item_pos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (nullptr == ch)
		return false;
	if (!ch->dragon_soul_refine_window_can_refine())
	{
		metin2::log::error("{} do not activate DragonSoulRefineWindow. But how can he come here?", ch->name());
		ch->chat_packet(CHAT_TYPE_INFO, "[SYSTEM ERROR]You cannot use dragon soul refine window.");
		return false;
	}

	// Use a set to remove any possible duplicate item pointers
	// If a weird packet is sent, there may be duplicate or invalid TItemPos entries.
	std::set <item *> set_items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++)
	{
		item * pItem = ch->get_item(aItemPoses[i]);
		if (nullptr != pItem)
		{
			// Materials are consumed: never take an equipped item or one offered in a trade window.
			if (pItem->is_equipped() || pItem->is_exchanging())
				return false;

			// Non-dragon-soul items cannot be in the refine window.
			if (!pItem->is_dragon_soul())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 필요한 재료가 아닙니다."));
				send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
				return false;
			}
			set_items.insert(pItem);
		}
	}

	if (set_items.size() == 0)
	{
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	std::string stGroupName;
	int count = set_items.size();
	int need_count = 0;
	int fee = 0;
	std::vector <float> vec_probs;

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	int result_step;

	// Use the first one as the basis for the upgrade.
	std::set <item *>::iterator it = set_items.begin(); 
	{
		item * pItem = *it;
		dragon_soul_info(pItem->vnum(), ds_type, grade_idx, step_idx, strength_idx);

		if (!table_->refine_step_values(ds_type, step_idx, need_count, fee, vec_probs))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량할 수 없는 용혼석입니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
			return false;
		}
	}

	while(++it != set_items.end())
	{
		item * pItem = *it;
		// Since the client UI prevents equipped items from being placed in the refine window,
		// no separate notification is done.
		if (pItem->is_equipped())
		{
			return false;
		}
		if (ds_type != get_type(pItem->vnum()) || grade_idx != get_grade_idx(pItem->vnum()) || step_idx != get_step_idx(pItem->vnum()))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 필요한 재료가 아닙니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
			return false;
		}
	}

	// The client checks the count once, so if count != need_count it's very likely an invalid client.
	if (count != need_count)
	{
		metin2::log::error("Possiblity of invalid client. Name {}", ch->name());
		uint8_t bSubHeader = count < need_count? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
		send_refine_result_packet(ch, bSubHeader, NPOS);
		return false;
	}
	
	if (ch->gold() < fee)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}
	

	if (-1 == (result_step = gamble(vec_probs)))
	{
		metin2::log::error("Gamble failed. See RefineStepTables' probabilities");
		return false;
	}

	item * pResultItem = item_manager::instance().create_item(make_dragon_soul_vnum(ds_type, grade_idx, (uint8_t)result_step, 0));

	if (nullptr == pResultItem)
	{
		metin2::log::error("INVALID DRAGON SOUL({:d})", make_dragon_soul_vnum(ds_type, grade_idx, (uint8_t)result_step, 0));
		return false;
	}

	ch->point_change(POINT_GOLD, -fee);
	int left_count = need_count;
	for (std::set <item *>::iterator it = set_items.begin(); it != set_items.end(); it++)
	{
		item * pItem = *it;
		int n = pItem->count();
		if (left_count > n)
		{
			pItem->remove_from_character();
			item_manager::instance().destroy_item(pItem);
			left_count -= n;
		}
		else
		{
			pItem->set_count(n - left_count);
		}
	}

	ch->auto_give_item(pResultItem, true);
	if (result_step > step_idx)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "STEP : %d -> %d", step_idx, result_step);
		log_manager::instance().item_log(ch, pResultItem, "DS_STEP_REFINE_SUCCESS", buf);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 성공했습니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_SUCCEED, item_pos (pResultItem->window(), pResultItem->cell()));
		return true;
	}
	else
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "STEP : %d -> %d", step_idx, result_step);
		log_manager::instance().item_log(ch, pResultItem, "DS_STEP_REFINE_FAIL", buf);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 실패했습니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL, item_pos (pResultItem->window(), pResultItem->cell()));
		return false;
	}
}

bool is_dragon_soul_refine_material(item * pItem)
{
	if (pItem->type() != ITEM_MATERIAL)
		return false;
	return (pItem->sub_type() == MATERIAL_DS_REFINE_NORMAL ||
		pItem->sub_type() == MATERIAL_DS_REFINE_BLESSED ||
		pItem->sub_type() == MATERIAL_DS_REFINE_HOLLY);
}

bool ds_manager::do_refine_strength(character * ch, item_pos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (nullptr == ch)
		return false;
	if (!ch->dragon_soul_refine_window_can_refine())
	{
		metin2::log::error("{} do not activate DragonSoulRefineWindow. But how can he come here?", ch->name());
		ch->chat_packet(CHAT_TYPE_INFO, "[SYSTEM ERROR]You cannot use dragon soul refine window.");
		return false;
	}

	// Use a set to remove any possible duplicate item pointers
	// If a weird packet is sent, there may be duplicate or invalid TItemPos entries.
	std::set <item *> set_items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++)
	{
		item * pItem = ch->get_item(aItemPoses[i]);
		if (pItem)
		{
			// Materials are consumed: never take an equipped item or one offered in a trade window.
			if (pItem->is_equipped() || pItem->is_exchanging())
				return false;

			set_items.insert(pItem);
		}
	}
	if (set_items.size() == 0)
	{
		return false;
	}

	int fee;

	item * pRefineStone = nullptr;
	item * pDragonSoul = nullptr;
	for (std::set <item *>::iterator it = set_items.begin(); it != set_items.end(); it++)
	{
		item * pItem = *it;
		// Since the client UI prevents equipped items from being placed in the refine window,
		// no separate notification is done.
		if (pItem->is_equipped())
		{
			return false;
		}

		// Only dragon soul stones and strengthening stones can be in the refine window.
		// And there must be only one of each.
		if (pItem->is_dragon_soul())
		{
			if (pDragonSoul != nullptr)
			{
				send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, item_pos(pItem->window(), pItem->cell()));
				return false;	
			}
			pDragonSoul = pItem;
		}
		else if(is_dragon_soul_refine_material(pItem))
		{
			if (pRefineStone != nullptr)
			{
				send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, item_pos(pItem->window(), pItem->cell()));
				return false;	
			}
			pRefineStone = pItem;
		}
		else
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 필요한 재료가 아닙니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
			return false;
		}
	}

	uint8_t bType = 0, bGrade = 0, bStep = 0, bStrength = 0;
	
	if (!pDragonSoul || !pRefineStone)
	{
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);

		return false;
	}
	
	if (nullptr != pDragonSoul)
	{
		dragon_soul_info(pDragonSoul->vnum(), bType, bGrade, bStep, bStrength);

		float fWeight = 0.f;
		// Dragon soul stones with no weight value cannot be strengthened
		if (!table_->weight(bType, bGrade, bStep, bStrength + 1, fWeight))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, item_pos(pDragonSoul->window(), pDragonSoul->cell()));
			return false;
		}
		// If the weight is 0 after strengthening, it must not be strengthened further.
		if (fWeight < FLT_EPSILON)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
			send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, item_pos(pDragonSoul->window(), pDragonSoul->cell()));
			return false;
		}
	}

	float fProb;
	if (!table_->refine_strength_values(bType, pRefineStone->sub_type(), bStrength, fee, fProb))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pDragonSoul->window(), pDragonSoul->cell()));

		return false;
	}

	if (ch->gold() < fee)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
		send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}
	
	ch->point_change(POINT_GOLD, -fee);
	item * pResult = nullptr;
	uint8_t bSubHeader;

	if (fnumber(0.f, 100.f) <= fProb)
	{
		pResult = item_manager::instance().create_item(make_dragon_soul_vnum(bType, bGrade, bStep, bStrength + 1));
		if (nullptr == pResult)
		{
			metin2::log::error("INVALID DRAGON SOUL({:d})", make_dragon_soul_vnum(bType, bGrade, bStep, bStrength + 1));
			return false;
		}
		pDragonSoul->remove_from_character();

		pDragonSoul->copy_attribute_to(pResult);
		refresh_item_attributes(pResult);

		pDragonSoul->set_count(pDragonSoul->count() - 1);
		pRefineStone->set_count(pRefineStone->count() - 1);

		char buf[128];
		snprintf(buf, sizeof(buf), "STRENGTH : %d -> %d", bStrength, bStrength + 1);
		log_manager::instance().item_log(ch, pDragonSoul, "DS_STRENGTH_REFINE_SUCCESS", buf);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 성공했습니다."));
		ch->auto_give_item(pResult, true);
		bSubHeader = DS_SUB_HEADER_REFINE_SUCCEED;
	}
	else
	{
		if (bStrength != 0)
		{
			pResult = item_manager::instance().create_item(make_dragon_soul_vnum(bType, bGrade, bStep, bStrength - 1));
			if (nullptr == pResult)
			{
				metin2::log::error("INVALID DRAGON SOUL({:d})", make_dragon_soul_vnum(bType, bGrade, bStep, bStrength - 1));
				return false;
			}
			pDragonSoul->copy_attribute_to(pResult);
			refresh_item_attributes(pResult);
		}
		bSubHeader = DS_SUB_HEADER_REFINE_FAIL;

		char buf[128];
		snprintf(buf, sizeof(buf), "STRENGTH : %d -> %d", bStrength, bStrength - 1);
		// Strength upgrades can break the item on failure, so log based on the original item.
		log_manager::instance().item_log(ch, pDragonSoul, "DS_STRENGTH_REFINE_FAIL", buf);

		ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 실패했습니다."));
		pDragonSoul->set_count(pDragonSoul->count() - 1);
		pRefineStone->set_count(pRefineStone->count() - 1);
		if (nullptr != pResult)
			ch->auto_give_item(pResult, true);
		
	}
	
	send_refine_result_packet(ch, bSubHeader, nullptr == pResult? NPOS : item_pos (pResult->window(), pResult->cell()));

	return true;
}

void ds_manager::send_refine_result_packet(character * ch, uint8_t bSubHeader, const item_pos& pos)
{
	packet_gc_dragon_soul_refine pack;
	pack.sub_type = bSubHeader;

	if (pos.is_valid_item_position())
	{
		pack.pos = pos;
	}
	desc * d = ch->get_desc();
	if (nullptr == d)
	{
		return ;
	}
	else
	{
		d->packet(&pack, sizeof(pack));
	}
}

int ds_manager::left_time(item * pItem) const
{
	if (pItem == nullptr)
		return false;

	// For now, only timer-based-on-wear dragon soul stones don't disappear when their time runs out.
	if (pItem->proto()->c_limit_timer_based_on_wear_index >= 0)
	{
		return pItem->get_socket(ITEM_SOCKET_REMAIN_SEC);
	}
	// Dragon soul stones of other limit types all disappear when time runs out, so items reaching here are assumed to have time left.
	else
	{
		return INT_MAX;
	}
}

bool ds_manager::is_time_left_dragon_soul(item * pItem) const
{
	if (pItem == nullptr)
		return false;

	// For now, only timer-based-on-wear dragon soul stones don't disappear when their time runs out.
	if (pItem->proto()->c_limit_timer_based_on_wear_index >= 0)
	{
		return pItem->get_socket(ITEM_SOCKET_REMAIN_SEC) > 0;
	}
	// Dragon soul stones of other limit types all disappear when time runs out, so items reaching here are assumed to have time left.
	else
	{
		return true;
	}
}

bool ds_manager::is_active_dragon_soul(item * pItem) const
{
	return pItem->get_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX);
}

bool ds_manager::activate_dragon_soul(item * pItem)
{
	if (nullptr == pItem)
		return false;
	character * pOwner = pItem->owner();
	if (nullptr == pOwner)
		return false;

	int deck_idx = pOwner->dragon_soul_get_active_deck();
	
	if (deck_idx < 0)
		return false;
	
	if (INVENTORY_MAX_NUM + WEAR_MAX_NUM + DS_SLOT_MAX * deck_idx <= pItem->cell() &&
			pItem->cell() < INVENTORY_MAX_NUM + WEAR_MAX_NUM + DS_SLOT_MAX * (deck_idx + 1))
	{
		if (is_time_left_dragon_soul(pItem) && !is_active_dragon_soul(pItem))
		{
			char buf[128];
			sprintf (buf, "LEFT TIME(%d)", left_time(pItem));
			log_manager::instance().item_log(pOwner, pItem, "DS_ACTIVATE", buf);
			pItem->modify_points(true);
			pItem->set_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 1);

			pItem->start_timer_based_on_wear_expire_event();
		}
		return true;
	}
	else
		return false;
}

bool ds_manager::deactivate_dragon_soul(item * pItem, bool bSkipRefreshOwnerActiveState)
{
	if (nullptr == pItem)
		return false;

	character * pOwner = pItem->owner();
	if (nullptr == pOwner)
		return false;

	if (!is_active_dragon_soul(pItem))
		return false;

	char buf[128];
	pItem->stop_timer_based_on_wear_expire_event();
	pItem->set_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
	pItem->modify_points(false);

	sprintf (buf, "LEFT TIME(%d)", left_time(pItem));
	log_manager::instance().item_log(pOwner, pItem, "DS_DEACTIVATE", buf);

	if (false == bSkipRefreshOwnerActiveState)
		refresh_dragon_soul_state(pOwner);

	return true;
}

void ds_manager::refresh_dragon_soul_state(character * ch)
{
	if (nullptr == ch)
		return ;
	for (int i = WEAR_MAX_NUM; i < WEAR_MAX_NUM + static_cast<int>(DS_SLOT_MAX) * DRAGON_SOUL_DECK_MAX_NUM; i++)
	{
		item * pItem = ch->wear(i);
		if (pItem != nullptr)
		{
			if(is_active_dragon_soul(pItem))
			{
				return;
			}
		}
	}
	ch->dragon_soul_deactivate_all();
}

ds_manager::ds_manager()
{
}

ds_manager::~ds_manager()
{
}
}
