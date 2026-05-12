/**
 * @brief: 类模板单例，用以创建各种类的单例
 */

 #pragma once

 //继承模板单例：class Test : public Singleton<Test>
 //friend class Singleton<Test>;
 template <typename T>
 class Singleton
 {
protected:
    //构造函数私有化；
    //默认构造函数
    Singleton()  = default;
    ~Singleton() = default;

public:
    Singleton(const Singleton<T> &) = delete;
    Singleton<T> & operator=(const Singleton<T> &) = delete;

    static T& instance()
    {
        //静态局部变量实现单例， T必须能被Singleton<T>访问构造函数
        //是线程安全的，自动析构
        static T obj;
        return obj;
    }
 };


 //测试：
// class Test : public Singleton<Test>
// {
//     friend class Singleton<Test>;   //!!!重要
//     //允许 Singleton<Test> 访问 Test
// private:
//     Test() {}     //重要！！！构造函数私有化

// public:
//     void func()
//     {

//     }
// };


//  int main()
// {
//     Test& log = Singleton<Test> ::instance();
//     Test& t2 = Test::instance();
//     //Test 继承了：“以 Test 为参数生成出来的 Singleton 类”
//    // Test t2 = log;//禁止拷贝
//     // t2 = log;//禁止赋值
//     cout << &log << endl;
//     cout << &t2 << endl;
//     return 0;
// }
 