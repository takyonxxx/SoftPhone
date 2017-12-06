
#ifndef __PJMEDIA_TRANSPORT_ADAPTER_H__
#define __PJMEDIA_TRANSPORT_ADAPTER_H__

#include <pjsua.h>
#include <pjmedia/transport.h>
#include <pjmedia/endpoint.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pjmedia.h>
#include <pjmedia/stream.h>
#include <arpa/inet.h>
#include "ed137_rtp.h"

PJ_BEGIN_DECL

PJ_DEF(pj_status_t) pjmedia_custom_tp_adapter_create( pjmedia_endpt *endpt,
                           const char *name,
                           pjmedia_transport *transport,
                           pj_bool_t del_base,pj_bool_t radiocall, const char *calltype, pjsua_call_id callId,
                           pjmedia_transport **p_tp);

pj_status_t decodeRtp (void* pkt, custom_rtp_hdr **hdr);
pj_uint32_t get_ed137_value(pjmedia_transport *tp);
pj_status_t setAdapterPtt (pjmedia_transport *tp,bool pttval,int priority);
pj_status_t setAdapterPttId (pjmedia_transport *tp,int pttid);
pj_status_t setCallType (pjmedia_transport *tp,char const* calltype);


struct tp_adapter
{
    pjmedia_transport  base;
    pj_bool_t		   del_base;
    pj_pool_t		  *pool;
    void		      *stream_user_data;
    void	         (*stream_rtp_cb)(void *user_data, void *pkt, pj_ssize_t);
    void	         (*stream_rtcp_cb)(void *user_data, void *pkt, pj_ssize_t);
    pjmedia_transport *slave_tp;
    pj_bool_t          radiostatus;
    pj_bool_t          pttstatus;
    pjsua_call_id      callID;
    pj_uint8_t         pttpriority;
    pj_uint8_t         pttid;
    pj_uint32_t        ed137_value;
    char               calltype[64];
    pj_uint8_t         pkt_buff[256];
    pj_size_t          bufSize;
    pj_uint8_t         payload_buff[256];
    pj_size_t          payload_bufSize;
};

PJ_END_DECL

#endif
