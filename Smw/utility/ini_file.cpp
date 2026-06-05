/**
 * @brief: INI 配置文件解析器，用于解析配置文件
 * 把配置文件里的配置解析出来，存储到一个map中
 * m_sections[name][key] = value;
 * 
 * std::map<string, Section> m_sections;
 * std::map<string, Value> Section;
 * 
 * 第一层section索引得到Section, 第二层key索引，得到Value
*/ 


/**
 * 配置文件格式：
[Feature configuration]
auto_enble = true
device_type = tty
sensor_state = Not_inserted   
net_device = eth0
 */
#include <iostream>
#include <sstream>
using std::stringstream;

#include<fstream>
using std::ifstream;
using std::ofstream;

#include <algorithm>

#include "ini_file.h"
using std::string;


    IniFile::IniFile()
    {

    }
    /* 带文件名构造 &引用，不用拷贝， const 不修改原来的字符串*/
    IniFile::IniFile(const string & filename)
    {
        load(filename);
    }
    IniFile::~IniFile() { }
    
    /**
     * 作用：去除字符串前后空格 
     * find_first_not_of()：找到第一个不是这些字符的位置
     *  空格 \r回车 \n换行
     * erase(0,len) 从位置0开始删len个字符
     */
    string IniFile::trim(string s)
    {
        if(s.empty()) return s;
        s.erase(0, s.find_first_not_of(" \r\n"));
        s.erase(s.find_last_not_of(" \r\n") + 1);
        return s;
    }

    /* 从文件读取所有配置，存到m_sections */
    bool IniFile::load(const string& filename)
    {
        m_filename = filename;
        /* 清空存储新的 */
        m_sections.clear();

        /* 第一层索引，对应[Feature configuration]*/
        string section; 
        string line;

        ifstream fin(filename.c_str());
        if(fin.fail())
        {
            printf("loading file failed: %s is not found.\n", m_filename.c_str());
            return false;
        }
        while(std::getline(fin,line))
        {
            line = trim(line);
            if(line == "") continue; //空
            if(line[0] == '#' || line[0] == ';') continue; //注释
            if(line[0] == '[') //section
            {
                int pos = line.find_first_of(']');
                if(pos < 0) return false;
                else
                {
                    /* 截取[ ]中间的字符为section索引 substr(pos,len)从pos开始截取长为len的*/
                    section = trim(line.substr(1, pos-1));
                    /* 类名字（） = 创建一个空对象 */
                    m_sections[section] = Section();
                }
            }
            else //不是section索引，现在是key对应value了,也就是Section的map
            {
                int pos = line.find_first_of('=');
                if(pos < 0) return false;
                else
                {
                    string key = trim(line.substr(0, pos));
                    /* 从文件里读出来的，不管是 true / 123 / "abc"，本质全都是 字符串 */
                    /* 所以先用string接收，后面用的时候再转 */
                    string value = trim(line.substr(pos + 1, line.size() - pos - 1));
                    value = trim(value);
                    /* 存到了Value 类里面 */
                    /* 调用赋值运算符重载 */
                    /**
                    Value & Value::operator = (const std::string & value)
                    {
                        m_type = V_STRING;
                        m_value = value;
                        return *this;
                    }
                     */
                    m_sections[section][key] = value;
                }
            }
        }
        return true;
    }

    /* 把当前配置写回文件，set函数把更改写到m_sections里面，save函数保存到ini配置文件中 */
    void IniFile::save(const string& filename)
    {
        /* 创建一个文件输出流，并 打开文件 用于写入 */
        /* 覆盖写回，会清空原来的文件内容，然后写入新内容*/
        ofstream fout(filename.c_str());
        if(fout.fail())
        {
            printf("opening file failed :%s.\n", m_filename.c_str());
            return;
        }
        fout << to_str();
        fout.close();
    }

    /**
     * 把 整个 m_sections 转换成 ini 格式文本，用于 save() 和 show()
     */
    string IniFile::to_str()
    {
        stringstream ss;
        for(auto it = m_sections.begin(); it != m_sections.end(); it++)
        {
            ss << "[" << it->first << "]" << std::endl;
            {
                for( auto iter = it->second.begin(); iter != it->second.end(); iter++)
                {
                    ss << iter->first << " = " << (string)iter->second << std:: endl;
                }
                ss << std::endl;
            }
        }
        /* str()返回输出这个ss字符串流 */
        return ss.str();
    }

    /* 打印所有配置 */
    void IniFile::show()
    {
        std::cout << to_str();
    }
    /* 清空所有配置 */
    void IniFile::clear()
    {
        m_sections.clear();
    }

    /* 配置文件已经加载到了m_sections中，所以从m_sections中获取配置值 */
    Value& IniFile::get(const string& section, const string& key)
    {
        return m_sections[section][key];
    }
    /* 用户更改/设置配置，存到m_sections */
    void IniFile::set(const string& section, const string& key, const Value& value)
    {
        m_sections[section][key] = value;
    }

    /* 判断 section 是否存在 */
    bool IniFile::has(const string& section)
    {
        return(m_sections.find(section) != m_sections.end());
    }
    /* 判断 key 是否存在 */
    bool IniFile::has(const string& section, const string& key)
    {
        auto it = m_sections.find(section);
        if(it != m_sections.end())
        {
            return(it->second.find(key) != it->second.end());
        }
        return false;
    }

    /* 删除整个section */
    void IniFile::remove(const string& section)
    {
        m_sections.erase(section);
    }
    /* 删除某个key */
    void IniFile::remove(const string& section, const string& key)
    {
        auto it = m_sections.find(section);
        if( it != m_sections.end())
        {
            it->second.erase(key);
        }
    }
/* 获取所有 section 名 */
std::vector<std::string> IniFile::GetSectionNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_sections) {
        names.push_back(pair.first);
    }
    return names;
}
