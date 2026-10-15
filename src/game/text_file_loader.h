#pragma once

#include "file_loader.h"
#include "vector.h"

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace metin2::game {

typedef std::map<std::string, TTokenVector>	TTokenVectorMap;

class CTextFileLoader
{
	public:
		struct TGroupNode {
			std::string strGroupName;

			TTokenVectorMap LocalTokenVectorMap;

			TGroupNode * pParentNode;
			std::vector<TGroupNode*> ChildNodeVector;
		};

	public:
		static void DestroySystem();

	public:
		CTextFileLoader();
		virtual ~CTextFileLoader();

		bool Load(const char * c_szFileName);

		void SetTop();
		uint32_t GetChildNodeCount();
		bool SetChildNode(const char * c_szKey);
		bool SetChildNode(const std::string & c_rstrKeyHead, uint32_t dwIndex);
		bool SetChildNode(uint32_t dwIndex);
		bool SetParentNode();
		bool GetCurrentNodeName(std::string * pstrName);

		bool GetTokenVector(const std::string & c_rstrKey, TTokenVector ** ppTokenVector);
		bool GetTokenInteger(const std::string & c_rstrKey, int * pData);
		bool GetTokenDoubleWord(const std::string & c_rstrKey, uint32_t * pData);
		bool GetTokenFloat(const std::string & c_rstrKey, float * pData);

		bool GetTokenVector3(const std::string & c_rstrKey, VECTOR * pVector3);

		bool GetTokenPosition(const std::string & c_rstrKey, VECTOR * pVector);
		bool GetTokenString(const std::string & c_rstrKey, std::string * pString);

	protected:
		bool LoadGroup(TGroupNode * pGroupNode);

	protected:
		std::string					m_strFileName;
		uint32_t						m_dwcurLineIndex;
		const void *					mc_pData;

		CMemoryTextFileLoader				m_fileLoader;

		TGroupNode					m_globalNode;
		TGroupNode *					m_pcurNode;

	private:
		// every group node of every loaded file; released by DestroySystem
		static std::deque<TGroupNode>			ms_groupNodes;
};
}
