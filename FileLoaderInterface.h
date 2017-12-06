#ifndef FILE_LOADER_INTERFACE_H
#define FILE_LOADER_INTERFACE_H

#include <string>

#include "FileIteratorInterface.h"

class FileLoaderInterface
{
public:

	virtual ~FileLoaderInterface(){};

	virtual FileIteratorInterface* load(std::string const& filePath, bool parseFromMem = false) = 0;

};

#endif
