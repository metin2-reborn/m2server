#include "text_file_loader.h"

#include "file_loader.h"
#include "vector.h"

#include <common/utils.h>
#include <libthecore/log.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <utility>

std::deque<CTextFileLoader::TGroupNode> CTextFileLoader::ms_groupNodes;

void CTextFileLoader::DestroySystem()
{
	ms_groupNodes.clear();
}

CTextFileLoader::CTextFileLoader()
	: m_dwcurLineIndex(0), mc_pData(nullptr)
{
	SetTop();

	m_globalNode.strGroupName = "global";
	m_globalNode.pParentNode = nullptr;
}

CTextFileLoader::~CTextFileLoader()
{	
}

bool CTextFileLoader::Load(const char * c_szFileName)
{
	m_strFileName = c_szFileName;

	m_dwcurLineIndex = 0;

	FILE* fp = fopen(c_szFileName, "rb");

	if (nullptr == fp)
		return false;

	fseek(fp, 0L, SEEK_END);
	const size_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char * pData = new char[fileSize];
	fread(pData, fileSize, 1, fp);
	fclose(fp);

	m_fileLoader.Bind(fileSize, pData);
	delete[] pData;

	LoadGroup(&m_globalNode);
	return true;
}

bool CTextFileLoader::LoadGroup(TGroupNode * pGroupNode)
{
	TTokenVector stTokenVector;
	for (; m_dwcurLineIndex < m_fileLoader.GetLineCount(); ++m_dwcurLineIndex)
	{
		if (!m_fileLoader.SplitLine(m_dwcurLineIndex, &stTokenVector))
			continue;

		to_lower(stTokenVector[0]);

		if ('{' == stTokenVector[0][0])
			continue;

		if ('}' == stTokenVector[0][0])
			break;

		// Group
		if (0 == stTokenVector[0].compare("group"))
		{
			if (2 != stTokenVector.size())
			{
				sys_err("Invalid group syntax token size: %zu != 2 (DO NOT SPACE IN NAME)", stTokenVector.size());
				for (unsigned int i = 0; i < stTokenVector.size(); ++i)
					sys_err("  %u %s", i, stTokenVector[i].c_str());
				exit(1);
				continue;
			}

			TGroupNode * pNewNode = &ms_groupNodes.emplace_back();
			pNewNode->pParentNode = pGroupNode;
			pNewNode->strGroupName = stTokenVector[1];
			to_lower(pNewNode->strGroupName);
			pGroupNode->ChildNodeVector.push_back(pNewNode);

			++m_dwcurLineIndex;

			LoadGroup(pNewNode);
		}
		// List
		else if (0 == stTokenVector[0].compare("list"))
		{
			if (2 != stTokenVector.size())
			{
				assert(!"There is no list name!");
				continue;
			}

			TTokenVector stSubTokenVector;

			to_lower(stTokenVector[1]);
			std::string key = stTokenVector[1];
			stTokenVector.clear();

			++m_dwcurLineIndex;
			for (; m_dwcurLineIndex < m_fileLoader.GetLineCount(); ++m_dwcurLineIndex)
			{
				if (!m_fileLoader.SplitLine(m_dwcurLineIndex, &stSubTokenVector))
					continue;

				if ('{' == stSubTokenVector[0][0])
					continue;

				if ('}' == stSubTokenVector[0][0])
					break;

				for (uint32_t j = 0; j < stSubTokenVector.size(); ++j)
				{
					stTokenVector.push_back(stSubTokenVector[j]);
				}
			}

			pGroupNode->LocalTokenVectorMap.insert(std::make_pair(key, stTokenVector));
		}
		else
		{
			std::string key = stTokenVector[0];

			if (1 == stTokenVector.size())
			{
				sys_err("CTextFileLoader::LoadGroup : must have a value (filename: %s line: %d key: %s)",
						m_strFileName.c_str(),
						m_dwcurLineIndex,
						key.c_str());
				break;
			}

			stTokenVector.erase(stTokenVector.begin());
			pGroupNode->LocalTokenVectorMap.insert(std::make_pair(key, stTokenVector));
		}
	}

	return true;
}

void CTextFileLoader::SetTop()
{
	m_pcurNode = &m_globalNode;
}

uint32_t CTextFileLoader::GetChildNodeCount()
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return 0;
	}

	return m_pcurNode->ChildNodeVector.size();
}

bool CTextFileLoader::SetChildNode(const char * c_szKey)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	for (uint32_t i = 0; i < m_pcurNode->ChildNodeVector.size(); ++i)
	{
		TGroupNode * pGroupNode = m_pcurNode->ChildNodeVector[i];
		if (0 == pGroupNode->strGroupName.compare(c_szKey))
		{
			m_pcurNode = pGroupNode;
			return true;
		}
	}

	return false;
}

bool CTextFileLoader::SetChildNode(const std::string & c_rstrKeyHead, uint32_t dwIndex)
{
	char szKey[32];
	snprintf(szKey, sizeof(szKey), "%s%02u", c_rstrKeyHead.c_str(), (unsigned int) dwIndex);
	return SetChildNode(szKey);
}

bool CTextFileLoader::SetChildNode(uint32_t dwIndex)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	if (dwIndex >= m_pcurNode->ChildNodeVector.size())
	{
		assert(!"Node index to set is too large to access!");
		return false;
	}

	m_pcurNode = m_pcurNode->ChildNodeVector[dwIndex];

	return true;
}

bool CTextFileLoader::SetParentNode()
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	if (nullptr == m_pcurNode->pParentNode)
	{
		assert(!"Current group node is already top!");
		return false;
	}

	m_pcurNode = m_pcurNode->pParentNode;

	return true;
}

bool CTextFileLoader::GetCurrentNodeName(std::string * pstrName)
{
	if (!m_pcurNode)
		return false;
	if (nullptr == m_pcurNode->pParentNode)
		return false;

	*pstrName = m_pcurNode->strGroupName;

	return true;
}

bool CTextFileLoader::GetTokenVector(const std::string & c_rstrKey, TTokenVector ** ppTokenVector)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	TTokenVectorMap::iterator it = m_pcurNode->LocalTokenVectorMap.find(c_rstrKey);
	if (m_pcurNode->LocalTokenVectorMap.end() == it)
	{
		sys_log(2, " CTextFileLoader::GetTokenVector - Failed to find the key %s [%s :: %s]", m_strFileName.c_str(), m_pcurNode->strGroupName.c_str(), c_rstrKey.c_str());
		return false;
	}

	*ppTokenVector = &it->second;

	return true;
}

bool CTextFileLoader::GetTokenInteger(const std::string & c_rstrKey, int * pData)
{
	TTokenVector * pTokenVector;
	if (!GetTokenVector(c_rstrKey, &pTokenVector))
		return false;

	if (pTokenVector->empty())
	{
		sys_log(2, " CTextFileLoader::GetTokenInteger - Failed to find the value %s [%s : %s]", m_strFileName.c_str(), m_pcurNode->strGroupName.c_str(), c_rstrKey.c_str());
		return false;
	}

	int out = 0;
	str_to_number(out, pTokenVector->at(0).c_str());
	*pData = out;

	return true;
}

bool CTextFileLoader::GetTokenDoubleWord(const std::string & c_rstrKey, uint32_t * pData)
{
	return GetTokenInteger(c_rstrKey, (int *) pData);
}

bool CTextFileLoader::GetTokenFloat(const std::string & c_rstrKey, float * pData)
{
	TTokenVector * pTokenVector;
	if (!GetTokenVector(c_rstrKey, &pTokenVector))
		return false;

	if (pTokenVector->empty())
	{
		sys_log(2, " CTextFileLoader::GetTokenFloat - Failed to find the value %s [%s : %s]", m_strFileName.c_str(), m_pcurNode->strGroupName.c_str(), c_rstrKey.c_str());
		return false;
	}

	*pData = atof(pTokenVector->at(0).c_str());

	return true;
}

bool CTextFileLoader::GetTokenVector3(const std::string & c_rstrKey, VECTOR * pVector3)
{
	TTokenVector * pTokenVector;
	if (!GetTokenVector(c_rstrKey, &pTokenVector))
		return false;

	if (pTokenVector->size() != 3)
	{
		sys_log(2, " CTextFileLoader::GetTokenVector3 - This key should have 3 values %s [%s : %s]", m_strFileName.c_str(), m_pcurNode->strGroupName.c_str(), c_rstrKey.c_str());
		return false;
	}

	pVector3->x = atof(pTokenVector->at(0).c_str());
	pVector3->y = atof(pTokenVector->at(1).c_str());
	pVector3->z = atof(pTokenVector->at(2).c_str());

	return true;
}


bool CTextFileLoader::GetTokenPosition(const std::string & c_rstrKey, VECTOR * pVector)
{
	return GetTokenVector3(c_rstrKey, pVector);
}

bool CTextFileLoader::GetTokenString(const std::string & c_rstrKey, std::string * pString)
{
	TTokenVector * pTokenVector;
	if (!GetTokenVector(c_rstrKey, &pTokenVector))
		return false;

	if (pTokenVector->empty())
	{
		sys_log(2, " CTextFileLoader::GetTokenString - Failed to find the value %s [%s : %s]", m_strFileName.c_str(), m_pcurNode->strGroupName.c_str(), c_rstrKey.c_str());
		return false;
	}

	*pString = pTokenVector->at(0);

	return true;
}

