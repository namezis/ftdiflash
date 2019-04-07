#ifndef UTILS_H
#define UTILS_H

#include <sstream>
#include <string>

struct FlashConfig
{
    std::string manfacturerName;
    uint8_t manufacturerId;
    uint8_t ID15_ID8;
    uint8_t ID7_ID0;
    std::string memoryName;
    uint32_t size;
    uint32_t pageProgramTime;
    uint32_t blockEraseTime64k;
};

class Formatter
{
public:
    Formatter() {}
    ~Formatter() {}

    template <typename Type>
    Formatter & operator << (const Type & value)
    {
        m_stream << value;
        return *this;
    }

    std::string str() const         { return m_stream.str(); }
    operator std::string () const   { return m_stream.str(); }

    enum ConvertToString 
    {
        to_str
    };
    std::string operator >> (ConvertToString) { return m_stream.str(); }

private:
    std::stringstream m_stream;

    Formatter(const Formatter &);
    Formatter & operator = (Formatter &);
};

#endif // UTILS_H