#include "IniFileLoader.h"
#include "IniFileIterator.h"

IniFileLoader::IniFileLoader()
{
    m_pIterator = 0;
}

IniFileLoader::~IniFileLoader()
{
    releaseIterator();
}

FileIteratorInterface* IniFileLoader::load(std::string const& filePath, bool parseFromMem)
{
    releaseIterator();

    m_pIterator = new IniFileIterator(filePath, parseFromMem);

    return m_pIterator;
}

void IniFileLoader::releaseIterator()
{
    if (m_pIterator)
    {
        delete m_pIterator;
        m_pIterator = 0;
    }
}
