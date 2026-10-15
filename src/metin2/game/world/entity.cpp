#include <metin2/game/world/entity.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace metin2::game::inline world {

CEntity::CEntity()
{
	Initialize();
}

CEntity::~CEntity()
{
	if (!m_bIsDestroyed)
		assert(!"You must call CEntity::destroy() method in your derived class destructor");
}

void CEntity::Initialize(int type)
{
	m_bIsDestroyed = false;

	m_iType = type;
	m_iViewAge = 0;
	m_pos.x = m_pos.y = m_pos.z = 0;
	m_map_view.clear();

	m_pSectree = nullptr;
	m_lpDesc = nullptr;
	m_lMapIndex = 0;
	m_bIsObserver = false;
	m_bObserverModeChange = false;
}

void CEntity::Destroy()
{
	if (m_bIsDestroyed) {
		return;
	}
	ViewCleanup();
	m_bIsDestroyed = true;
}

int CEntity::GetType() const
{
	return m_iType;
}

bool CEntity::IsType(int type) const
{
	return (m_iType == type ? true : false);
}

struct FuncPacketAround
{
	const void *        m_data;
	int                 m_bytes;
	CEntity *            m_except;

	FuncPacketAround(const void * data, int bytes, CEntity * except = nullptr) :m_data(data), m_bytes(bytes), m_except(except)
	{
	}

	void operator () (CEntity * ent)
	{
		if (ent == m_except)
			return;

		if (ent->GetDesc())
			ent->GetDesc()->Packet(m_data, m_bytes);
	}
};

struct FuncPacketView : public FuncPacketAround
{
	FuncPacketView(const void * data, int bytes, CEntity * except = nullptr) : FuncPacketAround(data, bytes, except)
	{}

	void operator() (const CEntity::ENTITY_MAP::value_type& v)
	{
		FuncPacketAround::operator() (v.first);
	}
};

void CEntity::PacketAround(const void * data, int bytes, CEntity * except)
{
	PacketView(data, bytes, except);
}

void CEntity::PacketView(const void * data, int bytes, CEntity * except)
{
	if (!GetSectree())
		return;

	FuncPacketView f(data, bytes, except);

	// In observer mode, only I receive my packets.
	if (!m_bIsObserver)
		for_each(m_map_view.begin(), m_map_view.end(), f);

	f(std::make_pair(this, 0));
}

void CEntity::SetObserverMode(bool bFlag)
{
	if (m_bIsObserver == bFlag)
		return;

	m_bIsObserver = bFlag;
	m_bObserverModeChange = true;
	UpdateSectree();

	if (IsType(ENTITY_CHARACTER))
	{
		CHARACTER * ch = (CHARACTER *) this;
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ObserverMode %d", m_bIsObserver ? 1 : 0);
	}
}
}
