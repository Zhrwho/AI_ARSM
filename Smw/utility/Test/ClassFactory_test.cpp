#include "../ClassFactory.h"
#include <iostream>

using namespace std;

// ===================== 测试基类 =====================

class Animal {
public:
    virtual ~Animal() = default;

    virtual void Speak() = 0;
};

// ===================== Dog =====================

class Dog : public Animal {
public:
    void Speak() override
    {
        cout << "Dog Wang Wang\n";
    }
};

// 自动注册 Dog
CLASS_LOADER_REGISTER_CLASS(Dog, Animal)

// ===================== Cat =====================

class Cat : public Animal {
public:
    void Speak() override
    {
        cout << "Cat Miao Miao\n";
    }
};

// 自动注册 Cat
CLASS_LOADER_REGISTER_CLASS(Cat, Animal)

// ===================== Vehicle =====================

class Vehicle {
public:
    virtual ~Vehicle() = default;

    virtual void Run() = 0;
};

// ===================== Car =====================

class Car : public Vehicle {
public:
    void Run() override
    {
        cout << "Car Running\n";
    }
};

// 自动注册 Car
CLASS_LOADER_REGISTER_CLASS(Car, Vehicle)

// ===================== main =====================

int main()
{
    // ========= 创建Dog =========

    auto dog = CreateObject<Animal>("Dog");

    if(dog)
    {
        dog->Speak();
    }

    // ========= 创建Cat =========

    auto cat = CreateObject<Animal>("Cat");

    if(cat)
    {
        cat->Speak();
    }

    // ========= 错误测试：不存在类 =========

    auto bird = CreateObject<Animal>("Bird");

    if(!bird)
    {
        cout << "Bird create failed\n";
    }

    // ========= 错误测试：类型不匹配 =========

    auto car = CreateObject<Animal>("Car");

    if(!car)
    {
        cout << "Car is not Animal\n";
    }

    return 0;
}