#ifndef UTILITY_H
#define UTILITY_H

#include <string>

class Utility
{
public:

    static std::string hexToString(char const* input, unsigned int length);

    static std::string getMacAddress();

    static std::string getIpAddress();

    static std::string nowStr();

    static std::string getTime();

    static double now();

    static double nowMilliSeconds();

};

#endif
