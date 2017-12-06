#include <pjmedia/endpoint.h>

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

struct custom_rtp_hdr
{
#if defined(PJ_IS_BIG_ENDIAN) && (PJ_IS_BIG_ENDIAN!=0)
    pj_uint16_t v:2;		/**< packet type/version */
    pj_uint16_t p:1;		/**< padding flag */
    pj_uint16_t x:1;		/**< extension flag */
    pj_uint16_t cc:4;		/**< CSRC count */
    pj_uint16_t m:1;		/**< marker bit */
    pj_uint16_t pt:7;		/**< payload type */
#else
    pj_uint16_t cc:4;		/**< CSRC count	*/
    pj_uint16_t x:1;		/**< header extension flag */
    pj_uint16_t p:1;		/**< padding flag */
    pj_uint16_t v:2;		/**< packet type/version  */
    pj_uint16_t pt:7;		/**< payload type */
    pj_uint16_t m:1;		/**< marker bit */
#endif
    pj_uint16_t seq;		/**< sequence number */
    pj_uint32_t ts;		    /**< timestamp */
    pj_uint32_t ssrc;		/**< synchronization source */
    pj_uint16_t	profile_data;
    pj_uint16_t	length;
    pj_uint32_t	ed137;

};
typedef struct custom_rtp_hdr custom_rtp_hdr;

struct extension_ed137
{
    pj_uint16_t ptt_type:3;
    pj_uint16_t squ:1;
    pj_uint16_t pptt_id:6;
    pj_uint16_t ptt_mute:1;
    pj_uint16_t ptt_sum:1;
    pj_uint16_t sct:1;
    pj_uint16_t reserved:2;
    pj_uint16_t x:1;
    pj_uint16_t ftype:4;
    pj_uint16_t	flenght:4;
    pj_uint16_t	bss_val:5;
    pj_uint16_t	bss_type:3;

};
typedef struct extension_ed137 extension_ed137;

/**
 * RTP extendsion header.
 */
struct custom_rtp_ext_hdr
{
    pj_uint16_t	profile_data;	/**< Profile data.	    */
    pj_uint16_t	length;		/**< Length.		    */
};

/**
 * @see pjmedia_rtp_ext_hdr
 */
typedef struct custom_rtp_ext_hdr custom_rtp_ext_hdr;
/*[RTP] RTP BSS quality index method- RSSI
The  values  calculated  using  the  RSSI  Best  Signal  Selection  quality  index  method
SHALL
 conform  to  the  Signal  Quality  Parameter  (SQP)  described  in  the  ETSI
standard EN 301-841-1.
0
Signal  quality  analysis
SHALL
 be  performed  on  the  demodulator  evaluation  process
and on the receive evaluation process; this analysis
SHALL
be normalized between a
scale of 0 and 15, where 0 represents a received signal
strength lower than -100 dBm
and 15 for a signal strength higher than -70 dBm.
SQP value between -100 dBm and -70 dBm
SHALL
be linear.
The SQP value normalized between 0 and 15
SHALL
be coded in the
BSS-qidx
field
as  the  bits  b24  to  b28  where  b28  is  the  lower  significant  bit  and  b24  the  upper
significant bit. */
/* RTP header ED137A extension fields   */
   /*static int hf_rtp_hdr_ed137s    = -1;
   static int hf_rtp_hdr_ed137     = -1;
   static int hf_rtp_hdr_ed137a     = -1;
   static int hf_rtp_hdr_ed137_ptt_type  = -1;
   static int hf_rtp_hdr_ed137_squ       = -1;
   static int hf_rtp_hdr_ed137_ptt_id    = -1;
   static int hf_rtp_hdr_ed137_sct       = -1;
   static int hf_rtp_hdr_ed137_x         = -1;
   static int hf_rtp_hdr_ed137_x_nu      = -1;
   static int hf_rtp_hdr_ed137_ft_type   = -1;
   static int hf_rtp_hdr_ed137_ft_len    = -1;
   static int hf_rtp_hdr_ed137_ft_value  = -1;
   static int hf_rtp_hdr_ed137_ft_bss_qidx  = -1;
   static int hf_rtp_hdr_ed137_ft_bss_rssi_qidx  = -1;
   static int hf_rtp_hdr_ed137_ft_bss_qidx_ml  = -1;
   static int hf_rtp_hdr_ed137_ft_bss_nu  = -1;
   static int hf_rtp_hdr_ed137_vf  = -1;
   static int hf_rtp_hdr_ed137a_ptt_type  = -1;
   static int hf_rtp_hdr_ed137a_squ       = -1;
   static int hf_rtp_hdr_ed137a_ptt_id    = -1;
   static int hf_rtp_hdr_ed137a_pm        = -1;
   static int hf_rtp_hdr_ed137a_ptts      = -1;
   static int hf_rtp_hdr_ed137a_sct       = -1;
   static int hf_rtp_hdr_ed137a_reserved  = -1;
   static int hf_rtp_hdr_ed137a_x         = -1;
   static int hf_rtp_hdr_ed137a_x_nu      = -1;
   static int hf_rtp_hdr_ed137a_ft_type   = -1;
   static int hf_rtp_hdr_ed137a_ft_len    = -1;
   static int hf_rtp_hdr_ed137a_ft_value  = -1;
   static int hf_rtp_hdr_ed137a_ft_sqi_qidx  = -1;
   static int hf_rtp_hdr_ed137a_ft_sqi_rssi_qidx  = -1;
   static int hf_rtp_hdr_ed137a_ft_sqi_qidx_ml  = -1;
   static gint ett_hdr_ext_ed137s  = -1;
   static gint ett_hdr_ext_ed137   = -1;
   static gint ett_hdr_ext_ed137a   = -1;*/
