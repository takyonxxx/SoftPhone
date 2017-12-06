#ifndef WAVWRITER_H
#define WAVWRITER_H

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <netinet/in.h>
#include <sys/time.h>

#define byte u_int8_t

struct rtpHeader {
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned int v:2;		/**< packet type/version */
    unsigned int p:1;		/**< padding flag */
    unsigned int x:1;		/**< extension flag */
    unsigned int cc:4;		/**< CSRC count */
    unsigned int m:1;		/**< marker bit */
    unsigned int pt:7;		/**< payload type */
#else
    unsigned int cc:4;		/**< CSRC count	*/
    unsigned int x:1;		/**< header extension flag */
    unsigned int p:1;		/**< padding flag */
    unsigned int v:2;		/**< packet type/version  */
    unsigned int pt:7;		/**< payload type */
    unsigned int m:1;		/**< marker bit */
#endif
    unsigned int seq;		/**< sequence number */
    u_int32_t ts;		    /**< timestamp */
    u_int32_t ssrc;		/**< synchronization source */
};
typedef struct rtpHeader rtpHeader;


class WavWriter
{
public:
    WavWriter();

    void writeRTPWav(const char *pktbuf,const char *payloadbuf, unsigned int pktlen, unsigned int payloadlen);
    void DecodeRtp (const char *pkt, rtpHeader **hdr);
    void start(std::string prefix,int rate);
    void stop();    
    void wav_write(unsigned char *buf,unsigned int len);
    void write_little_endian(unsigned int word, unsigned int num_bytes, FILE *wav_file);
    void DumpSamples(unsigned char *data, unsigned int len);    
    void DumpRTPHeader (rtpHeader *p);
    void generateFileName(std::string& filename,std::string prefix);
    bool isRunning();   

};

#endif // WAVWRITER_H


/*
 *    The RTP header has the following format:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timestamp                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           synchronization source (SSRC) identifier            |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |            contributing source (CSRC) identifiers             |
 * |                             ....                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * RTP data header
 */

/*PT 	Encoding Name 	Audio/Video (A/V) 	Clock Rate (Hz) 	Channels 	Reference
0	PCMU	A	8000	1	[RFC3551]
1	Reserved
2	Reserved
3	GSM	A	8000	1	[RFC3551]
4	G723	A	8000	1	[Vineet_Kumar][RFC3551]
5	DVI4	A	8000	1	[RFC3551]
6	DVI4	A	16000	1	[RFC3551]
7	LPC	A	8000	1	[RFC3551]
8	PCMA	A	8000	1	[RFC3551]
9	G722	A	8000	1	[RFC3551]
10	L16	A	44100	2	[RFC3551]
11	L16	A	44100	1	[RFC3551]
12	QCELP	A	8000	1	[RFC3551]
13	CN	A	8000	1	[RFC3389]
14	MPA	A	90000		[RFC3551][RFC2250]
15	G728	A	8000	1	[RFC3551]
16	DVI4	A	11025	1	[Joseph_Di_Pol]
17	DVI4	A	22050	1	[Joseph_Di_Pol]
18	G729	A	8000	1	[RFC3551]
19	Reserved	A
20	Unassigned	A
21	Unassigned	A
22	Unassigned	A
23	Unassigned	A
24	Unassigned	V
25	CelB	V	90000		[RFC2029]
26	JPEG	V	90000		[RFC2435]
27	Unassigned	V
28	nv	V	90000		[RFC3551]
29	Unassigned	V
30	Unassigned	V
31	H261	V	90000		[RFC4587]
32	MPV	V	90000		[RFC2250]
33	MP2T	AV	90000		[RFC2250]
34	H263	V	90000		[Chunrong_Zhu]
35-71	Unassigned	?
72-76	Reserved for RTCP conflict avoidance				[RFC3551]
77-95	Unassigned	?
96-127	dynamic	?			[RFC3551]*/
