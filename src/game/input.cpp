#include "input.h"

#include "char.h"
#include "cmd.h"
#include "config.h"
#include "db.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "fishing.h"
#include "log.h"
#include "p2p.h"
#include "packet.h"
#include "packet_info.h"
#include "priv_manager.h"
#include "questmanager.h"
#include "typedef.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <vector>


bool IsEmptyAdminPage()
{
	return metin2::config::server().admin.ips.empty();
}

bool IsAdminPage(const char * ip)
{
	for (size_t n = 0; n < metin2::config::server().admin.ips.size(); ++n)
	{
		if (metin2::config::server().admin.ips[n] == ip)
			return 1; 
	}	
	return 0;
}

CInputProcessor::CInputProcessor() : m_pPacketInfo(nullptr), m_iBufferLeft(0)
{
	if (!m_pPacketInfo)
		BindPacketInfo(&m_packetInfoCG);
}

void CInputProcessor::BindPacketInfo(CPacketInfo * pPacketInfo)
{
	m_pPacketInfo = pPacketInfo;
}

bool CInputProcessor::Process(DESC * lpDesc, const void * c_pvOrig, int iBytes, int & r_iBytesProceed)
{
	const char * c_pData = (const char *) c_pvOrig;

	uint8_t	bLastHeader = 0;
	int		iLastPacketLen = 0;
	int		iPacketLen;

	if (!m_pPacketInfo)
	{
		metin2::log::error("No packet info has been binded to");
		return true;
	}

	for (m_iBufferLeft = iBytes; m_iBufferLeft > 0;)
	{
		uint8_t bHeader = (uint8_t) *(c_pData);
		const char * c_pszName;

		if (bHeader == 0) // Skip header 0 since encryption handling exists.
			iPacketLen = 1;
		else if (!m_pPacketInfo->Get(bHeader, &iPacketLen, &c_pszName))
		{
			metin2::log::error("UNKNOWN HEADER: {:d}, LAST HEADER: {:d}({:d}), REMAIN BYTES: {:d}, host: {}",
					bHeader, bLastHeader, iLastPacketLen, m_iBufferLeft, lpDesc->GetHostName());
			lpDesc->SetPhase(PHASE_CLOSE);
			return true;
		}

		if (m_iBufferLeft < iPacketLen)
			return true;

		if (bHeader)
		{
			if (metin2::config::test_server() && bHeader != HEADER_CG_MOVE)
				metin2::log::info( "Packet Analyze [Header {:d}][bufferLeft {:d}] ", bHeader, m_iBufferLeft);

			m_pPacketInfo->Start();

			// Handlers check their variable-size data against m_iBufferLeft. The sequence byte
			// follows that data, so it is not counted: a packet whose sequence byte has not
			// arrived yet then waits for it instead of being read past the end of the buffer.
			const int iBufferLeft = m_iBufferLeft;

			if (m_pPacketInfo->IsSequence(bHeader))
				--m_iBufferLeft;

			int iExtraPacketSize = Analyze(lpDesc, bHeader, c_pData);
			m_iBufferLeft = iBufferLeft;

			if (iExtraPacketSize < 0)
				return true;

			// A handler that claims more than was received would make us read and consume past
			// the buffer.
			if (iExtraPacketSize > m_iBufferLeft - iPacketLen)
			{
				metin2::log::error("packet {} (header {:d}) claims {:d} extra bytes, only {:d} left, host: {}",
						c_pszName, bHeader, iExtraPacketSize, m_iBufferLeft - iPacketLen, lpDesc->GetHostName());
				lpDesc->SetPhase(PHASE_CLOSE);
				return true;
			}

			iPacketLen += iExtraPacketSize;
			m_pPacketInfo->End();
		}

		if (bHeader == HEADER_CG_PONG)
			metin2::log::info( "PONG! {:d} {:d}", m_pPacketInfo->IsSequence(bHeader), *(uint8_t *) (c_pData + iPacketLen - sizeof(uint8_t)));

		if (m_pPacketInfo->IsSequence(bHeader))
		{
			uint8_t bSeq = lpDesc->GetSequence();
			uint8_t bSeqReceived = *(uint8_t *) (c_pData + iPacketLen - sizeof(uint8_t));

			if (bSeq != bSeqReceived)
			{
				metin2::log::error("SEQUENCE {} mismatch 0x{:x} != 0x{:x} header {:d}", static_cast<const void *>(static_cast<void*>(lpDesc)), bSeq, bSeqReceived, bHeader);

				CHARACTER *	ch = lpDesc->GetCharacter();

				char buf[1024];
				int	offset, len;

				offset = snprintf(buf, sizeof(buf), "SEQUENCE_LOG [%s]-------------\n", ch ? ch->GetName() : "UNKNOWN");

				if (offset < 0 || offset >= (int) sizeof(buf))
					offset = sizeof(buf) - 1;

				for (size_t i = 0; i < lpDesc->m_seq_vector.size(); ++i)
				{
					len = snprintf(buf + offset, sizeof(buf) - offset, "\t[%03d : 0x%x]\n",
							lpDesc->m_seq_vector[i].hdr,
							lpDesc->m_seq_vector[i].seq);

					if (len < 0 || len >= (int) sizeof(buf) - offset)
						offset += (sizeof(buf) - offset) - 1;
					else
						offset += len;
				}

				snprintf(buf + offset, sizeof(buf) - offset, "\t[%03d : 0x%x]\n", bHeader, bSeq);
				metin2::log::error("{}", buf);

				lpDesc->SetPhase(PHASE_CLOSE);
				return true;
			}
			else
			{
				lpDesc->push_seq(bHeader, bSeq);
				lpDesc->SetNextSequence();
			}
		}

		c_pData	+= iPacketLen;
		m_iBufferLeft -= iPacketLen;
		r_iBytesProceed += iPacketLen;

		iLastPacketLen = iPacketLen;
		bLastHeader	= bHeader;

		if (GetType() != lpDesc->GetInputProcessor()->GetType())
			return false;
	}

	return true;
}

void CInputProcessor::Pong(DESC * d)
{
	d->SetPong(true);


}

void CInputProcessor::Handshake(DESC * d, const char * c_pData)
{
	TPacketCGHandshake * p = (TPacketCGHandshake *) c_pData;

	if (d->GetHandshake() != p->dwHandshake)
	{
		metin2::log::error("Invalid Handshake from {}", d->GetHostName());
		d->SetPhase(PHASE_CLOSE);
	}
	else
	{
		if (d->IsPhase(PHASE_HANDSHAKE))
		{
			if (d->HandshakeProcess(p->dwTime, p->lDelta, false))
			{
				d->SendKeyAgreement();
			}
		}
		else
			d->HandshakeProcess(p->dwTime, p->lDelta, true);
	}
}

void CInputProcessor::Version(CHARACTER * ch, const char* c_pData)
{
	if (!ch)
		return;

	if (!ch->GetDesc())
		return;

	TPacketCGClientVersion * p = (TPacketCGClientVersion *) c_pData;

	char szTimestamp[sizeof(p->timestamp)];
	char szFilename[sizeof(p->filename)];
	CopyPacketString(szTimestamp, p->timestamp);
	CopyPacketString(szFilename, p->filename);

	metin2::log::info( "VERSION: {} {} {}", ch->GetName(), szTimestamp, szFilename);
	ch->GetDesc()->SetClientVersion(szTimestamp);
}

void LoginFailure(DESC * d, const char * c_pszStatus)
{
	if (!d)
		return;

	TPacketGCLoginFailure failurePacket;

	failurePacket.header = HEADER_GC_LOGIN_FAILURE;
	strlcpy(failurePacket.szStatus, c_pszStatus, sizeof(failurePacket.szStatus));

	d->Packet(&failurePacket, sizeof(failurePacket));
}

CInputHandshake::CInputHandshake()
{
	m_pkHandshakePacketInfo = std::make_unique<CPacketInfoCG>();
	m_pkHandshakePacketInfo->SetSequence(HEADER_CG_PONG, false);

	m_pMainPacketInfo = m_pPacketInfo;
	BindPacketInfo(m_pkHandshakePacketInfo.get());
}

CInputHandshake::~CInputHandshake()
{
}



// BLOCK_CHAT
ACMD(do_block_chat);
// END_OF_BLOCK_CHAT

int CInputHandshake::Analyze(DESC * d, uint8_t bHeader, const char * c_pData)
{
	if (bHeader == 10) // Ignore enter
		return 0;

	if (bHeader == HEADER_CG_TEXT)
	{
		if (IsEmptyAdminPage() || !IsAdminPage(d->GetHostName())) // block if adminpage is not set or if not admin
		{
			metin2::log::info( "PORT_SECURITY: BLOCK FROM({})", d->GetHostName());
			d->SetPhase(PHASE_CLOSE);
			return -1;
		}

		++c_pData;

		// The line ends at \n within what was received; the data is not NUL-terminated.
		const char * c_pSep = (const char *) memchr(c_pData, '\n', m_iBufferLeft - 1);

		if (!c_pSep)
			return -1;

		if (c_pSep > c_pData && *(c_pSep - 1) == '\r')
			--c_pSep;

		std::string stResult;
		std::string stBuf;
		stBuf.assign(c_pData, c_pSep - c_pData);

		metin2::log::info( "SOCKET_CMD: FROM({}) CMD({})", d->GetHostName(), stBuf.c_str());

		if (!stBuf.compare("IS_SERVER_UP"))
		{
			if (g_bNoMoreClient)
				stResult = "NO";
			else
				stResult = "YES";
		}
		//else if (!stBuf.compare("SHOWMETHEMONEY"))
		else if (!metin2::config::server().admin.password.empty() && stBuf == metin2::config::server().admin.password)
		{
			if (!IsEmptyAdminPage())
			{
				if (!IsAdminPage(d->GetHostName()))
				{
					char szTmp[64];
					snprintf(szTmp, sizeof(szTmp), "WEBADMIN : Wrong Connector : %s", d->GetHostName());
					stResult += szTmp;
				}
				else
				{
					d->SetAdminMode();
					stResult = "UNKNOWN";
				}
			}
			else
			{
				d->SetAdminMode();
				stResult = "UNKNOWN";
			}
		}
		else if (!stBuf.compare("USER_COUNT"))
		{
			char szTmp[64];

			if (!IsEmptyAdminPage())
			{
				if (!IsAdminPage(d->GetHostName()))
				{
					snprintf(szTmp, sizeof(szTmp), "WEBADMIN : Wrong Connector : %s", d->GetHostName());
				}
				else
				{
					int iTotal;
					int * paiEmpireUserCount;
					int iLocal;
					DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);
					snprintf(szTmp, sizeof(szTmp), "%d %d %d %d %d", iTotal, paiEmpireUserCount[1], paiEmpireUserCount[2], paiEmpireUserCount[3], iLocal);
				}
			}
			else
			{
				int iTotal;
				int * paiEmpireUserCount;
				int iLocal;
				DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);
				snprintf(szTmp, sizeof(szTmp), "%d %d %d %d %d", iTotal, paiEmpireUserCount[1], paiEmpireUserCount[2], paiEmpireUserCount[3], iLocal);
			}
			stResult += szTmp;
		}
		// The commands below need the admin password (admin mode), not only an admin IP.
		else if (d->IsAdminMode() && !stBuf.compare("CHECK_P2P_CONNECTIONS"))
		{
			std::ostringstream oss(std::ostringstream::out);
			
			oss << "P2P CONNECTION NUMBER : " << P2P_MANAGER::instance().GetDescCount() << "\n";
			std::string hostNames;
			P2P_MANAGER::instance().GetP2PHostNames(hostNames);
			oss << hostNames;
			stResult = oss.str();
			TPacketGGCheckAwakeness packet;
			packet.bHeader = HEADER_GG_CHECK_AWAKENESS;

			P2P_MANAGER::instance().Send(&packet, sizeof(packet));
		}
		else if (d->IsAdminMode() && !stBuf.compare("PACKET_INFO"))
		{
			m_pMainPacketInfo->Log("packet_info.txt");
			stResult = "OK";
		}
		//gift notify delete command
		else if (d->IsAdminMode() && !stBuf.compare(0,15,"DELETE_AWARDID "))
			{
				char szTmp[64];
				std::string msg = stBuf.substr(15,26);	// item_award id range?
				
				TPacketDeleteAwardID p;
				p.dwID = (uint32_t)(atoi(msg.c_str()));
				snprintf(szTmp,sizeof(szTmp),"Sent to DB cache to delete ItemAward, id: %d",p.dwID);
				db_clientdesc->DBPacket(HEADER_GD_DELETE_AWARDID, 0, &p, sizeof(p));
				stResult += szTmp;
			}
		else
		{
			stResult = "UNKNOWN";
			
			if (d->IsAdminMode())
			{
				// Admin commands
				if (!stBuf.compare(0, 7, "NOTICE "))
				{
					std::string msg = stBuf.substr(7);
					msg.resize(utf8_truncated_size(msg.c_str(), msg.size(), 50));
					LogManager::instance().CharLog(0, 0, 0, 1, "NOTICE", msg.c_str(), d->GetHostName());
					BroadcastNotice(msg.c_str());
				}
				else if (!stBuf.compare("SHUTDOWN"))
				{
					LogManager::instance().CharLog(0, 0, 0, 2, "SHUTDOWN", "", d->GetHostName());
					TPacketGGShutdown p;
					p.bHeader = HEADER_GG_SHUTDOWN;
					P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShutdown));
					metin2::log::error("Accept shutdown command from {}.", d->GetHostName());
					Shutdown(10);
				}
				else if (!stBuf.compare("SHUTDOWN_ONLY"))
				{
					LogManager::instance().CharLog(0, 0, 0, 2, "SHUTDOWN", "", d->GetHostName());
					metin2::log::error("Accept shutdown only command from {}.", d->GetHostName());
					Shutdown(10);
				}
				else if (!stBuf.compare(0, 3, "DC "))
				{
					std::string msg = stBuf.substr(3, LOGIN_MAX_LEN);

metin2::log::debug( "DC : '{}'", msg.c_str());

					TPacketGGDisconnect pgg;

					pgg.bHeader = HEADER_GG_DISCONNECT;
					strlcpy(pgg.szLogin, msg.c_str(), sizeof(pgg.szLogin));

					P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGDisconnect));

					// delete login key
					{
						TPacketDC p;
						strlcpy(p.login, msg.c_str(), sizeof(p.login));
						db_clientdesc->DBPacket(HEADER_GD_DC, 0, &p, sizeof(p));
					}
				}
				else if (!stBuf.compare(0, 20, "CHECK_CLIENT_VERSION"))
				{
					CheckClientVersion();

					uint8_t bHeader = HEADER_GG_CHECK_CLIENT_VERSION;
					P2P_MANAGER::instance().Send(&bHeader, sizeof(uint8_t));
					stResult = "OK";
				}
				else if (!stBuf.compare(0, 6, "RELOAD"))
				{
					if (stBuf.size() == 6)
					{
						LoadStateUserCount();
						db_clientdesc->DBPacket(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
						DBManager::instance().LoadDBString();
					}
					else
					{
						char c = stBuf[7];

						switch (std::tolower(static_cast<unsigned char>(c)))
						{
							case 'u':
								LoadStateUserCount();
								break;

							case 'p':
								db_clientdesc->DBPacket(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
								break;

							case 's':
								DBManager::instance().LoadDBString();
								break;

							case 'q':
								quest::CQuestManager::instance().Reload();
								break;

							case 'f':
								fishing::Initialize();
								break;

							case 'a':
								db_clientdesc->DBPacket(HEADER_GD_RELOAD_ADMIN, 0, nullptr, 0);
								metin2::log::info( "Reloading admin infomation.");
								break;
						}
					}
				}
				else if (!stBuf.compare(0, 6, "EVENT "))
				{
					std::istringstream is(stBuf);
					std::string strEvent, strFlagName;
					int32_t lValue;
					is >> strEvent >> strFlagName >> lValue;

					if (!is.fail())
					{
						metin2::log::info( "EXTERNAL EVENT FLAG name {} value {:d}", strFlagName.c_str(), lValue);
						quest::CQuestManager::instance().RequestSetEventFlag(strFlagName, lValue);
						stResult = "EVENT FLAG CHANGE ";
						stResult += strFlagName;
					}
					else
					{
						stResult = "EVENT FLAG FAIL";
					}
				}
				// BLOCK_CHAT
				else if (!stBuf.compare(0, 11, "BLOCK_CHAT "))
				{
					std::istringstream is(stBuf);
					std::string strBlockChat, strCharName;
					int32_t lDuration;
					is >> strBlockChat >> strCharName >> lDuration;

					if (!is.fail())
					{
						metin2::log::info( "EXTERNAL BLOCK_CHAT name {} duration {:d}", strCharName.c_str(), lDuration);

						do_block_chat(nullptr, const_cast<char*>(stBuf.c_str() + 11), 0, 0);

						stResult = "BLOCK_CHAT ";
						stResult += strCharName;
					}
					else
					{
						stResult = "BLOCK_CHAT FAIL";
					}
				}
				// END_OF_BLOCK_CHAT
				else if (!stBuf.compare(0, 12, "PRIV_EMPIRE "))
				{
					int	empire, type, value, duration;
					std::istringstream is(stBuf);
					std::string strPrivEmpire;
					is >> strPrivEmpire >> empire >> type >> value >> duration;

					// Max 10x
					value = std::clamp(value, 0, 1000);
					stResult = "PRIV_EMPIRE FAIL";

					if (!is.fail())
					{
						// check parameter
						if (empire < 0 || 3 < empire);
						else if (type < 1 || 4 < type);
						else if (value < 0);
						else if (duration < 0);
						else
						{
							stResult = "PRIV_EMPIRE SUCCEED";

							// Convert to hours
							duration = duration * (60 * 60);

							metin2::log::info( "_give_empire_privileage(empire={:d}, type={:d}, value={:d}, duration={:d}) by web", 
									empire, type, value, duration);
							CPrivManager::instance().RequestGiveEmpirePriv(empire, type, value, duration);
						}
					}
				}
			}
		}

		metin2::log::debug( "TEXT {} RESULT {}", stBuf.c_str(), stResult.c_str());
		stResult += "\n";
		d->Packet(stResult.c_str(), stResult.length());
		return (c_pSep - c_pData) + 1;
	}
	else if (bHeader == HEADER_CG_MARK_LOGIN)
	{
		if (!metin2::config::game().mark.server)
		{
			// Disconnect! - requesting a mark when this isn't the mark server?
			metin2::log::error("Guild Mark login requested but i'm not a mark server!");
			d->SetPhase(PHASE_CLOSE);
			return 0;
		}

		// Marks are public, so anyone may download them. Uploads need the key that the login of
		// the player's game connection on this core issued (one core serves every map here); the
		// client's first download starts before it has one, so a missing key means download only.
		const TPacketCGMarkLogin * p = (const TPacketCGMarkLogin *) c_pData;
		const bool bAuthenticated = DESC_MANAGER::instance().CheckRandomKey(p->handle, p->random_key);

		metin2::log::info( "MARK_SERVER: Login{}", bAuthenticated ? "" : " (download only)");
		d->SetMarkLogin(bAuthenticated ? p->handle : 0);
		d->SetPhase(PHASE_LOGIN);
		return 0;
	}
	else if (bHeader == HEADER_CG_STATE_CHECKER)
	{
		if (d->isChannelStatusRequested()) {
			return 0;
		}
		d->SetChannelStatusRequested(true);
		db_clientdesc->DBPacket(HEADER_GD_REQUEST_CHANNELSTATUS, d->GetHandle(), nullptr, 0);
	}
	else if (bHeader == HEADER_CG_PONG)
		Pong(d);
	else if (bHeader == HEADER_CG_HANDSHAKE)
		Handshake(d, c_pData);
	else if (bHeader == HEADER_CG_KEY_AGREEMENT)
	{
		// Send out the key agreement completion packet first
		// to help client to enter encryption mode
		d->SendKeyAgreementCompleted();
		// Write it now, on its own: the client switches decryption on per received chunk, so
		// the encrypted packets queued next must not arrive in the same segment.
		d->SendOutputNow();

		TPacketKeyAgreement* p = (TPacketKeyAgreement*)c_pData;
		if (!d->IsCipherPrepared())
		{
			metin2::log::error("Cipher isn't prepared. {} maybe a Hacker.", d->GetHostName());
			d->DelayedDisconnect(5);
			return 0;
		}
		if (d->FinishHandshake(p->wAgreedLength, p->data, p->wDataLength)) {
			// Handshaking succeeded
			if (metin2::config::is_auth()) {
				d->SetPhase(PHASE_AUTH);
			} else {
				d->SetPhase(PHASE_LOGIN);
			}
		} else {
			metin2::log::info( "[CInputHandshake] Key agreement failed: al={:d} dl={:d}",
				p->wAgreedLength, p->wDataLength);
			d->SetPhase(PHASE_CLOSE);
		}
	}
	else
		metin2::log::error("Handshake phase does not handle packet {:d} (host {})", bHeader, d->GetHostName());

	return 0;
}


