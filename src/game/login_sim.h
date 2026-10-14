#ifndef __INC_METIN_II_SERVER_LOGINSIM__
#define __INC_METIN_II_SERVER_LOGINSIM__

#include "desc_client.h"

#include <common/tables.h>

#include <cstdint>
#include <vector>

class CLoginSim
{
	public:

		bool IsCheck()
		{
			return bCheck;
		}

		void SendLogin()
		{
			bCheck = true;

			if (IsDone())
				return;

			if (vecIdx == 0)
				db_clientdesc->DBPacket(HEADER_GD_AUTH_LOGIN, 0, &auth, sizeof(TPacketGDAuthLogin));

			load.player_id = vecID[vecIdx++];
			db_clientdesc->DBPacket(HEADER_GD_LOGIN_BY_KEY, 0, &login, sizeof(TPacketGDLoginByKey));
		}

		void SendLoad()
		{
			db_clientdesc->DBPacket(HEADER_GD_PLAYER_LOAD, 0, &load, sizeof(TPlayerLoadPacket));
		}

		bool IsDone()
		{
			if (vecIdx >= vecID.size())
				return true;

			return false;
		}

		TPacketGDAuthLogin auth;
		TPacketGDLoginByKey login;
		TPlayerLoadPacket load;
		TLogoutPacket logout;

		std::vector<uint32_t> vecID;
		unsigned int vecIdx;
		bool bCheck;
};

#endif
