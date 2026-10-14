#ifndef __INC_MESSENGER_MANAGER_H
#define __INC_MESSENGER_MANAGER_H

#include "typedef.h"

#include <common/singleton.h>
#include <libsql/AsyncSQL.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

class MessengerManager : public singleton<MessengerManager>
{
	public:
		typedef std::string keyT;
		typedef const std::string & keyA;

		MessengerManager();
		~MessengerManager() override;

	public:
		void	P2PLogin(keyA account);
		void	P2PLogout(keyA account);

		void	Login(keyA account);
		void	Logout(keyA account);

		void	RequestToAdd(CHARACTER * ch, CHARACTER * target);
		bool	AuthToAdd(keyA account, keyA companion, bool bDeny);

		void	__AddToList(keyA account, keyA companion);	// Method that actually modifies m_Relation, m_InverseRelation
		void	AddToList(keyA account, keyA companion);

		void	__RemoveFromList(keyA account, keyA companion); // Method that actually modifies m_Relation, m_InverseRelation
		void	RemoveFromList(keyA account, keyA companion);	

		void	RemoveAllList(keyA account);

		void	Initialize();
		
		bool	IsInList(MessengerManager::keyA account, MessengerManager::keyA companion);

	private:
		void	SendList(keyA account);
		void	SendLogin(keyA account, keyA companion);
		void	SendLogout(keyA account, keyA companion);

		void	LoadList(SQLMsg * pmsg);

		void	Destroy();

		std::set<keyT>			m_set_loginAccount;
		std::map<keyT, std::set<keyT> >	m_Relation;
		std::map<keyT, std::set<keyT> >	m_InverseRelation;
		std::set<uint32_t>			m_set_requestToAdd;
};

#endif
