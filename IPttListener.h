#ifndef PTT_LISTENER_INTERFACE_H
#define PTT_LISTENER_INTERFACE_H

struct PttEvent
{
    unsigned int SourceId;
};

class IPttListener
{
public:
    virtual ~IPttListener(){}

    virtual void pttPressed(PttEvent const& event) = 0;

    virtual void pttReleased(PttEvent const& event) = 0;

    virtual void onHook(PttEvent const& event) = 0;

    virtual void offHook(PttEvent const& event) = 0;
};

#endif
