#include <metin2/game/admin/gm.h>

#include <metin2/game/app/config.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace metin2::game::inline admin {


//ADMIN_MANAGER
struct tGM {
	tAdminInfo Info;		
	std::set<std::string> *pset_Host;	
};

std::set<std::string> g_set_Host;
std::map<std::string, tGM> g_map_GM;

void gm_new_clear()
{
	g_set_Host.clear();
	g_map_GM.clear();
}

void gm_new_insert( const tAdminInfo &rAdminInfo )
{	
	metin2::log::info( "InsertGMList(account:{}, player:{}, contact_ip:{}, server_ip:{}, auth:{:d})", 
			rAdminInfo.m_szAccount,
			rAdminInfo.m_szName,
			rAdminInfo.m_szContactIP,
			rAdminInfo.m_szServerIP,
			rAdminInfo.m_Authority);

	tGM t;

	if ( strlen( rAdminInfo.m_szContactIP ) == 0 )
	{
		t.pset_Host = &g_set_Host;
		metin2::log::info( "GM Use ContactIP" );
	}
	else
	{
		t.pset_Host = nullptr;
		metin2::log::info( "GM Use Default Host List" );
	}

	memcpy ( &t.Info, &rAdminInfo, sizeof ( rAdminInfo ) );

	g_map_GM[rAdminInfo.m_szName] = t;

}

void gm_new_host_inert( const char * host )
{
	g_set_Host.insert( host );
	metin2::log::info( "InsertGMHost(ip:{})", host );
}

uint8_t gm_new_get_level( const char * name, const char * host, const char* account)
{
	if ( metin2::config::test_server() ) return GM_IMPLEMENTOR;

	std::map<std::string, tGM >::iterator it = g_map_GM.find(name);

	if (g_map_GM.end() == it)
		return GM_PLAYER;

	// GERMAN_GM_NOT_CHECK_HOST
	// The German version does not check the host.
	if (account)
	{
	if ( strcmp ( it->second.Info.m_szAccount, account  ) != 0 )
	{
	    metin2::log::info( "GM_NEW_GET_LEVEL : BAD ACCOUNT [ACCOUNT:{}/{}", it->second.Info.m_szAccount, account);
	    return GM_PLAYER;
	}
	}
	metin2::log::info( "GM_NEW_GET_LEVEL : FIND ACCOUNT");
	return it->second.Info.m_Authority;
	return GM_PLAYER;
}
	
//END_ADMIN_MANAGER
uint8_t gm_get_level(const char * name, const char * host, const char* account)
{
	return gm_new_get_level( name, host, account );
}
}
