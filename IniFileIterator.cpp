
#include <stdlib.h>
#include <string>

#include "IniFileIterator.h"

struct mem_file
{
    char* data;
    char* d_ptr;
};

void* open_file(char const* file_path)
{
    return (void*) fopen(file_path, "r");
}

int end_of_file(void* ptr)
{
    return feof((FILE*) ptr);
}

char* file_get_line(char* str, int num, void* ptr)
{
    return fgets(str, num, (FILE*)ptr);
}

void close_file(void* ptr)
{
    fclose((FILE*) ptr);
    ptr = 0;
}

void* init_mem_file(char const* file_data)
{
    mem_file* mf = (mem_file*) malloc(sizeof(mem_file));

    mf->data = (char*)file_data;
    mf->d_ptr = mf->data;

    return (void*) mf;
}

int end_of_mem_file(void* ptr)
{
    mem_file* mf = (mem_file*) ptr;
    return *(mf->d_ptr) == '\0';
}

char* get_mem_line(char* str, int n, void* vptr)
{
    mem_file* mf = (mem_file*) vptr;
    char* ptr = mf->d_ptr;

    while (n--)
    {
        *str = *ptr;

        if (*ptr == '\0') {
            break;
        }
        if (*ptr == '\n') {
            ++ptr;
            break;
        }
        ++str;
        ++ptr;
    }

    mf->d_ptr = ptr;

    return (char*) str;
}

void finalize(void* vptr)
{
    free(vptr);
}

IniFileIterator::IniFileIterator()
{
    m_Loaded = false;
}

IniFileIterator::IniFileIterator(std::string const& filePath, bool parseFromMem)
{
    m_Loaded = false;

    struct ParserSt parser;

    if (!parseFromMem)
    {
        parser.init = &open_file;
        parser.is_end = &end_of_file;
        parser.get_line = &file_get_line;
        parser.finalize = &close_file;
    }
    else {
        parser.init = &init_mem_file;
        parser.is_end = &end_of_mem_file;
        parser.get_line = &get_mem_line;
        parser.finalize = &finalize;
    }

    load(&parser, filePath);
}

IniFileIterator::~IniFileIterator()
{
}

bool IniFileIterator::isLoaded() const
{
    return m_Loaded;
}

void IniFileIterator::load(struct ParserSt* parser, std::string const& filePath)
{
    std::string section;
    bool validSection = false;
    bool validEntries = false;
    std::vector<IniFileEntry> entryList;

    bool noSection = false;

    parser->ptr = parser->init(filePath.c_str());

    if (parser->ptr == 0)
    {
        m_Loaded = false;
        return;
    }

    m_Loaded = true;

    while(!parser->is_end(parser->ptr))
    {
        std::string line = ReadLine( parser );

        if (isBlank(line) || isComment(line))
        {
            continue;
        }

        if (isSection(line))
        {
            if (validSection && validEntries)
            {
                m_SectionEntiesMap[section] = entryList;
                entryList.clear();
            }
            section = getSection(line);

            validSection = true;
            validEntries = false;

            continue;
        }

        if (validSection)
        {
            validEntries = true;

            std::string key = getKey(line);

            if (key.size() > 0)
            {
                std::string value = getValue(line);

                IniFileEntry entry;
                entry.key = key;
                entry.value = value;

                entryList.push_back(entry);
            }
        }
        else 
        {
            noSection = true;
            validEntries = true;

            std::string key = getKey(line);

            if (key.size() > 0)
            {
                std::string value = getValue(line);

                IniFileEntry entry;
                entry.key = key;
                entry.value = value;

                entryList.push_back(entry);
            }
        }
    }

    if (validSection && validEntries)
    {
        m_SectionEntiesMap[section] = entryList;
        entryList.clear();
    }
    else if (noSection && validEntries)
    {
        m_SectionEntiesMap[""] = entryList;
        entryList.clear();
    }

    parser->finalize(parser->ptr);
}

std::string IniFileIterator::getSection(std::string const& str)
{
    std::string retVal;

    retVal = str.substr(1, str.size( ) - 2);

    trim( retVal );

    return retVal;
}

std::string IniFileIterator::getKey(std::string const& str)
{
    std::vector< std::string > components;

    splitString(str, '=', components );

    std::string key = components[0];

    trim(key);

    return key;
}

std::string IniFileIterator::getValue(std::string const& str)
{
    std::vector< std::string > components;
    splitString( str, '=', components );

    if (components.size() == 1)
    {
        return "";
    }

    std::string value = components[1];

    trim(value);
    removeComment(value);
    trim(value);

    return value;
}

void IniFileIterator::splitString(std::string const& str, char delimiter, std::vector<std::string>& components)
{
    std::string token;
    char c;

    components.clear();

    unsigned int stringSize = (unsigned int)str.length();

    if (stringSize > 0) 
    {
        for (unsigned int i = 0; ; i++)
        {
            if (i == stringSize) 
            {
                if (token.length() > 0) 
                {
                    components.push_back(token);
                }
                break;
            }

            c = str.at(i);

            if (c == delimiter) 
            {
                if (token.length() > 0) 
                {
                    components.push_back(token);
                }

                token.erase();

                continue;
            }

            token += c;
        }
    }
}

std::string IniFileIterator::ReadLine(struct ParserSt* parser)
{
    char buffer[1024];

    memset( buffer, 0, sizeof(buffer));

    parser->get_line(buffer, sizeof(buffer), parser->ptr);

    std::string line(buffer);

    trim(line);

    return line;
}

void IniFileIterator::removeSN(char* buffer )
{
    int strLen = (unsigned int) strlen(buffer);

    if (strLen > 0)
    {
        if (buffer[strLen - 1] == '\n')
        {
            buffer[strLen - 1] = 0;
        }
    }
    else
    {
        buffer[0] = 0;
    }
}

void IniFileIterator::removeComment(std::string& str)
{
    if (str.find_first_of("#") != std::string::npos)
    {
        str.erase(str.find_first_of("#"), str.length());
    }

    if (str.find_first_of(";") != std::string::npos)
    {
        str.erase(str.find_first_of(";"), str.length());
    }
}

void IniFileIterator::trim( std::string& str)
{
    if (str.size() > 0)
    {
        str.erase ( 0, str.find_first_not_of( " \t\n\r" ));
        str.erase ( str.find_last_not_of( " \t\n\r" ) + 1, str.length());
    }
}

bool IniFileIterator::isBlank(std::string const& str)
{
    return (str.size() == 0);
}

bool IniFileIterator::isComment(std::string const& str)
{
    bool retVal = false;

    if (str.size() > 0)
    {
        retVal = (str[ 0 ] == '#') || (str[ 0 ] == ';');
    }

    return retVal;
}

bool IniFileIterator::isSection(std::string const& str)
{
    bool retVal = false;

    if (str.size( ) > 0)
    {
        retVal = (str[0] == '[');
    }

    return retVal;
}

FileItem IniFileIterator::getItemByKey(std::string const& key) throw (ItemNotFoundException)
{
	return getItemByKey("", key);
}

FileItem IniFileIterator::getItemByKey(std::string const& section, std::string const& key)  throw (ItemNotFoundException)
{
    std::string trimmedSection = section;
    std::string trimmedKey = key;

    trim(trimmedSection);
    trim(trimmedKey);

    std::map<std::string, std::vector<IniFileEntry> >::iterator it =
        m_SectionEntiesMap.find(trimmedSection);

    if (it != m_SectionEntiesMap.end())
    {
        std::vector<IniFileEntry> const& entryList = it->second;

        std::vector<IniFileEntry>::const_iterator vIt = entryList.begin();

        for (; vIt != entryList.end(); ++vIt)
        {
            IniFileEntry const& entry = *vIt;

            if (entry.key == trimmedKey)
            {
                // entry found
                FileItem item(entry.key, entry.value);

                return item;
            }
        }
    }

    throw ItemNotFoundException("ItemNotFoundException: " + key + " not found in section " + section + "!!!");
}

std::string IniFileIterator::getSectionByKey(std::string const& key)  throw (SectionNotFoundException)
{
    std::string trimmedKey = key;
    trim(trimmedKey);

    std::map<std::string, std::vector<IniFileEntry> >::const_iterator it =
        m_SectionEntiesMap.begin();

    for (; it != m_SectionEntiesMap.end(); ++it)
    {
        std::vector<IniFileEntry> const& entryList = it->second;

        std::vector<IniFileEntry>::const_iterator vIt = entryList.begin();

        for (; vIt != entryList.end(); ++vIt)
        {
            if (vIt->key == trimmedKey)
            {
                return it->first;
            }
        }
    }

    throw SectionNotFoundException("SectionNotFoundException: " + key + " not found!!!");
}

void IniFileIterator::getItemsBySection(std::string const& section, std::vector<FileItem>& fileItems)  throw (SectionNotFoundException)
{
    fileItems.clear();

    std::map<std::string, std::vector<IniFileEntry> >::iterator it =
        m_SectionEntiesMap.find(section);

    if (it != m_SectionEntiesMap.end())
    {
        std::vector<IniFileEntry> const& entryList = it->second;

        std::vector<IniFileEntry>::const_iterator vIt = entryList.begin();

        for (; vIt != entryList.end(); ++vIt)
        {
            fileItems.push_back(FileItem(vIt->key, vIt->value));
        }
    }
    else {
        throw SectionNotFoundException("SectionNotFoundException: " + section + " not found!!!");
    }
}
