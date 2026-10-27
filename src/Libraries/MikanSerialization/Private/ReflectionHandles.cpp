#include "ReflectionHandles.h"
#include "SerializationProperty.h"

#include <Refureku/Refureku.h>

#include <unordered_set>

namespace Serialization
{
const char* getStructTypeName(StructTypeHandle structType)
{
	return structType != nullptr ? structType->getName() : "";
}

const char* getEnumTypeName(EnumTypeHandle enumType) { return enumType != nullptr ? enumType->getName() : ""; }

void forEachEnumValue(EnumTypeHandle enumType, const std::function<void(const char* valueName, int64_t value)>& visitor)
{
	if (enumType == nullptr)
	{
		return;
	}

	const std::size_t valueCount= enumType->getEnumValuesCount();
	for (std::size_t valueIndex= 0; valueIndex < valueCount; ++valueIndex)
	{
		rfk::EnumValue const& enumValue= enumType->getEnumValueAt(valueIndex);

		visitor(getEnumStringValue(enumValue), enumValue.getValue());
	}
}

const char* getEnumValueName(EnumTypeHandle enumType, int64_t value)
{
	rfk::EnumValue const* enumValue= enumType != nullptr ? enumType->getEnumValue(value) : nullptr;

	return enumValue != nullptr ? getEnumStringValue(*enumValue) : nullptr;
}

bool findEnumValueByName(EnumTypeHandle enumType, const char* valueName, int64_t& outValue)
{
	if (enumType == nullptr || valueName == nullptr)
	{
		return false;
	}

	rfk::EnumValue const* enumValue= findEnumValueByString(*enumType, valueName);
	if (enumValue == nullptr)
	{
		return false;
	}

	outValue= enumValue->getValue();
	return true;
}

const char* getResponseTypeName(StructTypeHandle requestType)
{
	auto const* responseType= requestType != nullptr ? requestType->getProperty<ResponseType>() : nullptr;

	return responseType != nullptr ? responseType->getResponseTypeName() : nullptr;
}

static void forEachSubclassRecursive(StructTypeHandle structType,
									 const std::function<void(StructTypeHandle subclass)>& visitor,
									 std::unordered_set<StructTypeHandle>& visited)
{
	for (rfk::Struct const* subclass : structType->getDirectSubclasses())
	{
		// A struct reached through two parents is still visited once
		if (subclass == nullptr || !visited.insert(subclass).second)
		{
			continue;
		}

		visitor(subclass);
		forEachSubclassRecursive(subclass, visitor, visited);
	}
}

void forEachSubclass(StructTypeHandle structType, const std::function<void(StructTypeHandle subclass)>& visitor)
{
	if (structType == nullptr)
	{
		return;
	}

	std::unordered_set<StructTypeHandle> visited;
	forEachSubclassRecursive(structType, visitor, visited);
}
} // namespace Serialization
