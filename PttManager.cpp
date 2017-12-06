
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/hidraw.h>
#include <linux/input.h>

#include "PttManager.h"

std::string PttManager::m_PttDevice;
static const unsigned int SLEEP_PERIOD           = 50;
static const unsigned int THREAD_SLEEP_DURATION  = SLEEP_PERIOD * 1000; // 50 ms


PttManager::PttManager()
: m_IsRunning(false),  
  m_FD(-1)
{
}

PttManager::PttManager(std::string const& deviceNodePath, std::string const& vendorId, std::string const& productId)
: m_DeviceNodePath(deviceNodePath),
  m_IsRunning(true), 
  m_FD(-1),
  m_deviceVendor(vendorId),
  m_deviceProduct(productId)
{    

    m_PttEvent.SourceId = 1;
    m_PttEvent.SourceId = 1;

    m_PttpressedTime = 0;
    m_PttReleasedTime = 0;
    m_PttDelay = 250;
    m_isTimerRunning = false;
    m_shortPress = false;

    char* envVarValue = ::getenv("PTTRELEASEDELAY");
    if (envVarValue != 0)
    {
        int pttDelay = atoi(envVarValue);
        if(pttDelay!=0)
        {
             m_PttDelay = pttDelay;
        }
    }

   // ptt::logger.logf(__FILE__, "PTTRELEASEDELAY set to: %d\n", m_PttDelay);

    m_PttStatusMp = false;
    m_PttStatus = false;

    m_ThreadInstance.reset(new boost::thread(boost::bind(&PttManager::runThread, this)));

}

PttManager::~PttManager()
{
    m_IsRunning = false;

    if (!m_ThreadInstance->try_join_for(boost::chrono::milliseconds(5000)))
    {
        m_ThreadInstance->interrupt();
    }
}

PttManager* PttManager::initialize(std::string const& pttDevice)
{
    PttManager* pttManager = 0;

    m_PttDevice = pttDevice;

    std::string deviceNodePath;
    std::string vendorId;
    std::string productId;

    if (findPttDev(pttDevice, deviceNodePath, vendorId, productId))
    {
        pttManager = new PttManager(deviceNodePath, vendorId, productId);
    }

    return pttManager;
}

bool PttManager::findPttDev(std::string const& pttDevice, std::string& deviceNodePath, std::string& vendorId, std::string& productId)
{
    bool found = false;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    std::vector<std::string> devAttrVec;
    std::vector<std::string>::const_iterator devAttrIt;
    devAttrVec.push_back("idVendor");
    devAttrVec.push_back("idProduct");
    devAttrVec.push_back("manufacturer");
    devAttrVec.push_back("product");
    devAttrVec.push_back("serial");

    // Create the udev object
    udev = udev_new();
    if (!udev)
    {
       // ptt::logger.logf(__FILE__, "Error: Can't create udev!!!\n");
        return false;
    }

    // Create a list of the devices in the 'hidraw' subsystem.
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "hidraw");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    // For each item enumerated, print out its information.
    // udev_list_entry_foreach is a macro which expands to
    // a loop. The loop will be executed for each member in
    // devices, setting dev_list_entry to a list entry
    // which contains the device's path in /sys.
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path;

        /// Get the filename of the /sys entry for the device
        //  and create a udev_device object (dev) representing it
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        // usb_device_get_devnode() returns the path to the device node itself in /dev.
        deviceNodePath = udev_device_get_devnode(dev);

        // The device pointed to by dev contains information about
        // the hidraw device. In order to get information about the
        // USB device, get the parent device with the
        // subsystem/devtype pair of "usb"/"usb_device". This will
        // be several levels up the tree, but the function will find it.
        dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        if (!dev)
        {
           // ptt::logger.logf(__FILE__, "Error: Unable to find parent USB device!!!\n");
            return false;
        }

        // From here, we can call get_sysattr_value() for each file
        // in the device's /sys entry. The strings passed into these
        // functions (idProduct, idVendor, serial, etc.) correspond
        // directly to the files in the directory which represents
        // the USB device. Note that USB strings are Unicode, UCS2
        // encoded, but the strings returned from
        // udev_device_get_sysattr_value() are UTF-8 encoded.
        for (devAttrIt = devAttrVec.begin(); devAttrIt != devAttrVec.end(); ++devAttrIt)
        {
            const char* deviceInfo = udev_device_get_sysattr_value(dev, devAttrIt->c_str());

            if (deviceInfo) {

                found = (strstr(deviceInfo, pttDevice.c_str()) != 0);
                if (found) {
                    vendorId = udev_device_get_sysattr_value(dev, "idVendor");
                    productId = udev_device_get_sysattr_value(dev, "idProduct");
                    break;
                }
            }
        }

        udev_device_unref(dev);

        if (found) {
            break;
        }
    }
    // Free the enumerator object
    udev_enumerate_unref(enumerate);

    udev_unref(udev);

    return found;
}

static const char* bus_str(int bus)
{
    switch (bus)
    {
    case BUS_USB:
        return "USB";
        break;
    case BUS_HIL:
        return "HIL";
        break;
    case BUS_BLUETOOTH:
        return "Bluetooth";
        break;
    case BUS_VIRTUAL:
        return "Virtual";
        break;
    default:
        return "Other";
        break;
    }
}

void PttManager::printDevInfo(int fd)
{
    char buf[256];
    struct hidraw_devinfo info;

    ::memset(&info, 0x0, sizeof(info));
    ::memset(buf, 0x0, sizeof(buf));

    int res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
    if (res < 0) {
        perror("HIDIOCGRAWNAME");
    }
    else {
        printf("Raw Name: %s\n", buf);
    }

    // Get Physical Location
    res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);

    if (res < 0) {
        perror("HIDIOCGRAWPHYS");
    }
    else {
        printf("Raw Phys: %s\n", buf);
    }

    // Get Raw Info
    res = ioctl(fd, HIDIOCGRAWINFO, &info);

    if (res < 0) {
        perror("HIDIOCGRAWINFO");
    }
    else {
        printf("Raw Info:\n");
        printf("\tbustype: %d (%s)\n", info.bustype, bus_str(info.bustype));
        printf("\tvendor: 0x%04hx\n",  info.vendor);
        printf("\tproduct: 0x%04hx\n", info.product);
    }
}

bool PttManager::openDevice(char const* const dev)
{
    // Open Blocked
    m_FD = ::open(dev, O_RDWR);

    if (m_FD < 0) {
        std::string errMsg = "Error: Unable to open device " + std::string(dev);
        perror(errMsg.c_str());
        return false;
    }
    else {

            printf("PTT Device Node: %s\n", dev);
    }

    return true;
}

bool PttManager::tryToReOpenDevice()
{
   // ptt::logger.logf(__FILE__, "Trying re-open device %s\n", m_PttDevice.c_str());

    if (findPttDev(m_PttDevice, m_DeviceNodePath, m_deviceVendor, m_deviceProduct))
    {
        return openDevice(m_DeviceNodePath.c_str());
    }

    return false;
}


void PttManager::runThread()
{
    bool (PttManager::* pttHandlerFn)(const unsigned char[], ePttState *);

    ePttState pttState = ePTT_RELEASED;
    ePttState hookState = eHOOK_OFF;

    try
    {
        if (!m_DeviceNodePath.empty() && m_IsRunning)
        {
            if (!openDevice(m_DeviceNodePath.c_str()))
            {
                return;
            }

            printDevInfo(m_FD);

            unsigned char eventBuffer[16];
            int res = 0;

            // set TiPro ptt handler
            if (m_deviceVendor.compare("1222") == 0)
                pttHandlerFn = &PttManager::tiproPttHandler;
            // set other ptt handler
            else
                pttHandlerFn = &PttManager::othersPttHandler;

            while (m_IsRunning)
            {
                if(!isTimerRunning() && m_shortPress)
                {
                    m_shortPress = false;

                    if(m_PttStatusMp && !m_PttStatus)
                    {                      

                        PttAdapter::pttReleased(m_PttEvent);
                        m_PttStatusMp = false;

                        pttState = ePTT_RELEASED;
                    }
                    else if(!m_PttStatusMp && m_PttStatus)
                    {

                        PttAdapter::pttPressed(m_PttEvent);
                        m_PttStatusMp = true;

                        pttState = ePTT_PRESSED;
                    }
                }

                res = read(m_FD, eventBuffer, 2);

                if (res < 0)
                {
                    perror("PttManager Error");
                    ::close(m_FD);

                    if (!tryToReOpenDevice())
                    {
                        ::sleep(5);
                    }
                }
                else { // read succeed

                    if (res >= 2)
                    {                       
                        //printf("%02X ",  eventBuffer[0]);
                        //printf("%02X\n", eventBuffer[1]);

                        ePttState state;

                        bool eventFound = (this->*pttHandlerFn)(eventBuffer, &state);


                        if (eventFound) {                            

                            if(m_PttReleasedTime > m_PttpressedTime)
                            {
                                unsigned int pttTimeDiff = m_PttReleasedTime - m_PttpressedTime;

                                if(pttTimeDiff < m_PttDelay && pttTimeDiff != 0)
                                {
                                    m_shortPress = true;
                                   // ptt::logger.logf(__FILE__, "Ptt Short Press Warning: Time Diff (msec): %d\n",pttTimeDiff);
                                    starTimer(m_PttDelay - pttTimeDiff);
                                    continue;
                                }

                            }

                            switch (state) {

                                case ePTT_PRESSED:
                                    if (pttState != ePTT_PRESSED )
                                    {                                      

                                        PttAdapter::pttPressed(m_PttEvent);
                                        m_PttStatusMp = true;

                                        pttState = ePTT_PRESSED;

                                    }
                                    break;

                                case ePTT_RELEASED:

                                    if (pttState != ePTT_RELEASED )
                                    {

                                        PttAdapter::pttReleased(m_PttEvent);
                                        m_PttStatusMp = false;

                                        pttState = ePTT_RELEASED;

                                     }

                                    break;

                                case eHOOK_OFF:
                                    if (hookState != eHOOK_OFF)
                                    {                                        
                                        hookState = eHOOK_OFF;
                                    }
                                    PttAdapter::offHook(m_PttEvent);
                                    break;

                                case eHOOK_ON:
                                    if (hookState != eHOOK_ON)
                                    {
                                        hookState = eHOOK_ON;
                                    }
                                    PttAdapter::onHook(m_PttEvent);
                                    break;

                                default:
                                    break;
                            }
                        }
                    }
                } // else read succeed

            } // end while

            ::close(m_FD);
        }
    }
    catch (boost::thread_interrupted const&)
    {
       // ptt::logger.logf(__FILE__, "Ptt Thread interrupted\n");
    }
}

// for Tipro Handset
bool PttManager::tiproPttHandler(const unsigned char bytes[16], ePttState *state) {

    // Tipro USB Handset Ptt & Hook Events
    static const unsigned char PTT_KEY_RELEASED         = 0x20;
    static const unsigned char PTT_KEY_IS_BEING_PRESSED = 0x1F;

    static const unsigned char HOOK_KEY_ON              = 0x21;
    static const unsigned char HOOK_KEY_OFF             = 0x22;

    bool retval = false;


    switch (bytes[1]) {
        case PTT_KEY_IS_BEING_PRESSED:
            m_PttpressedTime = Utility::nowMilliSeconds();
            *state = ePTT_PRESSED;
            retval = true;
             m_PttStatus = retval;
            break;

        case PTT_KEY_RELEASED:
            m_PttReleasedTime = Utility::nowMilliSeconds();
            *state = ePTT_RELEASED;
            retval = true;
             m_PttStatus = false;
            break;

        case HOOK_KEY_ON:
            *state = eHOOK_ON;
            retval = true;
            break;

        case HOOK_KEY_OFF:
            *state = eHOOK_OFF;
            retval = true;
            break;

        default:
            retval = false;
            break;
    }   

    return retval;
}

// for JoyWarrior and Imtradex
bool PttManager::othersPttHandler(const unsigned char bytes[16], ePttState *state) {    

    // USB Ptt Events
    static const unsigned char PTT_KEY_RELEASED          = 0x00;
    static const unsigned char PTT_KEY_IS_BEING_PRESSED  = 0x01;

    // Joystick Ptt Events
    static const unsigned char JS_KEY_RELEASED          = 0x7F;
    static const unsigned char JS_KEY_IS_BEING_PRESSED  = 0xFF;

    static bool stateDev4 = false;
    static bool stateDev1 = false;

    bool retval = false;

    // Handle USB Ptt Events
    if (bytes[1] == 0x00)
    {
        switch (bytes[0]) {
            case PTT_KEY_IS_BEING_PRESSED:
                if( *state == ePTT_RELEASED)
                {
                    m_PttpressedTime = Utility::nowMilliSeconds();
                   // ptt::logger.logf(__FILE__, "m_PttpressedTime: %s\n",  Utility::nowStr().c_str());
                }
                *state = ePTT_PRESSED;
                retval = true;
                 m_PttStatus = retval;
                break;

            case PTT_KEY_RELEASED:
                if( *state == ePTT_PRESSED)
                {
                    m_PttReleasedTime = Utility::nowMilliSeconds();
                   // ptt::logger.logf(__FILE__, "m_PttReleasedTime: %s\n",  Utility::nowStr().c_str());
                }
                *state = ePTT_RELEASED;
                retval = true;
                 m_PttStatus = false;
                break;

            default:
               // ptt::logger.logError(__FILE__, "Invalid Ptt event\n");
                break;
        }
    }
    // Handle joystick Ptt Events
    else if (bytes[0] == JS_KEY_IS_BEING_PRESSED || bytes[0] == JS_KEY_RELEASED)
    {
        // handset
        if (bytes[0] == JS_KEY_IS_BEING_PRESSED && stateDev4 == false)
        {
            if( *state == ePTT_RELEASED)
            {
                m_PttpressedTime = Utility::nowMilliSeconds();
               // ptt::logger.logf(__FILE__, "m_PttpressedTime: %s\n",  Utility::nowStr().c_str());
            }

            stateDev4 = true;
            m_PttEvent.SourceId = 4;            

            *state = ePTT_PRESSED;
            retval = true;
            m_PttStatus = retval;           
        }
        else if (bytes[0] == JS_KEY_RELEASED && stateDev4 == true)
        {
            if( *state == ePTT_PRESSED)
            {
                m_PttReleasedTime = Utility::nowMilliSeconds();
               // ptt::logger.logf(__FILE__, "m_PttReleasedTime: %s\n",  Utility::nowStr().c_str());
            }

            stateDev4 = false;
            m_PttEvent.SourceId = 4;

            *state = ePTT_RELEASED;
            retval = true;
             m_PttStatus = false;             
        }
        // headset
        if (bytes[1] == JS_KEY_IS_BEING_PRESSED && stateDev1 == false)
        {
            if( *state == ePTT_RELEASED)
            {
                m_PttpressedTime = Utility::nowMilliSeconds();
               // ptt::logger.logf(__FILE__, "m_PttpressedTime: %s\n",  Utility::nowStr().c_str());
            }

            stateDev1 = true;
            m_PttEvent.SourceId = 1;

            *state = ePTT_PRESSED;
            retval = true;
             m_PttStatus = retval;           
        }
        else if (bytes[1] == JS_KEY_RELEASED && stateDev1 == true)
        {
            if( *state == ePTT_PRESSED)
            {
                m_PttReleasedTime = Utility::nowMilliSeconds();
               // ptt::logger.logf(__FILE__, "m_PttReleasedTime: %s\n",  Utility::nowStr().c_str());
            }

            stateDev1 = false;
            m_PttEvent.SourceId = 1;

            *state = ePTT_RELEASED;
            retval = true;
            m_PttStatus = false;
        }
    }    

    return retval;
}

/////Ptt Timer///////

bool PttManager::isTimerRunning()
{    
    return m_isTimerRunning;
}

void PttManager::timer_handler(const boost::system::error_code &ec)
{
    if (ec == boost::asio::error::operation_aborted)
    {
       // ptt::logger.logf(__FILE__, "%s Ptt Timer was cancelled or retriggered.\n",  Utility::nowStr().c_str());
    }
    else
    {
       // ptt::logger.logf(__FILE__, "%s Ptt Timer has been expired.\n",  Utility::nowStr().c_str());
        m_isTimerRunning = false;
    }   
}

void PttManager::starTimer(unsigned int time)
{
    (void) time;
   // ptt::logger.logf(__FILE__, "%s Ptt Timer has been started.\n",  Utility::nowStr().c_str());
    m_isTimerRunning = true;

    boost::asio::io_service io_service;

    boost::asio::deadline_timer t(io_service, boost::posix_time::milliseconds(time));
    t.async_wait(boost::bind(&PttManager::timer_handler, this,boost::asio::placeholders::error));
    io_service.run();
}

