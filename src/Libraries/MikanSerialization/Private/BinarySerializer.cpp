#include "BinarySerializer.h"
#include "BinaryUtility.h"
#include "SerializationUtility.h"
#include "SerializableList.h"
#include "SerializableMap.h"
#include "SerializableObjectPtr.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#include "TypeRegistry.h"

#include "Refureku/Refureku.h"

namespace Serialization
{
class BinaryWriteVisitor : public IVisitor
{
public:
	BinaryWriteVisitor(BinaryWriter& writer)
		: m_binaryWriter(writer)
	{
	}

	virtual void visitClass(ValueAccessor const& accessor) override
	{
		rfk::Type const& fieldType= accessor.getType();
		rfk::Class const* fieldClassType= accessor.getClassType();
		rfk::EClassKind classKind= fieldClassType->getClassKind();

		if (fieldType == rfk::getType<Serialization::String>())
		{
			visitString(accessor);
		}
		else if (fieldType == rfk::getType<Serialization::PolymorphicObjectPtr>())
		{
			visitObjectPtr(accessor);
		}
		else if (classKind == rfk::EClassKind::TemplateInstantiation)
		{
			const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(fieldClassType);
			std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

			// See if the field is a Serialization::List<T>
			if (templateTypeName == "List" && templateClassInstanceType->getTemplateArgumentsCount() == 1)
			{
				visitList(accessor, *templateClassInstanceType);
			}
			// See if the field is a Serialization::Map<K,V>
			else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
			{
				visitMap(accessor, *templateClassInstanceType);
			}
			else
			{
				setError(stringify("BinaryWriteVisitor::visitClass() ", "Class Field ", accessor.getName(),
								   " was of expected type"));
				return;
			}
		}
		else
		{
			BinaryWriteVisitor::visitStruct(accessor);
		}
	}

	void visitString(ValueAccessor const& accessor)
	{
		const auto* stringPtr= accessor.getTypedValuePtr<Serialization::String>();

		// Pass the String directly (not getValue()): a raw const char* would bind to the bool
		// to_binary overload (standard pointer->bool beats the const char*->std::string
		// user-defined conversion), serializing a single byte instead of the string.
		to_binary(m_binaryWriter, *stringPtr);
	}

	void visitObjectPtr(ValueAccessor const& accessor)
	{
		// Get the shared pointer instance
		const void* objPtrInstance= accessor.getUntypedValuePtr();
		rfk::Class const* objPtrClassType= accessor.getClassType();

		// Use reflection to get the runtime class name of the object pointed at
		rfk::Method const* getRuntimeClassNameMethod=
			objPtrClassType->getMethodByName("getRuntimeClassName", rfk::EMethodFlags::Default, true);
		const std::string className= getRuntimeClassNameMethod->invokeUnsafe<std::string>(objPtrInstance);

		// Get the runtime class for the object
		rfk::Struct const* objectStruct= nullptr;
		if (!className.empty())
		{
			// If the class name is not empty, look up the struct in the TypeRegistry
			objectStruct= TypeRegistry::getStructByName(className);
			if (objectStruct == nullptr)
			{
				setError(stringify("BinaryWriteVisitor::visitObjectPtr() ", "TypedObjectPtr Accessor ",
								   accessor.getName(), " has an unknown class name ", className));
				return;
			}
		}

		// Write the runtime class name of the object
		to_binary(m_binaryWriter, className);

		// Get the raw pointer to the object pointed to by the shared pointer
		rfk::Method const* getRawPtrMethod=
			objPtrClassType->getMethodByName("getRawPtr", rfk::EMethodFlags::Default, true);
		const void* objectInstance= getRawPtrMethod->invokeUnsafe<const void*>(objPtrInstance);

		// Write out whether the object is valid or not
		bool isValidObject= objectInstance != nullptr;
		to_binary(m_binaryWriter, isValidObject);

		// Serialize the object
		if (isValidObject)
		{
			Serialization::visitStruct(objectInstance, *objectStruct, this);
		}
	}

	void visitList(ValueAccessor const& arrayAccessor, rfk::ClassTemplateInstantiation const& templatedArrayType)
	{
		const void* arrayInstance= arrayAccessor.getUntypedValuePtr();

		// Get the type of the elements in the array from the template argument
		auto const& templateArg=
			static_cast<rfk::TypeTemplateArgument const&>(templatedArrayType.getTemplateArgumentAt(0));
		rfk::Type const& elementType= templateArg.getType();

		// Use reflection to get the methods to resize the array and get a reference to an element
		rfk::Method const* getSizeMethod= templatedArrayType.getMethodByName("size");
		rfk::Method const* getRawElementMethod= templatedArrayType.getMethodByName("getRawElement");

		// Write the size of the array
		int32_t arraySize= (int32_t)getSizeMethod->invokeUnsafe<std::size_t>(arrayInstance, arraySize);
		to_binary(m_binaryWriter, arraySize);

		// Serialize each element of the array
		for (size_t elementIndex= 0; elementIndex < arraySize; ++elementIndex)
		{
			// Get the target element instance in the array
			const void* elementInstance=
				getRawElementMethod->invokeUnsafe<const void*, const std::size_t&>(arrayInstance, elementIndex);

			// Make a fake "field" for an element in the array
			ValueAccessor elementAccessor(elementInstance, elementType);

			// Serialize the element into the json object
			Serialization::visitValue(elementAccessor, this);
		}
	}

	void visitMap(ValueAccessor const& mapAccessor, rfk::ClassTemplateInstantiation const& templatedMapType)
	{
		// Get the key type of the map from the template argument
		auto const& templateKeyArg=
			static_cast<rfk::TypeTemplateArgument const&>(templatedMapType.getTemplateArgumentAt(0));
		rfk::Type const& keyType= templateKeyArg.getType();

		// Use reflection to get the the number of elements in the map
		rfk::Method const* getSizeMethod= templatedMapType.getMethodByName("size");

		// Write the number of elements in the map
		const void* mapInstance= mapAccessor.getUntypedValuePtr();
		int32_t arraySize= (int32_t)getSizeMethod->invokeUnsafe<std::size_t>(mapInstance);
		to_binary(m_binaryWriter, arraySize);

		// Do serialization based on the key type
		if (keyType == rfk::getType<int32_t>())
		{
			visitMapOfKey<int32_t>(mapAccessor, templatedMapType);
		}
		else if (keyType == rfk::getType<std::string>())
		{
			visitMapOfKey<std::string>(mapAccessor, templatedMapType);
		}
		else if (keyType == rfk::getType<Serialization::String>())
		{
			visitMapOfKey<Serialization::String>(mapAccessor, templatedMapType);
		}
		else
		{
			rfk::Archetype const* keyArchetype= keyType.getArchetype();

			setError(stringify("BinaryWriteVisitor::visitMap() ", "Map Key Archetype ",
							   keyArchetype != nullptr ? keyArchetype->getName() : "<Null Archetype>",
							   " is not supported"));
			return;
		}
	}

	template <typename t_key>
	void visitMapOfKey(ValueAccessor const& mapAccessor, rfk::ClassTemplateInstantiation const& templatedMapType)
	{
		// Get the type of the elements in the array from the template argument
		auto const& templateValueArg=
			static_cast<rfk::TypeTemplateArgument const&>(templatedMapType.getTemplateArgumentAt(1));
		rfk::Type const& valueType= templateValueArg.getType();

		// Use reflection to get the method used to enumerate key-value pairs in the map
		rfk::Method const* getConstEnumeratorMethod= templatedMapType.getMethodByName("getConstEnumerator");

		// Get a const enumerator to the map
		const void* mapInstance= mapAccessor.getUntypedValuePtr();

		// Serialize each element of the map
		for (auto enumerator= getConstEnumeratorMethod->invokeUnsafe<std::shared_ptr<IMapConstEnumerator>>(mapInstance);
			 enumerator->isValid(); enumerator->next())
		{
			// Serialize the key
			const void* rawKey= enumerator->getKeyRaw();
			const t_key& key= *reinterpret_cast<const t_key*>(rawKey);
			to_binary(m_binaryWriter, key);

			// Serialize the value
			const void* rawValue= enumerator->getValueRaw();
			ValueAccessor valueAccessor(rawValue, valueType);
			Serialization::visitValue(valueAccessor, this);
		}
	}

	virtual void visitStruct(ValueAccessor const& accessor) override
	{
		const void* childObjectInstance= accessor.getUntypedValuePtr();
		rfk::Struct const* structType= accessor.getStructType();

		Serialization::visitStruct(childObjectInstance, *structType, this);
	}

	virtual void visitEnum(ValueAccessor const& accessor) override
	{
		rfk::Enum const& enumType= *accessor.getEnumType();
		rfk::Archetype const& enumArchetype= enumType.getUnderlyingArchetype();
		const void* untypedValue= accessor.getUntypedValuePtr();

		int64_t enumIntValue= 0;
		if (enumArchetype.getMemorySize() == sizeof(int64_t))
		{
			enumIntValue= *reinterpret_cast<const int64_t*>(untypedValue);
		}
		else if (enumArchetype.getMemorySize() == sizeof(int32_t))
		{
			enumIntValue= (int64_t)(*reinterpret_cast<const int32_t*>(untypedValue));
		}
		else if (enumArchetype.getMemorySize() == sizeof(int16_t))
		{
			enumIntValue= (int64_t)(*reinterpret_cast<const int16_t*>(untypedValue));
		}
		else if (enumArchetype.getMemorySize() == sizeof(int8_t))
		{
			enumIntValue= (int64_t)(*reinterpret_cast<const int8_t*>(untypedValue));
		}
		else
		{
			setError(stringify("BinaryWriteVisitor::visitEnum() ", "Enum Accessor ", accessor.getName(),
							   " has an invalid memory size ", enumArchetype.getMemorySize()));
			return;
		}

		rfk::EnumValue const* enumValue= enumType.getEnumValue(enumIntValue);
		if (enumValue == nullptr)
		{
			setError(stringify("BinaryWriteVisitor::visitEnum() ", "Enum Accessor ", accessor.getName(),
							   " has an invalid int value ", enumIntValue));
			return;
		}

		std::string enumValueString= Serialization::getEnumStringValue(*enumValue);
		to_binary(m_binaryWriter, enumValueString);
	}

	virtual void visitBool(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<bool>());
	}

	virtual void visitByte(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<int8_t>());
	}

	virtual void visitUByte(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<uint8_t>());
	}

	virtual void visitShort(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<int16_t>());
	}

	virtual void visitUShort(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<uint16_t>());
	}

	virtual void visitInt(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<int32_t>());
	}

	virtual void visitUInt(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<uint32_t>());
	}

	virtual void visitLong(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<int64_t>());
	}

	virtual void visitULong(ValueAccessor const& accessor)
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<uint64_t>());
	}

	virtual void visitFloat(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<float>());
	}

	virtual void visitDouble(ValueAccessor const& accessor) override
	{
		to_binary(m_binaryWriter, *accessor.getTypedValuePtr<double>());
	}

private:
	BinaryWriter& m_binaryWriter;
};

// Public API
bool serializeToBytes(const void* instance, StructTypeHandle structType, std::vector<uint8_t>& outBytes,
					  std::string& outErrorMsg)
{
	BinaryWriter writer(outBytes);
	BinaryWriteVisitor visitor(writer);
	Serialization::visitStruct(const_cast<void*>(instance), *structType, &visitor);

	if (visitor.hasError())
	{
		outErrorMsg= visitor.getError();
		return false;
	}

	return true;
}
}; // namespace Serialization
