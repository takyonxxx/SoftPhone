#ifndef PTT_ADAPTER_H
#define PTT_ADAPTER_H

#include <vector>

#include "IPttListener.h"

class PttAdapter : public IPttListener
{
public:

    virtual ~PttAdapter();

    virtual void pttPressed(PttEvent const& event);

    virtual void pttReleased(PttEvent const& event);

    void addPttListener(IPttListener* listener);

    virtual void onHook(PttEvent const& event);

    virtual void offHook(PttEvent const& event);

protected:

    PttAdapter();

private:

    std::vector<IPttListener*> m_PttListeners;
};

#endif
