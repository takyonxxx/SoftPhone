#include <stdlib.h>
#include <iostream>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// sudo apt-get install libsnmp-dev

class SnmpStack
{
public:

    //
    // Retrives singleton instace of SnmpStack class
    //
    // @return      SnmpStack instance
    //
    static SnmpStack* getInstance();   
private:
    //
    // Hidden constructor
    //
    SnmpStack();

    //
    // Hidden destructor
    //
    ~SnmpStack();

public:
    unsigned long getsnmp_int(std::string ip,const char* oidname);
    std::string getsnmp_string(std::string ip,const char* oidname);
    bool setsnmp_int(std::string ip,oid* oidname,const char* value);
    bool setsnmp_string(std::string ip,oid* oidname,const char* value);
    bool snmp_checkConnection(std::string ip);

    bool init();
    bool uninit();

private:


private:    
    //
    // static instance of SnmpStack class
    //
    static SnmpStack* ins;
};


