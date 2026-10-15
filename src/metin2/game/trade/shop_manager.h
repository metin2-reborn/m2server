#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <map>

namespace metin2::game::inline trade {

class CShop;

class CShopManager : public singleton<CShopManager>
{
public:
	typedef std::map<uint32_t, CShop *> TShopMap;

public:
	CShopManager();
	~CShopManager() override;

	bool	Initialize(TShopTable * table, int size);
	void	Destroy();

	CShop *	Get(uint32_t dwVnum);
	CShop *	GetByNPCVnum(uint32_t dwVnum);

	bool	StartShopping(CHARACTER * pkChr, CHARACTER * pkShopKeeper, int iShopVnum = 0);
	void	StopShopping(CHARACTER * ch);

	void	Buy(CHARACTER * ch, uint8_t pos);
	void	Sell(CHARACTER * ch, uint8_t bCell, uint8_t bCount=0);

	CShop *	CreatePCShop(CHARACTER * ch, TShopItemTable * pTable, uint8_t bItemCount);
	CShop *	FindPCShop(uint32_t dwVID);
	void	DestroyPCShop(CHARACTER * ch);

private:
	TShopMap	m_map_pkShop;
	TShopMap	m_map_pkShopByNPCVnum;
	TShopMap	m_map_pkShopByPC;

	bool	ReadShopTableEx(const char* stFileName);
};
}
