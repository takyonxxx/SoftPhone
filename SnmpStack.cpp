#include "SnmpStack.h"
#include "StaticVariables.h"

SnmpStack* SnmpStack::ins = NULL;
long session_timeout = 500;

unsigned long snmp_get_int(struct snmp_session *sess_handle, const char* oidname);
std::string snmp_get_string(struct snmp_session *sess_handle, const char* oidname);
bool snmp_set_int(struct snmp_session *sess_handle, oid* oidname, const char* value);
bool snmp_set_string(struct snmp_session *sess_handle, oid* oidname, const char* value);

void int64ToChar(char a[], int64_t n) {
  memcpy(a, &n, 8);
}

int64_t charTo64bitNum(char a[]) {
  int64_t n = 0;
  memcpy(&n, a, 8);
  return n;
}

namespace
{
    static bool initialized = false;
}

SnmpStack::SnmpStack()
{   
}

SnmpStack::~SnmpStack()
{
    (void)getRadioCommon;
    (void)getFrequency;;
    (void)getRadioType;
    (void)setFrequency;
    (void)setRadioCommon;
}

SnmpStack* SnmpStack::getInstance()
{
	if (ins == NULL)
	{
        ins = new SnmpStack();
	}
	return ins;
}

bool SnmpStack::init()
{
    if (!initialized)
    {
        init_snmp("snmpapp");
        initialized = true;
        return initialized;
    }

    return false;
}

bool SnmpStack::uninit()
{
    if(initialized)
    {
        snmp_shutdown("snmpapp");
        initialized = false;
        return initialized;
    }

    return false;
}

struct snmp_session *setup_snmp_session(int version, char* community, char* host){

    struct snmp_session session;
    struct snmp_session *sess_handle;

    init_snmp("poller");
    snmp_sess_init( &session );
    session.version = version;
    session.community = (u_char*)community;
    session.community_len = strlen((char*)session.community);
    session.peername = host;
    sess_handle = snmp_open(&session);
    return sess_handle;

}

bool SnmpStack::setsnmp_int(std::string ip,oid* oidname,const char* value)
{
    struct snmp_session   *sess_handle=setup_snmp_session(SNMP_VERSION_2c,(char*)("public"),(char*)ip.c_str());
    sess_handle->timeout = session_timeout;
    snmp_set_int(sess_handle,oidname,value);
    snmp_close(sess_handle);
    return true;
}

bool SnmpStack::setsnmp_string(std::string ip,oid* oidname,const char* value)
{
    struct snmp_session   *sess_handle=setup_snmp_session(SNMP_VERSION_2c,(char*)("public"),(char*)ip.c_str());
    sess_handle->timeout = session_timeout;
    snmp_set_string(sess_handle,oidname,value);
    snmp_close(sess_handle);
    return true;
}

unsigned long SnmpStack::getsnmp_int(std::string ip,const char* oidname)
{
    struct snmp_session   *sess_handle=setup_snmp_session(SNMP_VERSION_1,(char*)("public"),(char*)ip.c_str());
    sess_handle->timeout = session_timeout;
    int64_t val64 = (int64_t) snmp_get_int(sess_handle,oidname);
    //printf("getsnmp_int : %d\n",val64);
    snmp_close(sess_handle);   
    return val64;
}

std::string  SnmpStack::getsnmp_string(std::string ip,const char* oidname)
{
    std::string mesg;
    struct snmp_session   *sess_handle=setup_snmp_session(SNMP_VERSION_1,(char*)("public"),(char*)ip.c_str());
    sess_handle->timeout = session_timeout;
    mesg = snmp_get_string(sess_handle,oidname);
    //printf("getsnmp_int : %s\n",mesg.c_str());
    snmp_close(sess_handle);
    return mesg;
}

unsigned long snmp_get_int(struct snmp_session *sess_handle, const char* oidname){
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    struct variable_list *vars;

    oid id_oid[MAX_OID_LEN];
    size_t id_len = MAX_OID_LEN;
    int status;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    read_objid(oidname, id_oid, &id_len);
    snmp_add_null_var(pdu, id_oid, id_len);

    status = snmp_synch_response(sess_handle, pdu, &response);
    if (status == STAT_SUCCESS) {
       for(vars = response->variables; vars; vars = vars->next_variable)
       {
           //print_value(vars->name, vars->name_length, vars);
           int64_t val64 = (int64_t) vars->val.counter64->high;
           return val64;
       }
       snmp_free_pdu(response);
    }

    return 0;
}
std::string snmp_get_string(struct snmp_session *sess_handle, const char* oidname){
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    struct variable_list *vars;

    oid id_oid[MAX_OID_LEN];
    size_t id_len = MAX_OID_LEN;
    int status;
    char buff[MAX_OID_LEN];
    char mesg[MAX_OID_LEN];

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    read_objid(oidname, id_oid, &id_len);
    snmp_add_null_var(pdu, id_oid, id_len);

    status = snmp_synch_response(sess_handle, pdu, &response);
    if (status == STAT_SUCCESS) {
       for(vars = response->variables; vars; vars = vars->next_variable)
       {           
           int64_t val64 = (int64_t) vars->val.counter64->high;
           //*(int64_t *)mesg = val64;
           memcpy(mesg, &val64, 8);

           sprintf(buff,"%s",mesg);
           return buff;
       }
       snmp_free_pdu(response);
    }

    return std::string();
}

bool snmp_set_int(struct snmp_session *sess_handle, oid* oidname, const char* value){
    //snmpset -v1 -c public 10.1.10.79 1.3.6.1.4.1.22154.3.1.2.3.4.0 i 119600
    struct snmp_pdu *pdu;

    int length=0;
    while (oidname[length] != '\0')
    {
       length++;
    }
    size_t id_len =length+1;

    pdu = snmp_pdu_create(SNMP_MSG_SET);

    snmp_add_var (pdu, oidname, id_len, 'i', value);
    pdu->trap_type = 0;
    pdu->specific_type=0;

    int status = snmp_send(sess_handle, pdu) == 0;
    if (status == STAT_SUCCESS)
        return true;

    return false;
}
bool snmp_set_string(struct snmp_session *sess_handle, oid* oidname, const char* value){

    struct snmp_pdu *pdu;

    int length=0;
    while (oidname[length] != '\0')
    {
       length++;
    }
    size_t id_len =length+1;

    pdu = snmp_pdu_create(SNMP_MSG_SET);

    snmp_add_var (pdu, oidname, id_len, 'x', value);
    pdu->trap_type = 0;
    pdu->specific_type=0;

    int status = snmp_send(sess_handle, pdu) == 0;
    if (status == STAT_SUCCESS)
        return true;

    return false;
}

bool SnmpStack::snmp_checkConnection(std::string ip)
{    
    struct snmp_session *sess_handle=setup_snmp_session(SNMP_VERSION_1,(char*)("public"),(char*)ip.c_str());
    sess_handle->timeout=1000;

        struct snmp_pdu *pdu;
        struct snmp_pdu *response;

        oid id_oid[MAX_OID_LEN];
        size_t id_len = MAX_OID_LEN;
        int status;

        pdu = snmp_pdu_create(SNMP_MSG_GET);
        read_objid(getRadioName, id_oid, &id_len);
        snmp_add_null_var(pdu, id_oid, id_len);

        status = snmp_synch_response(sess_handle, pdu, &response);

        if (status == STAT_SUCCESS) {
           snmp_free_pdu(response);
           snmp_close(sess_handle);
           return true;
        }

    snmp_free_pdu(response);
    snmp_close(sess_handle);
    return false;
}

