#include "SerializationProperty.h"
#include "SerializationProperty.rfks.h"

#include "string.h"

namespace Serialization
{
// -- EnumStringValue -----
EnumStringValue::EnumStringValue(char const* value) noexcept
{
	if (value != nullptr)
	{
		size_t len= strlen(value);
		m_stringValue= new char[len + 1];
		std::memmove(m_stringValue, value, len);
		m_stringValue[len]= '\0';
	}
	else
	{
		m_stringValue= nullptr;
	}
}

EnumStringValue::~EnumStringValue()
{
	if (m_stringValue != nullptr)
	{
		delete[] m_stringValue;
		m_stringValue= nullptr;
	}
}

rfk::EnumValue const* findEnumValueByString(rfk::Enum const& enumType, char const* stringValue)
{
	// First try and find a value with the EnumStringValue property that has a matching string value
	rfk::EnumValue const* enumValue= enumType.getEnumValueByPredicate(
		[](rfk::EnumValue const& ev, void* userdata)
		{
			const char* desiredValue= (const char*)userdata;
			auto const* property= ev.getProperty<Serialization::EnumStringValue>();
			if (property != nullptr)
			{
				return std::strcmp(property->getValue(), desiredValue) == 0;
			}
			return false;
		},
		(void*)stringValue);

	// Fall back to searching by full enum value name
	if (enumValue == nullptr)
	{
		enumValue= enumType.getEnumValueByName(stringValue);
	}

	return enumValue;
}

char const* getEnumStringValue(rfk::EnumValue const& enumValue)
{
	auto const* property= enumValue.getProperty<Serialization::EnumStringValue>();
	if (property != nullptr)
	{
		return property->getValue();
	}

	return enumValue.getName();
}

static char* duplicatePropertyString(char const* value)
{
	assert(value != nullptr && value[0] != '\0');
	size_t len= strlen(value);
	char* copy= new char[len + 1];
	std::memmove(copy, value, len);
	copy[len]= '\0';

	return copy;
}

// -- CodeGenTag -----
CodeGenModule::CodeGenModule(char const* moduleName) noexcept
	: m_moduleName(duplicatePropertyString(moduleName))
{
}

CodeGenModule::~CodeGenModule() { delete[] m_moduleName; }

// -- ClientComponentNames -----
ClientComponentNames::ClientComponentNames(char const* componentClassName, char const* ownerSystemName) noexcept
	: m_componentClassName(duplicatePropertyString(componentClassName))
	, m_ownerSystemName(duplicatePropertyString(ownerSystemName))
{
}

ClientComponentNames::~ClientComponentNames()
{
	delete[] m_componentClassName;
	delete[] m_ownerSystemName;
}

// -- ClientSystemName -----
ClientSystemName::ClientSystemName(char const* systemName) noexcept
	: m_systemName(duplicatePropertyString(systemName))
{
}

ClientSystemName::~ClientSystemName() { delete[] m_systemName; }

// -- ResponseType -----
ResponseType::ResponseType(char const* responseTypeName) noexcept
	: m_responseTypeName(duplicatePropertyString(responseTypeName))
{
}

ResponseType::~ResponseType() { delete[] m_responseTypeName; }
} // namespace Serialization