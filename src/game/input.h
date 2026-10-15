#pragma once

#include "packet_info.h"
#include "typedef.h"

#include <common/tables.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

enum
{
	INPROC_CLOSE,
	INPROC_HANDSHAKE,
	INPROC_LOGIN,
	INPROC_MAIN,
	INPROC_DB,
	INPROC_P2P,
	INPROC_AUTH,
};

void LoginFailure(DESC * d, const char * c_pszStatus);

// Copies a fixed-size string field of a client packet, which may lack its NUL terminator, into
// dst: at most N - 1 bytes and never past the end of the field. Always NUL-terminates dst.
template <size_t N, size_t M>
inline void CopyPacketString(char (&dst)[N], const char (&src)[M])
{
	utf8_strnlcpy(dst, N, src, M);
}

// The same for a field of srcMax bytes reached through a pointer.
inline void CopyPacketString(char * dst, size_t dstSize, const char * src, size_t srcMax)
{
	utf8_strnlcpy(dst, dstSize, src, srcMax);
}

class CInputProcessor
{
	public:
		CInputProcessor();
		virtual ~CInputProcessor() {};

		virtual bool Process(DESC * d, const void * c_pvOrig, int iBytes, int & r_iBytesProceed);
		virtual uint8_t GetType() = 0;

		void BindPacketInfo(CPacketInfo * pPacketInfo);
		void Pong(DESC * d);
		void Handshake(DESC * d, const char * c_pData);
		void Version(CHARACTER * ch, const char* c_pData);

	protected:
		virtual int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) = 0;

		CPacketInfo * m_pPacketInfo;
		int	m_iBufferLeft;

		CPacketInfoCG 	m_packetInfoCG;
};

class CInputClose : public CInputProcessor
{
	public:
		uint8_t	GetType() override { return INPROC_CLOSE; }

		// A closing connection reads nothing: everything received is dropped unparsed.
		bool	Process(DESC * d, const void * c_pvOrig, int iBytes, int & r_iBytesProceed) override
		{
			r_iBytesProceed += iBytes;
			m_iBufferLeft = 0;
			return true;
		}

	protected:
		int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override { return 0; }
};

class CInputHandshake : public CInputProcessor
{
	public:
		CInputHandshake();
		~CInputHandshake() override;

		uint8_t	GetType() override { return INPROC_HANDSHAKE; }

	protected:
		int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override;

	protected:

		CPacketInfo *	m_pMainPacketInfo;
		std::unique_ptr<CPacketInfoCG>	m_pkHandshakePacketInfo;
};

class CInputLogin : public CInputProcessor
{
	public:
		uint8_t	GetType() override { return INPROC_LOGIN; }

	protected:
		int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override;

	protected:
		void		Login(DESC * d, const char * data);
		void		LoginByKey(DESC * d, const char * data);

		void		CharacterSelect(DESC * d, const char * data);
		void		CharacterCreate(DESC * d, const char * data);
		void		CharacterDelete(DESC * d, const char * data);
		void		Entergame(DESC * d, const char * data);
		void		Empire(DESC * d, const char * c_pData);
		void		GuildMarkCRCList(DESC * d, const char* c_pData);
		// MARK_BUG_FIX
		void		GuildMarkIDXList(DESC * d, const char* c_pData);
		// END_OF_MARK_BUG_FIX
		void		GuildMarkUpload(DESC * d, const char* c_pData);
		int			GuildSymbolUpload(DESC * d, const char* c_pData, size_t uiBytes);
		void		GuildSymbolCRC(DESC * d, const char* c_pData);
		void		ChangeName(DESC * d, const char * data);
};

class CInputMain : public CInputProcessor
{
	public:
		uint8_t	GetType() override { return INPROC_MAIN; }

	protected:
		int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override;

	protected:
		void		Attack(CHARACTER * ch, const uint8_t header, const char* data);

		int			Whisper(CHARACTER * ch, const char * data, size_t uiBytes);
		int			Chat(CHARACTER * ch, const char * data, size_t uiBytes);
		void		ItemUse(CHARACTER * ch, const char * data);
		void		ItemDrop(CHARACTER * ch, const char * data);
		void		ItemDrop2(CHARACTER * ch, const char * data);
		void		ItemMove(CHARACTER * ch, const char * data);
		void		ItemPickup(CHARACTER * ch, const char * data);
		void		ItemToItem(CHARACTER * ch, const char * pcData);
		void		QuickslotAdd(CHARACTER * ch, const char * data);
		void		QuickslotDelete(CHARACTER * ch, const char * data);
		void		QuickslotSwap(CHARACTER * ch, const char * data);
		int			Shop(CHARACTER * ch, const char * data, size_t uiBytes);
		void		OnClick(CHARACTER * ch, const char * data);
		void		Exchange(CHARACTER * ch, const char * data);
		void		Position(CHARACTER * ch, const char * data);
		void		Move(CHARACTER * ch, const char * data);
		int			SyncPosition(CHARACTER * ch, const char * data, size_t uiBytes);
		void		FlyTarget(CHARACTER * ch, const char * pcData, uint8_t bHeader);
		void		UseSkill(CHARACTER * ch, const char * pcData);
		
		void		ScriptAnswer(CHARACTER * ch, const void * pvData);
		void		ScriptButton(CHARACTER * ch, const void * pvData);
		void		ScriptSelectItem(CHARACTER * ch, const void * pvData);

		void		QuestInputString(CHARACTER * ch, const void * pvData);
		void		QuestConfirm(CHARACTER * ch, const void* pvData);
		void		Target(CHARACTER * ch, const char * pcData);
		void		Warp(CHARACTER * ch, const char * pcData);
		void		SafeboxCheckin(CHARACTER * ch, const char * c_pData);
		void		SafeboxCheckout(CHARACTER * ch, const char * c_pData, bool bMall);
		void		SafeboxItemMove(CHARACTER * ch, const char * data);
		int			Messenger(CHARACTER * ch, const char* c_pData, size_t uiBytes);

		void 		PartyInvite(CHARACTER * ch, const char * c_pData);
		void 		PartyInviteAnswer(CHARACTER * ch, const char * c_pData);
		void		PartyRemove(CHARACTER * ch, const char * c_pData);
		void		PartySetState(CHARACTER * ch, const char * c_pData);
		void		PartyUseSkill(CHARACTER * ch, const char * c_pData);
		void		PartyParameter(CHARACTER * ch, const char * c_pData);

		int			Guild(CHARACTER * ch, const char * data, size_t uiBytes);
		void		AnswerMakeGuild(CHARACTER * ch, const char* c_pData);

		void		Fishing(CHARACTER * ch, const char* c_pData);
		void		ItemGive(CHARACTER * ch, const char* c_pData);
		void		Hack(CHARACTER * ch, const char * c_pData);
		int			MyShop(CHARACTER * ch, const char * c_pData, size_t uiBytes);

		void		Refine(CHARACTER * ch, const char* c_pData);

};

class CInputDB : public CInputProcessor
{
public:
	bool Process(DESC * d, const void * c_pvOrig, int iBytes, int & r_iBytesProceed) override;
	uint8_t GetType() override { return INPROC_DB; }

protected:
	int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override;

protected:
	void		MapLocations(const char * c_pData);
	void		LoginSuccess(uint32_t dwHandle, const char *data);
	void		PlayerCreateFailure(DESC * d, uint8_t bType);	// 0 = general failure 1 = already exists
	void		PlayerDeleteSuccess(DESC * d, const char * data);
	void		PlayerDeleteFail(DESC * d);
	void		PlayerLoad(DESC * d, const char* data);
	void		PlayerCreateSuccess(DESC * d, const char * data);
	void		Boot(const char* data);
	void		QuestLoad(DESC * d, const char * c_pData);
	void		SafeboxLoad(DESC * d, const char * c_pData);
	void		SafeboxChangeSize(DESC * d, const char * c_pData);
	void		SafeboxWrongPassword(DESC * d);
	void		SafeboxChangePasswordAnswer(DESC * d, const char* c_pData);
	void		MallLoad(DESC * d, const char * c_pData);
	void		EmpireSelect(DESC * d, const char * c_pData);
	void		P2P(const char * c_pData);
	void		ItemLoad(DESC * d, const char * c_pData);
	void		AffectLoad(DESC * d, const char * c_pData);

	void		GuildLoad(const char * c_pData);
	void		GuildSkillUpdate(const char* c_pData);
	void		GuildSkillRecharge();
	void		GuildExpUpdate(const char* c_pData);
	void		GuildAddMember(const char* c_pData);
	void		GuildRemoveMember(const char* c_pData);
	void		GuildChangeGrade(const char* c_pData);
	void		GuildChangeMemberData(const char* c_pData);
	void		GuildDisband(const char* c_pData);
	void		GuildLadder(const char* c_pData);
	void		GuildWar(const char* c_pData);
	void		GuildWarScore(const char* c_pData);
	void		GuildSkillUsableChange(const char* c_pData);
	void		GuildMoneyChange(const char* c_pData);
	void		GuildWithdrawMoney(const char* c_pData);
	void		GuildWarReserveAdd(TGuildWarReserve * p);
	void		GuildWarReserveDelete(uint32_t dwID);
	void		GuildWarBet(TPacketGDGuildWarBet * p);
	void		GuildChangeMaster(TPacketChangeGuildMaster* p);

	void		LoginAlready(DESC * d, const char * c_pData);

	void		PartyCreate(const char* c_pData);
	void		PartyDelete(const char* c_pData);
	void		PartyAdd(const char* c_pData);
	void		PartyRemove(const char* c_pData);
	void		PartyStateChange(const char* c_pData);
	void		PartySetMemberLevel(const char* c_pData);

	void		Time(const char * c_pData);

	void		ReloadProto(const char * c_pData);
	void		ChangeName(DESC * d, const char * data);

	void		AuthLogin(DESC * d, const char * c_pData);
	void		AuthLoginOpenID(DESC * d, const char * c_pData);
	void		ItemAward(const char * c_pData);

	void		ChangeEmpirePriv(const char* c_pData);
	void		ChangeGuildPriv(const char* c_pData);
	void		ChangeCharacterPriv(const char* c_pData);

	void		MoneyLog(const char* c_pData);

	void		SetEventFlag(const char* c_pData);

	void		CreateObject(const char * c_pData);
	void		DeleteObject(const char * c_pData);
	void		UpdateLand(const char * c_pData);

	void		Notice(const char * c_pData);

	void		MarriageAdd(TPacketMarriageAdd * p);
	void		MarriageUpdate(TPacketMarriageUpdate * p);
	void		MarriageRemove(TPacketMarriageRemove * p);

	void		WeddingRequest(TPacketWeddingRequest* p);
	void		WeddingReady(TPacketWeddingReady* p);
	void		WeddingStart(TPacketWeddingStart* p);
	void		WeddingEnd(TPacketWeddingEnd* p);

	void		TakeMonarchMoney(DESC * d, const char * data );
	void		AddMonarchMoney(DESC * d, const char * data );
	void		DecMonarchMoney(DESC * d, const char * data );
	void		SetMonarch( DESC * d, const char * data );

	void		ChangeMonarchLord(TPacketChangeMonarchLordACK* data);
	void		UpdateMonarchInfo(MonarchInfo* data);

	// MYSHOP_PRICE_LIST
	/// Handler for the response packet to the item price list request (HEADER_DG_MYSHOP_PRICELIST_RES)
	/**
	* @param	d descriptor of the player who requested the item price list
	* @param	p pointer to the packet data
	*/
	void		MyshopPricelistRes( DESC * d, const TPacketMyshopPricelistHeader* p );
	// END_OF_MYSHOP_PRICE_LIST
	//
	//RELOAD_ADMIN
	void ReloadAdmin( const char * c_pData );
	//END_RELOAD_ADMIN

	void		DetailLog(const TPacketNeedLoginLogInfo* info);
	// German gift feature test
	void		ItemAwardInformer(TPacketItemAwardInfromer* data);

	void		RespondChannelStatus(DESC * desc, const char* pcData);

	protected:
		uint32_t		m_dwHandle;
};

class CInputP2P : public CInputProcessor
{
	public:
		CInputP2P();
		uint8_t	GetType() override { return INPROC_P2P; }

	protected:
		int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override;

	public:
		void		Setup(DESC * d, const char * c_pData);
		void		Login(DESC * d, const char * c_pData);
		void		Logout(DESC * d, const char * c_pData);
		int			Relay(DESC * d, const char * c_pData, size_t uiBytes);
		int			Notice(DESC * d, const char * c_pData, size_t uiBytes);
		int			MonarchNotice(DESC * d, const char * c_pData, size_t uiBytes);
		int			MonarchTransfer(DESC * d, const char * c_pData);
		int			Guild(DESC * d, const char* c_pData, size_t uiBytes);
		void		Shout(const char * c_pData);
		void		Disconnect(const char * c_pData);
		void		MessengerAdd(const char * c_pData);
		void		MessengerRemove(const char * c_pData);
		void		FindPosition(DESC * d, const char* c_pData);
		void		WarpCharacter(const char* c_pData);
		void		GuildWarZoneMapIndex(const char* c_pData);
		void		Transfer(const char * c_pData);
		void		XmasWarpSanta(const char * c_pData);
		void		XmasWarpSantaReply(const char * c_pData);
		void		BlockChat(const char * c_pData);
		void		IamAwake(DESC * d, const char * c_pData);

	protected:
		CPacketInfoGG 	m_packetInfoGG;
};

class CInputAuth : public CInputProcessor
{
	public:
		CInputAuth();
		uint8_t GetType() override { return INPROC_AUTH; }

	protected:
		int	Analyze(DESC * d, uint8_t bHeader, const char * c_pData) override;
		int auth_OpenID(const char *authKey, const char *ipAddr, char *rID);

	public:
		void		Login(DESC * d, const char * c_pData);
		void		LoginOpenID(DESC * d, const char * c_pData);		//2012.07.19 OpenID : Kim Yong-uk

};

extern bool RaceToJob(unsigned race, unsigned *ret_job);
extern void SendShout(const char * szText, uint8_t bEmpire);
