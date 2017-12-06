#ifndef PTT_MANAGER_H
#define PTT_MANAGER_H

#include <string>

#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "Utility.h"


#include "PttAdapter.h"
#include "IPttListener.h"

#include <libudev.h>

class PttManager : public PttAdapter
{
public:

    ~PttManager();

    static PttManager* initialize(std::string const& pttDevice);  

private:
    enum ePttState
    {
        ePTT_RELEASED,
        ePTT_PRESSED,
        eHOOK_OFF,
        eHOOK_ON
    };

    PttManager(std::string const& deviceNodePath, std::string const& vendorId, std::string const& productId);

    PttManager();

    static bool findPttDev(std::string const& pttDevice, std::string& deviceNodePath, std::string& vendorId, std::string& productId);

    void runThread();

    void runPttCheckThread();

    void printDevInfo(int fd);

    bool openDevice(char const* const dev);

    bool tryToReOpenDevice();

    bool tiproPttHandler(const unsigned char bytes[16], ePttState *state);
    bool othersPttHandler(const unsigned char bytes[16], ePttState *state);    

    void runTimerThread();
    void timer_handler(const boost::system::error_code &ec);
    bool isTimerRunning();
    void starTimer(unsigned int time);

private:

    std::string m_DeviceNodePath;

    boost::atomic_bool m_IsRunning;

    boost::shared_ptr<boost::thread> m_ThreadInstance;  

    PttEvent m_PttEvent;

    static std::string m_PttDevice;

    int m_FD;

    // used for handling device specific codes
    std::string m_deviceVendor;
    std::string m_deviceProduct;

    // used for handling ptt events
    double m_PttpressedTime;
    double m_PttReleasedTime;
    unsigned int m_PttDelay;
    bool m_PttStatusMp;
    bool m_PttStatus;
    bool m_isTimerRunning;
    bool m_shortPress;

};

#endif
