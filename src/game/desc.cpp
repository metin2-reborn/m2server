#include "desc.h"

#include "buffer_manager.h"
#include "char.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "event.h"
#include "guild.h"
#include "guild_manager.h"
#include "locale.hpp"
#include "packet.h"
#include "sequence.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <libnet/connection.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <asio/error.hpp>

#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <span>
#include <system_error>
#include <utility>

#include <arpa/inet.h>

namespace metin2::game {


DESC::DESC()
{
	Initialize();
}

DESC::~DESC()
{
}

void DESC::Initialize()
{
	m_bDestroyed = false;

	m_pInputProcessor = nullptr;
	m_connection.reset();
	m_iPhase = PHASE_CLOSE;
	m_dwHandle = 0;

	m_wPort = 0;
	m_LastTryToConnectTime = 0;

	m_dwHandshake = 0;
	m_dwHandshakeSentTime = 0;
	m_iHandshakeRetry = 0;
	m_dwClientTime = 0;
	m_bHandshaking = false;

	m_bufferedOutput.clear();

	m_pkPingEvent = nullptr;
	m_lpCharacter = nullptr;
	memset( &m_accountTable, 0, sizeof(m_accountTable) );

	m_pLogFile = nullptr;


	m_wP2PPort = 0;

	m_bAdminMode = false;
	m_bMarkLogin = false;
	m_dwMarkOwnerHandle = 0;
	m_bPong = true;
	m_bChannelStatusRequested = false;

	m_iCurrentSequence = 0;


	m_pkLoginKey = nullptr;
	m_dwLoginKey = 0;


	m_bCRCMagicCubeIdx = 0;
	m_dwProcCRC = 0;
	m_dwFileCRC = 0;



	m_pkDisconnectEvent = nullptr;

	m_seq_vector.clear();
}

void DESC::Destroy()
{
	if (m_bDestroyed) {
		return;
	}
	m_bDestroyed = true;

	if (m_pkLoginKey)
		m_pkLoginKey->Expire();

	if (GetAccountTable().id)
		DESC_MANAGER::instance().DisconnectAccount(GetAccountTable().login);

	if (m_pLogFile)
	{
		fclose(m_pLogFile);
		m_pLogFile = nullptr;
	}

	if (m_lpCharacter)
	{
		m_lpCharacter->Disconnect("DESC::~DESC");
		m_lpCharacter = nullptr;
	}

	event_cancel(&m_pkPingEvent);
	event_cancel(&m_pkDisconnectEvent);

	if (!metin2::config::is_auth())
	{
		if (m_accountTable.login[0] && m_accountTable.passwd[0])
		{
			TLogoutPacket pack;

			strlcpy(pack.login, m_accountTable.login, sizeof(pack.login));
			strlcpy(pack.passwd, m_accountTable.passwd, sizeof(pack.passwd));

			db_clientdesc->DBPacket(HEADER_GD_LOGOUT, m_dwHandle, &pack, sizeof(TLogoutPacket));
		}
	}

	if (m_connection)
	{
		metin2::log::info( "SYSTEM: closing connection of {} (handle {:d})", m_stHost.c_str(), m_dwHandle);

		cipher_.CleanUp();

		m_connection->close();
		m_connection.reset();
	}

	m_seq_vector.clear();
}

EVENTFUNC(ping_event)
{
	DESC::desc_event_info* info = dynamic_cast<DESC::desc_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "ping_event> <Factor> Null pointer" );
		return 0;
	}

	DESC * desc = info->desc;

	if (desc->IsAdminMode())
		return (PASSES_PER_SEC(metin2::config::server().ping_seconds));

	if (!desc->IsPong())
	{
		metin2::log::info( "PING_EVENT: no pong {}", desc->GetHostName());

		desc->SetPhase(PHASE_CLOSE);

		return (PASSES_PER_SEC(metin2::config::server().ping_seconds));
	}
	else
	{
		TPacketGCPing p;
		p.header = HEADER_GC_PING;
		desc->Packet(&p, sizeof(struct TPacketGCPing));
		desc->SetPong(false);
	}


	desc->SendHandshake(get_dword_time(), 0);

	return (PASSES_PER_SEC(metin2::config::server().ping_seconds));
}

bool DESC::IsPong()
{
	return m_bPong;
}

void DESC::SetPong(bool b)
{
	m_bPong = b;
}

bool DESC::Setup(std::shared_ptr<net::connection> connection, uint32_t _handle, uint32_t _handshake)
{
	m_connection	= std::move(connection);
	m_stHost		= m_connection->remote_address();
	m_wPort			= m_connection->remote_port();
	m_dwHandle		= _handle;

	m_connection->start(*this);

	// Ping Event 
	desc_event_info* info = AllocEventInfo<desc_event_info>();

	info->desc = this;
	assert(m_pkPingEvent == nullptr);

	m_pkPingEvent = event_create(ping_event, info, PASSES_PER_SEC(metin2::config::server().ping_seconds));


	// Set Phase to handshake
	SetPhase(PHASE_HANDSHAKE);
	StartHandshake(_handshake);

	metin2::log::info( "SYSTEM: new connection from [{}] handle {:d} handshake {:d} ptr {}",
			m_stHost.c_str(), m_dwHandle, m_dwHandshake, static_cast<const void *>(this));

	return true;
}

auto DESC::on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t
{
	// Only the new bytes: the rest was decrypted when it arrived.
	if (cipher_.activated())
		cipher_.Decrypt(input.data() + input.size() - received, received);

	std::size_t consumed = 0;

	// If false is returned, the phase has changed, so go through processing again!
	while (m_pInputProcessor)
	{
		int iBytesProceed = 0;
		const bool bDone = m_pInputProcessor->Process(this, input.data() + consumed, (int) (input.size() - consumed), iBytesProceed);
		consumed += iBytesProceed;

		if (bDone)
			return consumed;
	}

	// Nobody reads in this phase (closing, or a connector that is not set up yet).
	return input.size();
}

void DESC::on_close(const std::error_code & error)
{
	if (error && error != asio::error::eof)
		metin2::log::info( "SYSTEM: connection of {} lost: {}", m_stHost.c_str(), error.message().c_str());

	SetPhase(PHASE_CLOSE);
}

void DESC::BufferedPacket(const void * c_pvData, int iSize)
{
	if (m_iPhase == PHASE_CLOSE)
		return;

	const auto * data = static_cast<const uint8_t *>(c_pvData);
	m_bufferedOutput.insert(m_bufferedOutput.end(), data, data + iSize);
}

void DESC::Packet(const void * c_pvData, int iSize)
{
	assert(iSize > 0);

	if (m_iPhase == PHASE_CLOSE || !m_connection) // Don't send while disconnecting.
		return;

	if (m_stRelayName.length() != 0)
	{
		// Relay packets are not encrypted.
		TPacketGGRelay p;

		p.bHeader = HEADER_GG_RELAY;
		strlcpy(p.szName, m_stRelayName.c_str(), sizeof(p.szName));
		p.lSize = iSize;

		m_stRelayName.clear();

		if (!m_connection->send(&p, sizeof(p)) || !m_connection->send(c_pvData, iSize))
			m_iPhase = PHASE_CLOSE;

		return;
	}

	if (!m_bufferedOutput.empty())
	{
		const auto * data = static_cast<const uint8_t *>(c_pvData);
		m_bufferedOutput.insert(m_bufferedOutput.end(), data, data + iSize);

		c_pvData = m_bufferedOutput.data();
		iSize = (int) m_bufferedOutput.size();
	}

	// The output limit closes connections that stopped reading.
	const std::span<uint8_t> out = m_connection->prepare(iSize);

	if (out.empty())
	{
		metin2::log::info( "SYSTEM: output of {} exceeds its limit, closing", m_stHost.c_str());
		m_iPhase = PHASE_CLOSE;
	}
	else
	{
		std::memcpy(out.data(), c_pvData, iSize);

		if (cipher_.activated())
			cipher_.Encrypt(out.data(), out.size());
	}

	m_bufferedOutput.clear();
}

void DESC::LargePacket(const void * c_pvData, int iSize)
{
	if (m_connection)
		m_connection->raise_output_limit(iSize);

	metin2::log::info( "LargePacket Size {:d}", iSize);

	Packet(c_pvData, iSize);
}

void DESC::SetPhase(int _phase)
{
	m_iPhase = _phase;

	TPacketGCPhase pack;
	pack.header = HEADER_GC_PHASE;
	pack.phase = _phase;
	Packet(&pack, sizeof(TPacketGCPhase));

	switch (m_iPhase)
	{
		case PHASE_CLOSE:
			// Removed since the messenger became per-character
			m_pInputProcessor = &m_inputClose;
			break;

		case PHASE_HANDSHAKE:
			m_pInputProcessor = &m_inputHandshake;
			break;

		case PHASE_SELECT:
			// Removed since the messenger became per-character
			//MessengerManager::instance().Logout(GetAccountTable().login); // intentionally no break
		case PHASE_LOGIN:
		case PHASE_LOADING:
			m_pInputProcessor = &m_inputLogin;
			break;

		case PHASE_GAME:
		case PHASE_DEAD:
			m_pInputProcessor = &m_inputMain;
			break;

		case PHASE_AUTH:
			m_pInputProcessor = &m_inputAuth;
			metin2::log::info( "AUTH_PHASE {}", static_cast<const void *>(this));
			break;
	}
}

void DESC::BindAccountTable(TAccountTable * pAccountTable)
{
	assert(pAccountTable != nullptr);

	// A second login on this connection (two requests in flight): the login map must not keep
	// pointing here under the old name once this connection is gone.
	if (m_accountTable.id && strcmp(m_accountTable.login, pAccountTable->login) != 0)
		DESC_MANAGER::instance().DisconnectAccount(m_accountTable.login);

	std::memcpy(&m_accountTable, pAccountTable, sizeof(TAccountTable));
	DESC_MANAGER::instance().ConnectAccount(m_accountTable.login, this);
}

void DESC::StartHandshake(uint32_t _handshake)
{
	// Handshake
	m_dwHandshake = _handshake;

	SendHandshake(get_dword_time(), 0);

	m_iHandshakeRetry = 0;
}

void DESC::SendHandshake(uint32_t dwCurTime, int32_t lNewDelta)
{
	TPacketGCHandshake pack;

	pack.bHeader		= HEADER_GC_HANDSHAKE;
	pack.dwHandshake	= m_dwHandshake;
	pack.dwTime			= dwCurTime;
	pack.lDelta			= lNewDelta;

	Packet(&pack, sizeof(TPacketGCHandshake));

	m_dwHandshakeSentTime = dwCurTime;
	m_bHandshaking = true;
}

bool DESC::HandshakeProcess(uint32_t dwTime, int32_t lDelta, bool bInfiniteRetry)
{
	uint32_t dwCurTime = get_dword_time();

	if (lDelta < 0)
	{
		metin2::log::error("Desc::HandshakeProcess : value error (lDelta {:d}, ip {})", lDelta, m_stHost.c_str());
		return false;
	}

	int bias = (int) (dwCurTime - (dwTime + lDelta));

	if (bias >= 0 && bias <= 50)
	{
		if (bInfiniteRetry)
		{
			uint8_t bHeader = HEADER_GC_TIME_SYNC;
			Packet(&bHeader, sizeof(uint8_t));
		}

		if (GetCharacter())
			metin2::log::info( "Handshake: client_time {:d} server_time {:d} name: {}", m_dwClientTime, dwCurTime, GetCharacter()->GetName());
		else
			metin2::log::info( "Handshake: client_time {:d} server_time {:d} delta {:d}", m_dwClientTime, dwCurTime, static_cast<long>(lDelta));

		m_dwClientTime = dwCurTime;
		m_bHandshaking = false;
		return true; 
	}

	int32_t lNewDelta = (int32_t) (dwCurTime - dwTime) / 2;

	if (lNewDelta < 0)
	{
		metin2::log::info( "Handshake: lower than zero {:d}", lNewDelta);
		lNewDelta = (dwCurTime - m_dwHandshakeSentTime) / 2;
	}

	metin2::log::debug( "Handshake: ServerTime {:d} dwTime {:d} lDelta {:d} SentTime {:d} lNewDelta {:d}", dwCurTime, dwTime, lDelta, m_dwHandshakeSentTime, lNewDelta);

	if (!bInfiniteRetry)
		if (++m_iHandshakeRetry > HANDSHAKE_RETRY_LIMIT)
		{
			metin2::log::error("handshake retry limit reached! (limit {:d} character {})", 
					HANDSHAKE_RETRY_LIMIT, GetCharacter() ? GetCharacter()->GetName() : "!NO CHARACTER!");
			SetPhase(PHASE_CLOSE);
			return false;
		}

	SendHandshake(dwCurTime, lNewDelta);
	return false;
}

bool DESC::IsHandshaking()
{
	return m_bHandshaking;
}

uint32_t DESC::GetClientTime()
{
	return m_dwClientTime;
}

void DESC::SendKeyAgreement()
{
	TPacketKeyAgreement packet;

	size_t data_length = TPacketKeyAgreement::MAX_DATA_LEN;
	size_t agreed_length = cipher_.Prepare(packet.data, &data_length);
	if (agreed_length == 0) {
		// Initialization failure
		SetPhase(PHASE_CLOSE);
		return;
	}
	assert(data_length <= TPacketKeyAgreement::MAX_DATA_LEN);

	packet.bHeader = HEADER_GC_KEY_AGREEMENT;
	packet.wAgreedLength = (uint16_t)agreed_length;
	packet.wDataLength = (uint16_t)data_length;

	Packet(&packet, sizeof(packet));
}

void DESC::SendKeyAgreementCompleted()
{
	TPacketKeyAgreementCompleted packet;

	packet.bHeader = HEADER_GC_KEY_AGREEMENT_COMPLETED;

	Packet(&packet, sizeof(packet));
}

bool DESC::FinishHandshake(size_t agreed_length, const void* buffer, size_t length)
{
	if (!cipher_.Activate(false, agreed_length, buffer, length))
		return false;

	// The server encrypts from here on (the key agreement completed packet already went out in
	// the clear); the client switches on when that packet arrives.
	cipher_.set_activated(true);
	return true;
}

bool DESC::IsCipherPrepared()
{
	return cipher_.IsKeyPrepared();
}

void DESC::SetRelay(const char * c_pszName)
{
	m_stRelayName = c_pszName;
}

void DESC::BindCharacter(CHARACTER * ch)
{
	m_lpCharacter = ch;
}

void DESC::SendOutputNow()
{
	if (m_connection)
		m_connection->write_now();
}

void DESC::FlushOutput()
{
	if (!m_connection || m_connection->pending_output() == 0)
		return;

	metin2::log::info( "FLUSH START {:d}", m_connection->pending_output());

	if (m_connection->flush(std::chrono::seconds(20)))
		metin2::log::info( "FLUSH SUCCESS");
	else
		metin2::log::info( "FLUSH FAIL");
}

EVENTFUNC(disconnect_event)
{
	DESC::desc_event_info* info = dynamic_cast<DESC::desc_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "disconnect_event> <Factor> Null pointer" );
		return 0;
	}

	DESC * d = info->desc;

	d->m_pkDisconnectEvent = nullptr;
	d->SetPhase(PHASE_CLOSE);
	return 0;
}

bool DESC::DelayedDisconnect(int iSec)
{
	if (m_pkDisconnectEvent != nullptr) {
		return false;
	}

	desc_event_info* info = AllocEventInfo<desc_event_info>();
	info->desc = this;

	m_pkDisconnectEvent = event_create(disconnect_event, info, PASSES_PER_SEC(iSec));
	return true;
}

void DESC::DisconnectOfSameLogin()
{
	if (GetCharacter())
	{
		if (m_pkDisconnectEvent)
			return;

		GetCharacter()->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다른 컴퓨터에서 로그인 하여 접속을 종료 합니다."));
		DelayedDisconnect(5);
	}
	else
	{
		SetPhase(PHASE_CLOSE);
	}
}

void DESC::SetAdminMode()
{
	m_bAdminMode = true;
}

bool DESC::IsAdminMode()
{
	return m_bAdminMode;
}

uint8_t DESC::GetSequence()
{
	return gc_abSequence[m_iCurrentSequence];
}

void DESC::SetNextSequence()
{
	if (++m_iCurrentSequence == SEQUENCE_MAX_NUM)
		m_iCurrentSequence = 0;
}

void DESC::SendLoginSuccessPacket()
{
	TAccountTable & rTable = GetAccountTable();

	TPacketGCLoginSuccess p;

	p.bHeader    = HEADER_GC_LOGIN_SUCCESS_NEWSLOT;

	p.handle     = GetHandle();
	p.random_key = DESC_MANAGER::instance().MakeRandomKey(GetHandle()); // FOR MARK
	std::memcpy(p.players, rTable.players, sizeof(rTable.players));

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
	{   

		if (!g_stProxyIP.empty())
			rTable.players[i].lAddr=inet_addr(g_stProxyIP.c_str());

		CGuild* g = CGuildManager::instance().GetLinkedGuild(rTable.players[i].dwID);

		if (g)
		{   
			p.guild_id[i] = g->GetID();
			strlcpy(p.guild_name[i], g->GetName(), sizeof(p.guild_name[i]));
		}   
		else
		{
			p.guild_id[i] = 0;
			p.guild_name[i][0] = '\0';
		}
	}

	Packet(&p, sizeof(TPacketGCLoginSuccess));
}

void DESC::SetLoginKey(uint32_t dwKey)
{
	m_dwLoginKey = dwKey;
}

void DESC::SetLoginKey(CLoginKey * pkKey)
{
	// The replaced key must not keep pointing at this connection after it is destroyed.
	if (m_pkLoginKey && m_pkLoginKey != pkKey)
		m_pkLoginKey->Expire();

	m_pkLoginKey = pkKey;
	metin2::log::info( "SetLoginKey {:d}", m_pkLoginKey->m_dwKey);
}

uint32_t DESC::GetLoginKey()
{
	if (m_pkLoginKey)
		return m_pkLoginKey->m_dwKey;

	return m_dwLoginKey;
}


void DESC::AssembleCRCMagicCube(uint8_t bProcPiece, uint8_t bFilePiece)
{
	static uint8_t abXORTable[32] =
	{
		102,  30, 0, 0, 0, 0, 0, 0,
		188,  44, 0, 0, 0, 0, 0, 0,
		39, 201, 0, 0, 0, 0, 0, 0,
		43,   5, 0, 0, 0, 0, 0, 0,
	};

	bProcPiece = (bProcPiece ^ abXORTable[m_bCRCMagicCubeIdx]);
	bFilePiece = (bFilePiece ^ abXORTable[m_bCRCMagicCubeIdx+1]);

	m_dwProcCRC |= static_cast<uint32_t>(bProcPiece) << m_bCRCMagicCubeIdx;
	m_dwFileCRC |= static_cast<uint32_t>(bFilePiece) << m_bCRCMagicCubeIdx;

	m_bCRCMagicCubeIdx += 8;

	if (!(m_bCRCMagicCubeIdx & 31))
	{
		m_dwProcCRC = 0;
		m_dwFileCRC = 0;
		m_bCRCMagicCubeIdx = 0;
	}
}

void DESC::push_seq(uint8_t hdr, uint8_t seq)
{
	if (m_seq_vector.size()>=20)
	{
		m_seq_vector.erase(m_seq_vector.begin());
	}

	seq_t info = { hdr, seq };
	m_seq_vector.push_back(info);
}

uint8_t DESC::GetEmpire()
{
	return m_accountTable.bEmpire;
}

void DESC::ChatPacket(uint8_t type, const char * format, ...)
{
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	if (len < 0)
		return;

	// vsnprintf returns the untruncated length: send what fits, cut on a character boundary
	if (len >= (int) sizeof(chatbuf))
	{
		len = (int) utf8_truncated_size(chatbuf, sizeof(chatbuf) - 1, sizeof(chatbuf) - 1);
		chatbuf[len] = '\0';
	}

	struct TPacketGCChat pack_chat;

	pack_chat.header    = HEADER_GC_CHAT;
	pack_chat.size      = sizeof(struct TPacketGCChat) + len;
	pack_chat.type      = type;
	pack_chat.id        = 0;
	pack_chat.bEmpire   = GetEmpire();

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(struct TPacketGCChat));
	buf.write(chatbuf, len);

	Packet(buf.read_peek(), buf.size());
}
}
