/**
 * 注册类工厂基类；
 * 分为抽象工厂基类、抽象模板工厂、子工厂
 * 用哈希表来存储所有注册过的工厂
 */
/* map只能存一种类型，所以必须建立统一父类，也就是AbstractClassFactoryBase，
第二层建立AbstractClassFactory<Base>，统一某一类Base的创建接口： 规定创建什么类型的产品
第三层：决定创建哪个产品；*/
#include "ClassFactory.h"

AbstractClassFactoryBase::AbstractClassFactoryBase(const std::string& className, const std::string& baseClassName)
{
    m_baseClassName = baseClassName;
    m_className = className;
}

const std::string& AbstractClassFactoryBase::GetBaseClassName() const
{
    return m_baseClassName;
}
const std::string& AbstractClassFactoryBase::GetClassName() const
{
    return m_className;
}