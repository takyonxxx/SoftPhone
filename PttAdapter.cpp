
#include "PttAdapter.h"

PttAdapter::PttAdapter()
{
}

PttAdapter::~PttAdapter()
{
}

void PttAdapter::pttPressed(PttEvent const& event)
{
    std::vector<IPttListener*>::const_iterator it = m_PttListeners.begin();
    for (; it != m_PttListeners.end(); ++it)
    {
        (*it)->pttPressed(event);
    }
}

void PttAdapter::pttReleased(PttEvent const& event)
{
    std::vector<IPttListener*>::const_iterator it = m_PttListeners.begin();
    for (; it != m_PttListeners.end(); ++it)
    {
        (*it)->pttReleased(event);
    }
}

void PttAdapter::addPttListener(IPttListener* listener)
{
    m_PttListeners.push_back(listener);
}

void PttAdapter::onHook(const PttEvent& event)
{
    std::vector<IPttListener*>::const_iterator it = m_PttListeners.begin();
    for (; it != m_PttListeners.end(); ++it)
    {
        (*it)->onHook(event);
    }
}

void PttAdapter::offHook(const PttEvent& event)
{
    std::vector<IPttListener*>::const_iterator it = m_PttListeners.begin();
    for (; it != m_PttListeners.end(); ++it)
    {
        (*it)->offHook(event);
    }
}
