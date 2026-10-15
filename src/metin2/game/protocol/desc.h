#pragma once

#include <metin2/game/protocol/cipher.h>
#include <metin2/game/app/event.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>
#include <metin2/net/connection.h>
#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace metin2::game::inline protocol {

inline constexpr int MAX_ALLOW_USER = 4096;
// Limits of a game client's connection: unprocessed input and unsent output.
inline constexpr std::size_t MAX_CLIENT_INPUT = 256 * 1024;
inline constexpr std::size_t MAX_CLIENT_OUTPUT = 128 * 1024;
// Limits of the connections between the servers.
inline constexpr std::size_t MAX_SERVER_INPUT = 256 * 1024 * 1024;
inline constexpr std::size_t MAX_SERVER_OUTPUT = 256 * 1024 * 1024;
inline constexpr int HANDSHAKE_RETRY_LIMIT = 32;
class CInputProcessor;

enum EDescType
{
	DESC_TYPE_ACCEPTOR,
	DESC_TYPE_CONNECTOR
};

class CLoginKey
{
	public:
		CLoginKey(uint32_t dwKey, DESC * pkDesc) : m_dwKey(dwKey), m_pkDesc(pkDesc)
		{
			m_dwExpireTime = 0;
		}

		void Expire()
		{
			m_dwExpireTime = get_dword_time();
			m_pkDesc = nullptr;
		}

		uint32_t   m_dwKey;
		uint32_t   m_dwExpireTime;
		DESC *  m_pkDesc;
};

// Data for tracking down the sequence bug
struct seq_t
{
	uint8_t	hdr;
	uint8_t	seq;
};
typedef std::vector<seq_t>	seq_vector_t;
// Data for tracking down the sequence bug

class DESC : public net::connection_handler
{
	public:
		EVENTINFO(desc_event_info)
		{
			DESC * desc;

			desc_event_info() 
			: desc(nullptr)
			{
			}
		};

	public:
		DESC();
		virtual ~DESC();

		virtual uint8_t		GetType() { return DESC_TYPE_ACCEPTOR; }
		virtual void		Destroy();
		virtual void		SetPhase(int _phase);

		void			FlushOutput();
		void			SendOutputNow();	// writes the queued output without waiting for the event loop

		bool			Setup(std::shared_ptr<net::connection> connection, uint32_t _handle, uint32_t _handshake);

		bool			IsConnected() const	{ return m_connection != nullptr; }
		const char *	GetHostName()		{ return m_stHost.c_str(); }

		void			SetP2P(const char * h, uint16_t w) { m_stP2PHost = h; m_wP2PPort = w; }
		const char *	GetP2PHost()		{ return m_stP2PHost.c_str();	}
		uint16_t			GetP2PPort() const		{ return m_wP2PPort; }

		void			BufferedPacket(const void * c_pvData, int iSize);
		void			Packet(const void * c_pvData, int iSize);
		void			LargePacket(const void * c_pvData, int iSize);

		auto			on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t override;
		void			on_close(const std::error_code & error) override;

		CInputProcessor	*	GetInputProcessor()	{ return m_pInputProcessor; }

		uint32_t			GetHandle() const	{ return m_dwHandle; }

		void			BindAccountTable(TAccountTable * pTable);
		TAccountTable &		GetAccountTable()	{ return m_accountTable; }

		void			BindCharacter(CHARACTER * ch);
		CHARACTER *		GetCharacter()		{ return m_lpCharacter; }

		bool			IsPhase(int phase) const	{ return m_iPhase == phase ? true : false; }


		// Handshake (time synchronization)
		void			StartHandshake(uint32_t _dw);
		void			SendHandshake(uint32_t dwCurTime, int32_t lNewDelta);
		bool			HandshakeProcess(uint32_t dwTime, int32_t lDelta, bool bInfiniteRetry=false);
		bool			IsHandshaking();

		uint32_t			GetHandshake() const	{ return m_dwHandshake; }
		uint32_t			GetClientTime();

		void SendKeyAgreement();
		void SendKeyAgreementCompleted();
		bool FinishHandshake(size_t agreed_length, const void* buffer, size_t length);
		bool IsCipherPrepared();

		// Empire
		uint8_t			GetEmpire();

		// for p2p
		void			SetRelay(const char * c_pszName);
		bool			DelayedDisconnect(int iSec);
		void			DisconnectOfSameLogin();

		void			SetAdminMode();
		bool			IsAdminMode();		// Can admin commands be used during the Handshake?

		// A guild mark connection (HEADER_CG_MARK_LOGIN): unencrypted, mark packets only. It belongs
		// to the game connection with handle dwOwnerHandle, whose login issued the random key.
		void			SetMarkLogin(uint32_t dwOwnerHandle)	{ m_bMarkLogin = true; m_dwMarkOwnerHandle = dwOwnerHandle; }
		bool			IsMarkLogin() const	{ return m_bMarkLogin; }
		uint32_t		GetMarkOwnerHandle() const	{ return m_dwMarkOwnerHandle; }

		void			SetPong(bool b);
		bool			IsPong();

		uint8_t			GetSequence();
		void			SetNextSequence();

		void			SendLoginSuccessPacket();

		void			SetLoginKey(uint32_t dwKey);
		void			SetLoginKey(CLoginKey * pkKey);
		uint32_t			GetLoginKey();

		void			AssembleCRCMagicCube(uint8_t bProcPiece, uint8_t bFilePiece);

		void			SetClientVersion(const char * c_pszTimestamp) { m_stClientVersion = c_pszTimestamp; }
		const char *		GetClientVersion() { return m_stClientVersion.c_str(); }

		bool			isChannelStatusRequested() const { return m_bChannelStatusRequested; }
		void			SetChannelStatusRequested(bool bChannelStatusRequested) { m_bChannelStatusRequested = bChannelStatusRequested; }

	protected:
		void			Initialize();

	protected:
		CInputProcessor *	m_pInputProcessor;
		CInputClose		m_inputClose;
		CInputHandshake	m_inputHandshake;
		CInputLogin		m_inputLogin;
		CInputMain		m_inputMain;
		CInputAuth		m_inputAuth;

		std::shared_ptr<net::connection>	m_connection;
		int				m_iPhase;
		uint32_t			m_dwHandle;

		std::string		m_stHost;
		uint16_t			m_wPort;
		time_t			m_LastTryToConnectTime;

		uint32_t			m_dwHandshake;
		uint32_t			m_dwHandshakeSentTime;
		int				m_iHandshakeRetry;
		uint32_t			m_dwClientTime;
		bool			m_bHandshaking;

		std::vector<uint8_t>	m_bufferedOutput;	// BufferedPacket data, sent ahead of the next Packet

		LPEVENT			m_pkPingEvent;
		CHARACTER *		m_lpCharacter;
		TAccountTable		m_accountTable;

		FILE *			m_pLogFile;
		std::string		m_stRelayName;

		std::string		m_stP2PHost;
		uint16_t			m_wP2PPort;

		bool			m_bAdminMode; // Can admin commands be used during the Handshake?
		bool			m_bMarkLogin;
		uint32_t		m_dwMarkOwnerHandle;
		bool			m_bPong;

		int			m_iCurrentSequence;

		CLoginKey *		m_pkLoginKey;
		uint32_t			m_dwLoginKey;

		uint8_t                    m_bCRCMagicCubeIdx;
		uint32_t                   m_dwProcCRC;
		uint32_t                   m_dwFileCRC;

		std::string		m_stClientVersion;

		std::string		m_Login;

		bool			m_bDestroyed;
		bool			m_bChannelStatusRequested;

		Cipher cipher_;

	public:
		LPEVENT			m_pkDisconnectEvent;

	protected:

	public:

		void SetLogin( const char * login ) { m_Login = login; }
		const std::string& GetLogin() { return m_Login; }

		void ChatPacket(uint8_t type, const char * format, ...);

		/* Code for tracking down the sequence bug */
	public:
		seq_vector_t	m_seq_vector;
		void			push_seq (uint8_t hdr, uint8_t seq);
		
};
}
