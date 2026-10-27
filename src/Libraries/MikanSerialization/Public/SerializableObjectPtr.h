#pragma once

#include "SerializationExport.h"

#ifdef SERIALIZATION_REFLECTION_ENABLED
#include "SerializableObjectPtr.rfkh.h"
#include "Refureku/Refureku.h"
#endif

#include <memory>

namespace rfk
{
class Struct;
};

namespace Serialization NAMESPACE()
{

struct SERIALIZATION_API STRUCT() PolymorphicStruct{
#ifdef SERIALIZATION_REFLECTION_ENABLED
	Serialization_PolymorphicStruct_GENERATED
#endif
};

class SERIALIZATION_API CLASS() PolymorphicObjectPtr
{
public:
	PolymorphicObjectPtr();
	PolymorphicObjectPtr(PolymorphicObjectPtr&& other) noexcept;
	PolymorphicObjectPtr(const PolymorphicObjectPtr& other);
	virtual ~PolymorphicObjectPtr();

	PolymorphicObjectPtr& operator=(PolymorphicObjectPtr&& other) noexcept;
	PolymorphicObjectPtr& operator=(const PolymorphicObjectPtr& other);

	template <typename t_derived_class>
	t_derived_class* allocatedByType()
	{
		auto derivedObjectPtr= std::make_shared<t_derived_class>();
		auto baseObjectPtr= std::static_pointer_cast<PolymorphicStruct>(derivedObjectPtr);

		setPolymorphicStructPtrInternal(baseObjectPtr, t_derived_class::staticGetArchetype());

		return getTypedPointerMutable<t_derived_class>();
	}

	void* allocateByType(rfk::Struct const* objectClass);

	template <typename t_derived_class>
	const t_derived_class* getTypedPointer() const
	{
#ifdef SERIALIZATION_REFLECTION_ENABLED
		// If reflection is enabled,
		// check if the stored object is compatible with the requested type before returning the pointer
		if (isTypeCompatibleWith(t_derived_class::staticGetArchetype()))
		{
			return reinterpret_cast<const t_derived_class*>(getRawPtr());
		}

		return nullptr;
#else
		return reinterpret_cast<const t_derived_class*>(getRawPtr());
#endif
	}

	template <typename t_derived_class>
	t_derived_class* getTypedPointerMutable()
	{
		return reinterpret_cast<t_derived_class*>(getRawPtrMutable());
	}

	void reset();

	// Methods invoked by reflection (serialization code). Class names cross as const char*, never
	// std::string, since a client built against another CRT lays std::string out differently.
	METHOD() void* allocateByClassName(const char* className);

	METHOD() const void* getRawPtr() const;

	METHOD() void* getRawPtrMutable();

	// Empty when nothing is stored. Owned by the pointer, valid until it is next assigned or reset.
	METHOD() const char* getRuntimeClassName() const;

#ifdef SERIALIZATION_REFLECTION_ENABLED
Serialization_PolymorphicObjectPtr_GENERATED
#endif

	protected : bool
				isTypeCompatibleWith(rfk::Struct const& objectClass) const;
	void setPolymorphicStructPtrInternal(std::shared_ptr<PolymorphicStruct> objPtr, rfk::Struct const& objectClass);

	struct PolymorphicObjectPtrImpl* m_impl= nullptr;
};
}; // namespace Serialization

#ifdef SERIALIZATION_REFLECTION_ENABLED
File_SerializableObjectPtr_GENERATED
#endif