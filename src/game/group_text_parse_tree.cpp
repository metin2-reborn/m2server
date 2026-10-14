#include "group_text_parse_tree.h"

#include "file_loader.h"

#include <common/utils.h>
#include <libthecore/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <utility>

CGroupTextParseTreeLoader::CGroupTextParseTreeLoader()
	: m_dwcurLineIndex(0), m_pRootGroupNode(nullptr)
{
}

CGroupTextParseTreeLoader::~CGroupTextParseTreeLoader()
{
	if (nullptr != m_pRootGroupNode)
		delete m_pRootGroupNode;
}

bool CGroupTextParseTreeLoader::Load(const char * c_szFileName)
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

	if (nullptr != m_pRootGroupNode)
	{
		delete m_pRootGroupNode;
	}

	m_pRootGroupNode = new CGroupNode();
	if (!LoadGroup(m_pRootGroupNode))
	{
		delete m_pRootGroupNode;
		return false;
	}
	return true;
}

bool CGroupTextParseTreeLoader::LoadGroup(CGroupNode * pGroupNode)
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

			CGroupNode * pNewNode = new CGroupNode();

			pNewNode->strGroupName = stTokenVector[1];
			to_lower(pNewNode->strGroupName);
			
			pGroupNode->SetChildNode(pNewNode->strGroupName.c_str(), pNewNode);

			++m_dwcurLineIndex;

			LoadGroup(pNewNode);
		}
		// Column Names
		else if (0 == stTokenVector[0].compare("#--#"))
		{
			for (int i = 1; i < stTokenVector.size(); i++)
			{
				to_lower(stTokenVector[i]);
				pGroupNode->m_map_columnNameToIndex.insert(TMapNameToIndex::value_type (stTokenVector[i], i - 1));
			}
		}
		else
		{
			std::string key = stTokenVector[0];

			if (1 == stTokenVector.size())
			{
				sys_err("CGroupTextParseTreeLoader::LoadGroup : must have a value (filename: %s line: %d key: %s)",
						m_strFileName.c_str(),
						m_dwcurLineIndex,
						key.c_str());
				break;
			}

			stTokenVector.erase(stTokenVector.begin());
			pGroupNode->m_map_rows.insert(std::make_pair(key, CGroupNode::CGroupNodeRow(pGroupNode, stTokenVector)));
		}
	}

	return true;
}

CGroupNode* CGroupTextParseTreeLoader::GetGroup(const char * c_szGroupName)
{
	if (nullptr == m_pRootGroupNode)
		return nullptr;
	return m_pRootGroupNode->GetChildNode(c_szGroupName);
}

CGroupNode::CGroupNode()
{
}

CGroupNode::~CGroupNode()
{
	for (TMapGroup::iterator it = m_mapChildNodes.begin(); it != m_mapChildNodes.end(); it++)
	{
		delete it->second;
	}
}

bool CGroupNode::SetChildNode(const char * c_szKey, CGroupNode* pChildGroup)
{
	if (nullptr == pChildGroup)
	{
		m_mapChildNodes.erase(c_szKey);
		return true;
	}
	TMapGroup::iterator it = m_mapChildNodes.find(c_szKey);
	if (it != m_mapChildNodes.end())
		return false;

	m_mapChildNodes.insert (TMapGroup::value_type (c_szKey, pChildGroup));
	
	return true;
}

CGroupNode* CGroupNode::GetChildNode(const std::string & c_rstrKey) const
{
	TMapGroup::const_iterator it = m_mapChildNodes.find(c_rstrKey);
	if (it != m_mapChildNodes.end())
		return it->second;
	else
		return nullptr;
}

std::string CGroupNode::GetNodeName() const
{
	return strGroupName;
}

int CGroupNode::GetRowCount()
{
	return m_map_rows.size();
}

bool CGroupNode::GetRow(const std::string & c_rstrRowKey, const CGroupNode::CGroupNodeRow ** ppRow) const
{
	TMapRow::const_iterator row_it = m_map_rows.find(c_rstrRowKey);
	if (m_map_rows.end() == row_it)
	{
		return false;
	}

	*ppRow = &row_it->second;

	return true;
}

// Note: idx is unrelated to the order written in the txt.
bool CGroupNode::GetRow(int idx, const CGroupNode::CGroupNodeRow ** ppRow) const
{
	if (idx >= m_map_rows.size())
		return false;

	TMapRow::const_iterator row_it = m_map_rows.begin();

	std::advance(row_it, idx);

	*ppRow = &row_it->second;

	return true;
}

bool CGroupNode::GetGroupRow(const std::string& stGroupName, const std::string& stRow, const CGroupNode::CGroupNodeRow ** ppRow) const
{
	CGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetRow(stRow, ppRow))
			return true;
	}
	// Look in the default group.
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetRow(stRow, ppRow))
			return true;
	}
	return false;
}

int	CGroupNode::GetColumnIndexFromName(const std::string& stName) const
{
	TMapNameToIndex::const_iterator it = m_map_columnNameToIndex.find(stName);
	if (m_map_columnNameToIndex.end() == it)
		return -1;
	else
		return it->second;
}

CGroupNode::CGroupNodeRow::CGroupNodeRow(CGroupNode* pGroupNode, TTokenVector& vec_values)
{
	m_pOwnerGroupNode = pGroupNode;
	m_vec_values.clear();
	std::copy(vec_values.begin(), vec_values.end(), std::back_inserter(m_vec_values));
}

CGroupNode::CGroupNodeRow::~CGroupNodeRow()
{
}

int CGroupNode::CGroupNodeRow::GetSize() const
{
	return m_vec_values.size();
}
