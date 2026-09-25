#include "JsonSerializer.h"
#include "SerializationUtility.h"
#include "SerializableList.h"
#include "SerializableMap.h"
#include "SerializableObjectPtr.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#include "TypeRegistry.h"

#include "nlohmann/json.hpp"
#include "Refureku/Refureku.h"

using json= nlohmann::json;

namespace Serialization
{
// Adapter used to convert a Serialization::String into a JSON string value
inline void to_json(nlohmann::json& j, const String& s) { j= s.getUtf8Value(); }

class JsonWriteVisitor : public IVisitor
{
public:
	JsonWriteVisitor(json& jsonObject)
		: m_jsonObject(jsonObject)
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
			visitObjectPtr(accessor, m_jsonObject);
		}
		else if (classKind == rfk::EClassKind::TemplateInstantiation)
		{
			const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(fieldClassType);
			std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

			// See if the field is a Serialization::List<T>
			if (templateTypeName == "List" && templateClassInstanceType->getTemplateArgumentsCount() == 1)
			{
				visitList(accessor, *templateClassInstanceType, m_jsonObject);
			}
			// See if the field is a Serialization::Map<K,V>
			else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
			{
				visitMap(accessor, *templateClassInstanceType, m_jsonObject);
			}
			else
			{
				setError(stringify("JsonWriteVisitor::visitClass() ", "Class Field ", accessor.getName(),
								   " was of expected type"));
				return;
			}
		}
		else
		{
			JsonWriteVisitor::visitStruct(accessor);
		}
	}

	void visitObjectPtr(ValueAccessor const& accessor, json& ownerJsonObject)
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
				setError(stringify("JsonWriteVisitor::visitObjectPtr() ", "TypedObjectPtr Accessor ",
								   accessor.getName(), " has an unknown class name ", className));
				return;
			}
		}

		// Serialize the class name and object value
		json objectPtrJson= json::object();
		objectPtrJson["class_name"]= className;

		// Get the raw pointer to the object pointed to by the shared pointer
		rfk::Method const* getRawPtrMethod=
			objPtrClassType->getMethodByName("getRawPtr", rfk::EMethodFlags::Default, true);
		const void* objectInstance= getRawPtrMethod->invokeUnsafe<const void*>(objPtrInstance);

		// Serialize the object into json
		json objectJson= json::object();
		if (objectInstance != nullptr)
		{
			JsonWriteVisitor elementVisitor(objectJson);
			Serialization::visitStruct(objectInstance, *objectStruct, &elementVisitor);
			if (elementVisitor.hasError())
			{
				setError(elementVisitor.getError());
				return;
			}
		}

		// Add the child serialized object
		objectPtrJson["value"]= objectJson;

		// Add the json array to the owner json object
		ownerJsonObject[accessor.getName()]= objectPtrJson;
	}

	void visitList(ValueAccessor const& arrayAccessor, rfk::ClassTemplateInstantiation const& templatedArrayType,
				   json& ownerJsonObject)
	{
		const void* arrayInstance= arrayAccessor.getUntypedValuePtr();

		// Get the type of the elements in the array from the template argument
		auto const& templateArg=
			static_cast<rfk::TypeTemplateArgument const&>(templatedArrayType.getTemplateArgumentAt(0));
		rfk::Type const& elementType= templateArg.getType();

		// Use reflection to get the methods to resize the array and get a mutable reference to an element
		rfk::Method const* getSizeMethod= templatedArrayType.getMethodByName("size");
		rfk::Method const* getRawElementMethod= templatedArrayType.getMethodByName("getRawElement");

		// Resize the array to the desired target size
		std::size_t arraySize= getSizeMethod->invokeUnsafe<std::size_t>(arrayInstance, arraySize);
		json arrayJson= json::array();

		// Serialize each element of the array
		for (size_t elementIndex= 0; elementIndex < arraySize; ++elementIndex)
		{
			// Get the target element instance in the array
			const void* elementInstance=
				getRawElementMethod->invokeUnsafe<const void*, const std::size_t&>(arrayInstance, elementIndex);

			// Make a fake "field" for an element in the array
			ValueAccessor elementAccessor(elementInstance, elementType);

			// Serialize the element into the json object
			json elementJson;
			JsonWriteVisitor elementVisitor(elementJson);
			Serialization::visitValue(elementAccessor, &elementVisitor);
			if (elementVisitor.hasError())
			{
				setError(elementVisitor.getError());
				return;
			}

			// Add the json element to the json array
			arrayJson.push_back(elementJson);
		}

		// Add the json array to the owner json object
		ownerJsonObject[arrayAccessor.getName()]= arrayJson;
	}

	void visitMap(ValueAccessor const& mapAccessor, rfk::ClassTemplateInstantiation const& templatedMapType,
				  json& ownerJsonObject)
	{
		// Get the key type of the map from the template argument
		auto const& templateKeyArg=
			static_cast<rfk::TypeTemplateArgument const&>(templatedMapType.getTemplateArgumentAt(0));
		rfk::Type const& keyType= templateKeyArg.getType();

		if (keyType == rfk::getType<int32_t>())
		{
			visitMapOfKey<int32_t>(mapAccessor, templatedMapType, ownerJsonObject);
		}
		else if (keyType == rfk::getType<std::string>())
		{
			visitMapOfKey<std::string>(mapAccessor, templatedMapType, ownerJsonObject);
		}
		else if (keyType == rfk::getType<Serialization::String>())
		{
			visitMapOfKey<Serialization::String>(mapAccessor, templatedMapType, ownerJsonObject);
		}
		else
		{
			rfk::Archetype const* keyArchetype= keyType.getArchetype();

			setError(stringify("JsonWriteVisitor::visitMap() ", "Map Key Archetype ",
							   keyArchetype != nullptr ? keyArchetype->getName() : "<Null Archetype>",
							   " is not supported"));
			return;
		}
	}

	template <typename t_key>
	void visitMapOfKey(ValueAccessor const& mapAccessor, rfk::ClassTemplateInstantiation const& templatedMapType,
					   json& ownerJsonObject)
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
		json arrayJson= json::array();
		for (auto enumerator= getConstEnumeratorMethod->invokeUnsafe<std::shared_ptr<IMapConstEnumerator>>(mapInstance);
			 enumerator->isValid(); enumerator->next())
		{
			json pairJson;

			// Serialize the key
			const void* rawKey= enumerator->getKeyRaw();
			pairJson["key"]= *reinterpret_cast<const t_key*>(rawKey);

			// Serialize the value
			json valueJson;
			const void* rawValue= enumerator->getValueRaw();
			ValueAccessor valueAccessor(rawValue, valueType);
			JsonWriteVisitor valueVisitor(valueJson);
			Serialization::visitValue(valueAccessor, &valueVisitor);
			if (valueVisitor.hasError())
			{
				setError(valueVisitor.getError());
				return;
			}
			pairJson["value"]= valueJson;

			// Add the key-value json object to the json array
			arrayJson.push_back(pairJson);
		}

		// Add the json map-array to the owner json object
		ownerJsonObject[mapAccessor.getName()]= arrayJson;
	}

	virtual void visitStruct(ValueAccessor const& accessor) override
	{
		const void* childObjectInstance= accessor.getUntypedValuePtr();
		rfk::Struct const* structType= accessor.getStructType();
		rfk::Field const* field= accessor.getField();

		if (field != nullptr)
		{
			char const* fieldName= field->getName();
			json childJsonObject;
			JsonWriteVisitor jsonVisitor(childJsonObject);
			Serialization::visitStruct(childObjectInstance, *structType, &jsonVisitor);
			if (jsonVisitor.hasError())
			{
				setError(jsonVisitor.getError());
				return;
			}

			m_jsonObject[fieldName]= childJsonObject;
		}
		else
		{
			Serialization::visitStruct(childObjectInstance, *structType, this);
		}
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
			setError(stringify("JsonWriteVisitor::visitEnum() ", "Enum Accessor ", accessor.getName(),
							   " has an invalid memory size ", enumArchetype.getMemorySize()));
			return;
		}

		rfk::EnumValue const* enumValue= enumType.getEnumValue(enumIntValue);
		if (enumValue == nullptr)
		{
			setError(stringify("JsonWriteVisitor::visitEnum() ", "Enum Accessor ", accessor.getName(),
							   " has an invalid int value ", enumIntValue));
			return;
		}

		std::string enumValueString= Serialization::getEnumStringValue(*enumValue);
		rfk::Field const* field= accessor.getField();
		if (field != nullptr)
		{
			char const* fieldName= field->getName();

			m_jsonObject[fieldName]= enumValueString;
		}
		else
		{
			m_jsonObject= enumValueString;
		}
	}

	virtual void visitBool(ValueAccessor const& accessor) override { setJsonValueFromAccessor<bool>(accessor); }

	virtual void visitByte(ValueAccessor const& accessor) override { setJsonValueFromAccessor<int8_t>(accessor); }

	virtual void visitUByte(ValueAccessor const& accessor) override { setJsonValueFromAccessor<uint8_t>(accessor); }

	virtual void visitShort(ValueAccessor const& accessor) override { setJsonValueFromAccessor<int16_t>(accessor); }

	virtual void visitUShort(ValueAccessor const& accessor) override { setJsonValueFromAccessor<uint16_t>(accessor); }

	virtual void visitInt(ValueAccessor const& accessor) override { setJsonValueFromAccessor<int32_t>(accessor); }

	virtual void visitUInt(ValueAccessor const& accessor) override { setJsonValueFromAccessor<uint32_t>(accessor); }

	virtual void visitLong(ValueAccessor const& accessor) override { setJsonValueFromAccessor<int64_t>(accessor); }

	virtual void visitULong(ValueAccessor const& accessor)
	{
		setError(stringify("JsonWriteVisitor::visitULong() ", "ULong Accessor ", accessor.getName(),
						   " type not supported by all JSON libraries"));
		return;
	}

	virtual void visitFloat(ValueAccessor const& accessor) override { setJsonValueFromAccessor<float>(accessor); }

	virtual void visitDouble(ValueAccessor const& accessor) override { setJsonValueFromAccessor<double>(accessor); }

private:
	void visitString(ValueAccessor const& accessor) const
	{
		rfk::Field const* field= accessor.getField();
		const auto* stringPtr= accessor.getTypedValuePtr<Serialization::String>();

		if (field != nullptr)
		{
			char const* fieldName= field->getName();

			m_jsonObject[fieldName]= stringPtr->getUtf8Value();
		}
		else
		{
			// The json object should contain the value we want to deserialize
			m_jsonObject= stringPtr->getUtf8Value();
		}
	}

	template <typename T>
	void setJsonValueFromAccessor(ValueAccessor const& accessor) const
	{
		rfk::Field const* field= accessor.getField();

		if (field != nullptr)
		{
			char const* fieldName= field->getName();

			m_jsonObject[fieldName]= *accessor.getTypedValuePtr<T>();
		}
		else
		{

			// The json object should contain the value we want to deserialize
			m_jsonObject= *accessor.getTypedValuePtr<T>();
		}
	}

	nlohmann::json& m_jsonObject;
};

// Public API
bool serializeToJsonString(const void* instance, StructTypeHandle structType, std::string& jsonString,
						   std::string& outErrorMsg)
{
	json jsonObject;

	if (serializeToJson(instance, structType, jsonObject, outErrorMsg))
	{
		jsonString= jsonObject.dump();
		return true;
	}

	return false;
}

bool serializeToJson(const void* instance, StructTypeHandle structType, nlohmann::json& jsonObject,
					 std::string& outErrorMsg)
{
	JsonWriteVisitor visitor(jsonObject);
	Serialization::visitStruct(const_cast<void*>(instance), *structType, &visitor);

	if (visitor.hasError())
	{
		outErrorMsg= visitor.getError();
		return false;
	}

	return true;
}
}; // namespace Serialization
