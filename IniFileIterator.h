#ifndef INI_FILE_ITERATOR_H
#define INI_FILE_ITERATOR_H

#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>

#include "FileIteratorInterface.h"

struct ParserSt
{
    void* ptr;

    void* (*init)(char const*);
    int (*is_end)(void*);
    void (*finalize)(void*);
    char* (*get_line)(char*, int , void*);
};

class IniFileIterator : public FileIteratorInterface
{
public:

	IniFileIterator(std::string const& filePath, bool parseFromMem);

	IniFileIterator();

	~IniFileIterator();

	virtual bool isLoaded() const;

	virtual FileItem getItemByKey(std::string const& key)  throw (ItemNotFoundException);

	virtual FileItem getItemByKey(std::string const& section, std::string const& key)  throw (ItemNotFoundException);

	virtual std::string getSectionByKey(std::string const& key)  throw (SectionNotFoundException);

	virtual void getItemsBySection(std::string const& section, std::vector<FileItem>& fileItems) throw (SectionNotFoundException);

protected:

	void load(struct ParserSt* parser, std::string const& filePath);

private:
	std::string ReadLine(struct ParserSt* parser);

	void removeSN(char* buffer);

	void removeComment(std::string& str);

	void trim( std::string& str);

	bool isBlank(std::string const& str);

	bool isComment(std::string const& str);

	bool isSection(std::string const& str);

	std::string getSection(std::string const& str);

	std::string getKey(std::string const& str);

	std::string getValue(std::string const& str);

	void splitString(std::string const& str, char delimiter, std::vector<std::string>& components);

private:

	struct IniFileEntry
	{
		std::string key;
		std::string value;
	};

	std::map<std::string, std::vector<IniFileEntry> > m_SectionEntiesMap;

	bool m_Loaded;
};

#endif
