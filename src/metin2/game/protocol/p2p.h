
#pragma once

#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace metin2::game::inline protocol {

struct CCI {
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	uint32_t	dwPID;
	uint8_t	bEmpire;
	int32_t	lMapIndex;
	uint8_t	bChannel;

	DESC *	pkDesc;
};

class P2P_MANAGER : public singleton<P2P_MANAGER>
{
	public:
		P2P_MANAGER();
		~P2P_MANAGER() override;

		// The Register* / Unregister* pairs below actually use the same routine internally.
		// They're just for explicitness
		void			RegisterAcceptor(DESC * d);
		void			UnregisterAcceptor(DESC * d);

		void			RegisterConnector(DESC * d);
		void			UnregisterConnector(DESC * d);

		void			EraseUserByDesc(DESC * d);	// Remove the users on this desc.

		void			FlushOutput();

		void			Boot(DESC * d);	// Send the info needed for p2p handling. (login info of all characters, etc.)

		void			Send(const void * c_pvData, int iSize, DESC * except = nullptr);

		void			Login(DESC * d, const TPacketGGLogin * p);
		void			Logout(const char * c_pszName);

		CCI *			Find(const char * c_pszName);
		CCI *			FindByPID(uint32_t pid);

		int				GetCount();
		int				GetEmpireUserCount(int idx);
		int				GetDescCount();
		void			GetP2PHostNames(std::string& hostNames);

	private:
		void			Logout(CCI * pkCCI);

		int			m_iHandleCount;

		typedef std::unordered_map<std::string, std::unique_ptr<CCI>> TCCIMap;
		typedef std::unordered_map<uint32_t, CCI*> TPIDCCIMap;
		std::unordered_set<DESC *> m_set_pkPeers;
		TCCIMap			m_map_pkCCI;
		TPIDCCIMap		m_map_dwPID_pkCCI;
		int			m_aiEmpireUserCount[EMPIRE_MAX_NUM];
};
}
