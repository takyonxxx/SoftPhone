
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include <ifaddrs.h>
#include <arpa/inet.h>

#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "Utility.h"

std::string Utility::hexToString(char const* input, unsigned int length)
{
    static const char* HexDigits = "0123456789ABCDEF";
    std::string output;

    for (size_t i = 0; i < length; ++i)
    {
        const unsigned char currentDigit = input[i];
        if (i != 0)
        {
            output.push_back(':');
        }

        output.push_back(HexDigits[currentDigit >> 4]);
        output.push_back(HexDigits[currentDigit & 15]);
    }

    return output;
}

std::string Utility::getIpAddress()
{
    struct ifaddrs *addrs,*tmp;
    getifaddrs(&addrs);
    tmp = addrs;
    void * tmpAddrPtr = NULL;
    while (tmp)
    {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET)
        {
                char mask[INET_ADDRSTRLEN];
                void* mask_ptr = &((struct sockaddr_in*) tmp->ifa_netmask)->sin_addr;
                inet_ntop(AF_INET, mask_ptr, mask, INET_ADDRSTRLEN);
                if (strcmp(mask, "255.0.0.0") != 0) {
                    tmpAddrPtr = &((struct sockaddr_in *) tmp->ifa_addr)->sin_addr;
                    char addressBuffer[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                    return std::string(addressBuffer);
                }
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
    return std::string("cannothandled");
}

std::string Utility::getMacAddress()
{
    std::string strMAC;
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1)
    {
        std::cerr << "ERROR: Socket error!" << std::endl;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
    {
        std::cerr << "ERROR: ioctl error!" << std::endl;
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it)
    {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
        {
            if (!(ifr.ifr_flags & IFF_LOOPBACK))
            { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
                {
                    success = 1;
                    break;
                }
            }
        }
        else
        {
            std::cerr << "ERROR: ioctl error!" << std::endl;
        }
    }

    char macAddress[7];

    if (success)
    {
        memcpy(macAddress, ifr.ifr_hwaddr.sa_data, 6);
        strMAC = Utility::hexToString(macAddress, 6);
    }

    return strMAC;
}

// http://stackoverflow.com/questions/16077299/how-to-print-current-time-with-milliseconds-using-c-c11
std::string Utility::nowStr()
{
    // Get current time from the clock, using microseconds resolution
    const boost::posix_time::ptime now =
            boost::posix_time::microsec_clock::local_time();

    // Get the time offset in current day
    const boost::posix_time::time_duration td = now.time_of_day();

    //
    // Extract hours, minutes, seconds and milliseconds.
    //
    // Since there is no direct accessor ".milliseconds()",
    // milliseconds are computed _by difference_ between total milliseconds
    // (for which there is an accessor), and the hours/minutes/seconds
    // values previously fetched.
    //
    const long hours        = td.hours();
    const long minutes      = td.minutes();
    const long seconds      = td.seconds();
    const long milliseconds = td.total_milliseconds() -
            ((hours * 3600 + minutes * 60 + seconds) * 1000);

    //
    // Format like this:
    //
    //      hh:mm:ss.SSS
    //
    // e.g. 02:15:40:321
    //
    //      ^          ^
    //      |          |
    //      123456789*12
    //      ---------10-     --> 12 chars + \0 --> 13 chars should suffice
    //
    //
    char buf[40];
    sprintf(buf, "%02ld:%02ld:%02ld.%03ld",
            hours, minutes, seconds, milliseconds);

    return buf;
}

std::string Utility::getTime()
{
    // Get current time from the clock, using microseconds resolution
    const boost::posix_time::ptime now =
            boost::posix_time::microsec_clock::local_time();

    // Get the time offset in current day
    const boost::posix_time::time_duration td = now.time_of_day();

    const long hours        = td.hours();
    const long minutes      = td.minutes();
    const long seconds      = td.seconds();

    char buf[40];
    sprintf(buf, "%02ld:%02ld:%02ld",
            hours, minutes, seconds);

    return buf;
}

double Utility::now()
{
	boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1));
	boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
	boost::posix_time::time_duration diff = now - time_t_epoch;
	return (double) diff.total_seconds();
}

double Utility::nowMilliSeconds()
{
    boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1));
    boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
    boost::posix_time::time_duration diff = now - time_t_epoch;
    return (double) diff.total_milliseconds();
}


