/**
 * @brief: 自定义的一个value类 支持 int double string bool转换
*/

#include <string>

// class Value
// {
// public:
//     Value & operator = (const std::string & value);

//     operator std::string();

    
// private:
//     std::string m_value;
            
// };

#ifndef VALUE_H
#define VALUE_H

#include <string>
#include <sstream>

class Value
{
public:
    Value() = default;

    // ✅ 支持 string
    Value(const std::string& str) : m_str(str) {}

    // ✅【关键修复】支持 const char*（比如 "inserted" / "false"）
    Value(const char* str) : m_str(str) {}

    // 赋值 string
    Value& operator=(const std::string& str) {
        m_str = str;
        return *this;
    }

    // 赋值 const char*
    Value& operator=(const char* str) {
        m_str = str;
        return *this;
    }

    // 强转 string（你 to_str 要用）
    operator std::string() const {
        return m_str;
    }

    std::string toString() const {
        return m_str;
    }

    int toInt() const {
        int v;
        std::stringstream ss(m_str);
        ss >> v;
        return v;
    }

    bool toBool() const {
        return m_str == "true" || m_str == "1";
    }

private:
    std::string m_str;
};

#endif