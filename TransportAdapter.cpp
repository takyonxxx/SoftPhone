
#include "TransportAdapter.h"
#include <pj/string.h>
#include "SoftPhone.h"

/* Transport functions prototypes */
#define THIS_FILE	"transport_adapter_sample.c"
#define RTP_VERSION 2

static pj_status_t transport_get_info (pjmedia_transport *tp,
                       pjmedia_transport_info *info);
static pj_status_t transport_attach   (pjmedia_transport *tp,
                       void *user_data,
                       const pj_sockaddr_t *rem_addr,
                       const pj_sockaddr_t *rem_rtcp,
                       unsigned addr_len,
                       void (*rtp_cb)(void*,
                              void*,
                              pj_ssize_t),
                       void (*rtcp_cb)(void*,
                               void*,
                               pj_ssize_t));
static void	   transport_detach   (pjmedia_transport *tp,
                       void *strm);
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
                       const void *pkt,
                       pj_size_t size);
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
                       const void *pkt,
                       pj_size_t size);
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
                       const pj_sockaddr_t *addr,
                       unsigned addr_len,
                       const void *pkt,
                       pj_size_t size);
static pj_status_t transport_media_create(pjmedia_transport *tp,
                       pj_pool_t *sdp_pool,
                       unsigned options,
                       const pjmedia_sdp_session *rem_sdp,
                       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
                       pj_pool_t *sdp_pool,
                       pjmedia_sdp_session *local_sdp,
                       const pjmedia_sdp_session *rem_sdp,
                       unsigned media_index);
static pj_status_t transport_media_start (pjmedia_transport *tp,
                       pj_pool_t *pool,
                       const pjmedia_sdp_session *local_sdp,
                       const pjmedia_sdp_session *rem_sdp,
                       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
                       pjmedia_dir dir,
                       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);

/* The transport operations */
static struct pjmedia_transport_op tp_adapter_op =
{
    &transport_get_info,
    &transport_attach,
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &transport_send_rtcp2,
    &transport_media_create,
    &transport_encode_sdp,
    &transport_media_start,
    &transport_media_stop,
    &transport_simulate_lost,
    &transport_destroy
};


static custom_rtp_hdr *rtphdr;
static struct tp_adapter *adapter;


PJ_DEF(pj_status_t) pjmedia_custom_tp_adapter_create( pjmedia_endpt *endpt,
                           const char *name,
                           pjmedia_transport *transport,
                           pj_bool_t del_base,pj_bool_t radiocall, const char *calltype, pjsua_call_id callId,
                           pjmedia_transport **p_tp)
{

        pj_pool_t *pool;

        if (name == NULL)
        name = "tpad%p";

        // Create the pool and initialize the adapter structure
        pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);

        adapter = PJ_POOL_ZALLOC_T(pool, struct tp_adapter);
        adapter->pool = pool;
        pj_ansi_strncpy(adapter->base.name, pool->obj_name,
                sizeof(adapter->base.name));
        adapter->base.type = (pjmedia_transport_type)
                 (PJMEDIA_TRANSPORT_TYPE_USER + 1);
        adapter->base.op = &tp_adapter_op;

        // Save the transport as the slave transport
        adapter->slave_tp = transport;
        adapter->del_base = del_base;
        adapter->radiostatus = radiocall;
        adapter->pttstatus = false;
        adapter->pttpriority = 0;
        adapter->pttid = 0;
        adapter->callID = callId;
        pj_memcpy(adapter->calltype,calltype,strlen(calltype) + 1);
        adapter->ed137_value = 0;
        // Done
        *p_tp = &adapter->base;
        return PJ_SUCCESS;
}

pj_status_t setAdapterPtt (pjmedia_transport *tp,bool pttval,int priority)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    if(adapter!=NULL)
    {
      adapter->pttstatus = pttval;
      adapter->pttpriority = priority;
      printf("Set Radio Ptt: %d\n",pttval);
    }
    return PJ_SUCCESS;
}

pj_status_t setAdapterPttId (pjmedia_transport *tp,int pttid)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    if(adapter!=NULL)
    {
      adapter->pttid = pttid;
    }
    return PJ_SUCCESS;
}

pj_status_t setCallType (pjmedia_transport *tp,char const* calltype)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    if(adapter!=NULL)
    {
      pj_memcpy(adapter->calltype,calltype,strlen(calltype) + 1);
    }
    return PJ_SUCCESS;
}

/*
 * get_info() is called to get the transport addresses to be put
 * in SDP c= line and a=rtcp line.
 */
static pj_status_t transport_get_info(pjmedia_transport *tp,
                      pjmedia_transport_info *info)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Since we don't have our own connection here, we just pass
     * this function to the slave transport.
     */
    return pjmedia_transport_get_info(adapter->slave_tp, info);
}

static void transport_rtp_cb(void *user_data, void *pkt, pj_ssize_t size)
{  
    struct tp_adapter *adapter = (struct tp_adapter*)user_data;   

    pj_assert(adapter->stream_rtp_cb != NULL);

      if(adapter->radiostatus)
      {
         pj_status_t status = decodeRtp(pkt,&rtphdr);
         if (status == PJ_SUCCESS)
         {
            adapter->ed137_value = rtphdr->ed137;
         }
      }      

      unsigned int payloadlen;

      if(!adapter->radiostatus)
      {
          //decode rtp pkt
          payloadlen = size - sizeof(pjmedia_rtp_hdr);
          adapter->payload_bufSize = payloadlen;

          //copy payload to payload buff  
          pj_memcpy(adapter->payload_buff, (char*)pkt + sizeof(pjmedia_rtp_hdr) ,payloadlen);
      }
      else
      {
          //decode rtp pkt
          payloadlen = size - sizeof(custom_rtp_hdr);
          adapter->payload_bufSize = payloadlen;

          //copy payload to payload buff
          pj_memcpy(adapter->payload_buff, (char*)pkt + sizeof(custom_rtp_hdr) ,payloadlen);
      }

      SoftPhone::instance()->setIncomingRTP(adapter);

    /* Call stream's callback */
   adapter->stream_rtp_cb(adapter->stream_user_data, pkt, size);
}

static void transport_rtcp_cb(void *user_data, void *pkt, pj_ssize_t size)
{    
    struct tp_adapter *adapter = (struct tp_adapter*)user_data;    

    pj_assert(adapter->stream_rtcp_cb != NULL);   

    /* Call stream's callback */
    adapter->stream_rtcp_cb(adapter->stream_user_data, pkt, size);
}

pj_uint32_t get_ed137_value(pjmedia_transport *tp)
{
   struct tp_adapter *adapter = (struct tp_adapter*)tp;
   if(adapter!=NULL)
   {
       return ntohl(adapter->ed137_value);
   }
   else
       return 0;
}

/*
 * attach() is called by stream to register callbacks that we should
 * call on receipt of RTP and RTCP packets.
 */
static pj_status_t transport_attach(pjmedia_transport *tp,
                    void *user_data,
                    const pj_sockaddr_t *rem_addr,
                    const pj_sockaddr_t *rem_rtcp,
                    unsigned addr_len,
                    void (*rtp_cb)(void*,
                           void*,
                           pj_ssize_t),
                    void (*rtcp_cb)(void*,
                            void*,
                            pj_ssize_t))
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    pj_status_t status;

    /* In this example, we will save the stream information and callbacks
     * to our structure, and we will register different RTP/RTCP callbacks
     * instead.
     */
    pj_assert(adapter->stream_user_data == NULL);
    adapter->stream_user_data = user_data;
    adapter->stream_rtp_cb = rtp_cb;
    adapter->stream_rtcp_cb = rtcp_cb;

    status = pjmedia_transport_attach(adapter->slave_tp, adapter, rem_addr,
                      rem_rtcp, addr_len, &transport_rtp_cb,
                      &transport_rtcp_cb);
    if (status != PJ_SUCCESS) {
    adapter->stream_user_data = NULL;
    adapter->stream_rtp_cb = NULL;
    adapter->stream_rtcp_cb = NULL;
    return status;
    }

    return PJ_SUCCESS;
}

/*
 * detach() is called when the media is terminated, and the stream is
 * to be disconnected from us.
 */
static void transport_detach(pjmedia_transport *tp, void *strm)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    PJ_UNUSED_ARG(strm);

    if (adapter->stream_user_data != NULL) {
    pjmedia_transport_detach(adapter->slave_tp, adapter);
    adapter->stream_user_data = NULL;
    adapter->stream_rtp_cb = NULL;
    adapter->stream_rtcp_cb = NULL;
    }
}

pj_status_t decodeRtp (void* pkt, custom_rtp_hdr **hdr)
{

    // Assume RTP header at the start of packet. We'll verify this later.
    *hdr = (custom_rtp_hdr*)pkt;

    return PJ_SUCCESS;
}

/*
 * send_rtp() is called to send RTP packet. The "pkt" and "size" argument
 * contain both the RTP header and the payload.
 */

static pj_status_t transport_send_rtp( pjmedia_transport *tp,const void *pkt,pj_size_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    /* You may do some processing to the RTP packet here if you want. */

    if(adapter->radiostatus)
    {
        pj_status_t status;
        const pjmedia_rtp_hdr *pj_hdr;
        pjmedia_rtp_session *ses = NULL;
        const void *payload;
        pj_uint32_t pttmask;
        unsigned int payloadlen;

        //decode rtp pkt
        status = pjmedia_rtp_decode_rtp(ses,pkt,size,&pj_hdr,&payload,&payloadlen);

        //copy standart 12 byte header to new packet
        pj_memcpy(adapter->pkt_buff, pj_hdr, sizeof(custom_rtp_hdr));
        //copy payload to new packet
        pj_memcpy(adapter->pkt_buff + sizeof(custom_rtp_hdr), payload, payloadlen);

        //Assume custom RTP header at the start of packet.
        status = decodeRtp(adapter->pkt_buff,&rtphdr);

        if (status == PJ_SUCCESS)
        {
            //set additional custom values to ED137 header...
            rtphdr->x = 1;//enable header extension
            rtphdr->profile_data = ntohs(0x0167);//0167
            rtphdr->length = ntohs(0x1);//0001
            rtphdr->ed137 = 0x00000000;

            ///send pttid
            pttmask = (pj_uint32_t) adapter->pttid;
            pttmask = pttmask << 22;
            rtphdr->ed137 = (rtphdr->ed137 | 0x0fc00000) & pttmask;

            if(adapter->pttstatus)
            {
              ///send pttpriority
              pttmask = (pj_uint32_t) adapter->pttpriority;
              pttmask = pttmask << 29;
              pttmask = pttmask | 0x1fffffff;
              rtphdr->ed137 = (rtphdr->ed137 | 0xe0000000) & pttmask;
            }

            rtphdr->ed137 = htonl(rtphdr->ed137);
        }

        size = sizeof(custom_rtp_hdr) + payloadlen;

        // Send the new ED137 packet using the slave transport
        status = pjmedia_transport_send_rtp(adapter->slave_tp, adapter->pkt_buff, size);
        return status;
    }
    else
    {
        return pjmedia_transport_send_rtp(adapter->slave_tp, pkt, size);
    }
}

/*
 * send_rtcp() is called to send RTCP packet. The "pkt" and "size" argument
 * contain the RTCP packet.
 */
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
                       const void *pkt,
                       pj_size_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* You may do some processing to the RTCP packet here if you want. */

    /* Send the packet using the slave transport */
    return pjmedia_transport_send_rtcp(adapter->slave_tp, pkt, size);
}


/*
 * This is another variant of send_rtcp(), with the alternate destination
 * address in the argument.
 */
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
                        const pj_sockaddr_t *addr,
                        unsigned addr_len,
                        const void *pkt,
                        pj_size_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    return pjmedia_transport_send_rtcp2(adapter->slave_tp, addr, addr_len,
                    pkt, size);
}

/*
 * The media_create() is called when the transport is about to be used for
 * a new call.
 */
static pj_status_t transport_media_create(pjmedia_transport *tp,
                          pj_pool_t *sdp_pool,
                          unsigned options,
                          const pjmedia_sdp_session *rem_sdp,
                          unsigned media_index)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* if "rem_sdp" is not NULL, it means we are UAS. You may do some
     * inspections on the incoming SDP to verify that the SDP is acceptable
     * for us. If the SDP is not acceptable, we can reject the SDP by
     * returning non-PJ_SUCCESS.
     */

    if (rem_sdp) {

    }

    /* Once we're done with our initialization, pass the call to the
     * slave transports to let it do it's own initialization too.
     */
    return pjmedia_transport_media_create(adapter->slave_tp, sdp_pool, options,
                       rem_sdp, media_index);
}

/*
 * The encode_sdp() is called when we're about to send SDP to remote party,
 * either as SDP offer or as SDP answer.
 */
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
                        pj_pool_t *sdp_pool,
                        pjmedia_sdp_session *local_sdp,
                        const pjmedia_sdp_session *rem_sdp,
                        unsigned media_index)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* If "rem_sdp" is not NULL, it means we're encoding SDP answer. You may
     * do some more checking on the SDP's once again to make sure that
     * everything is okay before we send SDP.
     */

    if (rem_sdp) {
    }

    /* You may do anything to the local_sdp, e.g. adding new attributes, or
     * even modifying the SDP if you want.
     */

    if(adapter->radiostatus)
    {
        pjmedia_sdp_attr *my_attr;

        my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);

        pj_strdup2(sdp_pool, &my_attr->name, "bss");
        pj_strdup2(sdp_pool, &my_attr->value, "RSSI");
        pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count,local_sdp->media[media_index]->attr,my_attr);

        my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);

        pj_strdup2(sdp_pool, &my_attr->name, "type");
        pj_strdup2(sdp_pool, &my_attr->value, adapter->calltype);
        pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count,local_sdp->media[media_index]->attr,my_attr);

    }

    /* And then pass the call to slave transport to let it encode its
     * information in the SDP. You may choose to call encode_sdp() to slave
     * first before adding your custom attributes if you want.
     */
    return pjmedia_transport_encode_sdp(adapter->slave_tp, sdp_pool, local_sdp,rem_sdp, media_index);
}

/*
 * The media_start() is called once both local and remote SDP have been
 * negotiated successfully, and the media is ready to start. Here we can start
 * committing our processing.
 */
static pj_status_t transport_media_start(pjmedia_transport *tp,
                         pj_pool_t *pool,
                         const pjmedia_sdp_session *local_sdp,
                         const pjmedia_sdp_session *rem_sdp,
                         unsigned media_index)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;


    /* And pass the call to the slave transport */
    return pjmedia_transport_media_start(adapter->slave_tp, pool, local_sdp,
                     rem_sdp, media_index);
}

/*
 * The media_stop() is called when media has been stopped.
 */
static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Do something.. */

    /* And pass the call to the slave transport */
    return pjmedia_transport_media_stop(adapter->slave_tp);
}

/*
 * simulate_lost() is called to simulate packet lost
 */
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
                           pjmedia_dir dir,
                           unsigned pct_lost)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    return pjmedia_transport_simulate_lost(adapter->slave_tp, dir, pct_lost);
}

/*
 * destroy() is called when the transport is no longer needed.
 */
static pj_status_t transport_destroy  (pjmedia_transport *tp)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Close the slave transport */
    if (adapter->del_base) {
    pjmedia_transport_close(adapter->slave_tp);
    }

    /* Self destruct.. */
    pj_pool_release(adapter->pool);

   return PJ_SUCCESS;
}




