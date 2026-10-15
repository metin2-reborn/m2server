#pragma once

#include "file_loader.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>

typedef std::map<std::string, TTokenVector>	TTokenVectorMap;
typedef std::map<std::string, int> TMapNameToIndex;

class CGroupNode
{
public:
	class CGroupNodeRow
	{
	public:
		CGroupNodeRow(CGroupNode* pGroupNode, TTokenVector& vec_values);
		virtual ~CGroupNodeRow();

		template <typename T>
		bool GetValue(const std::string & stColKey, T& value) const;
		template <typename T>
		bool GetValue(int idx, T& value) const;

		int GetSize() const;

	private:
		CGroupNode*		m_pOwnerGroupNode;
		TTokenVector	m_vec_values;
	};
public:
	CGroupNode();
	virtual ~CGroupNode();

	bool Load(const char * c_szFileName);

	bool SetChildNode(const char * c_szKey, CGroupNode* pChildNode);
	CGroupNode* GetChildNode(const std::string & c_rstrKey) const;
	std::string GetNodeName() const;

	int GetRowCount();
	
	template <typename T>
	bool GetValue(size_t i, const std::string & c_rstrColKey, T& tValue) const;	// Returns the value of a given column in the n-th row (n is just the order in the map, unrelated to the order in the txt). 
																				// Out of place, but a convenience function.
	template <typename T>
	bool GetValue(const std::string & c_rstrRowKey, const std::string & c_rstrColKey, T& tValue) const;
	template <typename T>
	bool GetValue(const std::string & c_rstrRowKey, int index, T& tValue) const;

	bool GetRow(const std::string & c_rstrKey, const CGroupNodeRow ** ppRow) const;
	// Note: idx is unrelated to the order written in the txt.
	bool GetRow(int idx, const CGroupNodeRow ** ppRow) const;
	bool GetGroupRow(const std::string& stGroupName, const std::string& stRow, const CGroupNode::CGroupNodeRow ** ppRow) const;

	template <typename T>
	bool GetGroupValue(const std::string& stGroupName, const std::string& stRow, int iCol, T& tValue) const;
	template <typename T>
	bool GetGroupValue(const std::string& stGroupName, const std::string& stRow, const std::string& stCol, T& tValue) const;

	int	GetColumnIndexFromName(const std::string& stName) const;

private:
	typedef std::map <std::string, CGroupNode*> TMapGroup;
	typedef std::map <std::string, CGroupNode::CGroupNodeRow> TMapRow;
	TMapGroup				m_mapChildNodes;
	std::string strGroupName;

	TMapNameToIndex			m_map_columnNameToIndex;
	TMapRow					m_map_rows;
	friend class CGroupTextParseTreeLoader;
};

class CGroupTextParseTreeLoader
{
public:
	CGroupTextParseTreeLoader();
	virtual ~CGroupTextParseTreeLoader();

	bool Load(const char * c_szFileName);

	CGroupNode*	GetGroup(const char * c_szGroupName);
private:
	bool LoadGroup(CGroupNode * pGroupNode);

	std::unique_ptr<CGroupNode>	m_pRootGroupNode;
	std::string					m_strFileName;
	uint32_t						m_dwcurLineIndex;

	CMemoryTextFileLoader		m_fileLoader;
};

template <typename T>
bool from_string(T& t, const std::string& s)
{
	std::istringstream iss(s);
	return !(iss >> t).fail();
}

template <>
inline bool from_string <uint8_t>(uint8_t& t, const std::string& s)
{
	std::istringstream iss(s);
	int temp;
	bool b = !(iss >> temp).fail();
	t = (uint8_t)temp;
	return b;
}

template <typename T>
bool CGroupNode::GetValue(size_t i, const std::string & c_rstrColKey, T& tValue) const
{
	if (i > m_map_rows.size())
		return false;
	
	TMapRow::const_iterator row_it = m_map_rows.begin();
	std::advance(row_it, i);
	
	auto col_idx_it = m_map_columnNameToIndex.find(c_rstrColKey);
	if (m_map_columnNameToIndex.end() == col_idx_it)
	{
		return false;
	}

	int index = col_idx_it->second;
	if (row_it->second.GetSize() <= index)
	{
		return false;
	}
	
	return row_it->second.GetValue(index, tValue);
}

template <typename T>
bool CGroupNode::GetValue(const std::string & c_rstrRowKey, const std::string & c_rstrColKey, T& tValue) const
{
	TMapRow::const_iterator row_it = m_map_rows.find(c_rstrRowKey);
	if (m_map_rows.end() == row_it)
	{
		return false;
	}
	auto col_idx_it = m_map_columnNameToIndex.find(c_rstrColKey);
	if (m_map_columnNameToIndex.end() == col_idx_it)
	{
		return false;
	}

	int index = col_idx_it->second;
	if (row_it->second.GetSize() <= index)
	{
		return false;
	}
	
	return row_it->second.GetValue(index, tValue);
}

template <typename T>
bool CGroupNode::GetValue(const std::string & c_rstrRowKey, int index, T& tValue) const
{
	TMapRow::const_iterator row_it = m_map_rows.find(c_rstrRowKey);
	if (m_map_rows.end() == row_it)
	{
		return false;
	}

	if (row_it->second.GetSize() <= index)
	{
		return false;
	}
	return row_it->second.GetValue(index, tValue);
}

template <typename T>
bool CGroupNode::GetGroupValue(const std::string& stGroupName, const std::string& stRow, int iCol, T& tValue) const
{
	CGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, iCol, tValue))
			return true;
	}
	// Look in the default group.
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, iCol, tValue))
			return true;
	}
	return false;
}

template <typename T>
bool CGroupNode::GetGroupValue(const std::string& stGroupName, const std::string& stRow, const std::string& stCol, T& tValue) const
{
	CGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, stCol, tValue))
			return true;
	}
	// Look in the default group.
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, stCol, tValue))
			return true;
	}
	return false;
}

template <typename T>
bool CGroupNode::CGroupNodeRow::GetValue(const std::string & stColKey, T& value) const
{
	int idx = m_pOwnerGroupNode->GetColumnIndexFromName(stColKey);
	if (idx < 0 || idx >= m_vec_values.size())
		return false;
	return from_string(value, m_vec_values[idx]);
}

template <typename T>
bool CGroupNode::CGroupNodeRow::GetValue(int idx, T& value) const
{
	if (idx < 0 || idx >= m_vec_values.size())
		return false;
	return from_string(value, m_vec_values[idx]);
}
