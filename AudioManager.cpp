#include "AudioManager.h"

#define THIS_FILE	"AudioManager.cpp"
#define MAX_DEVICES	64
#define WAV_FILE	"AudioTest.wav"


AudioManager* AudioManager::ins = NULL;
static unsigned dev_count;
static unsigned playback_lat = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
static unsigned capture_lat = PJMEDIA_SND_DEFAULT_REC_LATENCY;


namespace
{
    static bool initialized = false;
}


AudioManager::AudioManager()
{

}

AudioManager::~AudioManager()
{

}

AudioManager* AudioManager::getInstance()
{
    if (ins == NULL)
    {
        ins = new AudioManager();
    }
    return ins;
}

bool AudioManager::init()
{
    if (!initialized)
    {
        list_devices();
        initialized = true;
        return initialized;
    }

    return false;
}

static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    printf( "%s: %s (err=%d)\n",
        title, errmsg, status);
}

static const char *decode_caps(unsigned caps)
{
    static char text[200];
    unsigned i;

    text[0] = '\0';

    for (i=0; i<31; ++i) {
    if ((1 << i) & caps) {
        const char *capname;
        capname = pjmedia_aud_dev_cap_name((pjmedia_aud_dev_cap)(1 << i),
                           NULL);
        strcat(text, capname);
        strcat(text, " ");
    }
    }

    return text;
}

static pj_status_t wav_rec_cb(void *user_data, pjmedia_frame *frame)
{
    return pjmedia_port_put_frame((pjmedia_port*)user_data, frame);
}

static pj_status_t wav_play_cb(void *user_data, pjmedia_frame *frame)
{
    return pjmedia_port_get_frame((pjmedia_port*)user_data, frame);
}

void AudioManager::list_devices(void)
{
    unsigned i;
    pj_status_t status;

    dev_count = pjmedia_aud_dev_count();
    if (dev_count == 0) {
    PJ_LOG(3,(THIS_FILE, "No devices found"));
    return;
    }

    PJ_LOG(3,(THIS_FILE, "Found %d devices:", dev_count));

    for (i=0; i<dev_count; ++i) {
    pjmedia_aud_dev_info info;

    status = pjmedia_aud_dev_get_info(i, &info);
    if (status != PJ_SUCCESS)
        continue;

    PJ_LOG(3,(THIS_FILE," %2d: %s [%s] (%d/%d) %dHz",
              i, info.driver, info.name, info.input_count, info.output_count,info.default_samples_per_sec));
    }
}

void AudioManager::show_dev_info(unsigned index)
{
#define H   "%-20s"
    pjmedia_aud_dev_info info;
    char formats[200];
    pj_status_t status;

    if (index >= dev_count) {
    PJ_LOG(1,(THIS_FILE, "Error: invalid index %u", index));
    return;
    }

    status = pjmedia_aud_dev_get_info(index, &info);
    if (status != PJ_SUCCESS) {
    app_perror("pjmedia_aud_dev_get_info() error", status);
    return;
    }

    PJ_LOG(3, (THIS_FILE, "Device at index %u:", index));
    PJ_LOG(3, (THIS_FILE, "-------------------------"));

    PJ_LOG(3, (THIS_FILE, H": %u (0x%x)", "ID", index, index));
    PJ_LOG(3, (THIS_FILE, H": %s", "Name", info.name));
    PJ_LOG(3, (THIS_FILE, H": %s", "Driver", info.driver));
    PJ_LOG(3, (THIS_FILE, H": %u", "Input channels", info.input_count));
    PJ_LOG(3, (THIS_FILE, H": %u", "Output channels", info.output_count));
    PJ_LOG(3, (THIS_FILE, H": %s", "Capabilities", decode_caps(info.caps)));

    formats[0] = '\0';
    if (info.caps & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT) {
    unsigned i;

    for (i=0; i<info.ext_fmt_cnt; ++i) {
        char bitrate[32];

        switch (info.ext_fmt[i].id) {
        case PJMEDIA_FORMAT_L16:
        strcat(formats, "L16/");
        break;
        case PJMEDIA_FORMAT_PCMA:
        strcat(formats, "PCMA/");
        break;
        case PJMEDIA_FORMAT_PCMU:
        strcat(formats, "PCMU/");
        break;
        case PJMEDIA_FORMAT_AMR:
        strcat(formats, "AMR/");
        break;
        case PJMEDIA_FORMAT_G729:
        strcat(formats, "G729/");
        break;
        case PJMEDIA_FORMAT_ILBC:
        strcat(formats, "ILBC/");
        break;
        default:
        strcat(formats, "unknown/");
        break;
        }
        sprintf(bitrate, "%u", info.ext_fmt[i].det.aud.avg_bps);
        strcat(formats, bitrate);
        strcat(formats, " ");
    }
    }
    PJ_LOG(3, (THIS_FILE, H": %s", "Extended formats", formats));

#undef H
}

void AudioManager::test_device(pjmedia_dir dir, unsigned rec_id, unsigned play_id,
            unsigned clock_rate, unsigned ptime,
            unsigned chnum)
{
    pjmedia_aud_param param;
    pjmedia_aud_test_results result;
    pj_status_t status;

    if (dir & PJMEDIA_DIR_CAPTURE) {
    status = pjmedia_aud_dev_default_param(rec_id, &param);
    } else {
    status = pjmedia_aud_dev_default_param(play_id, &param);
    }

    if (status != PJ_SUCCESS) {
    app_perror("pjmedia_aud_dev_default_param()", status);
    return;
    }

    param.dir = dir;
    param.rec_id = rec_id;
    param.play_id = play_id;
    param.clock_rate = clock_rate;
    param.channel_count = chnum;
    param.samples_per_frame = clock_rate * chnum * ptime / 1000;

    /* Latency settings */
    param.flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
            PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param.input_latency_ms = capture_lat;
    param.output_latency_ms = playback_lat;

    PJ_LOG(3,(THIS_FILE, "Performing test.."));

    status = pjmedia_aud_test(&param, &result);
    if (status != PJ_SUCCESS) {
    app_perror("Test has completed with error", status);
    return;
    }

    PJ_LOG(3,(THIS_FILE, "Done. Result:"));

    if (dir & PJMEDIA_DIR_CAPTURE) {
    if (result.rec.frame_cnt==0) {
        PJ_LOG(1,(THIS_FILE, "Error: no frames captured!"));
    } else {
        PJ_LOG(3,(THIS_FILE, "  %-20s: interval (min/max/avg/dev)=%u/%u/%u/%u, burst=%u",
              "Recording result",
              result.rec.min_interval,
              result.rec.max_interval,
              result.rec.avg_interval,
              result.rec.dev_interval,
              result.rec.max_burst));
    }
    }

    if (dir & PJMEDIA_DIR_PLAYBACK) {
    if (result.play.frame_cnt==0) {
        PJ_LOG(1,(THIS_FILE, "Error: no playback!"));
    } else {
        PJ_LOG(3,(THIS_FILE, "  %-20s: interval (min/max/avg/dev)=%u/%u/%u/%u, burst=%u",
              "Playback result",
              result.play.min_interval,
              result.play.max_interval,
              result.play.avg_interval,
              result.play.dev_interval,
              result.play.max_burst));
    }
    }

    if (dir==PJMEDIA_DIR_CAPTURE_PLAYBACK) {
    if (result.rec_drift_per_sec == 0) {
        PJ_LOG(3,(THIS_FILE, " No clock drift detected"));
    } else {
        const char *which = result.rec_drift_per_sec>=0 ? "faster" : "slower";
        unsigned drift = result.rec_drift_per_sec>=0 ?
                result.rec_drift_per_sec :
                -result.rec_drift_per_sec;

        PJ_LOG(3,(THIS_FILE, " Clock drifts detected. Capture device "
                 "is running %d samples per second %s "
                 "than the playback device",
                 drift, which));
    }
    }
}

void AudioManager::record(unsigned rec_index, const char *filename)
{
    pj_pool_t *pool = NULL;
    pjmedia_port *wav = NULL;
    pjmedia_aud_param param;
    pjmedia_aud_stream *strm = NULL;
    char line[10], *dummy;
    pj_status_t status;

    if (filename == NULL)
    filename = WAV_FILE;

    pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(), "wav",
              1000, 1000, NULL);

    status = pjmedia_wav_writer_port_create(pool, filename, 16000,
                        1, 320, 16, 0, 0, &wav);
    if (status != PJ_SUCCESS) {
    app_perror("Error creating WAV file", status);
    goto on_return;
    }

    status = pjmedia_aud_dev_default_param(rec_index, &param);
    if (status != PJ_SUCCESS) {
    app_perror("pjmedia_aud_dev_default_param()", status);
    goto on_return;
    }

    param.dir = PJMEDIA_DIR_CAPTURE;
    param.clock_rate = PJMEDIA_PIA_SRATE(&wav->info);
    param.samples_per_frame = PJMEDIA_PIA_SPF(&wav->info);
    param.channel_count = PJMEDIA_PIA_CCNT(&wav->info);
    param.bits_per_sample = PJMEDIA_PIA_BITS(&wav->info);

    status = pjmedia_aud_stream_create(&param, &wav_rec_cb, NULL, wav,&strm);
    if (status != PJ_SUCCESS) {
    app_perror("Error opening the sound device", status);
    goto on_return;
    }

    status = pjmedia_aud_stream_start(strm);
    if (status != PJ_SUCCESS) {
    app_perror("Error starting the sound device", status);
    goto on_return;
    }

    PJ_LOG(3,(THIS_FILE, "Recording started, press ENTER to stop"));
    dummy = fgets(line, sizeof(line), stdin);
    PJ_UNUSED_ARG(dummy);
on_return:
    if (strm) {
    pjmedia_aud_stream_stop(strm);
    pjmedia_aud_stream_destroy(strm);
    }
    if (wav)
    pjmedia_port_destroy(wav);
    if (pool)
    pj_pool_release(pool);
}

void AudioManager::play_file(unsigned play_index, const char *filename)
{
    pj_pool_t *pool = NULL;
    pjmedia_port *wav = NULL;
    pjmedia_aud_param param;
    pjmedia_aud_stream *strm = NULL;
    char line[10], *dummy;
    pj_status_t status;

    if (filename == NULL)
    filename = WAV_FILE;

    pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(), "wav",
              1000, 1000, NULL);

    status = pjmedia_wav_player_port_create(pool, filename, 20, 0, 0, &wav);
    if (status != PJ_SUCCESS) {
    app_perror("Error opening WAV file", status);
    goto on_return;
    }

    status = pjmedia_aud_dev_default_param(play_index, &param);
    if (status != PJ_SUCCESS) {
    app_perror("pjmedia_aud_dev_default_param()", status);
    goto on_return;
    }

    param.dir = PJMEDIA_DIR_PLAYBACK;
    param.clock_rate = PJMEDIA_PIA_SRATE(&wav->info);
    param.samples_per_frame = PJMEDIA_PIA_SPF(&wav->info);
    param.channel_count = PJMEDIA_PIA_CCNT(&wav->info);
    param.bits_per_sample = PJMEDIA_PIA_BITS(&wav->info);

    status = pjmedia_aud_stream_create(&param, NULL, &wav_play_cb, wav,
                       &strm);
    if (status != PJ_SUCCESS) {
    app_perror("Error opening the sound device", status);
    goto on_return;
    }

    status = pjmedia_aud_stream_start(strm);
    if (status != PJ_SUCCESS) {
    app_perror("Error starting the sound device", status);
    goto on_return;
    }

    PJ_LOG(3,(THIS_FILE, "Playback started, press ENTER to stop"));
    dummy = fgets(line, sizeof(line), stdin);
    PJ_UNUSED_ARG(dummy);

on_return:
    if (strm) {
    pjmedia_aud_stream_stop(strm);
    pjmedia_aud_stream_destroy(strm);
    }
    if (wav)
    pjmedia_port_destroy(wav);
    if (pool)
    pj_pool_release(pool);
}

unsigned AudioManager::get_dev_count()
{
 return dev_count;
}
