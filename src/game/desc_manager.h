#ifndef __INC_METIN_II_GAME_DESC_MANAGER_H__
#define __INC_METIN_II_GAME_DESC_MANAGER_H__

#include "typedef.h"

#include <common/length.h>
#include <common/singleton.h>
#include <libnet/io.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

class CLoginKey;

class DESC_MANAGER : public singleton<DESC_MANAGER>
{
	public:
		typedef std::unordered_set<DESC *>			DESC_SET;
		typedef std::unordered_set<CLIENT_DESC *>	CLIENT_DESC_SET;
		typedef std::map<int, DESC *>					DESC_HANDLE_MAP;
		typedef std::map<uint32_t, DESC *>					DESC_HANDSHAKE_MAP;
		typedef std::unordered_map<std::string, DESC *>	DESC_LOGINNAME_MAP;
		typedef std::map<uint32_t, uint32_t>					DESC_HANDLE_RANDOM_KEY_MAP;

	public:
		DESC_MANAGER();
		~DESC_MANAGER() override;

		void			Initialize();
		void			Destroy();

		DESC *			AcceptDesc(net::tcp::socket socket);
		DESC *			AcceptP2PDesc(net::tcp::socket socket);
		void			DestroyDesc(DESC * d, bool erase_from_set = true);

		uint32_t			CreateHandshake();

		CLIENT_DESC *		CreateConnectionDesc(const char * host, uint16_t port, int iPhaseWhenSucceed, bool bRetryWhenClosed);
		void			TryConnect();

		DESC *			FindByHandle(uint32_t handle);

		DESC *			FindByCharacterName(const char* name);
		DESC *			FindByLoginName(const std::string& login);
		void			ConnectAccount(const std::string& login, DESC * d);
		void			DisconnectAccount(const std::string& login);

		void			DestroyClosed();

		void			UpdateLocalUserCount();
		uint32_t			GetLocalUserCount() { return m_iLocalUserCount; }
		void			GetUserCount(int & iTotal, int ** paiEmpireUserCount, int & iLocalCount);

		const DESC_SET &	GetClientSet();

		uint32_t			MakeRandomKey(uint32_t dwHandle);

		uint32_t			CreateLoginKey(DESC * d);
		DESC *			FindByLoginKey(uint32_t dwKey);
		void			ProcessExpiredLoginKey();


		bool			IsP2PDescExist(const char * szHost, uint16_t wPort);

		// for C/S hybrid crypt

	private:

		DESC_HANDLE_RANDOM_KEY_MAP	m_map_handle_random_key;

		CLIENT_DESC_SET		m_set_pkClientDesc;
		DESC_SET			m_set_pkDesc;

		DESC_HANDLE_MAP			m_map_handle;
		DESC_HANDSHAKE_MAP		m_map_handshake;
		DESC_LOGINNAME_MAP		m_map_loginName;
		std::map<uint32_t, CLoginKey *>	m_map_pkLoginKey;

		int				m_iSocketsConnected;

		int				m_iHandleCount;

		int				m_iLocalUserCount;
		int				m_aiEmpireUserCount[EMPIRE_MAX_NUM];

		bool			m_bDestroyed;

};

#endif
