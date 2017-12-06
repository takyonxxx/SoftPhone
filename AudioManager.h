#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia-audiodev/audiotest.h>
#include <pjmedia.h>
#include <pjlib.h>
#include <pjlib-util.h>

class AudioManager
{
public:
    static AudioManager* getInstance();

private:
    AudioManager();
    ~AudioManager();
public:
    void list_devices(void);
    void show_dev_info(unsigned index);
    void test_device(pjmedia_dir dir, unsigned rec_id, unsigned play_id,
                unsigned clock_rate, unsigned ptime,
                unsigned chnum);

    void record(unsigned rec_index, const char *filename);
    void play_file(unsigned play_index, const char *filename);

    unsigned get_dev_count();

    bool init();

private:
    static AudioManager* ins;
};

#endif // AUDIO_MANAGER_H
