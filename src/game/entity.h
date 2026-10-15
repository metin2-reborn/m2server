#pragma once

#include "typedef.h"

#include <cstdint>
#include <unordered_map>

class SECTREE;

class CEntity
{
	public:
		typedef std::unordered_map<CEntity *, int> ENTITY_MAP;

	public:
		CEntity();
		virtual	~CEntity();

		virtual void	EncodeInsertPacket(CEntity * entity) = 0;
		virtual	void	EncodeRemovePacket(CEntity * entity) = 0;

	protected:
		void			Initialize(int type = -1);
		void			Destroy();

	public:
		int				GetType() const;
		bool			IsType(int type) const;

		void			ViewCleanup();
		void			ViewInsert(CEntity * entity, bool recursive = true);
		void			ViewRemove(CEntity * entity, bool recursive = true);
		void			ViewReencode();	// Resend packets to surrounding entities.

		int32_t			GetX() const		{ return m_pos.x; }
		int32_t			GetY() const		{ return m_pos.y; }
		int32_t			GetZ() const		{ return m_pos.z; }
		const PIXEL_POSITION &	GetXYZ() const		{ return m_pos; }

		void			SetXYZ(int32_t x, int32_t y, int32_t z)		{ m_pos.x = x, m_pos.y = y, m_pos.z = z; }

		SECTREE *		GetSectree() const			{ return m_pSectree;	}
		void			SetSectree(SECTREE * tree)	{ m_pSectree = tree;	}

		void			UpdateSectree();
		void			PacketAround(const void * data, int bytes, CEntity * except = nullptr);
		void			PacketView(const void * data, int bytes, CEntity * except = nullptr);

		void			BindDesc(DESC * _d)     { m_lpDesc = _d; }
		DESC *			GetDesc() const			{ return m_lpDesc; }

		void			SetMapIndex(int32_t l)	{ m_lMapIndex = l; }
		int32_t			GetMapIndex() const	{ return m_lMapIndex; }

		void			SetObserverMode(bool bFlag);
		bool			IsObserverMode() const	{ return m_bIsObserver; }

	protected:
		bool			m_bIsObserver;
		bool			m_bObserverModeChange;
		ENTITY_MAP		m_map_view;
		int32_t			m_lMapIndex;

	private:
		DESC *			m_lpDesc;

		int			m_iType;
		bool			m_bIsDestroyed;

		PIXEL_POSITION		m_pos;

		int			m_iViewAge;

		SECTREE *		m_pSectree;
};
