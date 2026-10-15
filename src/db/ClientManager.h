// vim:ts=8 sw=4
#ifndef __INC_CLIENTMANAGER_H__
#define __INC_CLIENTMANAGER_H__

#include "LoginData.h"
#include "Peer.h"

#include <common/building.h>
#include <common/length.h>
#include <common/singleton.h>
#include <common/tables.h>
#include <libnet/io.h>
#include <libnet/listener.h>
#include <libsql/AsyncSQL.h>

#include <mysql.h>

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class CPlayerTableCache;
class CItemCache;
class CItemPriceListTableCache;

size_t CreatePlayerSaveQuery(char * pszQuery, size_t querySize, TPlayerTable * pkTab);

class CClientManager : public singleton<CClientManager>
{
    public:
	typedef std::list<CPeer *>			TPeerList;
	typedef std::unordered_map<uint32_t, CPlayerTableCache *> TPlayerTableCacheMap;
	typedef std::unordered_map<uint32_t, CItemCache *> TItemCacheMap;
	typedef std::unordered_set<CItemCache *, std::hash<CItemCache*> > TItemCacheSet;
	typedef std::unordered_map<uint32_t, TItemCacheSet *> TItemCacheSetPtrMap;
	typedef std::unordered_map<uint32_t, CItemPriceListTableCache*> TItemPriceListCacheMap;
	typedef std::unordered_map<short, uint8_t> TChannelStatusMap;

	// MYSHOP_PRICE_LIST
	/// Item price list request info
	/**
	 * first: Peer handle
	 * second: ID of the requesting player
	 */
	typedef std::pair< uint32_t, uint32_t >		TItemPricelistReqInfo;
	// END_OF_MYSHOP_PRICE_LIST

	class ClientHandleInfo
	{
	    public:
		uint32_t	dwHandle;
		uint32_t	account_id;
		uint32_t	player_id;
		uint8_t	account_index;
		char	login[LOGIN_MAX_LEN + 1];
		char	safebox_password[SAFEBOX_PASSWORD_MAX_LEN + 1];
		char	ip[MAX_HOST_LENGTH + 1];

		TAccountTable * pAccountTable;
		TSafeboxTable * pSafebox;

		ClientHandleInfo(uint32_t argHandle, uint32_t dwPID = 0)
		{
		    dwHandle = argHandle;
		    pSafebox = nullptr;
		    pAccountTable = nullptr;
		    player_id = dwPID;
		};
		//Constructor for the German gift feature
		ClientHandleInfo(uint32_t argHandle, uint32_t dwPID, uint32_t accountId)
		{
		    dwHandle = argHandle;
		    pSafebox = nullptr;
		    pAccountTable = nullptr;
		    player_id = dwPID;
			account_id = accountId;
		};

		~ClientHandleInfo()
		{
		    if (pSafebox)
			{
				delete pSafebox;
				pSafebox = nullptr;
			}
		}
	};

	public:
	CClientManager();
	~CClientManager() override;

	bool	Initialize();
	time_t	GetCurrentTime();

	// Handles the packets a peer has received (CPeer::on_receive).
	void	ProcessPackets(CPeer * peer);

	void	MainLoop();
	void	Quit();

	void	SetTablePostfix(const char* c_pszTablePostfix);
	void	SetPlayerIDStart(int iIDStart);


	uint32_t	GetUserCount();	// Returns the number of connected users.

	void	SendTime();

	CPlayerTableCache *	GetPlayerCache(uint32_t id);
	void			PutPlayerCache(TPlayerTable * pNew);

	void			CreateItemCacheSet(uint32_t dwID);
	TItemCacheSet *		GetItemCacheSet(uint32_t dwID);
	void			FlushItemCacheSet(uint32_t dwID);

	CItemCache *		GetItemCache(uint32_t id);
	void			PutItemCache(TPlayerItem * pNew, bool bSkipQuery = false);
	bool			DeleteItemCache(uint32_t id);

	void			UpdatePlayerCache();
	void			UpdateItemCache();

	// MYSHOP_PRICE_LIST
	/// Gets the price list cache.
	/**
	 * @param [in]	dwID owner of the price list (player ID)
	 * @return	pointer to the price list cache
	 */
	CItemPriceListTableCache*	GetItemPriceListCache(uint32_t dwID);

	/// Puts a price list cache.
	/**
	 * @param [in]	pItemPriceList item price list to put in the cache
	 *
	 * If the cache already exists it is replaced, not updated.
	 */
	void			PutItemPriceListCache(const TItemPriceListTable* pItemPriceList);


	/// Flushes item price list caches whose flush time has expired and removes them from the cache.
	void			UpdateItemPriceListCache(void);
	// END_OF_MYSHOP_PRICE_LIST


	void			SendGuildSkillUsable(uint32_t guild_id, uint32_t dwSkillVnum, bool bUsable);

	void			SetCacheFlushCountLimit(int iLimit);

	template <class Func>
	Func		for_each_peer(Func f);

	CPeer *		GetAnyPeer();

	void			ForwardPacket(uint8_t header, const void* data, int size, uint8_t bChannel = 0, CPeer * except = nullptr);

	void			SendNotice(const char * c_pszFormat, ...);

	char*			GetCommand(char* str);					//Function that gets commands for the German gift feature
	void			ItemAward(CPeer * peer, char* login);	//German gift feature

    protected:
	void	Destroy();

    private:
	bool		InitializeTables();
	bool		InitializeShopTable();
	bool		InitializeMobTable();
	bool		InitializeItemTable();
	bool		InitializeSkillTable();
	bool		InitializeRefineTable();
	bool		InitializeBanwordTable();
	bool		InitializeItemAttrTable();
	bool		InitializeItemRareTable();
	bool		InitializeLandTable();
	bool		InitializeObjectProto();
	bool		InitializeObjectTable();
	bool		InitializeMonarch();

	// Applies mob_proto and item_proto read from mob_proto.txt, item_proto.txt to the real db.
	//	Even without applying item_proto, mob_proto to the db the game runs fine,
	//	but problems occur because admin tools etc. read item_proto, mob_proto from the db.
	bool		MirrorMobTableIntoDB();
	bool		MirrorItemTableIntoDB();

	void		AddPeer(net::tcp::socket socket);
	void		RemovePeer(CPeer * pPeer);
	CPeer *		GetPeer(uint32_t ident);

	int		AnalyzeQueryResult(SQLMsg * msg);

	int		Process();


	CLoginData *	GetLoginData(uint32_t dwKey);
	CLoginData *	GetLoginDataByLogin(const char * c_pszLogin);
	CLoginData *	GetLoginDataByAID(uint32_t dwAID);

	void		InsertLoginData(CLoginData * pkLD);
	void		DeleteLoginData(CLoginData * pkLD);

	bool		InsertLogonAccount(const char * c_pszLogin, uint32_t dwHandle);
	bool		DeleteLogonAccount(const char * c_pszLogin, uint32_t dwHandle);
	bool		FindLogonAccount(const char * c_pszLogin);

	void		GuildCreate(CPeer * peer, uint32_t dwGuildID);
	void		GuildSkillUpdate(CPeer * peer, TPacketGuildSkillUpdate* p);
	void		GuildExpUpdate(CPeer * peer, TPacketGuildExpUpdate* p);
	void		GuildAddMember(CPeer * peer, TPacketGDGuildAddMember* p);
	void		GuildChangeGrade(CPeer * peer, TPacketGuild* p);
	void		GuildRemoveMember(CPeer * peer, TPacketGuild* p);
	void		GuildChangeMemberData(CPeer * peer, TPacketGuildChangeMemberData* p);
	void		GuildDisband(CPeer * peer, TPacketGuild * p);
	void		GuildWar(CPeer * peer, TPacketGuildWar * p);
	void		GuildWarScore(CPeer * peer, TPacketGuildWarScore * p);
	void		GuildChangeLadderPoint(TPacketGuildLadderPoint* p);
	void		GuildUseSkill(TPacketGuildUseSkill* p);
	void		GuildDepositMoney(TPacketGDGuildMoney* p);
	void		GuildWithdrawMoney(CPeer* peer, TPacketGDGuildMoney* p);
	void		GuildWithdrawMoneyGiveReply(TPacketGDGuildMoneyWithdrawGiveReply* p);
	void		GuildWarBet(TPacketGDGuildWarBet * p);
	void		GuildChangeMaster(TPacketChangeGuildMaster* p);


	void		QUERY_BOOT(CPeer * peer, TPacketGDBoot * p);

	void		QUERY_LOGOUT(CPeer * peer, uint32_t dwHandle, const char *);

	void		RESULT_LOGIN(CPeer * peer, SQLMsg *msg);

	void		QUERY_PLAYER_LOAD(CPeer * peer, uint32_t dwHandle, TPlayerLoadPacket*);
	void		RESULT_COMPOSITE_PLAYER(CPeer * peer, SQLMsg * pMsg, uint32_t dwQID);
	void		RESULT_PLAYER_LOAD(CPeer * peer, MYSQL_RES * pRes, ClientHandleInfo * pkInfo);
	void		RESULT_ITEM_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwPID);
	void		RESULT_QUEST_LOAD(CPeer * pkPeer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwPID);
	void		RESULT_AFFECT_LOAD(CPeer * pkPeer, MYSQL_RES * pRes, uint32_t dwHandle);

	// PLAYER_INDEX_CREATE_BUG_FIX
	void		RESULT_PLAYER_INDEX_CREATE(CPeer *pkPeer, SQLMsg *msg);
	// END_PLAYER_INDEX_CREATE_BUG_FIX
 
	// MYSHOP_PRICE_LIST
	/// Handles the result of the price info load query
	/**
	 * @param	peer pointer to the peer object of the game server that requested the price info
	 * @param	pMsg pointer to the object received as the query result
	 *
	 * Stores the loaded price list in the cache and sends the list to the peer.
	 */
	void		RESULT_PRICELIST_LOAD(CPeer* peer, SQLMsg* pMsg);

	/// Handles the result of the load query for a price info update
	/**
	 * @param	pMsg pointer to the object received as the query result
	 *
	 * Builds the price list cache from the loaded info and updates it with the received price info.
	 */
	void		RESULT_PRICELIST_LOAD_FOR_UPDATE(SQLMsg* pMsg);
	// END_OF_MYSHOP_PRICE_LIST

	void		QUERY_PLAYER_SAVE(CPeer * peer, uint32_t dwHandle, TPlayerTable*);

	void		__QUERY_PLAYER_CREATE(CPeer * peer, uint32_t dwHandle, TPlayerCreatePacket *);
	void		__QUERY_PLAYER_DELETE(CPeer * peer, uint32_t dwHandle, TPlayerDeletePacket *);
	void		__RESULT_PLAYER_DELETE(CPeer * peer, SQLMsg* msg);

	void		QUERY_PLAYER_COUNT(CPeer * pkPeer, TPlayerCountPacket *);

	void		QUERY_ITEM_SAVE(CPeer * pkPeer, const char * c_pData);
	void		QUERY_ITEM_DESTROY(CPeer * pkPeer, const char * c_pData);
	void		QUERY_ITEM_FLUSH(CPeer * pkPeer, const char * c_pData);


	void		QUERY_QUEST_SAVE(CPeer * pkPeer, TQuestTable *, uint32_t dwLen);
	void		QUERY_ADD_AFFECT(CPeer * pkPeer, TPacketGDAddAffect * p);
	void		QUERY_REMOVE_AFFECT(CPeer * pkPeer, TPacketGDRemoveAffect * p);

	void		QUERY_SAFEBOX_LOAD(CPeer * pkPeer, uint32_t dwHandle, TSafeboxLoadPacket *, bool bMall);
	void		QUERY_SAFEBOX_SAVE(CPeer * pkPeer, TSafeboxTable * pTable);
	void		QUERY_SAFEBOX_CHANGE_SIZE(CPeer * pkPeer, uint32_t dwHandle, TSafeboxChangeSizePacket * p);
	void		QUERY_SAFEBOX_CHANGE_PASSWORD(CPeer * pkPeer, uint32_t dwHandle, TSafeboxChangePasswordPacket * p);

	void		RESULT_SAFEBOX_LOAD(CPeer * pkPeer, SQLMsg * msg);
	void		RESULT_SAFEBOX_CHANGE_SIZE(CPeer * pkPeer, SQLMsg * msg);
	void		RESULT_SAFEBOX_CHANGE_PASSWORD(CPeer * pkPeer, SQLMsg * msg);
	void		RESULT_SAFEBOX_CHANGE_PASSWORD_SECOND(CPeer * pkPeer, SQLMsg * msg);

	void		QUERY_EMPIRE_SELECT(CPeer * pkPeer, uint32_t dwHandle, TEmpireSelectPacket * p);
	void		QUERY_SETUP(CPeer * pkPeer, uint32_t dwHandle, const char * c_pData);

	void		SendPartyOnSetup(CPeer * peer);

	void		QUERY_HIGHSCORE_REGISTER(CPeer * peer, TPacketGDHighscore* data);
	void		RESULT_HIGHSCORE_REGISTER(CPeer * pkPeer, SQLMsg * msg);

	void		QUERY_FLUSH_CACHE(CPeer * pkPeer, const char * c_pData);

	void		QUERY_PARTY_CREATE(CPeer * peer, TPacketPartyCreate* p);
	void		QUERY_PARTY_DELETE(CPeer * peer, TPacketPartyDelete* p);
	void		QUERY_PARTY_ADD(CPeer * peer, TPacketPartyAdd* p);
	void		QUERY_PARTY_REMOVE(CPeer * peer, TPacketPartyRemove* p);
	void		QUERY_PARTY_STATE_CHANGE(CPeer * peer, TPacketPartyStateChange* p);
	void		QUERY_PARTY_SET_MEMBER_LEVEL(CPeer * peer, TPacketPartySetMemberLevel* p);

	void		QUERY_RELOAD_PROTO();

	void		QUERY_CHANGE_NAME(CPeer * peer, uint32_t dwHandle, TPacketGDChangeName * p);

	void		QUERY_SMS(CPeer * pkPeer, TPacketGDSMS * p);

	void		AddGuildPriv(TPacketGiveGuildPriv* p);
	void		AddEmpirePriv(TPacketGiveEmpirePriv* p);
	void		AddCharacterPriv(TPacketGiveCharacterPriv* p);

	void		MoneyLog(TPacketMoneyLog* p);

	void		QUERY_AUTH_LOGIN(CPeer * pkPeer, uint32_t dwHandle, TPacketGDAuthLogin * p);

	void		QUERY_LOGIN_BY_KEY(CPeer * pkPeer, uint32_t dwHandle, TPacketGDLoginByKey * p);
	void		RESULT_LOGIN_BY_KEY(CPeer * peer, SQLMsg * msg);

	void		ChargeCash(const TRequestChargeCash * p);

	void		LoadEventFlag();
	void		SetEventFlag(TPacketSetEventFlag* p);
	void		SendEventFlagsOnSetup(CPeer* peer);

	// Marriage
	void		MarriageAdd(TPacketMarriageAdd * p);
	void		MarriageUpdate(TPacketMarriageUpdate * p);
	void		MarriageRemove(TPacketMarriageRemove * p);

	void		WeddingRequest(TPacketWeddingRequest * p);
	void		WeddingReady(TPacketWeddingReady * p);
	void		WeddingEnd(TPacketWeddingEnd * p);

	// MYSHOP_PRICE_LIST
	// Private shop price info

	/// Handler for the item price list update packet (HEADER_GD_MYSHOP_PRICELIST_UPDATE)
	/**
	 * @param [in]	pPacket pointer to the packet data
	 */
	void		MyshopPricelistUpdate(const TItemPriceListTable* pPacket);

	/// Handler for the item price list request packet (HEADER_GD_MYSHOP_PRICELIST_REQ)
	/**
	 * @param	peer pointer to the peer object of the game server that sent the packet
	 * @param [in]	dwHandle handle of the peer that requested the price info
	 * @param [in]	dwPlayerID ID of the player who requested the price list
	 */
	void		MyshopPricelistRequest(CPeer* peer, uint32_t dwHandle, uint32_t dwPlayerID);
	// END_OF_MYSHOP_PRICE_LIST

	// Building
	void		CreateObject(TPacketGDCreateObject * p);
	void		DeleteObject(uint32_t dwID);
	void		UpdateLand(uint32_t * pdw);


	// BLOCK_CHAT
	void		BlockChat(TPacketBlockChat * p);
	// END_OF_BLOCK_CHAT
   
    private:
	std::shared_ptr<net::listener>		m_listener;	// accepts the game and auth servers
	TPeerList				m_peerList;

	CPeer *					m_pkAuthPeer;

	// LoginKey, LoginData pair
	typedef std::unordered_map<uint32_t, CLoginData *> TLoginDataByLoginKey;
	TLoginDataByLoginKey			m_map_pkLoginData;

	// Login LoginData pair
	typedef std::unordered_map<std::string, CLoginData *> TLoginDataByLogin;
	TLoginDataByLogin			m_map_pkLoginDataByLogin;
	
	// AccountID LoginData pair
	typedef std::unordered_map<uint32_t, CLoginData *> TLoginDataByAID;
	TLoginDataByAID				m_map_pkLoginDataByAID;

	// Login LoginData pair (accounts actually logged in)
	typedef std::unordered_map<std::string, CLoginData *> TLogonAccountMap;
	TLogonAccountMap			m_map_kLogonAccount;

	int					m_iPlayerIDStart;
	int					m_iPlayerDeleteLevelLimit;
	int					m_iPlayerDeleteLevelLimitLower;

	std::vector<TMobTable>			m_vec_mobTable;
	std::vector<TItemTable>			m_vec_itemTable;
	std::map<uint32_t, TItemTable *>		m_map_itemTableByVnum;

	int					m_iShopTableSize;
	TShopTable *				m_pShopTable;

	int					m_iRefineTableSize;
	TRefineTable*				m_pRefineTable;

	std::vector<TSkillTable>		m_vec_skillTable;
	std::vector<TBanwordTable>		m_vec_banwordTable;
	std::vector<TItemAttrTable>		m_vec_itemAttrTable;
	std::vector<TItemAttrTable>		m_vec_itemRareTable;

	std::vector<building::TLand>		m_vec_kLandTable;
	std::vector<building::TObjectProto>	m_vec_kObjectProto;
	std::map<uint32_t, building::TObject *>	m_map_pkObjectTable;


	bool					m_bShutdowned;

	TPlayerTableCacheMap			m_map_playerCache;  // keyed by player id

	TItemCacheMap				m_map_itemCache;  // keyed by item id
	TItemCacheSetPtrMap			m_map_pkItemCacheSetPtr;  // keyed by player id; which item caches does this player have?

	// MYSHOP_PRICE_LIST
	/// Per-player item price list map. key: player ID, value: price list cache
	TItemPriceListCacheMap m_mapItemPriceListCache;  ///< Per-player item price lists
	// END_OF_MYSHOP_PRICE_LIST

	TChannelStatusMap m_mChannelStatus;

	struct TPartyInfo
	{
	    uint8_t bRole;
	    uint8_t bLevel;

		TPartyInfo() :bRole(0), bLevel(0)
		{
		}
	};

	typedef std::map<uint32_t, TPartyInfo>	TPartyMember;
	typedef std::map<uint32_t, TPartyMember>	TPartyMap;
	typedef std::map<uint8_t, TPartyMap>	TPartyChannelMap;
	TPartyChannelMap m_map_pkChannelParty;

	typedef std::map<std::string, int32_t>	TEventFlagMap;
	TEventFlagMap m_map_lEventFlag;

	int					m_iCacheFlushCount;
	int					m_iCacheFlushCountLimit;

    private :
	TItemIDRangeTable m_itemRange;

    public :
	bool InitializeNowItemID();
	uint32_t GetItemID();
	uint32_t GainItemID();
	TItemIDRangeTable GetItemRange() { return m_itemRange; }

	//BOOT_LOCALIZATION
    public:
	/* Initialize local info 
	 **/
	bool InitializeLocalization(); 

    private:
	std::vector<tLocale> m_vec_Locale;
	//END_BOOT_LOCALIZATION
	//ADMIN_MANAGER

	bool __GetAdminInfo(const char *szIP, std::vector<tAdminInfo> & rAdminVec);
	bool __GetHostInfo(std::vector<std::string> & rIPVec);
	//END_ADMIN_MANAGER

		
	//RELOAD_ADMIN
	void ReloadAdmin(CPeer * peer, TPacketReloadAdmin * p);
	//END_RELOAD_ADMIN
	void BreakMarriage(CPeer * peer, const char * data);

	struct TLogoutPlayer
	{
	    uint32_t	pid;
	    time_t	time;

	};

	typedef std::unordered_map<uint32_t, TLogoutPlayer*> TLogoutPlayerMap;
	TLogoutPlayerMap m_map_logout;
	
	void InsertLogoutPlayer(uint32_t pid);
	void DeleteLogoutPlayer(uint32_t pid);
	void UpdateLogoutPlayer();
	void UpdateItemCacheSet(uint32_t pid);

	void FlushPlayerCacheSet(uint32_t pid);

	//MONARCH
	void Election(CPeer * peer, uint32_t dwHandle, const char * p);
	void Candidacy(CPeer * peer, uint32_t dwHandle, const char * p);
	void AddMonarchMoney(CPeer * peer, uint32_t dwHandle, const char * p);
	void TakeMonarchMoney(CPeer * peer, uint32_t dwHandle, const char * p);
	void ComeToVote(CPeer * peer, uint32_t dwHandle, const char * p);
	void RMCandidacy(CPeer * peer, uint32_t dwHandle, const char * p);
	void SetMonarch(CPeer * peer, uint32_t dwHandle, const char * p);
	void RMMonarch(CPeer * peer, uint32_t dwHandle, const char * p);
								

	void DecMonarchMoney(CPeer * peer, uint32_t dwHandle, const char * p);
	//END_MONARCH

	void ChangeMonarchLord(CPeer* peer, uint32_t dwHandle, TPacketChangeMonarchLord* info);
	void BlockException(TPacketBlockException *data);

	void SendSpareItemIDRange(CPeer* peer);

	void UpdateHorseName(TPacketUpdateHorseName* data, CPeer* peer);
	void AckHorseName(uint32_t dwPID, CPeer* peer);
	void DeleteLoginKey(TPacketDC *data);
	void ResetLastPlayerID(const TPacketNeedLoginLogInfo* data);
	//delete gift notify icon
	void DeleteAwardId(TPacketDeleteAwardID* data);
	void UpdateChannelStatus(TChannelStatus* pData);
	void RequestChannelStatus(CPeer* peer, uint32_t dwHandle);
};

template<class Func>	
Func CClientManager::for_each_peer(Func f)
{
    TPeerList::iterator it;
    for (it = m_peerList.begin(); it!=m_peerList.end();++it)
    {
	f(*it);
    }
    return f;
}
// Packet and query statistics (ClientManager.cpp).
extern int g_item_count;
extern int g_query_count[2];

bool CreatePlayerTableFromRes(MYSQL_RES * res, TPlayerTable * pkTab);

#endif
