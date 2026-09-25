#include "SerializationVisitor.h"
#include "SerializationUtility.h"

#include <Refureku/Refureku.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace Serialization
{
struct AccessorData
{
	void* instance;
	bool isConst;
	rfk::Field const* field;
	rfk::Type const& type;
	std::string name;

	AccessorData(const void* instance, rfk::Field const& field)
		: instance(const_cast<void*>(instance))
		, isConst(true)
		, field(&field)
		, type(field.getType())
		, name(field.getName())
	{
	}

	AccessorData(const void* instance, rfk::Type const& type)
		: instance(const_cast<void*>(instance))
		, isConst(true)
		, field(nullptr)
		, type(type)
		, name(type.getArchetype()->getName())
	{
	}

	AccessorData(void* instance, rfk::Field const& field)
		: instance(instance)
		, isConst(false)
		, field(&field)
		, type(field.getType())
		, name(field.getName())
	{
	}

	AccessorData(void* instance, rfk::Type const& type)
		: instance(instance)
		, isConst(false)
		, field(nullptr)
		, type(type)
		, name(type.getArchetype()->getName())
	{
	}

	AccessorData(const AccessorData& other)
		: instance(other.instance)
		, isConst(other.isConst)
		, field(other.field)
		, type(other.type)
		, name(other.name)
	{
	}

	AccessorData(AccessorData&& other)
		: instance(other.instance)
		, isConst(other.isConst)
		, field(other.field)
		, type(other.type)
		, name(other.name)
	{
	}
};

struct VisitorData
{
	bool hasError= false;
	std::string errorMessage;
};

IVisitor::IVisitor()
	: m_pimpl(new VisitorData())
{
}
IVisitor::~IVisitor() { delete m_pimpl; }

bool IVisitor::hasError() const { return m_pimpl->hasError; }
const std::string& IVisitor::getError() const { return m_pimpl->errorMessage; }
void IVisitor::setError(const std::string& msg)
{
	if (!m_pimpl->hasError)
	{
		m_pimpl->hasError= true;
		m_pimpl->errorMessage= msg;
#ifdef _DEBUG
		__debugbreak();
#endif
	}
}

ValueAccessor::ValueAccessor(const void* instance, rfk::Field const& field)
	: m_pimpl(new AccessorData(instance, field))
{
}

ValueAccessor::ValueAccessor(const void* instance, rfk::Type const& type)
	: m_pimpl(new AccessorData(instance, type))
{
}

ValueAccessor::ValueAccessor(void* instance, rfk::Field const& field)
	: m_pimpl(new AccessorData(instance, field))
{
}

ValueAccessor::ValueAccessor(void* instance, rfk::Type const& type)
	: m_pimpl(new AccessorData(instance, type))
{
}

ValueAccessor::ValueAccessor(const ValueAccessor& other)
	: m_pimpl(new AccessorData(*other.m_pimpl))
{
}

ValueAccessor::ValueAccessor(ValueAccessor&& other)
	: m_pimpl(new AccessorData(*other.m_pimpl))
{
}

ValueAccessor::~ValueAccessor() { delete m_pimpl; }

ValueAccessor& ValueAccessor::operator=(const ValueAccessor& other)
{
	if (this != &other)
	{
		delete m_pimpl;
		m_pimpl= new AccessorData(*other.m_pimpl);
	}

	return *this;
}

const void* ValueAccessor::getInstance() const { return m_pimpl->instance; }

void* ValueAccessor::getInstanceMutable() const
{
	assert(!m_pimpl->isConst);
	return m_pimpl->instance;
}

rfk::Field const* ValueAccessor::getField() const { return m_pimpl->field; }

rfk::Type const& ValueAccessor::getType() const { return m_pimpl->type; }

std::string const& ValueAccessor::getName() const { return m_pimpl->name; }

rfk::Class const* ValueAccessor::getClassType() const { return rfk::classCast(m_pimpl->type.getArchetype()); }

rfk::Struct const* ValueAccessor::getStructType() const { return rfk::structCast(m_pimpl->type.getArchetype()); }

rfk::Enum const* ValueAccessor::getEnumType() const { return rfk::enumCast(m_pimpl->type.getArchetype()); }

std::string ValueAccessor::getTypeName() const
{
	rfk::Archetype const* archetype= m_pimpl->type.getArchetype();

	return archetype != nullptr ? archetype->getName() : "<Null Archetype>";
}

static rfk::Type const& fundamentalRfkType(FundamentalType fundamentalType)
{
	switch (fundamentalType)
	{
	case FundamentalType::Bool:
		return rfk::getType<bool>();
	case FundamentalType::Byte:
		return rfk::getType<int8_t>();
	case FundamentalType::UByte:
		return rfk::getType<uint8_t>();
	case FundamentalType::Short:
		return rfk::getType<int16_t>();
	case FundamentalType::UShort:
		return rfk::getType<uint16_t>();
	case FundamentalType::Int:
		return rfk::getType<int32_t>();
	case FundamentalType::UInt:
		return rfk::getType<uint32_t>();
	case FundamentalType::Long:
		return rfk::getType<int64_t>();
	case FundamentalType::ULong:
		return rfk::getType<uint64_t>();
	case FundamentalType::Float:
		return rfk::getType<float>();
	default:
		return rfk::getType<double>();
	}
}

bool ValueAccessor::isStructType(StructTypeHandle archetype) const
{
	return archetype != nullptr && m_pimpl->type.getArchetype() == archetype;
}

bool ValueAccessor::isFundamentalType(FundamentalType fundamentalType) const
{
	return m_pimpl->type == fundamentalRfkType(fundamentalType);
}

// The instantiation behind the accessed value, or null when it is not a templated class
static rfk::ClassTemplateInstantiation const* templateInstantiationOf(rfk::Type const& type)
{
	rfk::Class const* classType= rfk::classCast(type.getArchetype());
	if (classType == nullptr || classType->getClassKind() != rfk::EClassKind::TemplateInstantiation)
	{
		return nullptr;
	}

	return rfk::classTemplateInstantiationCast(classType);
}

// The type of one of the instantiation's arguments, or null when the index is out of range
static rfk::Type const* templateArgumentTypeOf(rfk::Type const& type, std::size_t index)
{
	rfk::ClassTemplateInstantiation const* instantiation= templateInstantiationOf(type);
	if (instantiation == nullptr || index >= instantiation->getTemplateArgumentsCount())
	{
		return nullptr;
	}

	auto const& argument= static_cast<rfk::TypeTemplateArgument const&>(instantiation->getTemplateArgumentAt(index));

	return &argument.getType();
}

bool ValueAccessor::isTemplateInstantiation() const { return templateInstantiationOf(m_pimpl->type) != nullptr; }

std::string ValueAccessor::getTemplateName() const
{
	rfk::ClassTemplateInstantiation const* instantiation= templateInstantiationOf(m_pimpl->type);

	return instantiation != nullptr ? instantiation->getClassTemplate().getName() : std::string();
}

std::size_t ValueAccessor::getTemplateArgumentCount() const
{
	rfk::ClassTemplateInstantiation const* instantiation= templateInstantiationOf(m_pimpl->type);

	return instantiation != nullptr ? instantiation->getTemplateArgumentsCount() : 0;
}

std::string ValueAccessor::getTemplateArgumentTypeName(std::size_t index) const
{
	rfk::Type const* argumentType= templateArgumentTypeOf(m_pimpl->type, index);
	rfk::Archetype const* archetype= argumentType != nullptr ? argumentType->getArchetype() : nullptr;

	return archetype != nullptr ? archetype->getName() : "<Null Archetype>";
}

bool ValueAccessor::isTemplateArgumentStructType(std::size_t index, StructTypeHandle archetype) const
{
	rfk::Type const* argumentType= templateArgumentTypeOf(m_pimpl->type, index);

	return argumentType != nullptr && archetype != nullptr && argumentType->getArchetype() == archetype;
}

bool ValueAccessor::isTemplateArgumentFundamentalType(std::size_t index, FundamentalType fundamentalType) const
{
	rfk::Type const* argumentType= templateArgumentTypeOf(m_pimpl->type, index);

	return argumentType != nullptr && *argumentType == fundamentalRfkType(fundamentalType);
}

bool ValueAccessor::setEnumValueFromInt(int sourceValue) const
{
	rfk::Enum const* enumType= getEnumType();
	if (enumType == nullptr)
	{
		return false;
	}

	rfk::EnumValue const* enumValue= enumType->getEnumValue(sourceValue);
	if (enumValue == nullptr)
	{
		return false;
	}

	// An enum is written at its underlying type's width, not int64's
	const int64_t enumInt64Value= enumValue->getValue();
	const std::size_t enumByteCount= enumType->getUnderlyingArchetype().getMemorySize();
	void* enumInstance= getInstanceMutable();
	rfk::Field const* enumField= m_pimpl->field;

	if (enumField != nullptr)
	{
		enumField->setUnsafe(enumInstance, &enumInt64Value, enumByteCount);
	}
	else
	{
		std::memcpy(enumInstance, &enumInt64Value, enumByteCount);
	}

	return true;
}

void ValueAccessor::setValueBytes(const void* bytes, std::size_t byteCount) const
{
	rfk::Field const* field= m_pimpl->field;

	if (field != nullptr)
	{
		field->setUnsafe(getInstanceMutable(), bytes, byteCount);
	}
	else
	{
		std::memcpy(getInstanceMutable(), bytes, byteCount);
	}
}

const void* ValueAccessor::getUntypedValuePtr() const
{
	assert(m_pimpl->type.isValue());
	assert(m_pimpl->instance != nullptr);

	if (m_pimpl->field)
	{
		return m_pimpl->field->getPtrUnsafe(m_pimpl->instance);
	}
	else
	{
		return m_pimpl->instance;
	}
}

void* ValueAccessor::getUntypedValueMutablePtr() const
{
	assert(!m_pimpl->isConst);
	return const_cast<void*>(getUntypedValuePtr());
}

FieldList getStructFieldsInWireOrder(StructTypeHandle structType)
{
	if (structType == nullptr)
	{
		return FieldList();
	}

	// Gather all the public, non-static fields on this struct
	FieldList fieldsOnThisStruct;
	structType->foreachField(
		[](rfk::Field const& field, void* userData) -> bool
		{
			FieldList* sortedFieldsPtr= reinterpret_cast<FieldList*>(userData);

			// Skip this field is it is non-public or is static
			if (field.getAccess() != rfk::EAccessSpecifier::Public || field.isStatic())
			{
				return true;
			}

			sortedFieldsPtr->push_back(&field);
			return true;
		},
		&fieldsOnThisStruct, false);

	// Reflection hands these back in no particular order, so memory offset is what puts them
	// in the order the binary encoding concatenates them
	if (fieldsOnThisStruct.size() > 1)
	{
		std::sort(fieldsOnThisStruct.begin(), fieldsOnThisStruct.end(),
				  [](rfk::Field const* a, rfk::Field const* b) { return a->getMemoryOffset() < b->getMemoryOffset(); });
	}

	return fieldsOnThisStruct;
}

void memoryOffsetSortStructFields(rfk::Struct const& structType, FieldList& outFields)
{
	// Recurse into parent structs first, since they will be laid out in memory first
	structType.foreachDirectParent(
		[](rfk::ParentStruct const& parentStruct, void* userData) -> bool
		{
			FieldList* outSortedFieldsPtr= reinterpret_cast<FieldList*>(userData);
			memoryOffsetSortStructFields(parentStruct.getArchetype(), *outSortedFieldsPtr);
			return true;
		},
		&outFields);

	// Then this struct's own fields, in the one order every consumer of the wire format agrees on
	const FieldList fieldsOnThisStruct= getStructFieldsInWireOrder(&structType);

	outFields.insert(outFields.end(), fieldsOnThisStruct.begin(), fieldsOnThisStruct.end());
}

void visitStruct(const void* instance, rfk::Struct const& structType, IVisitor* visitor)
{
	FieldList fields;
	memoryOffsetSortStructFields(structType, fields);

	for (rfk::Field const* field : fields)
	{
		Serialization::visitField(instance, *field, visitor);
		if (visitor->hasError())
			break;
	};
}

void visitStruct(void* instance, rfk::Struct const& structType, IVisitor* visitor)
{
	FieldList fields;
	memoryOffsetSortStructFields(structType, fields);

	for (rfk::Field const* field : fields)
	{
		Serialization::visitField(instance, *field, visitor);
		if (visitor->hasError())
			break;
	};
}

void visitField(const void* instance, rfk::Field const& field, IVisitor* visitor)
{
	// Error if this field is non-public or is static
	if (field.getAccess() != rfk::EAccessSpecifier::Public)
	{
		visitor->setError(stringify("Field ", field.getName(), " was not public"));
		return;
	}
	if (field.isStatic())
	{
		visitor->setError(stringify("Field ", field.getName(), " is static"));
		return;
	}

	visitValue(ValueAccessor(instance, field), visitor);
}

void visitField(void* instance, rfk::Field const& field, IVisitor* visitor)
{
	// Error if this field is non-public or is static
	if (field.getAccess() != rfk::EAccessSpecifier::Public)
	{
		visitor->setError(stringify("Field ", field.getName(), " was not public"));
		return;
	}
	if (field.isStatic())
	{
		visitor->setError(stringify("Field ", field.getName(), " is static"));
		return;
	}

	visitValue(ValueAccessor(instance, field), visitor);
}

void visitValue(ValueAccessor const& accessor, IVisitor* visitor)
{
	if (visitor->hasError())
		return;

	rfk::Type const& fieldType= accessor.getType();
	rfk::Archetype const* fieldArchetype= fieldType.getArchetype();
	rfk::EEntityKind fieldArchetypeKind= fieldArchetype ? fieldArchetype->getKind() : rfk::EEntityKind::Undefined;
	const char* fieldArchetypeName= fieldArchetype ? fieldArchetype->getName() : "";
	const std::string& fieldName= accessor.getName();

	if (fieldArchetypeKind == rfk::EEntityKind::Class)
	{
		rfk::Class const* classType= rfk::classCast(fieldArchetype);

		if (classType != nullptr)
		{
			visitor->visitClass(accessor);
		}
		else
		{
			visitor->setError(stringify("Accessor ", accessor.getName(), " was not an class type"));
		}
	}
	else if (fieldArchetypeKind == rfk::EEntityKind::Struct)
	{
		rfk::Struct const* structType= rfk::structCast(fieldArchetype);

		if (structType != nullptr)
		{
			visitor->visitStruct(accessor);
		}
		else
		{
			visitor->setError(stringify("Accessor ", accessor.getName(), " was not a struct type"));
		}
	}
	else if (fieldArchetypeKind == rfk::EEntityKind::Enum)
	{
		rfk::Enum const* enumType= rfk::enumCast(fieldArchetype);

		if (enumType != nullptr)
		{
			visitor->visitEnum(accessor);
		}
		else
		{
			visitor->setError(stringify("Accessor ", accessor.getName(), " was not an enum type"));
		}
	}
	else if (fieldArchetypeKind == rfk::EEntityKind::FundamentalArchetype)
	{
		if (fieldType == rfk::getType<bool>())
		{
			visitor->visitBool(accessor);
		}
		else if (fieldType == rfk::getType<uint8_t>())
		{
			visitor->visitUByte(accessor);
		}
		else if (fieldType == rfk::getType<int8_t>())
		{
			visitor->visitByte(accessor);
		}
		else if (fieldType == rfk::getType<uint16_t>())
		{
			visitor->visitUShort(accessor);
		}
		else if (fieldType == rfk::getType<int16_t>())
		{
			visitor->visitShort(accessor);
		}
		else if (fieldType == rfk::getType<uint32_t>())
		{
			visitor->visitUInt(accessor);
		}
		else if (fieldType == rfk::getType<int32_t>())
		{
			visitor->visitInt(accessor);
		}
		else if (fieldType == rfk::getType<uint64_t>())
		{
			visitor->visitULong(accessor);
		}
		else if (fieldType == rfk::getType<int64_t>())
		{
			visitor->visitLong(accessor);
		}
		else if (fieldType == rfk::getType<float>())
		{
			visitor->visitFloat(accessor);
		}
		else if (fieldType == rfk::getType<double>())
		{
			visitor->visitDouble(accessor);
		}
		else
		{
			visitor->setError(stringify("Accessor ", accessor.getName(), " has unsupported type"));
		}
	}
	else
	{
		visitor->setError(stringify("Unsupported archetype kind ", (int)fieldArchetypeKind));
	}
}
} // namespace Serialization