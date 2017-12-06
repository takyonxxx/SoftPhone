#include "WavWriter.h"
#include "Codecs.h"
#include <inttypes.h>

FILE *out, *dump;

size_t totaloffset, dataoffset;
long rate;
int channels;
unsigned short bitspersample, wavformat;
static long datalen;
int frameCount;
static rtpHeader *audio_rtp_hdr;
bool m_isRunning = false;
bool m_isDecodeRtp = false;
using namespace std;
using namespace AudioEncoding;

double swap(double d)
{
   double a;
   unsigned char *dst = (unsigned char *)&a;
   unsigned char *src = (unsigned char *)&d;

   dst[0] = src[7];
   dst[1] = src[6];
   dst[2] = src[5];
   dst[3] = src[4];
   dst[4] = src[3];
   dst[5] = src[2];
   dst[6] = src[1];
   dst[7] = src[0];

   return a;
}

WavWriter::WavWriter()
{
}

void WavWriter::writeRTPWav(const char *pktbuf,const char *payloadbuf, unsigned int pktlen, unsigned int payloadlen)
{
    (void)pktlen;
    if(!m_isDecodeRtp)
    {
        m_isDecodeRtp = true;
        DecodeRtp(pktbuf,&audio_rtp_hdr);
       // DumpRTPHeader(audio_rtp_hdr);
    }

    if(m_isRunning)
    {
         wav_write((unsigned char *)payloadbuf,payloadlen);
         //DumpSamples((unsigned char *)pktbuf,pktlen);        
    }
}

bool WavWriter::isRunning()
{
    return m_isRunning;
}

void WavWriter::start(std::string prefix,int rate)
{
    m_isDecodeRtp = false;
    std::string filename;

    generateFileName(filename, prefix);

    bitspersample = 16;
    wavformat = WAVE_FORMAT_MULAW;
    channels = 2;

    frameCount = 0;
    datalen = 0;

    out = fopen(filename.c_str(), "w");
    assert(out);

    //dump = fopen("dump.log", "wb");
    //assert(dump);

    unsigned int tmp32 = 0;
    unsigned short tmp16 = 0;

    fwrite("RIFF", 1, 4, out);
    totaloffset = ftell(out);

    fwrite(&tmp32, 1, 4, out); // total size
    fwrite("WAVE", 1, 4, out);
    fwrite("fmt ", 1, 4, out);
    tmp32 = 16;
    fwrite(&tmp32, 1, 4, out); // format length
    tmp16 = wavformat;
    fwrite(&tmp16, 1, WAVE_FORMAT_ADPCM, out); // format
    tmp16 = channels;
    fwrite(&tmp16, 1, 2, out); // channels
    tmp32 = rate;
    fwrite(&tmp32, 1, 4, out); // sample rate
    tmp32 = rate * bitspersample/8 * channels;
    fwrite(&tmp32, 1, 4, out); // bytes / second
    tmp16 = bitspersample/8 * channels; // float 16 or signed int 16
    fwrite(&tmp16, 1, 2, out); // block align
    tmp16 = bitspersample;
    fwrite(&tmp16, 1, 2, out); // bits per sample
    fwrite("data ", 1, 4, out);
    tmp32 = 0;
    dataoffset = ftell(out);
    fwrite(&tmp32, 1, 4, out); // data length

    m_isRunning = true;

}

void WavWriter::stop()
{
    unsigned int tmp32 = 0;

    long total = ftell(out);
    fseek(out, totaloffset, SEEK_SET);
    tmp32 = total - (totaloffset + 4);
    fwrite(&tmp32, 1, 4, out);
    fseek(out, dataoffset, SEEK_SET);
    tmp32 = total - (dataoffset + 4);   
    fwrite(&tmp32, 1, 4, out);
    //fclose(dump);
    fclose(out);

    double d = (int) tmp32 / 1024.0;
    printf("Recorded wav file size : %0.1f Kb\n",d);

    m_isRunning = false;
}


void WavWriter::wav_write(unsigned char *buf,unsigned int len)
{
    for (unsigned int i = 0; i< len; i++)
    {
        write_little_endian((unsigned int)(buf[i]),channels, out);
    }

    datalen += len;
    frameCount++;
}

void WavWriter::write_little_endian(unsigned int word, unsigned int num_bytes, FILE *wav_file)
{
    unsigned buf;
    while(num_bytes>0)
    {   buf = word & 0xff;
        fwrite(&buf, 1,1, wav_file);
        num_bytes--;
    word >>= 8;
    }
}

void WavWriter::DumpSamples(unsigned char *data, unsigned int len)
{
    if(len > 0)
     {
         unsigned int i;
         //fprintf(dump, "Frame: %d  DataLen: %d Payload type: %d\n",frameCount,datalen, (int) audio_rtp_hdr->payload);
         for (i = 0; i < len; i++)
         {
             if (i > 0)
             {
                 fprintf(dump, ":");
             }
             fprintf(dump, "%02X", data[i]);
         }
         fprintf(dump, "\n");
     }
}


void WavWriter::DecodeRtp (const char *pkt, rtpHeader **hdr)
{
    *hdr = (rtpHeader*)pkt;
}

void WavWriter::DumpRTPHeader (rtpHeader *p)
{
      printf ("\nRTP HEADER-->\n");
      printf ("Version (V)       : %d\n", p->v);
      printf ("Padding (P)       : %d\n", p->p);
      printf ("Extension (X)     : %d\n", p->x);
      printf ("CSRC count (CC)   : %d\n", p->cc);
      printf ("Marker bit (M)    : %d\n", p->m);
      printf ("Payload Type (PT) : %d\n", p->pt);
      printf ("Timestamp         : %" PRIu32 "\n",(unsigned int)  p->ts);
      printf ("Sequence Number   : %" PRIu16 "\n",(unsigned int)  p->seq);
}

std::string filePath = "";

void WavWriter::generateFileName(std::string& filename,std::string prefix)
{
    char filechr[150] = { };

    struct timeval tv;
    time_t now;
    struct tm *date;

    gettimeofday(&tv, NULL);
    now = tv.tv_sec;

    date = localtime(&now);

    snprintf(filechr, sizeof(filechr), "%s%s%04d%02d%02d%02d%02d%02d.wav",
                filePath.c_str(), prefix.c_str(),
                date->tm_year + 1900,
                date->tm_mon + 1,
                date->tm_mday,
                date->tm_hour,
                date->tm_min,
                date->tm_sec);

    filename = filechr;
}
