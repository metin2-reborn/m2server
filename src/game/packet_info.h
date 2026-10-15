#pragma once

#include <cstdint>
#include <map>
#include <string>

struct TPacketElement {
	int		iSize;
	std::string	stName;
	int		iCalled;
	uint32_t	dwLoad;
	bool	bSequencePacket;
};

class CPacketInfo
{
	public:
		CPacketInfo();
		virtual ~CPacketInfo();

		void Set(int header, int size, const char * c_pszName, bool bSeq=false);
		bool Get(int header, int * size, const char ** c_ppszName);

		void Start();
		void End();

		void Log(const char * c_pszFileName);

		bool IsSequence(int header);
		void SetSequence(int header, bool bSeq);

	private:
		TPacketElement * GetElement(int header);

	protected:
		std::map<int, TPacketElement> m_pPacketMap;
		TPacketElement * m_pCurrentPacket;
		uint32_t m_dwStartTime;
};

class CPacketInfoCG : public CPacketInfo
{
	public:
		CPacketInfoCG();
		~CPacketInfoCG() override;
};

// PacketInfo P2P 
class CPacketInfoGG : public CPacketInfo
{
	public:
		CPacketInfoGG();
		~CPacketInfoGG() override;
};

/// Implemented in input_udp.cpp
