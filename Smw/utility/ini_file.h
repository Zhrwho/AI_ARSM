/**
 * @brief: INI 配置文件解析器，用于解析配置文件
 * 把配置文件里的配置解析出来，存储到一个map中
 * m_sections[name][key] = value;
 * 
 * std::map<string, Section> m_sections;
 * std::map<string, Value> Section;
 * 即map<string, map<string, Value>> m_sections
 * 第一层section索引得到Section, 第二层key索引，得到Value
*/ 

#pragma once

#include <string.h>
#include <string>
#include <map>
#include <vector>
#include "Singleton.h"

#include "value.h"

using std::string;

typedef std::map<string, Value> Section;
/* 也实现单例？ */
class IniFile : public Singleton<IniFile>
{
    friend class Singleton<IniFile>;

private:
    IniFile();
    /* 带文件名构造 &引用，不用拷贝， const 不修改原来的字符串*/
    IniFile(const string & filename);
    ~IniFile();

    /* 去除字符串前后空格,内部细节 */
    string trim(string s);

public:
    /* 从文件读取配置 */
    bool load(const string& filename);
    /* 把当前配置写回文件，set函数写到m_sections里面，save函数保存到ini配置文件中 */
    void save(const string& filename);

    /**
     * 把 m_sections 转换成 ini 格式文本，用于 save() 和 show()
     */
    string to_str();

    /* 打印所有配置 */
    void show();
    /* 清空所有配置 */
    void clear();

    /* 获取配置值 */
    Value& get(const string& section, const string& key);
    /* 设置配置，存到m_sections */
    void set(const string& section, const string& key, const Value& value);

    /* 判断 section 是否存在 */
    bool has(const string& section);
    /* 判断 key 是否存在 */
    bool has(const string& section, const string& key);

    /* 获取所有 section 名（用于遍历配置） */
    std::vector<string> GetSectionNames() const;

    /* 删除整个section */
    void remove(const string& section);
    /* 删除某个key */
    void remove(const string& section, const string& key);

    /* 返回引用Section&，可以修改m_sections */
    /* 让ini调用读取value的时候，能像个二维数组一样进行读取！！如：ini["server"]["port"] = 8080;*/
    Section& operator [](const string& section)
    {
        return m_sections[section];
    }

private:
    string m_filename;
    std::map<string,Section> m_sections;
};
