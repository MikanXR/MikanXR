#pragma once

#include "SerializationExport.h"

#if defined(ENABLE_SERIALIZATION_REFLECTION) || defined(KODGEN_PARSING)
#include <Refureku/Properties/PropertySettings.h>
#include <Refureku/TypeInfo/Archetypes/EnumValue.h>

#ifndef KODGEN_PARSING
#include "SerializationProperty.rfkh.h"
#endif // !KODGEN_PARSING

namespace rfk
{
class Enum;
class EnumValue;
} // namespace rfk

namespace Serialization NAMESPACE()
{
class SERIALIZATION_API CLASS(rfk::PropertySettings(rfk::EEntityKind::EnumValue, false, true)) EnumStringValue
	: public rfk::Property
{
public:
	EnumStringValue() noexcept= default;
	EnumStringValue(char const* message) noexcept;
	virtual ~EnumStringValue();

	char const* getValue() const { return m_stringValue; }

#ifndef KODGEN_PARSING
Serialization_EnumStringValue_GENERATED
#endif // !KODGEN_PARSING

	private : char* m_stringValue= nullptr;
};

class SERIALIZATION_API
	CLASS(rfk::PropertySettings(rfk::EEntityKind::Class | rfk::EEntityKind::Struct | rfk::EEntityKind::Enum, true,
								true)) CodeGenModule : public rfk::Property
{
public:
	CodeGenModule() noexcept= default;
	CodeGenModule(char const* moduleName) noexcept;
	virtual ~CodeGenModule();

	char const* getModuleName() const { return m_moduleName; }

#ifndef KODGEN_PARSING
Serialization_CodeGenModule_GENERATED
#endif // !KODGEN_PARSING

	private : char* m_moduleName= nullptr;
};

// The names a client addresses a component class by in the property requests: the component
// class and the object system that owns it. Carried by the Mikan*ComponentValues structs, where
// the codegen turns it into constants on the generated types, so a client never spells them.
class SERIALIZATION_API
	CLASS(rfk::PropertySettings(rfk::EEntityKind::Class | rfk::EEntityKind::Struct, false, false)) ClientComponentNames
	: public rfk::Property
{
public:
	ClientComponentNames() noexcept= default;
	ClientComponentNames(char const* componentClassName, char const* ownerSystemName) noexcept;
	virtual ~ClientComponentNames();

	char const* getComponentClassName() const { return m_componentClassName; }
	char const* getOwnerSystemName() const { return m_ownerSystemName; }

#ifndef KODGEN_PARSING
Serialization_ClientComponentNames_GENERATED
#endif // !KODGEN_PARSING

	private : char* m_componentClassName= nullptr;
	char* m_ownerSystemName= nullptr;
};

// The name a client addresses an object system by, carried by the Mikan*SystemValues structs
class SERIALIZATION_API
	CLASS(rfk::PropertySettings(rfk::EEntityKind::Class | rfk::EEntityKind::Struct, false, false)) ClientSystemName
	: public rfk::Property
{
public:
	ClientSystemName() noexcept= default;
	ClientSystemName(char const* systemName) noexcept;
	virtual ~ClientSystemName();

	char const* getSystemName() const { return m_systemName; }

#ifndef KODGEN_PARSING
Serialization_ClientSystemName_GENERATED
#endif // !KODGEN_PARSING

	private : char* m_systemName= nullptr;
};

SERIALIZATION_API rfk::EnumValue const* findEnumValueByString(rfk::Enum const& enumType, char const* stringValue);
SERIALIZATION_API char const* getEnumStringValue(rfk::EnumValue const& enumValue);
} // namespace Serialization

// Helper macros to assign a reflection properties
#define ENUMVALUE_STRING(enumString) ENUMVALUE(Serialization::EnumStringValue(enumString))

#ifndef KODGEN_PARSING
File_SerializationProperty_GENERATED
#endif // !KODGEN_PARSING

#else

#ifndef ENUMVALUE_STRING
#define ENUMVALUE_STRING(...)
#endif

#endif // ENABLE_SERIALIZATION_REFLECTION || KODGEN_PARSING