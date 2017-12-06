
#include "FileFactory.h"
#include "IniFileLoader.h"

FileLoaderInterface* FileFactory::getIniFileLoader()
{
    return new IniFileLoader();
}

