#include "SoftPhone.h"
#define THIS_FILE "Functions.cpp"

// display error and exit application
void SoftPhone::error_exit(const char *title, pj_status_t status)
{
    (void)title;
    (void)status;
        pjsua_call_hangup_all();
        pjsua_acc_set_registration(acc_id, PJ_FALSE);
        pjsua_destroy();
        exit(1);
}

void SoftPhone::configurate()
{
    FileLoaderInterface* loader = FileFactory::getIniFileLoader();

    if (loader)
    {
        FileIteratorInterface* fileIt = loader->load("settings.ini");

        if (fileIt && fileIt->isLoaded())
        {
            try
            {
                SipServer         = fileIt->getItemByKey("SIP_SETTINGS", "SipServer").getString();
                SipUser           = fileIt->getItemByKey("SIP_SETTINGS", "SipUser").getString();
                SipPassword       = fileIt->getItemByKey("SIP_SETTINGS", "SipPassword").getString();
                SipCall           = fileIt->getItemByKey("SIP_SETTINGS", "SipCall").getString();
                RxIp              = fileIt->getItemByKey("SIP_SETTINGS", "RxIp").getString();
                TxIp              = fileIt->getItemByKey("SIP_SETTINGS", "TxIp").getString();
                PttDevice         = fileIt->getItemByKey("SIP_SETTINGS", "PttDevice").getString();
                m_RegTimeout      = fileIt->getItemByKey("SIP_SETTINGS", "RegTimeout").getInt16S();
                m_EnableRecording = (fileIt->getItemByKey("SIP_SETTINGS", "EnableRecord").getInt16S() != 0);

                localAddress      = fileIt->getItemByKey("UDP_SETTINGS", "localAddress").getString();
                localPort         = fileIt->getItemByKey("UDP_SETTINGS", "localPort").getString();
                remoteAddress     = fileIt->getItemByKey("UDP_SETTINGS", "remoteAddress").getString();
                remotePort        = fileIt->getItemByKey("UDP_SETTINGS", "remotePort").getString();

                clock_rate        = fileIt->getItemByKey("AUDIO_SETTINGS", "clockRate").getInt32S();

            }
            catch (std::exception const& ex)
            {
                sprintf(log_buffer, "Exception handled: %s", ex.what());
                AppentTextBrowser(log_buffer);
            }
        }
        else {
            AppentTextBrowser("Unable to open configuration file");
        }

        delete loader;
    }
}

void msgBox(const char *title,const char *msg)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if(msgBox.exec() == QMessageBox::Yes){

    }else {
      return;
    }
}

/////////ring//////////

void SoftPhone::playRing()
{
    pjsua_conf_connect(in_ring_slot_, 0);
}
void SoftPhone::stopRing()
{
       pjsua_conf_disconnect(in_ring_slot_, 0);
       pjmedia_tonegen_rewind(in_ring_port_);
}
void SoftPhone::init_ringTone()
{
    pj_str_t name = pj_str(const_cast<char*>("inring"));
    pjmedia_tone_desc tone[3];
    pj_status_t status;
    pj_pool_t* pool_;
    pool_ = pjsua_pool_create("blabble", 4096, 4096);
    if (pool_ == NULL)
        throw std::runtime_error("Ran out of memory creating pool!");

    tone[0].freq1 = 440;
    tone[0].freq2 = 480;
    tone[0].on_msec = 2000;
    tone[0].off_msec = 1000;
    tone[1].freq1 = 440;
    tone[1].freq2 = 480;
    tone[1].on_msec = 2000;
    tone[1].off_msec = 4000;
    tone[2].freq1 = 440;
    tone[2].freq2 = 480;
    tone[2].on_msec = 2000;
    tone[2].off_msec = 3000;

    status = pjmedia_tonegen_create2(pool_, &name, 8000, 1, 160, 16, PJMEDIA_TONEGEN_LOOP, &in_ring_port_);
    if (status != PJ_SUCCESS)        {
        throw std::runtime_error("Failed inring pjmedia_tonegen_create2");
    }

    status = pjmedia_tonegen_play(in_ring_port_, 1, tone, PJMEDIA_TONEGEN_LOOP);
    if (status != PJ_SUCCESS)
    {
        throw std::runtime_error("Failed inring pjmedia_tonegen_play");
    }

    status = pjsua_conf_add_port(pool_, in_ring_port_, &in_ring_slot_);
    if (status != PJ_SUCCESS)
    {
        throw std::runtime_error("Failed inring pjsua_conf_add_port");
    }
}

////////////////////////

void SoftPhone::connectCallToSoundCard(int CallID)
{
    int   clientPort = getConfPortNumber(CallID);

    if(clientPort != -1)
    {
        //-- found sound card list
        std::vector<int> soundCardPortList;
        getSoundCardPorts(soundCardPortList);

        // is listener port a sound port?
        for (std::vector<int>::iterator itSoundPortList = soundCardPortList.begin(); itSoundPortList != soundCardPortList.end(); ++itSoundPortList)
        {
            unsigned soundCardPortID = *itSoundPortList;

            if((unsigned int) clientPort != (unsigned int) PJSUA_INVALID_ID && soundCardPortID != (unsigned int) PJSUA_INVALID_ID)
            {
                connectPort(clientPort,soundCardPortID);
                //Log_Debug(mp::mp_logger, "connectCallToSoundCard Port: %d CallID: %d", soundCardPortID,CallID);
            }
        }
    }
}

void SoftPhone::disConnectCallFromSoundCard(int CallID)
{

    int clientPort = getConfPortNumber(CallID);

    if(clientPort != -1)
    {
        //-- found sound card list
        std::vector<int> soundCardPortList;
        getSoundCardPorts(soundCardPortList);

        // is listener port a sound port?
        for (std::vector<int>::iterator itSoundPortList = soundCardPortList.begin(); itSoundPortList != soundCardPortList.end(); ++itSoundPortList)
        {
            unsigned soundCardPortID = *itSoundPortList;

            if((unsigned int) clientPort != (unsigned int) PJSUA_INVALID_ID && soundCardPortID != (unsigned int) PJSUA_INVALID_ID)
            {
                disconnectPort(clientPort,soundCardPortID);
                //Log_Debug(mp::mp_logger, "disConnectCallFromSoundCard Port: %d CallID: %d", soundCardPortID,CallID);
            }
        }
    }
}


int SoftPhone::getConfPortNumber(int callId)
{
    int portNumber = -1;

    pjsua_call_info ci;

    if (callId != PJSUA_INVALID_ID)
    {
        pj_status_t status = pjsua_call_get_info(callId, &ci);

        if (status == PJ_SUCCESS)
        {
            portNumber = ci.conf_slot;
        }
        else {
            //VOIP_PJ_LOG(m_logger, "Error on getting call info", status);
        }
    }

    return portNumber;
}

bool SoftPhone::getExtFromCallID(int CallID, std::string& extNumber) const
{
    bool valid = false;

    if(CallID != -1)
    {
        pjsua_call_info ci;
        pj_status_t status;

        status = pjsua_call_get_info(CallID, &ci);
        if (status != PJ_SUCCESS)
        {
            return false;
        }

        std::string strRemoteInfo;
        strRemoteInfo.append(ci.remote_info.ptr, ci.remote_info.slen);

        size_t posrx = strRemoteInfo.find("<sip:rx");
        size_t postx = strRemoteInfo.find("<sip:tx");

        if (posrx != std::string::npos || postx != std::string::npos)
        {
            extNumber = strRemoteInfo;
            valid = true;
        }
        else
        {
            extNumber = strRemoteInfo;
            valid = true;
        }
    }
    return valid;
}

bool SoftPhone::connectPort(int srcPort, int dstPort)
{
    pj_status_t status = pjsua_conf_connect(srcPort, dstPort);

    if (status != PJ_SUCCESS)
    {
        return false;
    }

    return true;
}

bool SoftPhone::disconnectPort(int srcPort, int dstPort)
{
    pj_status_t status = pjsua_conf_disconnect(srcPort, dstPort);

    if (status != PJ_SUCCESS)
    {
        return false;
    }

    return true;
}

//////////////calllll////////////////


void SoftPhone::getCallPortInfo(std::string ext, pjsua_conf_port_info& returnInfo)
{
    unsigned i, count;
    pjsua_conf_port_id id[PJSUA_MAX_CALLS];

    count = PJ_ARRAY_SIZE(id);
    pjsua_enum_conf_ports(id, &count);

    for (i = 0; i < count; ++i)
    {
        pjsua_conf_port_info info;
        pjsua_conf_get_port_info(id[i], &info);

        std::string cardname(info.name.ptr,(int) info.name.slen);
        std::size_t found = ext.find(cardname);
        if (found != std::string::npos)
        {
             returnInfo = info;
        }
    }
}

void SoftPhone::listConfs()
{
    int indexOfBuffer = 0;
    char stringBuffer [8192];
    indexOfBuffer += sprintf(stringBuffer+indexOfBuffer, "\n");
    indexOfBuffer += sprintf(stringBuffer+indexOfBuffer, "listConfs:\n");
    indexOfBuffer += sprintf(stringBuffer+indexOfBuffer, "Active Call Count: %d Accounts: %d\n", getActiveCallCount(),pjsua_acc_get_count());
    indexOfBuffer += sprintf(stringBuffer+indexOfBuffer, "Conference ports:\n");

    unsigned i, count;
    pjsua_conf_port_id id[PJSUA_MAX_CALLS];

    count = PJ_ARRAY_SIZE(id);
    pjsua_enum_conf_ports(id, &count);

    for (i = 0; i < count; ++i)
    {
        char txlist[PJSUA_MAX_CALLS * 4 + 10];
        unsigned j;
        pjsua_conf_port_info info;

        pjsua_conf_get_port_info(id[i], &info);

        txlist[0] = '\0';
        for (j = 0; j < info.listener_cnt; ++j)
        {
            char s[10];
            pj_ansi_sprintf(s, "#%d ", info.listeners[j]);
            pj_ansi_strcat(txlist, s);
        }

        indexOfBuffer += sprintf(stringBuffer+indexOfBuffer, "Port #%02d[%2dKHz/%dms/%d] %20.*s  transmitting to: %s\n",
                info.slot_id, info.clock_rate / 1000,
                info.samples_per_frame * 1000 / info.channel_count
                        / info.clock_rate, info.channel_count,
                (int) info.name.slen, info.name.ptr, txlist);
    }

    stringBuffer[indexOfBuffer] = '\0';

    printf("%s", stringBuffer);
}

int SoftPhone::getActiveCallCount()
{
    return pjsua_call_get_count();
}

void SoftPhone::getSoundCardPorts(std::vector<int>& portList) const
{
    portList.clear();
    int index = 0;

    std::vector<pjmedia_aud_dev_info>::const_iterator it = m_AudioDevInfoVector.begin();

    for (; it != m_AudioDevInfoVector.end(); ++it)
    {
        portList.push_back(index);
        index++;
    }
}

std::string SoftPhone::getSipUrl(const char* ext)
{
    std::string const& sipServer = sip_domainstr.toStdString() ;
    std::string const& sipURL = std::string("<sip:") + ext + std::string("@") + sipServer + std::string(">");
    return sipURL;
}

void SoftPhone::initializePttManager(const std::string pttDeviceName)
{
    m_PttManager.reset(PttManager::initialize(pttDeviceName));
    printf("\n");

    if (m_PttManager.get() != 0)
    {
        m_PttManager->addPttListener(this);
        printf("PTT Device : %s initialized\n",pttDeviceName.c_str());
        sprintf(log_buffer, "PTT Device : %s initialized",pttDeviceName.c_str());
        AppentTextBrowser(log_buffer);
    }
    else {
        printf("PTT Device : %s not Found\n",pttDeviceName.c_str());
        sprintf(log_buffer, "PTT Device : %s not Found",pttDeviceName.c_str());
        AppentTextBrowser(log_buffer);
    }
}

bool SoftPhone::getExtFromRemoteInfo(pj_str_t const& remoteInfo, std::string& extNumber) const
{
    bool valid = false;

    std::string strRemoteInfo;
    strRemoteInfo.append(remoteInfo.ptr, remoteInfo.slen);

    size_t pos1 = strRemoteInfo.find_first_of(":");
    size_t pos2 = strRemoteInfo.find_first_of("@", pos1);
    if (pos1 != std::string::npos && pos2 != std::string::npos)
    {
        extNumber = strRemoteInfo.substr(pos1 + 1, pos2 - pos1 - 1);
        valid = true;
    }

    return valid;
}


////////////IP RADIO///////

void SoftPhone::setRadioPttbyCallID(bool pttval,int callId,int priority)
{
    if(callId == PJSUA_INVALID_ID)
    {
        printf("Error setRadioPttbyCallID : PJSUA_INVALID_ID\n");
        return;
    }

    std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(callId) ;

    if( iter != transport_map.end())
    {
        pj_status_t status = setAdapterPtt (iter->second, pttval, priority);
        if (status != PJ_SUCCESS) {
            printf("Error setRadioPttbyCallID : setAdapterPtt\n");
        }
    }
}

int SoftPhone::get_IPRadioBss(pjsua_call_id callId)
{
    pjsua_call_info ci;
    pj_status_t status = pjsua_call_get_info(callId, &ci);
    if (status == PJ_SUCCESS)
    {
        std::string extNumber;
        if (getExtFromRemoteInfo(ci.remote_info, extNumber))
        {
            if (extNumber.find("rx") !=  std::string::npos || extNumber.find("Rx") !=  std::string::npos
               || extNumber.find("rX") !=  std::string::npos || extNumber.find("RX") !=  std::string::npos)
            {
                pj_uint32_t ed137_val = 0;
                std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(callId) ;
                if( iter != transport_map.end())
                    ed137_val = get_ed137_value(iter->second);

                uint32_t i = ed137_val & 0x000000f8;//bss mask
                i = i >> 3;//shift 3 bits
                return i;
            }
        }
    }
    return 0;
}
int SoftPhone::get_IPRadioPttStatus(pjsua_call_id callId)
{
    pj_uint32_t ed137_val = 0;
    std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(callId) ;
    if( iter != transport_map.end())
        ed137_val = get_ed137_value(iter->second);

    uint32_t i = ed137_val & 0xe0000000;//pttid mask
    i = i >> 29;//shift 22 bits
    return i;
}

int SoftPhone::get_IPRadioPttId(pjsua_call_id callId)
{
    pj_uint32_t ed137_val=0;
    std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(callId) ;
    if( iter != transport_map.end())
        ed137_val = get_ed137_value(iter->second);

    uint32_t i = ed137_val & 0x0fc00000;//pttid mask
    i = i >> 22;//shift 22 bits
    return i;
}

int SoftPhone::get_IPRadioSquelch(pjsua_call_id callId)
{
    pj_uint32_t ed137_val=0;
    std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(callId) ;
    if( iter != transport_map.end())
        ed137_val = get_ed137_value(iter->second);

    uint32_t i=ed137_val & 0x10000000;//pttid mask
    i = i >> 28;//shift 22 bits
    return i;
}

bool SoftPhone::get_IPRadioStatus(pjsua_call_id callId)
{
    pj_uint32_t ed137_val=0;
    std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(callId) ;
    if( iter != transport_map.end())
        ed137_val = get_ed137_value(iter->second);

    uint32_t i = ed137_val;

    if(i>0)
    {
         return true;
    }
    return false;
}

//////////////////////////

bool SoftPhone::createNullPort()
{

    if(m_NullSlot == PJ_INVALID_SOCKET)
    {
        pj_status_t status = PJ_SUCCESS;
        pj_pool_t *pool;

        pool = pjsua_pool_create("VoIP-pool", (4 * 1024 * 1024), (1 * 1024 * 1024));
        if (pool == NULL)
        {
            printf("Error allocating memory pool\n");
            return false;
        }

        status = pjmedia_null_port_create(pool, CLOCK_RATE, 1, SAMPLES_PER_FRAME*2, 16, &m_NullPort);

        if (status != PJ_SUCCESS) {
            printf("Error Null Port");
            return false;
        }

        m_NullPort->put_frame = &SoftPhone::PutData;
        m_NullPort->get_frame = &SoftPhone::GetData;

        pj_cstr(&m_NullPort->info.name, "Null Port");

        status = pjsua_conf_add_port(pool, m_NullPort, &m_NullSlot);
        if (status != PJ_SUCCESS) {
            printf("Error Null Slot");
            return false;
        }

    }
    return true;
}

pj_status_t SoftPhone::GetData(struct pjmedia_port *this_port, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);

    return PJ_SUCCESS;
}

pj_status_t SoftPhone::PutData(struct pjmedia_port *this_port, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);

   return PJ_SUCCESS;
}

void SoftPhone::getSoundCardName(int number, std::string& cardName)
{
    int index = 0;
    std::vector<pjmedia_aud_dev_info>::const_iterator it = m_AudioDevInfoVector.begin();

    for (; it != m_AudioDevInfoVector.end(); ++it)
    {
        pjmedia_aud_dev_info const& info = *it;
        if(index == number)
        {
            cardName = std::string(info.name);
            break;
        }
        ++index;
    }
}


void SoftPhone::delete_oldrecords(std::string folderName)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp = opendir(folderName.c_str())) != NULL) {
        while ((dirp = readdir(dp)) != NULL) {
            std::string fileToDell = folderName + "/" + dirp->d_name ;
            if(fileToDell.substr(fileToDell.find_last_of(".") + 1) == "wav")
            {
               remove(fileToDell.c_str());
            }
        }
    }
}

void SoftPhone::registerThread()
{
    if (!pj_thread_is_registered())
    {
        pj_thread_desc desc;
        pj_thread_t* this_thread;

        pj_bzero(desc, sizeof(desc));

        pj_status_t status = pj_thread_register("CurrentThread", desc, &this_thread);

        if (status != PJ_SUCCESS)
        {
            printf("CurrentThread could not be registered. Status: %d\n", status);
            return;
        }

        this_thread = pj_thread_this();
        if (this_thread == NULL)
        {
            printf("pj_thread_this() returns NULL!\n");
            return;
        }

        if (pj_thread_get_name(this_thread) == NULL)
        {
            printf("pj_thread_get_name() returns NULL!\n");
            return;
        }
    }
}


void SoftPhone::recordRtp(const char *pktbuf,const char *payloadbuf,unsigned int pktlen,unsigned int payloadlen)
{
    if(!m_EnableRecording)
        return;
    //record data
    m_WavWriter->writeRTPWav(pktbuf,payloadbuf,pktlen,payloadlen);

    if(m_SendPackets)
    {
        //print rtp data
        unsigned int i;
        int indexOfBuffer = 0;
        char stringBuffer [1000];
        indexOfBuffer += sprintf(stringBuffer + indexOfBuffer, "\n");
        u_int8_t value;
        indexOfBuffer += sprintf(stringBuffer + indexOfBuffer, "RTP Data Size = %d\n", (int) pktlen);
        for (i = 0; i < pktlen; i++)
        {
        if (i > 0)
        {
           indexOfBuffer += sprintf(stringBuffer + indexOfBuffer, ":");
        }
        value = pktbuf[i];
        indexOfBuffer += sprintf(stringBuffer + indexOfBuffer,"%02X", value);
        }

        indexOfBuffer += sprintf(stringBuffer + indexOfBuffer, "\n");
        stringBuffer[indexOfBuffer] = '\0';
        printf("%s", stringBuffer);
        //AppentTextBrowser(stringBuffer);
    }
}


bool SoftPhone::isRadio(std::string extNumber)
{
    if(extNumber.substr(0, 7).compare("<sip:tx") == 0 || extNumber.substr(0, 2).compare("tx") == 0 ||
            extNumber.substr(0, 7).compare("<sip:rx") == 0 || extNumber.substr(0, 2).compare("rx") == 0)
    {
        return true;
    }
    return false;
}

bool SoftPhone::findSoundCard(std::string const& soundDevice)
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
        return false;
    }
    // Create a list of the devices in the 'hidraw' subsystem.
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "hidraw");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices) {

        const char *path;

        /// Get the filename of the /sys entry for the device
        //  and create a udev_device object (dev) representing it
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        if (!dev)
        {
            return false;
        }

        for (devAttrIt = devAttrVec.begin(); devAttrIt != devAttrVec.end(); ++devAttrIt)
        {
            const char* deviceInfo = udev_device_get_sysattr_value(dev, devAttrIt->c_str());

            if (deviceInfo) {
                //found = (strstr(deviceInfo, soundDevice.c_str()) != 0);
                std::string cardInfoName(deviceInfo);
                if(soundDevice.find(cardInfoName) != std::string::npos)
                    found = true;
                else
                    found =false;

                if (found) {
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

// clean application exit
void SoftPhone::app_exit()
{
        m_UdpCommunicator->Uinit();
        m_TcpCommunicator->Uinit();
        pjsua_call_hangup_all();
        removeDefault();
        removeUser();
        pjsua_destroy();
        exit(0);
}
