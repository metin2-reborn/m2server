#include "NetBase.h"

#include <libthecore/fdwatch.h>
#include <libthecore/log.h>
#include <libthecore/main.h>


fdwatch * CNetBase::m_fdWatcher = nullptr;

CNetBase::CNetBase()
{
}

CNetBase::~CNetBase()
{
}

CNetPoller::CNetPoller()
{
}

CNetPoller::~CNetPoller()
{
	Destroy();
}

bool CNetPoller::Create()
{
	sys_log(1, "NetPoller::Create()");

	if (m_fdWatcher)
		return true;

	m_fdWatcher = fdwatch_new(512);

	if (!m_fdWatcher)
	{
		Destroy();
		return false;
	}

	return true;
}

void CNetPoller::Destroy()
{
	sys_log(1, "NetPoller::Destroy()");

	if (m_fdWatcher)
	{
		fdwatch_delete(m_fdWatcher);
		m_fdWatcher = nullptr;
	}

	thecore_destroy();
}
