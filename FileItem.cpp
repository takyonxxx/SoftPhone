
#include <sstream>

#include "FileItem.h"

namespace
{
    template<typename T>
    static T fromString(const std::string& str)
    {
        std::istringstream istr(str);

        T t;
        istr >> t;

        return t;
    }

    enum MultiInputState
    {
        INPUT_HEAD_STATE,
        MULTI_BEG_TAG_STATE,
        DATA_FIELD_STATE,
        SEPERATOR_STATE,
        MULTI_END_TAG_STATE
    };
}

FileItem::FileItem()
{
    m_Key = "";
    m_Value = "";
}

FileItem::FileItem(std::string key, std::string value)
{
    m_Key = key;
    m_Value = value;
}

FileItem::~FileItem()
{

}

unsigned short FileItem::getInt16U() const
{
    return fromString<unsigned short>(m_Value);
}

short FileItem::getInt16S() const
{
    return fromString<short>(m_Value);
}

unsigned int FileItem::getInt32U() const
{
    return fromString<unsigned int>(m_Value);
}

int FileItem::getInt32S() const
{
    return fromString<int>(m_Value);
}

float FileItem::getFloat() const
{
    return fromString<float>(m_Value);
}

double FileItem::getDouble() const
{
    return fromString<double>(m_Value);
}

unsigned char FileItem::getUChar() const
{
    return fromString<unsigned char>(m_Value);
}

char FileItem::getChar() const
{
    return fromString<char>(m_Value);
}

std::string FileItem::getString() const
{
    return m_Value;
}

std::string FileItem::getValue() const
{
    return m_Value;
}

std::string FileItem::getKey() const
{
    return m_Key;
}

void FileItem::parseMultiInput(std::vector<FileItem>& inputVector) const
{
    static const char SEPERATOR_CHAR = ',';
    static const char BEGIN_CHAR     = '{';
    static const char END_CHAR       = '}';

    MultiInputState state = INPUT_HEAD_STATE;
    std::string input;

    for (unsigned i = 0; i < m_Value.size(); ++i)
    {
        char const& ch = m_Value[i];

        switch(state)
        {
        case INPUT_HEAD_STATE:
            if (ch == BEGIN_CHAR)
            {
                state = MULTI_BEG_TAG_STATE;
            }
            break;
        case MULTI_BEG_TAG_STATE:
            if (ch > 32 && ch < 126)
            {
                if (ch != SEPERATOR_CHAR && ch != END_CHAR)
                {
                    input.append(1, ch);
                    state = DATA_FIELD_STATE;
                }
                else if (ch == END_CHAR)
                {
                    state = MULTI_END_TAG_STATE;
                }
            }
            break;
        case DATA_FIELD_STATE:
            if (ch != SEPERATOR_CHAR && ch != BEGIN_CHAR && ch != END_CHAR && ch > 32 && ch < 126)
            {
                input.append(1, ch);
            }
            else if (ch == SEPERATOR_CHAR) {
                FileItem fileItem(m_Key, input);
                inputVector.push_back(fileItem);
                input.clear();
                state = SEPERATOR_STATE;
            }
            else if (ch == END_CHAR)
            {
                FileItem fileItem(m_Key, input);
                inputVector.push_back(fileItem);
                input.clear();
                state = MULTI_END_TAG_STATE;
            }
            break;
        case SEPERATOR_STATE:
            if (ch != SEPERATOR_CHAR && ch != BEGIN_CHAR && ch != END_CHAR && ch > 32 && ch < 126)
            {
                input.append(1, ch);
                state = DATA_FIELD_STATE;
            }
            else if (ch == END_CHAR)
            {
                state = MULTI_END_TAG_STATE;
            }
            break;
        default:
            break;
        }

        if (state == MULTI_END_TAG_STATE)
        {
            break;
        }
    }
}

