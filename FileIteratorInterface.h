#ifndef FILE_ITERATOR_INTERFACE_H
#define FILE_ITERATOR_INTERFACE_H

#include <string>
#include <vector>

#include "FileItem.h"
#include "ConfigException.h"

class FileIteratorInterface
{
public:

    virtual ~FileIteratorInterface(){}

    virtual bool isLoaded() const = 0;

    virtual FileItem getItemByKey(std::string const& key) throw (ItemNotFoundException) = 0;

    virtual FileItem getItemByKey(std::string const& section, std::string const& key) throw (ItemNotFoundException) = 0;

    virtual std::string getSectionByKey(std::string const& key) throw (SectionNotFoundException) = 0;

    virtual void getItemsBySection(std::string const& section, std::vector<FileItem>& fileItems) throw (SectionNotFoundException) = 0;

};

#endif
