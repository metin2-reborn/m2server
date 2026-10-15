#pragma once

#include "char_manager.h"
#include "event.h"
#include "lua_incl.h"
#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <list>
#include <map>
#include <memory>

namespace metin2::game {

enum MEMBER_IDENTITY {
    MEMBER_NO,
    MEMBER_DUELIST,
    MEMBER_OBSERVER,

    MEMBER_MAX
};

class CArena {
    friend class CArenaMap;

  private:
    uint32_t m_dwPIDA;
    uint32_t m_dwPIDB;

    LPEVENT m_pEvent;
    LPEVENT m_pTimeOutEvent;

    PIXEL_POSITION m_StartPointA;
    PIXEL_POSITION m_StartPointB;
    PIXEL_POSITION m_ObserverPoint;

    uint32_t m_dwSetCount;
    uint32_t m_dwSetPointOfA;
    uint32_t m_dwSetPointOfB;

    std::map<uint32_t, CHARACTER *> m_mapObserver;

  protected:
    CArena(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);

    bool StartDuel(CHARACTER * pCharFrom, CHARACTER * pCharTo, int nSetPoint, int nMinute = 5);

    bool IsEmpty() const { return ((m_dwPIDA == 0) && (m_dwPIDB == 0)); }
    bool IsMember(uint32_t dwPID) const { return ((m_dwPIDA == dwPID) || (m_dwPIDB == dwPID)); }

    bool CheckArea(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);
    void Clear();

    bool CanAttack(uint32_t dwPIDA, uint32_t dwPIDB);
    bool OnDead(uint32_t dwPIDA, uint32_t dwPIDB);

    bool IsObserver(uint32_t pid);
    bool IsMyObserver(uint16_t ObserverX, uint16_t ObserverY);
    bool AddObserver(CHARACTER * pChar);
    bool RegisterObserverPtr(CHARACTER * pChar);

  public:
    uint32_t GetPlayerAPID() { return m_dwPIDA; }
    uint32_t GetPlayerBPID() { return m_dwPIDB; }

    CHARACTER * GetPlayerA() { return CHARACTER_MANAGER::instance().FindByPID(m_dwPIDA); }
    CHARACTER * GetPlayerB() { return CHARACTER_MANAGER::instance().FindByPID(m_dwPIDB); }

    PIXEL_POSITION GetStartPointA() { return m_StartPointA; }
    PIXEL_POSITION GetStartPointB() { return m_StartPointB; }

    PIXEL_POSITION GetObserverPoint() { return m_ObserverPoint; }

    void EndDuel();
    void ClearEvent() { m_pEvent = nullptr; }
    void OnDisconnect(uint32_t pid);
    void RemoveObserver(uint32_t pid);

    void SendChatPacketToObserver(uint8_t type, const char *format, ...);
};

class CArenaMap {
    friend class CArenaManager;

  private:
    uint32_t m_dwMapIndex;
    std::list<std::unique_ptr<CArena>> m_listArena;

  protected:
    void Destroy();

    bool AddArena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);
    void SendArenaMapListTo(CHARACTER * pChar, uint32_t dwMapIndex);

    bool StartDuel(CHARACTER * pCharFrom, CHARACTER * pCharTo, int nSetPoint, int nMinute = 5);
    void EndAllDuel();
    bool EndDuel(uint32_t pid);

    int GetDuelList(lua_State *L, int index);

    bool CanAttack(CHARACTER * pCharAttacker, CHARACTER * pCharVictim);
    bool OnDead(CHARACTER * pCharKiller, CHARACTER * pCharVictim);

    bool AddObserver(CHARACTER * pChar, uint16_t ObserverX, uint16_t ObserverY);
    bool RegisterObserverPtr(CHARACTER * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY);

    MEMBER_IDENTITY IsMember(uint32_t PID);
};

class CArenaManager : public singleton<CArenaManager> {
  private:
    std::map<uint32_t, std::unique_ptr<CArenaMap>> m_mapArenaMap;

  public:
    bool Initialize();
    void Destroy();

    bool StartDuel(CHARACTER * pCharFrom, CHARACTER * pCharTo, int nSetPoint, int nMinute = 5);

    bool AddArena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);

    void SendArenaMapListTo(CHARACTER * pChar);

    void EndAllDuel();
    bool EndDuel(uint32_t pid);

    void GetDuelList(lua_State *L);

    bool CanAttack(CHARACTER * pCharAttacker, CHARACTER * pCharVictim);

    bool OnDead(CHARACTER * pCharKiller, CHARACTER * pCharVictim);

    bool AddObserver(CHARACTER * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY);
    bool RegisterObserverPtr(CHARACTER * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY);

    bool IsArenaMap(uint32_t dwMapIndex);
    MEMBER_IDENTITY IsMember(uint32_t dwMapIndex, uint32_t PID);

    bool IsLimitedItem(int32_t lMapIndex, uint32_t dwVnum);
};
}
