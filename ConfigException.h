#ifndef CONFIG_EXCEPTION_H
#define CONFIG_EXCEPTION_H

#include <exception>
#include <string>

#define DEFINE_EXCEPTION(EXCEPTION_CLASS)                                           \
    class EXCEPTION_CLASS : public std::exception                                   \
    {                                                                               \
    public:                                                                         \
        explicit EXCEPTION_CLASS(std::string const& exStr) : m_ExStr(exStr) { }     \
        virtual ~EXCEPTION_CLASS() throw() { }                                      \
        virtual char const* what() const throw() { return m_ExStr.c_str(); }        \
    private:                                                                        \
        std::string m_ExStr;                                                        \
    };

// Define Exceptions

DEFINE_EXCEPTION(ItemNotFoundException)
DEFINE_EXCEPTION(SectionNotFoundException)

#endif
