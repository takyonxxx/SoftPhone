#include "SoftPhone.h"
#include "ui_SoftPhone.h"
#include "StaticVariables.h"

SoftPhone *SoftPhone::theInstance_;
#define THIS_FILE "SoftPhone.cpp"
#define PI 3.141592653589793238462643383279

#define REAL 0
#define IMAG 1

static void on_reg_state(pjsua_acc_id acc_id);
static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
static void on_call_media_state(pjsua_call_id call_id);
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,pjsip_rx_data *rdata);
pjmedia_transport* on_create_media_transport(pjsua_call_id call_id, unsigned media_idx,pjmedia_transport *base_tp,unsigned flags);
void on_stream_created(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx,pjmedia_port **p_port);
void on_stream_destroyed(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx);

static fftw_plan  planFft;
static fftw_complex* in= (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*DEFAULT_FFT_SIZE);
static fftw_complex* out= (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*DEFAULT_FFT_SIZE);

pj_pool_t           *pool;
pjmedia_snd_port    *sndPlayback,*sndCapture;
pjmedia_port	    *sc, *sc_ch1,*sc_ch2,*sc_ch3,*sc_ch4,*sc_ch5;
pjsua_conf_port_id  sc_ch1_slot,sc_ch2_slot,sc_ch3_slot,sc_ch4_slot,sc_ch5_slot;
pjmedia_port        *conf_port;
char log_buffer[250];

using namespace std;

bool validateIpAddress(const string &ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));
    return result != 0;
}

SoftPhone::SoftPhone(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SoftPhone)
{
    ui->setupUi(this);

    (void)getRadioCommon;
    (void)getRadioName;
    (void)getRadioType;

    theInstance_ = this;
    acc_id = PJSUA_INVALID_ID;

    m_Register = false;
    m_AutoCall = false;
    m_SendPackets = false;
    m_PttPressed = false;
    m_RegTimeout = 60;
    m_isTimerRunning = false;
    m_NullSlot = PJ_INVALID_SOCKET;
    SLOT_VOLUME = 1.0f;

    udpActive = true;
    tcpActive = false;

    m_EnableRecording = false;

    sampleRate = DEFAULT_SAMPLE_RATE;
    sampleCount = 0;
    isSipCall = false;

    fftSize = DEFAULT_FFT_SIZE;

    memset(&log_buffer[0], 0, sizeof(log_buffer));
    memset(&plot_buffer[0], 0, sizeof(plot_buffer));

    configurate();
    initLabels();

    StartSip();   

    setAudioDevInfo();
    setCodecPriority();

    //listAudioDevInfo();
    //initSlaveSoundCard();
    //searchAudioDevice();

    initializePttManager(PttDevice);

    if(m_EnableRecording)
    {
        if(createNullPort())
        AppentTextBrowser("Null Port Created Successfully");
    }

    delete_oldrecords(getexepath());

    m_UdpCommunicator =  new UDPCommunicator(m_ioservice,localAddress.c_str() , localPort.c_str(), remoteAddress.c_str(), remotePort.c_str());
    m_TcpCommunicator =  new TCPCommunicator(m_ioservice,localAddress.c_str() , localPort.c_str(), remoteAddress.c_str(), remotePort.c_str());
    m_WavWriter       =  new WavWriter();

    snprintf(log_buffer, sizeof(log_buffer),"\nUdp Settings:\nlocalAddress: %s\nlocalPort: %s\nremoteAddress: %s\nremotePort: %s\n"
             ,localAddress.c_str(),localPort.c_str(),remoteAddress.c_str(),remotePort.c_str());

    AppentTextBrowser(log_buffer);

    SnmpStack::getInstance()->init();

    initSpectrumGraph();

    QObject::connect(this, SIGNAL(spectValueChanged(int)),this, SLOT(onSpectrumProcessed(int)));
    QObject::connect(this, SIGNAL(spectClear()),this, SLOT(onSpectrumClear()));
    QObject::connect(this, SIGNAL(stateRegChanged(bool)),this, SLOT(onRegStateChanged(bool)));
    //QObject::connect(this, SIGNAL(stateCallChanged(pjsua_call_id,pjsip_event)),this, SLOT(callStateChanged(pjsua_call_id,pjsip_event)));


    getRadioFrequency(true);
    m_IsEventThreadRunning = true;
    m_EventThreadInstance.reset(new boost::thread(boost::bind(&SoftPhone::checkEventsThread, this)));
    registerThread();

    listConfs();
}

SoftPhone::~SoftPhone()
{

    fftw_destroy_plan(planFft);
    fftw_free(in);
    fftw_free(out);

    delete [] d_realFftData;
    delete [] d_iirFftData;
    delete ui;
}

SoftPhone *SoftPhone::instance()
{
    return theInstance_;
}

void SoftPhone::AppentTextBrowser(const char* stringBuffer)
{
    ui->textBrowser_terminal->append(QString::fromUtf8(Utility::nowStr().c_str()) + " --> " + QString::fromUtf8(stringBuffer));
    ui->textBrowser_terminal->moveCursor( QTextCursor::End, QTextCursor::MoveAnchor );
}

void SoftPhone::StartSip()
{
    pj_status_t status;

    sip_port = rand() % 10 + 10000;
    ui->lbl_port->setText("Port: " + QString::number(sip_port));

    //pjsua_acc_id acc_id;

    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_create()", status);

    /* Init pjsua */
    {
    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config media_cfg;
    pjsua_transport_id tp_id;

    pjsua_config_default(&cfg);
    cfg.cb.on_incoming_call = &::on_incoming_call;
    cfg.cb.on_call_media_state = &::on_call_media_state;
    cfg.cb.on_call_state = &::on_call_state;
    cfg.cb.on_reg_state = &::on_reg_state;
    cfg.cb.on_create_media_transport= &::on_create_media_transport;
    //cfg.cb.on_stream_created = &::on_stream_created;
    //cfg.cb.on_stream_destroyed = &::on_stream_destroyed;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = PJSUA_LOG_LEVEL;

    pjsua_media_config_default(&media_cfg);

    media_cfg.no_vad = 1;
    media_cfg.enable_ice = 0;
    media_cfg.snd_auto_close_time = 1;
    media_cfg.clock_rate = clock_rate;
    media_cfg.snd_clock_rate = clock_rate;

    media_cfg.ec_tail_len = 0; // Disable echo cancellation

    printf("Media_cfg:\n");
    printf("%d %d %d %d %d %d\n", media_cfg.no_vad,media_cfg.enable_ice,media_cfg.snd_auto_close_time,media_cfg.clock_rate,media_cfg.snd_clock_rate,media_cfg.ec_tail_len);    

    /* Add UDP transport. */
    {
    pjsua_transport_config cfg;

    pjsua_transport_config_default(&cfg);    
    cfg.port = sip_port;
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &tp_id);
    if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
    }

   /* pj_status_t status = pjsua_acc_add_local(tp_id, true, &acc_id);
    if (status != PJ_SUCCESS)
    {
        printf("pjsua_acc_add_local Failed\n");
    }*/

    RegisterDefault();

    status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);
    }

    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) error_exit("Error starting pjsua", status);
    else if (status == PJ_SUCCESS)
    {
        init_ringTone();
    }
}

void SoftPhone::RegisterDefault()
{
    pj_status_t status;

    std::string SipIp = Utility::getIpAddress();
    std::string SipUser = std::string("default");

    char sipuri[50];
    char sipuser[50];
    //getlogin_r(sipuser, sizeof(sipuser));

    pj_ansi_snprintf(sipuri, sizeof(sipuri), "<sip:%s@%s>",SipUser.c_str(),SipIp.c_str());
    pjsua_acc_config cfg;
    pjsua_acc_config_default(&cfg);
    cfg.id = pj_str(sipuri);

    status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error adding account", status);
    }
    status = pjsua_acc_set_default(acc_id);
    if(status == PJ_SUCCESS)
    {
        sprintf(log_buffer, "Number of Accounts: %d Current Account: %d is default.\n",pjsua_acc_get_count(),acc_id);
        AppentTextBrowser(log_buffer);
    }   
}

void SoftPhone::UpdateDefault()
{
    pj_status_t status;

    if(pjsua_acc_is_valid(acc_id))
    {
        status = pjsua_acc_set_default(acc_id);
        if(status == PJ_SUCCESS)
        {
            sprintf(log_buffer, "Number of Accounts: %d Current Account: %d is default.\n",pjsua_acc_get_count(),acc_id);
            AppentTextBrowser(log_buffer);
        }
    }
}

void SoftPhone::RegisterUser()
{
    pj_status_t status;
    pjsua_acc_config cfg;

    sip_domainstr = ui->lineEdit_sipserver->text();
    char sipServer[sip_domainstr.length()+1];
    strcpy( sipServer, sip_domainstr.toStdString().c_str());

    QString sip_usernamestr = ui->lineEdit_sipusername->text();
    char sipUser[sip_usernamestr.length()+1];
    strcpy( sipUser, sip_usernamestr.toStdString().c_str());

    QString sip_passwordstr = ui->lineEdit_sippassword->text();
    char sipPassword[sip_passwordstr.length()+1];
    strcpy( sipPassword, sip_passwordstr.toStdString().c_str());

    char sipId[MAX_SIP_ID_LENGTH];
    sprintf(sipId, "<sip:%s@%s>", sipUser, sipServer);

    char regUri[MAX_SIP_REG_URI_LENGTH];
    sprintf(regUri, "<sip:%s>", sipServer);

    pjsua_acc_config_default(&cfg);

    cfg.id = pj_str(sipId);
    cfg.reg_uri = pj_str(regUri);
    cfg.cred_count = 1;
    cfg.reg_timeout = m_RegTimeout;
    cfg.cred_info[0].realm = pj_str((char *)"*");
    cfg.cred_info[0].scheme = pj_str((char *)"digest");
    cfg.cred_info[0].username = pj_str((char *)sipUser);
    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cfg.cred_info[0].data = pj_str((char *)sipPassword);

    status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error adding account", status);
    }

    status = pjsua_acc_set_default(acc_id);
    if(status == PJ_SUCCESS)
    {
         sprintf(log_buffer, "Number of Accounts: %d Current Account: %d is asterisk user.\n",pjsua_acc_get_count(),acc_id);
         AppentTextBrowser(log_buffer);
    }
}

void SoftPhone::UnRegisterUser()
{
    if(removeUser())
    {
        UpdateDefault();
        ui->pushButton_register->setText("Register");
        AppentTextBrowser("Not Registered...");
    }
}

bool SoftPhone::removeDefault()
{
    bool result = false;
    pj_status_t status;

    if(pjsua_acc_is_valid(acc_id))
    {
        if(current_call != PJSUA_INVALID_ID)
        {
             pjsua_call_hangup(current_call, PJSIP_SC_BUSY_HERE, NULL, NULL);
        }

        status = pjsua_acc_set_registration(acc_id, PJ_FALSE);
        if (status == PJ_SUCCESS)
        {
            status = pjsua_acc_del(acc_id);
            if (status != PJ_SUCCESS)
            {
               pjsua_perror(THIS_FILE, "Error removing default account", status);
            }
            else
            {
                acc_id = PJSUA_INVALID_ID;
                result = true;
            }
        }
    }
    return result;
}

bool SoftPhone::removeUser()
{
    bool result = false;
    pj_status_t status;

    if(pjsua_acc_is_valid(acc_id))
    {
        if(current_call != PJSUA_INVALID_ID)
        {
             pjsua_call_hangup(current_call, PJSIP_SC_BUSY_HERE, NULL, NULL);
        }

        status = pjsua_acc_set_registration(acc_id, PJ_FALSE);
        if (status == PJ_SUCCESS)
        {
            status = pjsua_acc_del(acc_id);
            if (status != PJ_SUCCESS)
            {
               pjsua_perror(THIS_FILE, "Error removing asterisk account", status);
            }
            else
            {
                acc_id = PJSUA_INVALID_ID;
                result = true;
            }
        }
    }
    return result;
}

void SoftPhone::searchAudioDevice()
{
  void **hints;
  const char *ifaces[] = {"pcm", 0};
  int index = 0;
  void **str;
  char *name;
  char *desc;
  int devIdx = 0;
  size_t tPos;

  snd_config_update();

  while (ifaces[index]) {

    printf("Querying interface %s \n", ifaces[index]);
    if (snd_device_name_hint(-1, ifaces[index], &hints) < 0)
    {
      printf("Querying devices failed for %s.\n", ifaces[index]);
      index++;
      continue;
    }
    str = hints;
    while (*str)
    {
      name = snd_device_name_get_hint(*str, "NAME");
      desc = snd_device_name_get_hint(*str, "DESC");

      string tNameStr = "";
      if (name != NULL)
          tNameStr = string(desc);

      printf("\n");

      // search for "default:", if negative result then go on searching for next device
      if ((tNameStr != "") && ((tPos = tNameStr.find("USB")) != string::npos))
      {
          printf("Deafult Sound Card : %d : %s\n%s\n\n",devIdx, name,desc);
          //snd_device_name_free_hint(hints);

         // return;
      }

      free(name);
      free(desc);
      devIdx++;
      str++;
    }
    index++;
    snd_device_name_free_hint(hints);
  }
  return;
}

/*
   +-----------+ stereo +-----------------+ 2x mono +-----------+
   | AUDIO DEV |<------>| SPLITCOMB   left|<------->|#0  BRIDGE |
   +-----------+        |            right|<------->|#1         |
                        +-----------------+         +-----------+
 */

bool SoftPhone::initSlaveSoundCard()
{
    pj_status_t status;

        int samples_per_frame = clock_rate * 20 / 1000;
        int bits_per_sample = 16;
        int channel_count = 6;
        int snd_dev_id = 6;

        pool = pjsua_pool_create("pjsua-app", 1000, 1000);

        // Disable existing sound device
        conf_port = pjsua_set_no_snd_dev();

        // Create stereo-mono splitter/combiner
        status = pjmedia_splitcomb_create(pool,
                          clock_rate, // clock rate ,
                          channel_count,
                          channel_count * samples_per_frame,
                          bits_per_sample,
                          0,    // options ,
                          &sc);
        pj_assert(status == PJ_SUCCESS);

        // Connect channel0 to conference port slot0
        status = pjmedia_splitcomb_set_channel(
                                sc,0, // ch0 ,
                                0, //options,
                                conf_port);
        pj_assert(status == PJ_SUCCESS);

        ///////////////////////////////////////////////////////

        // Create reverse channel for channel1
        status = pjmedia_splitcomb_create_rev_channel(pool,
                              sc,
                              1, // ch1 ,
                              0 , // options ,
                              &sc_ch1);
        pj_assert(status == PJ_SUCCESS);

        status = pjsua_conf_add_port(pool, sc_ch1,
                     &sc_ch1_slot);
        pj_assert(status == PJ_SUCCESS);


        // Create reverse channel for channel2
        status = pjmedia_splitcomb_create_rev_channel(pool,
                              sc,
                              2, // ch2 ,
                              0 , // options ,
                              &sc_ch2);
        pj_assert(status == PJ_SUCCESS);

        status = pjsua_conf_add_port(pool, sc_ch2,
                     &sc_ch2_slot);
        pj_assert(status == PJ_SUCCESS);

        // Create reverse channel for channel3
        status = pjmedia_splitcomb_create_rev_channel(pool,
                              sc,
                              3, // ch3 ,
                              0 , // options ,
                              &sc_ch3);
        pj_assert(status == PJ_SUCCESS);

        status = pjsua_conf_add_port(pool, sc_ch3,
                     &sc_ch3_slot);
        pj_assert(status == PJ_SUCCESS);

        // Create reverse channel for channel4
        status = pjmedia_splitcomb_create_rev_channel(pool,
                              sc,
                              4, // ch4 ,
                              0 , // options ,
                              &sc_ch4);
        pj_assert(status == PJ_SUCCESS);

        status = pjsua_conf_add_port(pool, sc_ch4,
                     &sc_ch4_slot);
        pj_assert(status == PJ_SUCCESS);

        // Create reverse channel for channel5
        status = pjmedia_splitcomb_create_rev_channel(pool,
                              sc,
                              5, // ch5 ,
                              0 , // options ,
                              &sc_ch5);
        pj_assert(status == PJ_SUCCESS);

        status = pjsua_conf_add_port(pool, sc_ch5,
                     &sc_ch5_slot);
        pj_assert(status == PJ_SUCCESS);


        ////////////////////////////////////////////////////////

       /* status = pjmedia_snd_port_create(pool, dev_id,dev_id,
                         clock_rate,
                         channel_count,
                         channel_count * samples_per_frame,
                         bits_per_sample,
                         0, &sndPlayback);
        pj_assert(status == PJ_SUCCESS);

        status = pjmedia_snd_port_connect(sndPlayback, sc);
        pj_assert(status == PJ_SUCCESS);*/

        // Create sound playback device
        status = pjmedia_snd_port_create_player(pool, snd_dev_id,
                         clock_rate,
                         channel_count,
                         channel_count * samples_per_frame,
                         bits_per_sample,
                         0, &sndPlayback);
        pj_assert(status == PJ_SUCCESS);

        status = pjmedia_snd_port_connect(sndPlayback, sc);
        pj_assert(status == PJ_SUCCESS);

    return true;
}

void SoftPhone::listAudioDevInfo()
{
    printf("\nSoftPhone::setAudioDevInfo() begins\n");
    unsigned devCount = pjmedia_aud_dev_count();

    for (unsigned i = 0; i < devCount; i++)
    {
        pjmedia_aud_dev_info info;
        pj_status_t status = pjmedia_aud_dev_get_info(i, &info);

        if (status == PJ_SUCCESS)
        {
           m_AudioDevInfoVector.push_back(info);
           sprintf(log_buffer, "Card Num: %d - Card Name: %s %dHz",i,info.name,info.default_samples_per_sec);
           AppentTextBrowser(log_buffer);
           printf("%s\n",log_buffer);
        }
    }
    printf("\n");
}

void SoftPhone::setAudioDevInfo()
{
    printf("\nSoftPhone::setAudioDevInfo() begins\n");
    unsigned devCount = pjmedia_aud_dev_count();
    char *name;
    size_t tPos;
    bool defaultfound = false;

    for (unsigned i = 0; i < devCount; i++)
    {
        pjmedia_aud_dev_info info;
        pj_status_t status = pjmedia_aud_dev_get_info(i, &info);

        if (status == PJ_SUCCESS)
        {
           m_AudioDevInfoVector.push_back(info);
           sprintf(log_buffer, "Card Num: %d - Card Name: %s %dHz",i,info.name,info.default_samples_per_sec);
           AppentTextBrowser(log_buffer);
           printf("%s\n",log_buffer);

           string tNameStr = "";
           name = info.name;
           if (name != NULL)
               tNameStr = string(name);

           // search for "default:", if negative result then go on searching for next device
           if (((tPos = tNameStr.find("USB")) != string::npos))
           {
               initSoundCard(i);
               ui->lineEdit_mastersoundcard->setText(QString::number(i));
               defaultfound = true;
           }
        }
    }
    printf("\n");

    if(!defaultfound)
          initSoundCard(0);
}

void SoftPhone::setCodecPriority()
{

    pjsua_codec_info c[32];
    unsigned i, count = PJ_ARRAY_SIZE(c);
    pj_str_t codec_s;

    printf("List of audio codecs:\n");

    pjsua_enum_codecs(c, &count);
    for (i = 0; i < count; ++i)
    {
        pjsua_codec_set_priority(pj_cstr(&codec_s, c[i].codec_id.ptr), PJMEDIA_CODEC_PRIO_DISABLED);
    }

    pjsua_codec_set_priority(pj_cstr(&codec_s, "PCMU/8000/1"), PJMEDIA_CODEC_PRIO_HIGHEST);
    pjsua_codec_set_priority(pj_cstr(&codec_s, "PCMA/8000/1"), PJMEDIA_CODEC_PRIO_NEXT_HIGHER);

    pjsua_enum_codecs(c, &count);
    for (i = 0; i < count; ++i)
    {
        printf("%d\t%.*s\n", c[i].priority, (int) c[i].codec_id.slen, c[i].codec_id.ptr);
    }

}

void SoftPhone::initSoundCard(int cardNumber)
{
    pj_status_t status = pjsua_set_snd_dev(cardNumber, cardNumber);
    if (status != PJ_SUCCESS)
    {
        printf("Error setting sound card device cardNumber: %d\n",cardNumber);
    }
    else
    {
       //connectPort(0,m_NullSlot);

        std::string cardName;
        getSoundCardName(cardNumber,cardName);
        printf("Card %s Set As Master SUCCESS.\n", cardName.c_str());
        sprintf(log_buffer, "Card: %s Set As Master",cardName.c_str());
        AppentTextBrowser(log_buffer);

        ui->lbl_soundcardname->setText(" " + QString::fromUtf8(cardName.c_str()) + " ");
    }
}

bool SoftPhone::checkSipUrl(std::string extNumber)
{
    bool valid = false;
    size_t pos1 = extNumber.find_first_of(":");
    size_t pos2 = extNumber.find_first_of("@", pos1);

    if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1 +1)
    {
        valid = true;        
    }

    return valid;
}

int SoftPhone::makeSipCall(std::string const& extNumber)
{
    registerThread();
    isSipCall = true;

    if (!checkSipUrl(extNumber.c_str()))
    {
        printf("Invalid Url: %s\n", extNumber.c_str());
        return -1;
    }

    pjsua_call_id callId = PJSUA_INVALID_ID;

    enableEd137 = PJ_FALSE;

    pj_status_t status;

    pjsua_acc_id accId = pjsua_acc_get_default();

    std::string strUrl = extNumber.c_str();
    pj_str_t url;
    pj_cstr(&url, strUrl.c_str());

    printf("Making Sip Call Ext: %s\n", strUrl.c_str());

    status = pjsua_call_make_call(accId, &url, NULL, NULL, NULL, &callId);

    if (status != PJ_SUCCESS)
    {
        stopRing();
        printf("Error on Making Call Ext: %s\n", extNumber.c_str());
        ui->pushButton_call->setText("Call");
        return PJSUA_INVALID_ID;
    }

    return callId;
}

int SoftPhone::makeCall(std::string const& extNumber)
{
    registerThread();
    isSipCall = false;
    printf("Making Asterisk Call Ext: %s\n", extNumber.c_str());

    pjsua_call_id callId = PJSUA_INVALID_ID;

    enableEd137 = PJ_FALSE;

    pj_status_t status;

    pjsua_acc_id accId = pjsua_acc_get_default();

    std::string strUrl = getSipUrl(extNumber.c_str());
    pj_str_t url;
    pj_cstr(&url, strUrl.c_str());

    status = pjsua_call_make_call(accId, &url, NULL, NULL, NULL, &callId);

    if (status != PJ_SUCCESS)
    {
        stopRing();
        printf("Error on Making Call Ext: %s\n", extNumber.c_str());
        ui->pushButton_call->setText("Call");
        return PJSUA_INVALID_ID;
    }


    return callId;
}

int SoftPhone::makeCall_Radio(std::string const& extNumber)
{
    registerThread();

    pjsua_call_id callId = PJSUA_INVALID_ID;

    enableEd137 = PJ_TRUE;

    pjsua_msg_data msg_data;
    pjsua_msg_data_init(&msg_data);

    // Header 1
    pj_str_t hname = pj_str((char*)"Subject");
    pj_str_t hvalue = pj_str((char*)"radio");
    pjsip_generic_string_hdr my_hdr1;
    pjsip_generic_string_hdr_init2(&my_hdr1, &hname, &hvalue);
    pj_list_push_back(&msg_data.hdr_list, &my_hdr1);

    // Header 2
    hname = pj_str((char*)"WG67-Version");
    hvalue = pj_str((char*)"radio.01");
    pjsip_generic_string_hdr my_hdr2;
    pjsip_generic_string_hdr_init2(&my_hdr2, &hname, &hvalue);
    pj_list_push_back(&msg_data.hdr_list, &my_hdr2);

    // Heade 3
    hname = pj_str((char*)"Priority");
    hvalue = pj_str((char*)"normal");
    pjsip_generic_string_hdr my_hdr3;
    pjsip_generic_string_hdr_init2(&my_hdr3, &hname, &hvalue);
    pj_list_push_back(&msg_data.hdr_list, &my_hdr3);


    pj_status_t status;

    pjsua_acc_id accId = pjsua_acc_get_default();

    std::string strUrl = extNumber.c_str();
    pj_str_t url;
    pj_cstr(&url, strUrl.c_str());

    status = pjsua_call_make_call(accId, &url, NULL, NULL, &msg_data, &callId);

    if (status != PJ_SUCCESS)
    {
        printf("Error on Making Radio Call Ext: %s\n", extNumber.c_str());
        return PJSUA_INVALID_ID;
    }

    return callId;
}


/////////////callbacks///////////////////

static void on_reg_state(pjsua_acc_id acc_id)
{
    SoftPhone::instance()->on_reg_state(acc_id);
}

void SoftPhone::on_reg_state(pjsua_acc_id acc_id)
{

    pjsua_acc_info ci;

    pj_status_t status = pjsua_acc_get_info(acc_id, &ci);
    if (status != PJ_SUCCESS)
    {   
     return;
    }

    if (ci.expires > 0)
    {
        m_Register = true;
    }
    else if(ci.expires < 0)
    {
        m_Register = false;
    }

    emit stateRegChanged(m_Register);
}
void SoftPhone::onRegStateChanged(bool regState)
{
    if(regState)
    {
        ui->pushButton_register->setText("Unregister");
        sprintf(log_buffer, "Registration success,  will re-register in %d seconds",m_RegTimeout);
        AppentTextBrowser(log_buffer);
    }
    else
    {
        ui->pushButton_register->setText("Register");
        AppentTextBrowser("Not Registered...");
    }
}

void SoftPhone::updateButtons(std::string extNumber,QString action)
{

    if (extNumber.find("sip:rx") != std::string::npos)
    {
        ui->pushButton_Rx->setText("Rx " + action);
    }
    else if (extNumber.find("sip:tx") != std::string::npos)
    {
        ui->pushButton_Tx->setText("Tx " + action);
    }
    else if(extNumber.find("sip:rx") == std::string::npos && extNumber.find("sip:tx") == std::string::npos)
    {
        ui->pushButton_call->setText(action);
    }
}

void SoftPhone::printCallState(int state,std::string extNumber)
{
    switch (state)
    {
        case PJSIP_INV_STATE_EARLY:
            snprintf(log_buffer, sizeof(log_buffer),"Call state Early: %s\n",extNumber.c_str());
            AppentTextBrowser(log_buffer);
            break;
        case PJSIP_INV_STATE_CALLING:
            snprintf(log_buffer, sizeof(log_buffer),"Call state Calling: %s\n",extNumber.c_str());
            AppentTextBrowser(log_buffer);
            break;
        case PJSIP_INV_STATE_INCOMING:
            snprintf(log_buffer, sizeof(log_buffer),"Call state Incoming: %s\n",extNumber.c_str());
            AppentTextBrowser(log_buffer);
            break;
        case PJSIP_INV_STATE_CONNECTING:
            snprintf(log_buffer, sizeof(log_buffer),"Call state Connecting: %s\n",extNumber.c_str());
            AppentTextBrowser(log_buffer);
            break;
        case PJSIP_INV_STATE_CONFIRMED:
            snprintf(log_buffer, sizeof(log_buffer),"Call state Confirmed: %s\n",extNumber.c_str());
            AppentTextBrowser(log_buffer);
            break;
        case PJSIP_INV_STATE_DISCONNECTED:
            snprintf(log_buffer, sizeof(log_buffer),"Call state Disconnected: %s\n",extNumber.c_str());
            AppentTextBrowser(log_buffer);
            break;
    default:
        break;
    }
}

void SoftPhone::callStateChanged(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info call_info;
    pjsua_call_get_info(call_id, &call_info);

    std::string extNumber;
    int clientPort = getConfPortNumber(call_id);

    if(getExtFromCallID(call_id,extNumber))
    {
       printCallState(call_info.state,extNumber);
    }
    else
        return;

    switch (call_info.state)
    {
        case PJSIP_INV_STATE_EARLY:
            if (e->type == PJSIP_EVENT_TSX_STATE)
            {
                pjsip_msg *msg;

                if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
                {
                    msg = e->body.tsx_state.src.rdata->msg_info.msg;
                }
                else {
                    msg = e->body.tsx_state.src.tdata->msg;
                }

                int code = msg->line.status.code;

                // Start ringback for 180 for UAC unless there's SDP in 180
                if (call_info.role == PJSIP_ROLE_UAC && code == 180  && msg->body == NULL && call_info.media_status == PJSUA_CALL_MEDIA_NONE)
                {
                    playRing();
                }
            }

            break;
        case PJSIP_INV_STATE_CALLING:
            break;
        case PJSIP_INV_STATE_INCOMING:
            playRing();
            break;
        case PJSIP_INV_STATE_CONNECTING:
            break;
        case PJSIP_INV_STATE_CONFIRMED:
            stopRing();
            updateButtons(extNumber,"End");

            pjsua_conf_port_info portInfo;
            getCallPortInfo(extNumber, portInfo);

            sampleRate = portInfo.clock_rate;

            if(sampleRate != PJSUA_INVALID_ID)
            {
                ui->Plotter->setSampleRate(sampleRate/2);
                ui->Plotter->setSpanFreq((quint32)sampleRate/2);
                ui->Plotter->setCenterFreq(sampleRate/4);
                ui->Plotter->setFftCenterFreq(0);
                ui->Plotter->setFftRate(sampleRate/fftSize);
                ui->Plotter->setFftRange(-140.0f, 20.0f);
                snprintf(log_buffer, sizeof(log_buffer),
                "\nCall Activated for Ext#: %s\nClock Rate: %d\nChannel Count: %d\nSamples Per Frame: %d\nBits Per Sample: %d\n",
                extNumber.c_str(),portInfo.clock_rate,portInfo.channel_count,portInfo.samples_per_frame,portInfo.bits_per_sample);
                AppentTextBrowser(log_buffer);
            }

            if (m_EnableRecording)
            {
                m_WavWriter->start(extNumber,sampleRate);
                connectPort(clientPort,m_NullSlot);
            }

            break;
        case PJSIP_INV_STATE_DISCONNECTED:
            stopRing();
            updateButtons(extNumber,"Call");

            if (extNumber.find("sip:rx") != std::string::npos)
            {
                current_call_rx = PJSUA_INVALID_ID;
                if(m_AutoCall)
                {
                    QString calluri = ui->lineEdit_sipurirx->text();
                    char sip_urichr[calluri.length()+1];
                    strcpy( sip_urichr, calluri.toStdString().c_str());

                    current_call_rx = makeCall_Radio(sip_urichr);
                }
            }
            else if (extNumber.find("sip:tx") != std::string::npos)
            {
                current_call_tx = PJSUA_INVALID_ID;
                if(m_AutoCall)
                {
                    QString calluri = ui->lineEdit_sipuritx->text();
                    char sip_urichr[calluri.length()+1];
                    strcpy( sip_urichr, calluri.toStdString().c_str());

                    current_call_tx = makeCall_Radio(sip_urichr);
                }
            }
            else if(extNumber.find("sip:rx") == std::string::npos && extNumber.find("sip:tx") == std::string::npos)
            {
                current_call = PJSUA_INVALID_ID;
            }

            {
                    std::map<int,pjmedia_transport*>::iterator iter = transport_map.find(call_id) ;
                    if( iter != transport_map.end())
                    {
                        transport_map.erase( iter );
                    }
                    if(m_WavWriter->isRunning())
                    {
                         m_WavWriter->stop();
                    }
            }

            break;
    default:
        break;
    }

}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    SoftPhone::instance()->on_call_state(call_id, e);
}

void SoftPhone::on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    callStateChanged(call_id,e);
}

////////////////////////////////////////

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    SoftPhone::instance()->on_incoming_call(acc_id, call_id, rdata);
}

////* Callback called by the library upon receiving incoming call */
void SoftPhone::on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);
    pjsua_call_info call_info;
    pjsua_call_get_info(call_id, &call_info);

    current_call = call_id;


    // Response Ringing = 180
    pj_status_t status = pjsua_call_answer(call_id, 180, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        printf("Error in status ringing\n");
    }

    snprintf(log_buffer, sizeof(log_buffer), "Incoming Call: %s",call_info.remote_info.ptr);
    AppentTextBrowser(log_buffer);
    ui->pushButton_call->setText("Answer");

}

//////////////////////////////////////////

static void on_call_media_state(pjsua_call_id call_id)
{
    SoftPhone::instance()->on_call_media_state(call_id);
}

/* Callback called by the library when call's media state has changed */
void SoftPhone::on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;
    unsigned mi;
    pj_bool_t has_error = PJ_FALSE;

    pjsua_call_get_info(call_id, &call_info);

    for (mi=0; mi<call_info.media_cnt; ++mi) {

    switch (call_info.media[mi].type) {
    case PJMEDIA_TYPE_AUDIO:
       on_call_audio_state(&call_info, mi, &has_error);

       break;
    default:
       break;
    }
    }

    if (has_error) {
    pj_str_t reason = pj_str((char*)("Media failed"));
    pjsua_call_hangup(call_id, 500, &reason, NULL);
    }
}

void SoftPhone::on_call_audio_state(pjsua_call_info *ci, unsigned mi,pj_bool_t *has_error)
{
    PJ_UNUSED_ARG(has_error);

    if (ci->media[mi].status == PJSUA_CALL_MEDIA_ACTIVE ||
    ci->media[mi].status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
    {
        pjsua_conf_port_id call_conf_slot;
        call_conf_slot = ci->media[mi].stream.aud.conf_slot;

       // pjsua_conf_connect(call_conf_slot, 0);
       // pjsua_conf_connect(0,call_conf_slot);


        std::string strRemoteInfo;
        strRemoteInfo.append(ci->remote_info.ptr, ci->remote_info.slen);

        if (strRemoteInfo.find("radio") != std::string::npos)
        {
             pjsua_conf_connect(call_conf_slot, sc_ch1_slot);
             pjsua_conf_connect(sc_ch1_slot,call_conf_slot);

             pjsua_conf_connect(call_conf_slot, sc_ch2_slot);
             pjsua_conf_connect(sc_ch2_slot,call_conf_slot);

             pjsua_conf_connect(call_conf_slot, sc_ch3_slot);
             pjsua_conf_connect(sc_ch3_slot,call_conf_slot);

             pjsua_conf_connect(call_conf_slot, sc_ch4_slot);
             pjsua_conf_connect(sc_ch4_slot,call_conf_slot);

             pjsua_conf_connect(call_conf_slot, sc_ch5_slot);
             pjsua_conf_connect(sc_ch5_slot,call_conf_slot);
        }
        else
        {
             pjsua_conf_connect(call_conf_slot, 0);
             pjsua_conf_connect(0,call_conf_slot);
        }
        listConfs();
    }
}

pjmedia_transport* on_create_media_transport(pjsua_call_id call_id, unsigned media_idx,pjmedia_transport *base_tp,unsigned flags)
{
    pjmedia_transport *adapter;
    adapter = SoftPhone::instance()->on_create_media_transport(call_id,media_idx,base_tp,flags);
    return adapter;
}

pjmedia_transport* SoftPhone::on_create_media_transport(pjsua_call_id call_id, unsigned media_idx,
        pjmedia_transport *base_tp, unsigned flags)
{
    registerThread();

    pjmedia_transport *adapter;
    pj_status_t status;
    PJ_UNUSED_ARG(media_idx);
        /* Create the adapter */

        status = pjmedia_custom_tp_adapter_create(pjsua_get_pjmedia_endpt(),
                                           NULL,
                                           base_tp,
                                           (flags & PJSUA_MED_TP_CLOSE_MEMBER),
                                           enableEd137,
                                           "Radio-RxTx",
                                           call_id,
                                           &adapter);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1,(NULL, status, "Error creating adapter\n"));
            return NULL;
        }

        if(adapter!=NULL)
        {
            transport_map.insert(std::make_pair(call_id ,adapter));
        }

    enableEd137 = PJ_FALSE;
    return adapter;
}

void on_stream_created(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx,pjmedia_port **p_port)
{
   SoftPhone::instance()->on_stream_created(call_id,strm,stream_idx,p_port);
}

void SoftPhone::on_stream_created(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx,pjmedia_port **p_port)
{
    (void) call_id;
    (void) strm;
    (void) stream_idx;
    (void) p_port;
}

void on_stream_destroyed(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx)
{
   SoftPhone::instance()->on_stream_destroyed(call_id,strm,stream_idx);
}

void SoftPhone::on_stream_destroyed(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx)
{
    (void) call_id;
    (void) strm;
    (void) stream_idx;
}


/////////////////////////////////////////

/////////PTT MANAGER CALLBACKS//////////////

void SoftPhone::pttPressed(PttEvent const& event)
{
    (void)event;
    AppentTextBrowser("PttPressed...");
    setRadioPttbyCallID(true,current_call_tx,1);
    ui->checkBox_pttset->setChecked(true);
    fflush(stdout);
}

void SoftPhone::pttReleased(PttEvent const& event)
{
    (void)event;
    AppentTextBrowser("PttReleased...");
    setRadioPttbyCallID(false,current_call_tx,1);
    ui->checkBox_pttset->setChecked(false);
    fflush(stdout);
}

void SoftPhone::onHook(PttEvent const& event)
{
    (void)event;
}

void SoftPhone::offHook(PttEvent const& event)
{
    (void)event;
}

///////////////////////

void SoftPhone::getRadioFrequency(bool setEdit)
{
    if(validateIpAddress(RxIp))
    {
        unsigned long rxval64 = SnmpStack::getInstance()->getsnmp_int(RxIp,getFrequency);
        QString rxfrequency = QString::number(rxval64);
        ui->lcdNumber_RxFreq->display(rxfrequency);
        if(setEdit)
        ui->lineEdit_rxsetfreq->setText(rxfrequency);
    }

    if(validateIpAddress(TxIp))
    {
        unsigned long txval64 = SnmpStack::getInstance()->getsnmp_int(TxIp,getFrequency);
        QString txfrequency = QString::number(txval64);
        ui->lcdNumber_TxFreq->display(txfrequency);
        if(setEdit)
        ui->lineEdit_txsetfreq->setText(txfrequency);
    }
}

bool SoftPhone::setSlotVolume(int callId, bool increase, bool current)
{
        pjsua_call_info ci;
        pj_status_t status = pjsua_call_get_info(callId, &ci);

        if (status != PJ_SUCCESS)
        {
            return false;
        }

        if(!current)
        {
            if(increase)
            {
                SLOT_VOLUME  += 0.1f;
                if (SLOT_VOLUME >= MAX_SLOT_VOLUME)
                    SLOT_VOLUME = MAX_SLOT_VOLUME;
            }
            else
            {
                SLOT_VOLUME -= 0.1f;
                if (SLOT_VOLUME <= MIN_SLOT_VOLUME)
                    SLOT_VOLUME =  MIN_SLOT_VOLUME;
            }

        }

        QString vollevel = QString::number(SLOT_VOLUME);
        ui->lcdNumber_Volume->display(vollevel);

        if (ci.conf_slot != PJSUA_INVALID_ID)
        {
            status = pjsua_conf_adjust_rx_level(ci.conf_slot, SLOT_VOLUME);

            if (status != PJ_SUCCESS)
            {
                return false;
            }
        }
        else {
            return false;
        }

    return true;
}

void SoftPhone::checkEventsThread()
{
    registerThread();
    int count = 0;

    while (m_IsEventThreadRunning)
    {
        boost::unique_lock<boost::mutex> scoped_lock(io_mutex);

        ui->lcdNumber_time->display(QString::fromUtf8(Utility::getTime().c_str()));

        if (current_call != PJSUA_INVALID_ID)
        {
            setSlotVolume(current_call,false,true);

        }

        if (current_call_rx != PJSUA_INVALID_ID)
        {
            int bssrx=(int)get_IPRadioBss(current_call_rx);
            ui->lcdNumber_RxBss->display(bssrx);
            setSlotVolume(current_call_rx,false,true);
        }

        if (current_call_tx != PJSUA_INVALID_ID)
        {
            setSlotVolume(current_call_tx,false,true);

            if(get_IPRadioPttStatus(current_call_tx) && !m_PttPressed)
            {
                ui->lbl_ptt_status->setText("Ptt ON");
                AppentTextBrowser("Ptt Status ON");
                m_PttPressed = true;
            }
            else if(!get_IPRadioPttStatus(current_call_tx) && m_PttPressed)
            {
                ui->lbl_ptt_status->setText("Ptt OFF");
                AppentTextBrowser("Ptt Status OFF");
                m_PttPressed = false;
            }

        }

        if(count > 10)
        {
            getRadioFrequency(false);
            count = 0;
        }
        count++;

        ::usleep(SLEEP_PERIOD * SLEEP_COUNTER);
    }
}

///////Spectrum/////////

void SoftPhone::initSpectrumGraph()
{   
    d_realFftData = (float*) malloc(DEFAULT_FFT_SIZE * sizeof(float));
    d_iirFftData  = (float*) malloc(DEFAULT_FFT_SIZE * sizeof(float));

    for (int i = 0; i < DEFAULT_FFT_SIZE; i++)
        d_iirFftData[i] = -70.0f;  // dBFS

    for (int i = 0; i < DEFAULT_FFT_SIZE; i++)
        d_realFftData[i] = -70.0f;

    ui->Plotter->setTooltipsEnabled(true);
    ui->Plotter->setSampleRate(sampleRate);
    ui->Plotter->setSpanFreq((quint32)sampleRate);
    ui->Plotter->setCenterFreq(sampleRate/2);
    ui->Plotter->setFftCenterFreq(0);
    ui->Plotter->setFftRate(sampleRate/fftSize);
    ui->Plotter->setFftRange(-140.0f, 20.0f);

    ui->Plotter->setFreqUnits(1000);
    ui->Plotter->setPercent2DScreen(50);
    ui->Plotter->setFreqDigits(1);
    ui->Plotter->setFilterBoxEnabled(false);
    ui->Plotter->setCenterLineEnabled(false);
    ui->Plotter->setBookmarksEnabled(false);
    ui->Plotter->setVdivDelta(40);
    ui->Plotter->setHdivDelta(40);
    ui->Plotter->setFftPlotColor(Qt::green);
    ui->Plotter->setFftFill(true);

    d_fftAvg = 1.0 - 1.0e-2 * ((float)75);
}

void SoftPhone::onSpectrumClear()
{
}

void SoftPhone::onSpectrumProcessed(int fftSize)
{    
    ui->Plotter->setNewFttData(d_iirFftData, d_realFftData, fftSize/2);
}

void SoftPhone::runFFTW(float *buffer, int fftsize)
{
    if(fftsize > 0)
    {
        float pwr, lpwr;
        float pwr_scale = 1.0 / ((float)fftsize * (float)fftsize);
        int i;

        planFft = fftw_plan_dft_1d(fftsize, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

        for (i=0; i < fftsize; i++)
        {
            in[i][0] = (float)buffer[i] / 127.5f - 1.f;
            in[i][1] = (float)-buffer[i+1] / 127.5f - 1.f;
        }

        fftw_execute(planFft);

        for (i = 0; i < fftsize; ++i)
        {
            pwr  = sqrt(out[i][REAL] * out[i][REAL] + out[i][IMAG] * out[i][IMAG]);
            lpwr = 20.f * log10f(pwr_scale * pwr);

            if(d_realFftData[i] < lpwr) d_realFftData[i] = lpwr; else d_realFftData[i] -= (d_realFftData[i] - lpwr) / 5.f;
            d_iirFftData[i] += d_fftAvg * (d_realFftData[i] - d_iirFftData[i]);
        }
        emit spectValueChanged(fftsize);
    }
}

void SoftPhone::setOutgoingRTP(tp_adapter* adapter)
{
    (void) adapter;
}

//////////button events//////////
void SoftPhone::on_pushButton_register_clicked()
{
    if(ui->pushButton_register->text() == "Register")
    {
            RegisterUser();
    }else
    {
            UnRegisterUser();
    }

}
void SoftPhone::on_pushButton_exit_clicked()
{
    app_exit();
}

void SoftPhone::on_pushButton_Rx_clicked()
{
    if (ui->pushButton_Rx->text() == "Rx Call")
    {      
        QString calluri = ui->lineEdit_sipurirx->text();
        char sip_urichr[calluri.length()+1];
        strcpy( sip_urichr, calluri.toStdString().c_str());

        current_call_rx = makeCall_Radio(sip_urichr);
        if(current_call_rx == PJSUA_INVALID_ID)
        {
            ui->pushButton_Rx->setText("Rx Call");
             stopRing();
        }
    }
    else
    {
        stopRing();
        if(current_call_rx != PJSUA_INVALID_ID)
        {
            pj_status_t status = pjsua_call_hangup(current_call_rx, PJSIP_SC_BUSY_HERE, NULL, NULL);
            if (status == PJ_SUCCESS) {

            }         
        }
     }
}

void SoftPhone::on_pushButton_Tx_clicked()
{
    if (ui->pushButton_Tx->text() == "Tx Call")
    {
        QString calluri = ui->lineEdit_sipuritx->text();
        char sip_urichr[calluri.length()+1];
        strcpy( sip_urichr, calluri.toStdString().c_str());

        current_call_tx = makeCall_Radio(sip_urichr);
        if(current_call_tx == PJSUA_INVALID_ID)
        {
            ui->pushButton_Tx->setText("Tx Call");
             stopRing();
        }
    }
    else
    {
        stopRing();
        if(current_call_tx != PJSUA_INVALID_ID)
        {
            pj_status_t status = pjsua_call_hangup(current_call_tx, PJSIP_SC_BUSY_HERE, NULL, NULL);
            if (status == PJ_SUCCESS) {
            }
        }
    }
}

void SoftPhone::on_pushButton_call_clicked()
{

    if(ui->pushButton_call->text()=="Answer")
    {
       if(current_call!= PJSUA_INVALID_ID){
           pjsua_call_answer(current_call, 200, NULL, NULL);
           ui->pushButton_call->setText("End");
       }

    }else if(ui->pushButton_call->text()=="Call")
    {
       QString calluri = ui->lineEdit_sipcall->text();
       char sip_urichr[calluri.length()+1];
       strcpy( sip_urichr, calluri.toStdString().c_str());
       if(calluri.contains("sip:"))
       {
           current_call = makeSipCall(sip_urichr);
       }
       else
       {
           current_call = makeCall(sip_urichr);
       }
    }
    else
    {
      if(current_call!= PJSUA_INVALID_ID){
           pj_status_t status = pjsua_call_hangup(current_call, PJSIP_SC_BUSY_HERE, NULL, NULL);
           if (status == PJ_SUCCESS) {
           }
       }
    }
}

void SoftPhone::on_pushButton_Sq_clicked()
{
    if(validateIpAddress(RxIp))
    SnmpStack::getInstance()->setsnmp_string(RxIp,setRadioCommon,"0200");
}

void SoftPhone::on_pushButton_setFreq_clicked()
{
    QString setfrequency = "Ip adress is not valid.";

    if(validateIpAddress(RxIp))
    {
        setfrequency = ui->lineEdit_rxsetfreq->text();
        SnmpStack::getInstance()->setsnmp_int(RxIp,setFrequency,setfrequency.toStdString().c_str());
    }

    if(validateIpAddress(TxIp))
    {
        setfrequency = ui->lineEdit_txsetfreq->text();
        SnmpStack::getInstance()->setsnmp_int(TxIp,setFrequency,setfrequency.toStdString().c_str());
    }

    sprintf(log_buffer,"setfrequency: %s",setfrequency.toStdString().c_str());
    AppentTextBrowser(log_buffer);
}

void SoftPhone::on_pushButton_Vp_clicked()
{
    if (current_call != PJSUA_INVALID_ID)
    {
        setSlotVolume(current_call,true,false);
    }
    if (current_call_rx != PJSUA_INVALID_ID)
    {
        setSlotVolume(current_call_rx,true,false);
    }
    if (current_call_tx != PJSUA_INVALID_ID)
    {
        setSlotVolume(current_call_tx,true,false);
    }
}

void SoftPhone::on_pushButton_Vm_clicked()
{
    if (current_call != PJSUA_INVALID_ID)
    {
        setSlotVolume(current_call,false,false);
    }
    if (current_call_rx != PJSUA_INVALID_ID)
    {
        setSlotVolume(current_call_rx,false,false);
    }
    if (current_call_tx != PJSUA_INVALID_ID)
    {
        setSlotVolume(current_call_tx,false,false);
    }
}

void SoftPhone::on_pushButton_mastersoundcard_clicked()
{
    int cardNumber = ui->lineEdit_mastersoundcard->text().toInt();
    initSoundCard(cardNumber);
}

void SoftPhone::on_pushButton_sendPacket_clicked()
{
    QString sendUdp,sendTcp;

    if(tcpActive && m_TcpCommunicator)
    {
        sendTcp = ui->lineEdit_udp->text() + "\n";
        m_TcpCommunicator->write(sendTcp.toStdString().c_str());
    }

    if(udpActive && m_UdpCommunicator)
    {
        sendUdp = ui->lineEdit_udp->text() + "\n";
        m_UdpCommunicator->write(sendUdp.toStdString().c_str(),0);
    }
}

void SoftPhone::on_checkBox_udp_clicked(bool checked)
{
    if(checked)
    {
        udpActive = true;
        tcpActive = false;
        ui->checkBox_tcp->setChecked(false);
    }
    else
    {
         ui->checkBox_udp->setChecked(true);
    }

}

void SoftPhone::on_checkBox_tcp_clicked(bool checked)
{
    if(checked)
    {
        udpActive = false;
        tcpActive = true;
        ui->checkBox_udp->setChecked(false);
    }
    else
    {
         ui->checkBox_tcp->setChecked(true);
    }
}

void SoftPhone::on_checkBox_pttset_clicked(bool checked)
{
    setRadioPttbyCallID(checked,current_call_tx,1);
}

void SoftPhone::on_checkBox_Auto_clicked(bool checked)
{
    m_AutoCall = checked;
}

void SoftPhone::on_checkBox_showPackets_clicked(bool checked)
{
    m_SendPackets = checked;
}


void SoftPhone::initLabels()
{
    QString const& pingURL = QString("xset -dpms");
    int status = system(pingURL.toStdString().c_str());
    if (-1 != status)
    {
        printf("Monitor auto turn off, Disabled.\n");
    }

    ui->lineEdit_sipserver->setStyleSheet("color: white;background-color: #333;");
    ui->lineEdit_sipusername->setStyleSheet("color: white;background-color: #333;");
    ui->lineEdit_sippassword->setStyleSheet("color: white;background-color: #333;");
    ui->lineEdit_sipuritx->setStyleSheet("font: 14px;color: yellow;background-color: #333;");
    ui->lineEdit_sipurirx->setStyleSheet("font: 14px;color: yellow;background-color: #333;");
    ui->lineEdit_sipcall->setStyleSheet("color: white;background-color:  #333;");
    ui->lineEdit_mastersoundcard->setStyleSheet("color: white;background-color:  #333;");
    ui->lineEdit_udp->setStyleSheet("color: white;background-color: #333;");
    ui->pushButton_Tx->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_Rx->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_exit->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_register->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_setFreq->setStyleSheet("color: white;background-color: CornflowerBlue;");
    ui->pushButton_call->setStyleSheet("color: white;background-color: CornflowerBlue;");
    ui->pushButton_Sq->setStyleSheet("color: white;background-color: CornflowerBlue;");
    ui->pushButton_Vp->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_Vm->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_mastersoundcard->setStyleSheet("color: white;background-color: CadetBlue;");
    ui->pushButton_sendPacket->setStyleSheet("color: white;background-color: CornflowerBlue;");

    ui->textBrowser_terminal->setStyleSheet("font: 12px; color: #00cccc; background-color: #001a1a;");

    ui->lbl_port->setStyleSheet("color: white;background-color:  #25383C;");
    ui->lbl_port->setAlignment(Qt::AlignCenter);
    ui->lbl_soundcardname->setStyleSheet("font: bold 13px;color: white;background-color:  #25383C;");
    ui->lbl_soundcardname->setAlignment(Qt::AlignCenter);
    ui->lbl_soundcard_title->setStyleSheet("font: bold 13px;color: white;background-color:  #25383C;");
    ui->lbl_soundcard_title->setAlignment(Qt::AlignCenter);

    ui->lcdNumber_RxFreq->setStyleSheet("color: white;background-color:  #0B3861;");
    ui->lcdNumber_TxFreq->setStyleSheet("color: white;background-color:  #0B3861;");
    ui->lcdNumber_Volume->setStyleSheet("color: white;background-color:  #0B3861;");

    ui->lcdNumber_RxBss->setStyleSheet("color: white;background-color:  #0B3861;");
    ui->lcdNumber_RxBss->setDigitCount(2);
    ui->lcdNumber_RxBss->setSegmentStyle(QLCDNumber::Flat);

    ui->lcdNumber_time->setStyleSheet("color: white;background-color:  #0B3861;");
    ui->lcdNumber_time->setDigitCount(8);
    ui->lcdNumber_time->setSegmentStyle(QLCDNumber::Flat);

    ui->lineEdit_sipserver->setText(QString::fromUtf8(SipServer.c_str()));
    ui->lineEdit_sipusername->setText(QString::fromUtf8(SipUser.c_str()));
    ui->lineEdit_sippassword->setText(QString::fromUtf8(SipPassword.c_str()));
    ui->lineEdit_sipcall->setText(QString::fromUtf8(SipCall.c_str()));
    ui->lineEdit_sipurirx->setText("<sip:rxradio@" + QString::fromUtf8(RxIp.c_str()) + ">");
    ui->lineEdit_sipuritx->setText("<sip:txradio@" + QString::fromUtf8(TxIp.c_str()) + ">");


    ui->lcdNumber_RxFreq->setDigitCount(6);
    ui->lcdNumber_TxFreq->setDigitCount(6);
    ui->lcdNumber_Volume->setDigitCount(3);
    ui->lcdNumber_RxBss->setDigitCount(2);

    ui->lcdNumber_RxFreq->display(0);
    ui->lcdNumber_TxFreq->display(0);
    ui->lcdNumber_time->display(QString::fromUtf8(Utility::getTime().c_str()));

    sip_domainstr = ui->lineEdit_sipserver->text();
}


void SoftPhone::resizeEvent(QResizeEvent *event)
{
    ui->Plotter->setMinimumHeight(event->size().height() * 1/3);
    ui->textBrowser_terminal->setMinimumWidth(event->size().width() * 2/7);
}

void SoftPhone::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (isMinimized() || !isVisible())
        {
            event->ignore();
            return;
        }
        if(isVisible()) {

            event->ignore();
            return;
        }
    }
}

void SoftPhone::changeUplinkOrder(const char* Input, char* Output, size_t sizeOfCoded, int g726UplinkBitrate) {


    if (g726UplinkBitrate == 1) //16kbps (2 bit)
    {
        for(unsigned int k = 0; k < sizeOfCoded ; k++)  // TO-DO
        {
            unsigned char temp = (Input[k] << 6) | (Input[k] >> 6) | ((Input[k] & 0x30) >> 2) | ((Input[k] & 0x0C) << 2);
            Output[k] = temp;
        }
    }
    else if (g726UplinkBitrate == 2) //24kbps (3 bit)
    {
        #pragma pack(push)
        #pragma pack(1)
            struct my_samples {
                unsigned char S3:2;
                unsigned char S2:3;
                unsigned char S1:3;
                unsigned char S6:1;
                unsigned char S5:3;
                unsigned char S4:3;
                unsigned char S3_:1;
                unsigned char S8:3;
                unsigned char S7:3;
                unsigned char S6_:2;
            };
            typedef struct my_samples my_samples_t;

            union data {
                unsigned char tempBuff[3];
                unsigned int v;
            };
        #pragma pack(pop)

        union data d;
        static my_samples_t samples;

        d.v = 0;
        int offset = 0;

        int numberOfBytes = sizeOfCoded;
        while(offset < (numberOfBytes))
        {
            memcpy (d.tempBuff, &Input[offset] , 3);

            unsigned int  V = d.v;

            samples.S1 = V & 0x00000007;
            samples.S2 = (V >> 3) & 0x00000007;
            samples.S3 = (V >> 7) & 0x00000003;
            samples.S3_ = (V >> 6) & 0x00000001;
            samples.S4 = (V >> 9) & 0x00000007;
            samples.S5 = (V >> 12) & 0x00000007;
            samples.S6 = (V >> 17) & 0x00000001;
            samples.S6_ = (V >> 15) & 0x00000003;
            samples.S7 = (V >> 18) & 0x00000007;
            samples.S8 = (V >> 21) & 0x00000007;

            memcpy (&Output[offset], &samples , 3);
            offset +=3;
        }
    }
    else if (g726UplinkBitrate == 3) //32kbps (4 bit)
    {
        for(unsigned int k = 0; k < sizeOfCoded ; k++)  // TO-DO
        {
            unsigned char temp = (Input[k]>>4) | (Input[k]<<4);
            Output[k] = temp;
        }
    }
    else if (g726UplinkBitrate == 4) //40kbps (5 bit)
    {
        #pragma pack(push)
        #pragma pack(1)
        struct my_samples {
        unsigned char S2:3;
        unsigned char S1:5;
        unsigned char S4:1;
        unsigned char S3:5;
        unsigned char S2_:2;
        unsigned char S5:4;
        unsigned char S4_:4;
        unsigned char S7:2;
        unsigned char S6:5;
        unsigned char S5_:1;
        unsigned char S8:5;
        unsigned char S7_:3;
        };
        typedef struct my_samples my_samples_t;

        unsigned char tempBuff[5];
        #pragma pack(pop)

        static my_samples_t samples;

        int offset = 0;

        int numberOfBytes = sizeOfCoded;
        while(offset < (numberOfBytes))
        {
            memcpy (tempBuff, &Input[offset] , 5);

            samples.S1 = tempBuff[0] & 0x1F;
            samples.S2 = ((tempBuff[1] << 1) | (tempBuff[0] >> 7)) & 0x07;
            samples.S2_ = (tempBuff[0] >> 5) & 0x04;
            samples.S3 = (tempBuff[1] >> 2) & 0x1F;
            samples.S4 = (tempBuff[2] >> 3) & 0x01;
            samples.S4_ = ((tempBuff[2] << 1) | (tempBuff[1] >> 7)) & 0x0F;
            samples.S5 = ((tempBuff[3] << 3) | (tempBuff[2] >> 5)) & 0x0F;
            samples.S5_ = (tempBuff[2] >> 4) & 0x01;
            samples.S6 = (tempBuff[3] >> 1) & 0x1F;
            samples.S7 = (tempBuff[4] >> 1) & 0x03;
            samples.S7_ = ((tempBuff[4] << 2) | (tempBuff[3] >> 6)) & 0x07;
            samples.S8 = tempBuff[4] >> 3;

            memcpy (&Output[offset], &samples, 5);
            offset +=5;
        }
    }
}

void SoftPhone::setIncomingRTP(tp_adapter* adapter)
{
    (void) adapter;

    if(isSipCall)
        return;

    unsigned int pktlen    = adapter->payload_bufSize;
    const char* payloadbuf = (char*)adapter->payload_buff;
    const char* pktbuf     = (char*)adapter->pkt_buff;
    int payloadlen         = adapter->payload_bufSize;
    char sendPacket[payloadlen];

    if(pktlen > 0)
     {
        for (int i = 0; i < payloadlen; i++)
        {
            if(sampleCount < DEFAULT_FFT_SIZE)
            {
                signalInput[sampleCount] = payloadbuf[i];
                sampleCount ++;
            }
            else
            {
                runFFTW(signalInput,DEFAULT_FFT_SIZE);
                sampleCount = 0;
            }
            sendPacket[i] =  payloadbuf[i];
        }

        if(udpActive && m_UdpCommunicator && m_SendPackets)
        {
             m_UdpCommunicator->write(sendPacket,payloadlen);
        }

        recordRtp(pktbuf,payloadbuf,pktlen,payloadlen);
     }
}
