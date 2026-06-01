/**
 * 注册类工厂基类；
 * 分为抽象工厂基类、抽象模板工厂、子工厂
 * 用哈希表来存储所有注册过的工厂
 */
/* map只能存一种类型，所以必须建立统一父类，也就是AbstractClassFactoryBase，
第二层建立AbstractClassFactory<Base>，统一某一类Base的创建接口： 规定创建什么类型的产品
第三层：决定创建哪个产品；*/

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

// ===================== 【工厂基类】 =====================
// 所有工厂的“爷爷类”
// 作用：只存类名，不涉及模板，让全局表能统一存放所有工厂
class AbstractClassFactoryBase {
public:
    /* 构造函数：传入子类名 + 基类名*/
    AbstractClassFactoryBase(const std::string& className, const std::string& baseClassName);
    /* 虚析构函数，子类必须重写 */
    virtual ~AbstractClassFactoryBase() = default;

    /* 获取类名、基类名*/
    /* 前面的const保证返回值不被修改，后面那个const表示这个成员函数不会修改对象 */
    const std::string& GetClassName() const;
    const std::string& GetBaseClassName() const;
protected:
    std::string m_className;
    std::string m_baseClassName;
};

// ===================== 【模板抽象工厂】 =====================
// 作用：定义统一接口 CreateObj  统一某一类Base的创建接口
// Base = 你要创建的对象的基类（比如 Animal）
template <typename Base>
class AbstractClassFactory : public AbstractClassFactoryBase{
public:
    AbstractClassFactory(const std::string& className, const std::string& baseClassName)
        : AbstractClassFactoryBase(className, baseClassName) { }
    
    /**
     * 核心接口：创建对象
     * 返回unique_ptr<Base>智能指针，自动释放，不用手动delete
     * 所有工厂都必须实现：创建一个Base类型对象
     */
    virtual std::unique_ptr<Base> CreateObj() const = 0;
};

// ===================== 【决定具体生产啥的子工厂】 =====================
// Derived：具体子类（如 Dog）
// Base：基类（如 Animal）
template <typename Derived, typename Base>
class ClassFactory : public AbstractClassFactory<Base> {
public:
    /* 继承父类的构造函数 */
    ClassFactory(const std::string& className, const std::string &baseClassName)
        :AbstractClassFactory<Base>(className, baseClassName) { }
    /*等价于： using AbstractClassFactory<Base>::AbstractClassFactory; 
    即 父类作用域中的构造函数 */

    /* 真正创建对象！！由子类工厂创建,可创建多个对象*/
    std::unique_ptr<Base> CreateObj() const override{
        /* make_unique安全创建对象，就是new Derived() */
        return std::make_unique<Derived>();
    }
};

// ===================== 【全局工厂注册表】 =====================
// 获取全局唯一的工厂哈希表 类名 → 工厂对象
/* value用共同的父辈类型来接收，统一map的value接收类型 */
// 静态变量，程序结束自动销毁，里面的智能指针也自动释放
inline std::unordered_map<std::string, std::unique_ptr<AbstractClassFactoryBase>>& GetClassFactoryMap() {
    static std::unordered_map<std::string, std::unique_ptr<AbstractClassFactoryBase>> g_classFactoryMap;
    return g_classFactoryMap;
}

// ===================== 【注册工厂到全局表】 =====================
// 把一个“工厂”注册到全局map里
// 使用 unique_ptr 管理工厂生命周期，不会内存泄漏

inline void RegisterClassFactory(const std::string& className, std::unique_ptr<AbstractClassFactoryBase> factory)
{
    /* move: 把工厂的所有权交给全局map，自动管理释放 */
    GetClassFactoryMap()[className] = std::move(factory);
}

// ===================== 【对外：注册类】 =====================
template <typename Derived, typename Base>
void RegisterClass(const std::string& className, const std::string& baseClassName) 
{
    /* 创建一个对应类的工厂 */
    auto factory = std::make_unique<ClassFactory<Derived, Base>>(className, baseClassName);
    /* 注册到全局表 */
    RegisterClassFactory(className, std::move(factory));
}

// ===================== 【对外：通过类名字符串创建对象】 =====================
// 创建对象：输入类名字符串 → 返回对象（智能指针，自动delete）
template <typename Base>
std::unique_ptr<Base> CreateObject(const std::string& className) {
    /* 获取全局工厂表 */
    auto &factoryMap = GetClassFactoryMap();
    /* 查找类名是否注册过，根据字符串找到工厂 */
    auto it = factoryMap.find(className);
    if(it == factoryMap.end())
    {
        /* cerr 错误输出流 */
        std::cerr << "类没有注册：" << className << std::endl;
        return nullptr;
    }
    /* 将基类工厂指针 转换为 对应基类的工厂指针 安全向下转型 */
    /* map的类型：unordered_map< string, unique_ptr<AbstractClassFactoryBase>>*/
    /* dynamic_cast 把 AbstractClassFactoryBase*转化成 AbstractClassFactory<Animal>* */
    /* 然后在从父类接口里面去CreateObj 虚函数动态绑定 */
    /* .get() 是智能指针中，去除里面保存的原始裸指针函数 */
    auto* factory = dynamic_cast<const AbstractClassFactory<Base>*>(it->second.get());
    /* 检查转换类型是否成功，例如传入CreateObject<Animal>("Car")
    但map里：ClassFactory<Car, Vehicle>， 
    dynamic_cast运行时检查真实类型发现：ClassFactory<Car, Vehicle>这时转换就会失败*/
    if(!factory) {
        std::cerr << "工厂类型不匹配：" << className << std::endl;
        return nullptr;
    }
    /* 调用工厂创建对象并返回（智能指针，不用手动delete） */
    return factory->CreateObj();
}

// ===================== 【自动注册宏】 =====================
// 宏的作用：程序启动时自动执行注册
// 内部宏：创建一个代理类，利用全局变量构造函数自动执行注册
// 生成唯一结构体：ProxyType+唯一ID
// ##是宏里面的字符串拼接符 
/*  static 创建一个全局静态对象，利用它的构造函数程序启动时自动调用注册函数 */
/* 全局静态对象会在main之前自动构造 */
#define CLASS_LOADER_REGISTER_CLASS_INTERNAL(Derived, Base, UniqueID) \
namespace {                                                            \
struct ProxyType##UniqueID {                                          \
    ProxyType##UniqueID() {                                           \
        RegisterClass<Derived, Base>(#Derived, #Base);                \
    }                                                                 \
};                                                                    \
static ProxyType##UniqueID g_register_class_##UniqueID;               \
}

// 辅助宏 保证 __COUNTER__ 会展开成数字
#define CLASS_LOADER_REGISTER_CLASS_INTERNAL_1(Derived, Base, UniqueID) \
    CLASS_LOADER_REGISTER_CLASS_INTERNAL(Derived, Base, UniqueID)

/**
 * @brief 工厂注册宏
 * @param Derived 子类名称
 * @param Base 基类名称
 * COUNTER 编译器提供的递增整数，表示唯一ID 
 */
#define CLASS_LOADER_REGISTER_CLASS(Derived, Base) \
    CLASS_LOADER_REGISTER_CLASS_INTERNAL_1(Derived, Base, __COUNTER__)

