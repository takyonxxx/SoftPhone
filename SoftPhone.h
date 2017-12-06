#ifndef SOFTPHONE_H
#define SOFTPHONE_H
#include <QMainWindow>
#include <QGridLayout>
#include <QMessageBox>
#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QVBoxLayout>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QWindowStateChangeEvent>
#include <QMessageBox>
#include <QTimer>
#include <QElapsedTimer>

#include <pjsua-lib/pjsua.h>

#include <fftw3.h>
#include <Utility.h>

#include "SnmpStack.h"
#include "TransportAdapter.h"
#include "FileLoaderInterface.h"
#include "FileFactory.h"
#include "PttManager.h"
#include "UDPCommunicator.h"
#include "TCPCommunicator.h"
#include "WavWriter.h"
#include <dirent.h>
#include <string.h>

#include <alsa/asoundlib.h>

#define CLOCK_RATE 8000
#define SAMPLES_PER_FRAME (CLOCK_RATE/100)
#define PTIME		    20
#define PJSUA_LOG_LEVEL 3
#define DEFAULT_SAMPLE_RATE		8000
#define DEFAULT_FFT_SIZE        1024

static const unsigned int SLEEP_PERIOD = 1000;
static const unsigned int SLEEP_COUNTER= 500;

static const size_t MAX_SIP_ID_LENGTH = 50;
static const size_t MAX_SIP_REG_URI_LENGTH = 50;

static const float MIN_SLOT_VOLUME  = 0.0;
static const float MAX_SLOT_VOLUME  = 2.0;

namespace Ui {
class SoftPhone;
}

class SoftPhone : public QMainWindow, public IPttListener
{
    Q_OBJECT

public:
    explicit SoftPhone(QWidget *parent = 0);
    ~SoftPhone();

    static SoftPhone *instance();

    void on_reg_state(pjsua_acc_id acc_id);
    void on_call_state(pjsua_call_id call_id, pjsip_event *e);
    void on_call_audio_state(pjsua_call_info *ci, unsigned mi,pj_bool_t *has_error);
    void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata);
    void on_call_media_state(pjsua_call_id call_id);
    pjmedia_transport* on_create_media_transport(pjsua_call_id call_id, unsigned media_idx,pjmedia_transport *base_tp,unsigned flags);
    void on_stream_created(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx,pjmedia_port **p_port);
    void on_stream_destroyed(pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx);

    void setIncomingRTP(tp_adapter* adapter);
    void setOutgoingRTP(tp_adapter* adapter);
    void runFFTW(float *buffer, int fftsize);

    void registerThread();

private:

    boost::asio::io_service m_ioservice;
    boost::atomic_bool m_IsEventThreadRunning;
    boost::shared_ptr<boost::thread> m_EventThreadInstance;
    boost::shared_ptr<PttManager> m_PttManager;
    boost::mutex io_mutex;

    UDPCommunicator *m_UdpCommunicator;
    TCPCommunicator *m_TcpCommunicator;
    WavWriter       *m_WavWriter;
    int clock_rate;

private:
    void error_exit(const char *title, pj_status_t status);
    void AppentTextBrowser(const char* stringBuffer);
    void initLabels();
    void configurate();
    void StartSip();
    void initSoundCard(int cardNumber);
    void setAudioDevInfo();
    void listAudioDevInfo();
    void searchAudioDevice();
    bool initSlaveSoundCard();
    void setCodecPriority();
    void initializePttManager(const std::string pttDeviceName);

    void playRing();
    void stopRing();
    void init_ringTone();

    bool createNullPort();
    static pj_status_t PutData(struct pjmedia_port *this_port, pjmedia_frame *frame);
    static pj_status_t GetData(struct pjmedia_port *this_port, pjmedia_frame *frame);

    int getConfPortNumber(int callId);
    bool getExtFromCallID(int CallID, std::string& extNumber) const;
    bool checkSipUrl(std::string extNumber);
    bool getExtFromRemoteInfo(pj_str_t const& remoteInfo, std::string& extNumber) const;
    void getSoundCardName(int number, std::string& cardName);
    void getCallPortInfo(std::string ext, pjsua_conf_port_info& returnInfo);
    void listConfs();
    int getActiveCallCount();
    void getSoundCardPorts(std::vector<int>& portList) const;
    void connectCallToSoundCard(int CallID);
    void disConnectCallFromSoundCard(int CallID);
    bool connectPort(int srcPort, int dstPort);
    bool disconnectPort(int srcPort, int dstPort);

    void changeUplinkOrder(const char* Input, char* Output, size_t sizeOfCoded, int g726UplinkBitrate);

    int makeCall(std::string const& extNumber);
    int makeSipCall(std::string const& extNumber);
    int makeCall_Radio(std::string const& extNumber);

    std::string getSipUrl(const char* ext);
    void delete_oldrecords(std::string folderName);
    bool setSlotVolume(int callId, bool increase, bool current);

    //---------------------------------------------------------------------------
    // IPttListener implementations
    //---------------------------------------------------------------------------

    virtual void pttPressed(PttEvent const& event);
    virtual void pttReleased(PttEvent const& event);
    virtual void onHook(PttEvent const& event);
    virtual void offHook(PttEvent const& event);

    bool findSoundCard(std::string const& soundDevice);

    void checkEventsThread();

    /////IP RADIO////
    void setRadioPttbyCallID(bool pttval,int callId,int priority);
    int get_IPRadioBss(pjsua_call_id callId);
    int get_IPRadioPttStatus(pjsua_call_id callId);
    int get_IPRadioPttId(pjsua_call_id callId);
    int get_IPRadioSquelch(pjsua_call_id callId);
    bool get_IPRadioStatus(pjsua_call_id callId);
    void getRadioFrequency(bool setEdit);
    bool isRadio(std::string extNumber);

    ///sip////
    void RegisterUser();
    void UnRegisterUser();

    void RegisterDefault();
    void UpdateDefault();
    bool removeDefault();

    bool removeUser();

    void app_exit();

    void recordRtp(const char *pktbuf,const char *payloadbuf,unsigned int pktlen,unsigned int payloadlen);
    void printCallState(int state,std::string extNumber);

    ////SPECTRUM///
    void initSpectrumGraph();
    //void FFT(int n, bool inverse, const double *gRe, const double *gIm, double *GRe, double *GIm);

    void updateButtons(std::string extNumber, QString action);

    std::string getexepath()
    {
      std::string path ="\0";
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)) != NULL)
            return std::string(cwd);
      return path;
    }

private:

    pjmedia_port *ring_port_, *in_ring_port_;
    pjsua_conf_port_id ring_slot_, in_ring_slot_;
    pj_bool_t enableEd137 = PJ_FALSE;

    pjsua_call_id current_call = PJSUA_INVALID_ID;
    pjsua_call_id current_call_tx = PJSUA_INVALID_ID;
    pjsua_call_id current_call_rx = PJSUA_INVALID_ID;

    pjmedia_port* m_NullPort;
    int m_NullSlot;

    char log_buffer[250];
    char plot_buffer[250];

    int m_RegTimeout;
    int sip_port;

    std::string SipServer;
    std::string SipUser;
    std::string SipPassword;
    std::string SipCall;
    std::string RxIp;
    std::string TxIp;
    std::string PttDevice;
    std::string localAddress;
    std::string localPort;
    std::string remoteAddress;
    std::string remotePort;
    QString sip_domainstr;

    pjsua_acc_id acc_id;

    bool m_Register;
    bool m_AutoCall;
    bool m_SendPackets;
    bool m_PttPressed;
    bool m_isTimerRunning;
    bool m_EnableRecording;
    bool udpActive;
    bool tcpActive;
    bool isSipCall;

    std::map<int,pjmedia_transport*> transport_map;
    std::vector<pjmedia_aud_dev_info> m_AudioDevInfoVector;

    unsigned int sampleRate;
    unsigned int sampleCount;
    unsigned int fftSize;

    float SLOT_VOLUME;

    float   *d_realFftData;
    float   *d_iirFftData;    
    float signalInput[DEFAULT_FFT_SIZE];
    float d_fftAvg;      /*!< FFT averaging parameter set by user (not the true gain). */


signals:
    void spectValueChanged(int fftSize);
    void spectClear();
    void stateRegChanged(bool regState);
    void stateCallChanged(pjsua_call_id call_id, pjsip_event *e);

private slots:

    void on_pushButton_exit_clicked();
    void on_pushButton_register_clicked();
    void on_pushButton_Rx_clicked();
    void on_pushButton_Tx_clicked();
    void on_pushButton_call_clicked();
    void on_pushButton_Sq_clicked();
    void on_pushButton_setFreq_clicked();
    void on_checkBox_pttset_clicked(bool checked);
    void on_checkBox_Auto_clicked(bool checked);
    void on_pushButton_Vp_clicked();
    void on_pushButton_Vm_clicked();
    void on_pushButton_mastersoundcard_clicked();
    void on_checkBox_showPackets_clicked(bool checked);
    void on_pushButton_sendPacket_clicked();
    void on_checkBox_udp_clicked(bool checked);
    void on_checkBox_tcp_clicked(bool checked);
    void onSpectrumProcessed(int fftSize);
    void onSpectrumClear();
    void onRegStateChanged(bool regState);
    void changeEvent(QEvent *event);
    void callStateChanged(pjsua_call_id call_id, pjsip_event *e);

protected:
    void resizeEvent(QResizeEvent* event);

private:
    Ui::SoftPhone *ui;
    static SoftPhone *theInstance_;
};

#endif // SOFTPHONE_H
