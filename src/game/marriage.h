#ifndef __MARRIAGE_H
#define __MARRIAGE_H

#include "event.h"
#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

namespace marriage
{
	struct TWeddingInfo
	{
		uint32_t dwMapIndex;
	};

	extern const int MARRIAGE_POINT_PER_DAY;
	struct TMarriage
	{
		uint32_t m_pid1;
		uint32_t m_pid2;
		int   love_point;
		time_t marry_time;
		CHARACTER * ch1;
		CHARACTER * ch2;
		bool bSave;
		bool is_married;
		std::string name1;
		std::string name2;

		TWeddingInfo * pWeddingInfo;

		TMarriage(uint32_t pid1, uint32_t pid2, int _love_point, time_t _marry_time, const char* name1, const char* name2) : 
			m_pid1(pid1), 
			m_pid2(pid2), 
			love_point(_love_point),
			marry_time(_marry_time), 
			is_married(false),
			name1(name1),
			name2(name2),
			pWeddingInfo(nullptr),
			eventNearCheck(nullptr)
		{
			ch1 = ch2 = nullptr;
			bSave = false;
			isLastNear = false;
			byLastLovePoint = 0;
		}

		~TMarriage();

		void Login(CHARACTER * ch);
		void Logout(uint32_t pid);

		bool IsOnline()
		{
			return ch1 && ch2;
		}

		bool IsNear();

		uint32_t GetOther(uint32_t PID) const
		{
			if (m_pid1 == PID)
				return m_pid2;

			if (m_pid2 == PID)
				return m_pid1;

			return 0;
		}

		int GetMarriagePoint();
		int GetMarriageGrade();

		int GetBonus(uint32_t dwItemVnum, bool bShare = true, CHARACTER * me = nullptr);

		void WarpToWeddingMap(uint32_t dwPID);
		void Save();
		void SetMarried();

		void Update(uint32_t point);
		void RequestEndWedding();

		void StartNearCheckEvent();
		void StopNearCheckEvent();
		void NearCheck();

		bool isLastNear;
		uint8_t byLastLovePoint;
		LPEVENT eventNearCheck;
	};

	class CManager : public singleton<CManager>
	{
		public:
			CManager();
			~CManager() override;

			bool	Initialize();
			void	Destroy();

			TMarriage*	Get(uint32_t dwPlayerID);

			bool	IsMarriageUniqueItem(uint32_t dwItemVnum);

			bool	IsMarried(uint32_t dwPlayerID);
			bool	IsEngaged(uint32_t dwPlayerID);
			bool	IsEngagedOrMarried(uint32_t dwPlayerID);

			void	RequestAdd(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2);
			void	Add(uint32_t dwPID1, uint32_t dwPID2, time_t tMarryTime, const char* szName1, const char* szName2);

			void	RequestUpdate(uint32_t dwPID1, uint32_t dwPID2, int iUpdatePoint, uint8_t byMarried);
			void	Update(uint32_t dwPID1, uint32_t dwPID2, int32_t lTotalPoint, uint8_t byMarried);

			void	RequestRemove(uint32_t dwPID1, uint32_t dwPID2);
			void	Remove(uint32_t dwPID1, uint32_t dwPID2);

			void	Login(CHARACTER * ch);

			void	Logout(uint32_t pid);
			void	Logout(CHARACTER * ch);

			void	WeddingReady(uint32_t dwPID1, uint32_t dwPID2, uint32_t dwMapIndex);
			void	WeddingStart(uint32_t dwPID1, uint32_t dwPID2);
			void	WeddingEnd(uint32_t dwPID1, uint32_t dwPID2);

			void	RequestEndWedding(uint32_t dwPID1, uint32_t dwPID2);

			template <typename Func>
				Func	for_each_wedding(Func f);

		private:
			std::unordered_set<TMarriage*> m_Marriages;
			std::map<uint32_t, TMarriage *> m_MarriageByPID;
			std::set<std::pair<uint32_t, uint32_t> > m_setWedding;
	};

	template <typename Func>
		Func CManager::for_each_wedding(Func f)
		{
			for (auto it = m_setWedding.begin(); it!=m_setWedding.end(); ++it)
			{
				TMarriage* pMarriage = Get(it->first);
				if (pMarriage)
					f(pMarriage);
			}
			return f;
		}

}

#endif
