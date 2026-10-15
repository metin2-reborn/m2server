#pragma once

#include "event.h"

#include <common/singleton.h>

#include <cstdint>
#include <cstring>
#include <list>
#include <map>

namespace metin2::game {

enum ETargetTypes
{
	TARGET_TYPE_POS	= (1 << 0),
	TARGET_TYPE_VID	= (1 << 1),
};

EVENTINFO(TargetInfo)
{
	// <Factor> Removed unsafe copy of CHARACTER* here

	int         iID;

	uint32_t       dwPID;
	uint32_t       dwQuestIndex;

	char        szTargetName[32+1]; // Name used in quests
	char        szTargetDesc[32+1]; // Name actually sent to the client

	int         iType;
	int         iArg1;
	int         iArg2;

	int		iMapIndex;
	int         iOldX;
	int         iOldY;

	bool	bSendToClient;

	TargetInfo() 
	: iID( 0 )
	, dwPID( 0 )
	, dwQuestIndex( 0 )
	, iType( 0 )
	, iArg1( 0 )
	, iArg2( 0 )
	, iMapIndex( 0 )
	, iOldX( 0 )
	, iOldY( 0 )
	, bSendToClient( false )
	{
		::memset(szTargetName, 0, 32+1);
		::memset(szTargetDesc, 0, 32+1);
	}
};

class CTargetManager : public singleton<CTargetManager>
{
	public:
		CTargetManager();
		~CTargetManager() override;

		void CreateTarget(uint32_t dwPID, uint32_t dwQuestIndex, const char * c_pszTargetName, int iType, int iArg1, int iArg2, int iMapIndex, const char * c_pszTargetDesc = nullptr, int iSendFlag = 1);
		void DeleteTarget(uint32_t dwPID, uint32_t dwQuestIndex, const char * c_pszTargetName);

		void Logout(uint32_t dwPID);
		TargetInfo * GetTargetInfo(uint32_t dwPID, int iType, int iArg1);

		LPEVENT GetTargetEvent(uint32_t dwPID, uint32_t dwQuestIndex, const char * c_pszTargetName);

	protected:
		// first: PID
		std::map<uint32_t, std::list<LPEVENT> > m_map_kListEvent;
		int m_iID;
};
}
