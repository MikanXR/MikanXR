#include "SerializableObjectPtr.h"
#include "SerializableObjectPtr.rfks.h"
#include "TypeRegistry.h"

#include <string>

namespace Serialization
{
struct PolymorphicObjectPtrImpl
{
	std::shared_ptr<PolymorphicStruct> serializableStructPtr;
	std::string runtimeClassName;
	void* runtimeClassRawPtr;
};

PolymorphicObjectPtr::PolymorphicObjectPtr()
{
	m_impl= new PolymorphicObjectPtrImpl;
	m_impl->serializableStructPtr= nullptr;
	m_impl->runtimeClassName= "";
	m_impl->runtimeClassRawPtr= nullptr;
}

PolymorphicObjectPtr::PolymorphicObjectPtr(PolymorphicObjectPtr&& other) noexcept
	: m_impl(std::move(other.m_impl))
{
	other.m_impl= nullptr;
}

PolymorphicObjectPtr::PolymorphicObjectPtr(const PolymorphicObjectPtr& other)
{
	m_impl= new PolymorphicObjectPtrImpl;
	m_impl->serializableStructPtr= other.m_impl->serializableStructPtr;
	m_impl->runtimeClassName= other.m_impl->runtimeClassName;
	m_impl->runtimeClassRawPtr= other.m_impl->runtimeClassRawPtr;
}

PolymorphicObjectPtr& PolymorphicObjectPtr::operator=(PolymorphicObjectPtr&& other) noexcept
{
	if (this != &other)
	{
		reset();
		m_impl= std::move(other.m_impl);
		other.m_impl= nullptr;
	}

	return *this;
}

PolymorphicObjectPtr& PolymorphicObjectPtr::operator=(const PolymorphicObjectPtr& other)
{
	if (this != &other)
	{
		reset();
		m_impl->serializableStructPtr= other.m_impl->serializableStructPtr;
		m_impl->runtimeClassName= other.m_impl->runtimeClassName;
		m_impl->runtimeClassRawPtr= other.m_impl->runtimeClassRawPtr;
	}

	return *this;
}

PolymorphicObjectPtr::~PolymorphicObjectPtr()
{
	if (m_impl != nullptr)
	{
		delete m_impl;
		m_impl= nullptr;
	}
}

void PolymorphicObjectPtr::reset()
{
	assert(m_impl != nullptr);
	m_impl->serializableStructPtr.reset();
	m_impl->runtimeClassName= "";
	m_impl->runtimeClassRawPtr= nullptr;
}

void* PolymorphicObjectPtr::allocateByClassName(const char* className)
{
	return className != nullptr ? allocateByType(TypeRegistry::getStructByName(className)) : nullptr;
}

void* PolymorphicObjectPtr::allocateByType(rfk::Struct const* objectClass)
{
	if (objectClass != nullptr)
	{
		auto objectPtr= objectClass->makeSharedInstance<PolymorphicStruct>();

		setPolymorphicStructPtrInternal(objectPtr, *objectClass);

		return getRawPtrMutable();
	}

	return nullptr;
}

const void* PolymorphicObjectPtr::getRawPtr() const { return m_impl->runtimeClassRawPtr; }

void* PolymorphicObjectPtr::getRawPtrMutable() { return const_cast<void*>(getRawPtr()); }

const char* PolymorphicObjectPtr::getRuntimeClassName() const { return m_impl->runtimeClassName.c_str(); }

bool PolymorphicObjectPtr::isTypeCompatibleWith(rfk::Struct const& objectClass) const
{
	if (m_impl->serializableStructPtr != nullptr)
	{
		rfk::Struct const* runtimeClass= TypeRegistry::getStructByName(m_impl->runtimeClassName);

		if (runtimeClass != nullptr)
		{
			return runtimeClass->getId() == objectClass.getId()
				   || runtimeClass->isSubclassOf(objectClass); // Strict subclass check
		}
	}

	return false;
}

void PolymorphicObjectPtr::setPolymorphicStructPtrInternal(std::shared_ptr<PolymorphicStruct> objPtr,
														   rfk::Struct const& objectClass)
{
	uint8_t* rawObjPtr= reinterpret_cast<uint8_t*>(objPtr.get());

	std::ptrdiff_t pointerOffset;
	if (!PolymorphicStruct::staticGetArchetype().getSubclassPointerOffset(objectClass, pointerOffset))
	{
		pointerOffset= 0;
	}

	m_impl->serializableStructPtr= objPtr;
	m_impl->runtimeClassName= objectClass.getName();
	m_impl->runtimeClassRawPtr= rawObjPtr - pointerOffset;
}
} // namespace Serialization