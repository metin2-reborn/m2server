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
struct t_gm {
	t_admin_info info;		
	std::set<std::string> *pset_host;	
};

std::set<std::string> set_host;
std::map<std::string, t_gm> map_gm;

void gm_new_clear()
{
	set_host.clear();
	map_gm.clear();
}

void gm_new_insert( const t_admin_info &rAdminInfo )
{	
	metin2::log::info( "InsertGMList(account:{}, player:{}, contact_ip:{}, server_ip:{}, auth:{:d})", 
			rAdminInfo.account,
			rAdminInfo.name,
			rAdminInfo.contact_ip,
			rAdminInfo.server_ip,
			rAdminInfo.authority);

	t_gm t;

	if ( strlen( rAdminInfo.contact_ip ) == 0 )
	{
		t.pset_host = &set_host;
		metin2::log::info( "GM Use ContactIP" );
	}
	else
	{
		t.pset_host = nullptr;
		metin2::log::info( "GM Use Default Host List" );
	}

	memcpy ( &t.info, &rAdminInfo, sizeof ( rAdminInfo ) );

	map_gm[rAdminInfo.name] = t;

}

void gm_new_host_inert( const char * host )
{
	set_host.insert( host );
	metin2::log::info( "InsertGMHost(ip:{})", host );
}

uint8_t gm_new_get_level( const char * name, const char * host, const char* account)
{
	if ( metin2::config::test_server() ) return GM_IMPLEMENTOR;

	std::map<std::string, t_gm >::iterator it = map_gm.find(name);

	if (map_gm.end() == it)
		return GM_PLAYER;

	// GERMAN_GM_NOT_CHECK_HOST
	// The German version does not check the host.
	if (account)
	{
	if ( strcmp ( it->second.info.account, account  ) != 0 )
	{
	    metin2::log::info( "GM_NEW_GET_LEVEL : BAD ACCOUNT [ACCOUNT:{}/{}", it->second.info.account, account);
	    return GM_PLAYER;
	}
	}
	metin2::log::info( "GM_NEW_GET_LEVEL : FIND ACCOUNT");
	return it->second.info.authority;
	return GM_PLAYER;
}
	
//END_ADMIN_MANAGER
uint8_t gm_get_level(const char * name, const char * host, const char* account)
{
	return gm_new_get_level( name, host, account );
}
}
