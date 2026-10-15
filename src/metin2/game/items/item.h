#pragma once

#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>

namespace metin2::game::inline protocol {
    class CInputDB;
}

namespace metin2::game::inline items {

class CItem : public CEntity
{
	protected:
		// override methods from ENTITY class
		void	EncodeInsertPacket(CEntity * entity) override;
		void	EncodeRemovePacket(CEntity * entity) override;

	public:
		CItem(uint32_t dwVnum);
		~CItem() override;

		int			GetLevelLimit();

		bool		CheckItemUseLevel(int nLevel);

		bool		IsPCBangItem();

		bool		IsStackable()		{ return (GetFlag() & ITEM_FLAG_STACKABLE)?true:false; }

		void		Initialize();
		void		Destroy();

		void		Save();

		void		SetWindow(uint8_t b)	{ m_bWindow = b; }
		uint8_t		GetWindow()		{ return m_bWindow; }

		void		SetID(uint32_t id)		{ m_dwID = id;	}
		uint32_t		GetID()			{ return m_dwID; }

		void			SetProto(const TItemTable * table);
		TItemTable const *	GetProto()	{ return m_pProto; }

		int		GetShopBuyPrice();
		const char *	GetName()		{ return m_pProto ? m_pProto->szLocaleName : nullptr; }
		const char *	GetBaseName()		{ return m_pProto ? m_pProto->szName : nullptr; }
		uint8_t		GetSize()		{ return m_pProto ? m_pProto->bSize : 0;	}

		void		SetFlag(int32_t flag)	{ m_lFlag = flag;	}
		int32_t		GetFlag()		{ return m_lFlag;	}

		uint32_t		GetWearFlag()		{ return m_pProto ? m_pProto->dwWearFlags : 0; }
		uint32_t		GetAntiFlag()		{ return m_pProto ? m_pProto->dwAntiFlags : 0; }
		uint32_t		GetImmuneFlag()		{ return m_pProto ? m_pProto->dwImmuneFlag : 0; }

		void		SetVID(uint32_t vid)	{ m_dwVID = vid;	}
		uint32_t		GetVID()		{ return m_dwVID;	}

		bool		SetCount(uint32_t count);
		uint32_t		GetCount();

		// Comment on GetVnum and GetOriginalVnum
		// GetVnum is the masked Vnum. With it, an item whose real Vnum is 10 can behave as if its Vnum were 20.
		// The masking values are defined in ori_to_new.txt.
		// GetOriginalVnum is the item's own Vnum; use it when logging, when sending item info to the client, and when saving.
		// 
		uint32_t		GetVnum() const		{ return m_dwMaskVnum ? m_dwMaskVnum : m_dwVnum;	}
		uint32_t		GetOriginalVnum() const		{ return m_dwVnum;	}
		uint8_t		GetType() const		{ return m_pProto ? m_pProto->bType : 0;	}
		uint8_t		GetSubType() const	{ return m_pProto ? m_pProto->bSubType : 0;	}
		uint8_t		GetLimitType(uint32_t idx) const { return m_pProto ? m_pProto->aLimits[idx].bType : 0;	}
		int32_t		GetLimitValue(uint32_t idx) const { return m_pProto ? m_pProto->aLimits[idx].lValue : 0;	}

		int32_t		GetValue(uint32_t idx);

		void		SetCell(CHARACTER * ch, uint16_t pos)	{ m_pOwner = ch, m_wCell = pos;	}
		uint16_t		GetCell()				{ return m_wCell;	}

		CItem *		RemoveFromCharacter();
		bool		AddToCharacter(CHARACTER * ch, TItemPos Cell);
		CHARACTER *	GetOwner()		{ return m_pOwner; }

		CItem *		RemoveFromGround();
		bool		AddToGround(int32_t lMapIndex, const PIXEL_POSITION & pos, bool skipOwnerCheck = false);

		int			FindEquipCell(CHARACTER * ch, int bCandidateCell = -1);
		bool		IsEquipped() const		{ return m_bEquipped;	}
		bool		EquipTo(CHARACTER * ch, uint8_t bWearCell);
		bool		IsEquipable() const;

		bool		CanUsedBy(CHARACTER * ch);

		bool		DistanceValid(CHARACTER * ch);

		void		UpdatePacket();

		void		SetExchanging(bool isOn = true);
		bool		IsExchanging()		{ return m_bExchanging;	}

		void		ModifyPoints(bool bAdd);	// Applies the item's effects to the character. Removes them if bAdd is false

		const int32_t *	GetSockets()		{ return &m_alSockets[0];	}
		int32_t		GetSocket(int i)	{ return m_alSockets[i];	}

		void		SetSockets(const int32_t * al);
		void		SetSocket(int i, int32_t v, bool bLog = true);

		int		GetSocketCount();
		bool		AddSocket();

		const TPlayerItemAttribute* GetAttributes()		{ return m_aAttr;	} 
		const TPlayerItemAttribute& GetAttribute(int i)	{ return m_aAttr[i];	}

		uint8_t		GetAttributeType(int i)	{ return m_aAttr[i].bType;	}
		short		GetAttributeValue(int i){ return m_aAttr[i].sValue;	}

		void		SetAttributes(const TPlayerItemAttribute* c_pAttribute);
		
		int		FindAttribute(uint8_t bType);
		bool		RemoveAttributeType(uint8_t bType);

		bool		HasAttr(uint8_t bApply);
		bool		HasRareAttr(uint8_t bApply);

		void		SetDestroyEvent(LPEVENT pkEvent);
		void		StartDestroyEvent(int iSec=300);

		uint32_t		GetRefinedVnum()	{ return m_pProto ? m_pProto->dwRefinedVnum : 0; }
		uint32_t		GetRefineFromVnum();
		int		GetRefineLevel();

		void		SetSkipSave(bool b)	{ m_bSkipSave = b; }
		bool		GetSkipSave()		{ return m_bSkipSave; }

		bool		IsOwnership(CHARACTER * ch);
		void		SetOwnership(CHARACTER * ch, int iSec = 10);
		void		SetOwnershipEvent(LPEVENT pkEvent);

		uint32_t		GetLastOwnerPID()	{ return m_dwLastOwnerPID; }

		int		GetAttributeSetIndex(); // Returns which index of the attribute-assignment array is used.
		void		AlterToMagicItem();
		void		AlterToSocketItem(int iSocketCount);

		uint16_t		GetRefineSet()		{ return m_pProto ? m_pProto->wRefineSet : 0;	}

		void		StartUniqueExpireEvent();
		void		SetUniqueExpireEvent(LPEVENT pkEvent);

		void		StartTimerBasedOnWearExpireEvent();
		void		SetTimerBasedOnWearExpireEvent(LPEVENT pkEvent);

		void		StartRealTimeExpireEvent();
		bool		IsRealTimeItem();

		void		StopUniqueExpireEvent();
		void		StopTimerBasedOnWearExpireEvent();
		void		StopAccessorySocketExpireEvent();

		//			Currently only works correctly for REAL_TIME and TIMER_BASED_ON_WEAR items.
		int			GetDuration();

		int		GetAttributeCount();
		void		ClearAttribute();
		void		ChangeAttribute(const int* aiChangeProb=nullptr);
		void		AddAttribute();
		void		AddAttribute(uint8_t bType, short sValue);

		void		ApplyAddon(int iAddonType);

		int		GetSpecialGroup() const;
		bool	IsSameSpecialGroup(CItem * const item) const;

		// ACCESSORY_REFINE
		// Add a socket to an accessory via the mine
		bool		IsAccessoryForSocket();

		int		GetAccessorySocketGrade();
		int		GetAccessorySocketMaxGrade();
		int		GetAccessorySocketDownGradeTime();

		void		SetAccessorySocketGrade(int iGrade);
		void		SetAccessorySocketMaxGrade(int iMaxGrade);
		void		SetAccessorySocketDownGradeTime(uint32_t time);

		void		AccessorySocketDegrade();

		// Timer that runs when an accessory (copper, etc.) is set into an item
		void		StartAccessorySocketExpireEvent();
		void		SetAccessorySocketExpireEvent(LPEVENT pkEvent);

		bool		CanPutInto(CItem * item);
		// END_OF_ACCESSORY_REFINE

		void		CopyAttributeTo(CItem * pItem);
		void		CopySocketTo(CItem * pItem);

		int			GetRareAttrCount();
		bool		AddRareAttribute();
		bool		ChangeRareAttribute();

		void		AttrLog();

		void		Lock(bool f) { m_isLocked = f; }
		bool		isLocked() const { return m_isLocked; }

	private :
		void		SetAttribute(int i, uint8_t bType, short sValue);
	public:
		void		SetForceAttribute(int i, uint8_t bType, short sValue);

	protected:
		bool		Unequip();

		void		AddAttr(uint8_t bApply, uint8_t bLevel);
		void		PutAttribute(const int * aiAttrPercentTable);
		void		PutAttributeWithLevel(uint8_t bLevel);

	protected:
		friend class protocol::CInputDB;
		bool		OnAfterCreatedItem();			// Called after the item has been fully created (loaded) on the server with all its data.

	public:
		bool		IsRideItem();

		void		ClearMountAttributeAndAffect();

		// Germany wanted cash items identical to the existing ones but tradable,
		// so new items, the originals with only the no-trade flag removed, were assigned to a new item range.
		// The problem is that the new items must have the same effects as the originals,
		// but both server and client are vnum based,
		// so we faced the sad situation of having to hardcode all the new vnums into the server.
		// So for an item with a new vnum, the server runs it as the original item vnum,
		// and changes it back to the real vnum when saving.

		// The mask vnum exists for items that run under a different vnum for some reason (e.g. the situation above).
		void		SetMaskVnum(uint32_t vnum)	{	m_dwMaskVnum = vnum; }

		// Dragon soul
		bool		IsDragonSoul();
		int		GiveMoreTime_Per(float fPercent);
		int		GiveMoreTime_Fix(uint32_t dwTime);

	private:
		TItemTable const * m_pProto;		// Prototype

		uint32_t		m_dwVnum;
		CHARACTER *	m_pOwner;

		uint8_t		m_bWindow;		// Window the item is currently in 
		uint32_t		m_dwID;			// Unique ID
		bool		m_bEquipped;	// Is it equipped?
		uint32_t		m_dwVID;		// VID
		uint16_t		m_wCell;		// Position
		uint32_t		m_dwCount;		// Count
		int32_t		m_lFlag;		// Extra flag
		uint32_t		m_dwLastOwnerPID;	// PID of the last owner

		bool		m_bExchanging;	///< Currently being traded 

		int32_t		m_alSockets[ITEM_SOCKET_MAX_NUM];	// Item sockets
		TPlayerItemAttribute	m_aAttr[ITEM_ATTRIBUTE_MAX_NUM];

		LPEVENT		m_pkDestroyEvent;
		LPEVENT		m_pkUniqueExpireEvent;
		LPEVENT		m_pkTimerBasedOnWearExpireEvent;
		LPEVENT		m_pkRealTimeExpireEvent;
		LPEVENT		m_pkAccessorySocketExpireEvent;
		LPEVENT		m_pkOwnershipEvent;

		uint32_t		m_dwOwnershipPID;

		bool		m_bSkipSave;

		bool		m_isLocked;
		
		uint32_t		m_dwMaskVnum;
		uint32_t		m_dwSIGVnum;
	public:
		void SetSIGVnum(uint32_t dwSIG)
		{
			m_dwSIGVnum = dwSIG;
		}
		uint32_t	GetSIGVnum() const
		{
			return m_dwSIGVnum;
		}
};

EVENTINFO(item_event_info)
{
	CItem * item;
	char szOwnerName[CHARACTER_NAME_MAX_LEN];

	item_event_info() 
	: item( nullptr )
	{
		::memset( szOwnerName, 0, CHARACTER_NAME_MAX_LEN );
	}
};

EVENTINFO(item_vid_event_info)
{
	uint32_t item_vid;

	item_vid_event_info() 
	: item_vid( 0 )
	{
	}
};
}
