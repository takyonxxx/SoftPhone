#ifndef FILE_FACTORY_H
#define FILE_FACTORY_H

#include <string>

#include "FileLoaderInterface.h"

class FileFactory
{
public:

	static FileLoaderInterface* getIniFileLoader();
};

#endif
