#ifndef FILE_ITEM_H
#define FILE_ITEM_H

#include <string>
#include <vector>

class FileItem
{
public:

    FileItem();

    FileItem(std::string key, std::string value);

    virtual ~FileItem();

    virtual unsigned short getInt16U() const;

    virtual short getInt16S() const;

    virtual unsigned int getInt32U() const;

    virtual int getInt32S() const;

    virtual float getFloat() const;

    virtual double getDouble() const;

    virtual unsigned char getUChar() const;

    virtual char getChar() const;

    virtual std::string getString() const;

    virtual std::string getKey() const;

    virtual std::string getValue() const;

    virtual void parseMultiInput(std::vector<FileItem>& inputVector) const;

protected:

    std::string m_Key;

    std::string m_Value;
};

#endif
