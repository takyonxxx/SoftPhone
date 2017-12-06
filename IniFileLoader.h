#ifndef INI_FILE_LOADER_H
#define INI_FILE_LOADER_H

#include "FileLoaderInterface.h"

class IniFileLoader : public FileLoaderInterface
{
public:

	IniFileLoader();

	~IniFileLoader();

	virtual FileIteratorInterface* load(std::string const& filePath, bool parseFromMem = false);

private:

	void releaseIterator();

private:

	FileIteratorInterface* m_pIterator;
};

#endif
