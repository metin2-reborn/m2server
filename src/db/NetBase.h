// vim: ts=8 sw=4
#ifndef __INC_NETWORKBASE_H__
#define __INC_NETWORKBASE_H__

#include <common/singleton.h>
#include <libthecore/fdwatch.h>

class CNetBase
{
    public:
	CNetBase();
	virtual ~CNetBase();

    protected:
	static fdwatch *	m_fdWatcher;
};

class CNetPoller : public CNetBase, public singleton<CNetPoller>
{
    public:
	CNetPoller();
	~CNetPoller() override;

	bool	Create();
	void	Destroy();
};

#endif
