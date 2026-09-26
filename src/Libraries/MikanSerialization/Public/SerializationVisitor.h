#pragma once

#include "SerializationExport.h"
#include "SerializableList.h"
#include "ReflectionHandles.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "assert.h"

namespace rfk
{
class Archetype;
class Enum;
class Field;
class Struct;
using Class= Struct;
class Type;
}; // namespace rfk

namespace Serialization
{
using FieldList= std::vector<rfk::Field const*>;

// The built-in types a reflected field can hold. ValueAccessor::isType resolves to one of
// these for a fundamental and to a struct archetype for everything else, which is what
// lets the accessor answer type questions without its callers naming the reflection
// library. Keep in step with IVisitor's visit methods below.
enum class FundamentalType
{
	Bool,
	Byte,
	UByte,
	Short,
	UShort,
	Int,
	UInt,
	Long,
	ULong,
	Float,
	Double
};

namespace Detail
{
template <typename t_value_type>
struct FundamentalTypeOf;

// clang-format off
template <> struct FundamentalTypeOf<bool>     { static constexpr FundamentalType value= FundamentalType::Bool; };
template <> struct FundamentalTypeOf<int8_t>   { static constexpr FundamentalType value= FundamentalType::Byte; };
template <> struct FundamentalTypeOf<uint8_t>  { static constexpr FundamentalType value= FundamentalType::UByte; };
template <> struct FundamentalTypeOf<int16_t>  { static constexpr FundamentalType value= FundamentalType::Short; };
template <> struct FundamentalTypeOf<uint16_t> { static constexpr FundamentalType value= FundamentalType::UShort; };
template <> struct FundamentalTypeOf<int32_t>  { static constexpr FundamentalType value= FundamentalType::Int; };
template <> struct FundamentalTypeOf<uint32_t> { static constexpr FundamentalType value= FundamentalType::UInt; };
template <> struct FundamentalTypeOf<int64_t>  { static constexpr FundamentalType value= FundamentalType::Long; };
template <> struct FundamentalTypeOf<uint64_t> { static constexpr FundamentalType value= FundamentalType::ULong; };
template <> struct FundamentalTypeOf<float>    { static constexpr FundamentalType value= FundamentalType::Float; };
template <> struct FundamentalTypeOf<double>   { static constexpr FundamentalType value= FundamentalType::Double; };
// clang-format on

// A reflected struct or class carries a staticGetArchetype(); a fundamental does not.
// Taking the archetype's address needs only the forward declaration above, so a type
// question stays compile-time checked without this header including Refureku.
template <typename t_value_type, typename= void>
struct HasStaticArchetype : std::false_type
{
};
template <typename t_value_type>
struct HasStaticArchetype<t_value_type, std::void_t<decltype(t_value_type::staticGetArchetype())>> : std::true_type
{
};
} // namespace Detail

class SERIALIZATION_API ValueAccessor
{
public:
	ValueAccessor(const void* instance, rfk::Field const& field);
	ValueAccessor(const void* instance, rfk::Type const& type);
	ValueAccessor(void* instance, rfk::Field const& field);
	ValueAccessor(void* instance, rfk::Type const& type);
	ValueAccessor(const ValueAccessor& other);
	ValueAccessor(ValueAccessor&& other);
	virtual ~ValueAccessor();

	ValueAccessor& operator=(const ValueAccessor& other);

	const void* getInstance() const;
	void* getInstanceMutable() const;

	rfk::Field const* getField() const;
	rfk::Type const& getType() const;
	std::string const& getName() const;

	rfk::Class const* getClassType() const;
	rfk::Struct const* getStructType() const;
	rfk::Enum const* getEnumType() const;

	const void* getUntypedValuePtr() const;
	void* getUntypedValueMutablePtr() const;

	// Name of the accessed value's archetype, for diagnostics
	std::string getTypeName() const;

	// Does the accessed value have exactly this type?
	template <typename t_value_type>
	bool isType() const
	{
		if constexpr (Detail::HasStaticArchetype<t_value_type>::value)
		{
			return isStructType(&t_value_type::staticGetArchetype());
		}
		else
		{
			return isFundamentalType(Detail::FundamentalTypeOf<t_value_type>::value);
		}
	}

	// Class template instantiation inspection, which is how a List<T> or Map<K,V> field
	// is recognized and its element types recovered.
	bool isTemplateInstantiation() const;
	std::string getTemplateName() const;
	std::size_t getTemplateArgumentCount() const;
	std::string getTemplateArgumentTypeName(std::size_t index) const;

	template <typename t_value_type>
	bool isTemplateArgumentType(std::size_t index) const
	{
		if constexpr (Detail::HasStaticArchetype<t_value_type>::value)
		{
			return isTemplateArgumentStructType(index, &t_value_type::staticGetArchetype());
		}
		else
		{
			return isTemplateArgumentFundamentalType(index, Detail::FundamentalTypeOf<t_value_type>::value);
		}
	}

	// Write an enum field from the integer a client sent. False when the integer names no
	// value of the enum, in which case nothing is written.
	bool setEnumValueFromInt(int sourceValue) const;

	template <typename t_value_type>
	const t_value_type& getTypedValueRef() const
	{
		return *getTypedValuePtr<t_value_type>();
	}

	template <typename t_value_type>
	t_value_type& getTypedValueMutableRef() const
	{
		return *getTypedValueMutablePtr<t_value_type>();
	}

	template <typename t_value_type>
	const t_value_type* getTypedValuePtr() const
	{
		assert(isType<t_value_type>());

		return reinterpret_cast<const t_value_type*>(getUntypedValuePtr());
	}

	template <typename t_value_type>
	t_value_type* getTypedValueMutablePtr() const
	{
		assert(isType<t_value_type>());

		return reinterpret_cast<t_value_type*>(getUntypedValueMutablePtr());
	}

	template <typename t_value_type>
	t_value_type getValue() const
	{
		assert(isType<t_value_type>());
		assert(getInstance() != nullptr);

		return *reinterpret_cast<const t_value_type*>(getUntypedValuePtr());
	}

	template <typename t_value_type>
	void setValueByType(const t_value_type& value) const
	{
		assert(isType<t_value_type>());
		assert(getInstance() != nullptr);

		setValueBytes(&value, sizeof(t_value_type));
	}

private:
	bool isStructType(StructTypeHandle archetype) const;
	bool isFundamentalType(FundamentalType fundamentalType) const;
	bool isTemplateArgumentStructType(std::size_t index, StructTypeHandle archetype) const;
	bool isTemplateArgumentFundamentalType(std::size_t index, FundamentalType fundamentalType) const;
	void setValueBytes(const void* bytes, std::size_t byteCount) const;

	struct AccessorData* m_pimpl;
};

class SERIALIZATION_API IVisitor
{
public:
	IVisitor();
	virtual ~IVisitor();

	bool hasError() const;
	const std::string& getError() const;
	void setError(const std::string& msg);

	virtual void visitClass(ValueAccessor const& accessor) {}
	virtual void visitStruct(ValueAccessor const& accessor) {}
	virtual void visitEnum(ValueAccessor const& accessor) {}
	virtual void visitBool(ValueAccessor const& accessor) {}
	virtual void visitByte(ValueAccessor const& accessor) {}
	virtual void visitUByte(ValueAccessor const& accessor) {}
	virtual void visitShort(ValueAccessor const& accessor) {}
	virtual void visitUShort(ValueAccessor const& accessor) {}
	virtual void visitInt(ValueAccessor const& accessor) {}
	virtual void visitUInt(ValueAccessor const& accessor) {}
	virtual void visitLong(ValueAccessor const& accessor) {}
	virtual void visitULong(ValueAccessor const& accessor) {}
	virtual void visitFloat(ValueAccessor const& accessor) {}
	virtual void visitDouble(ValueAccessor const& accessor) {}

private:
	struct VisitorData* m_pimpl;
};

template <typename t_struct_type>
void visitStruct(t_struct_type& instance, IVisitor* visitor)
{
	visitStruct(&instance, t_struct_type::staticGetArchetype(), visitor);
}

template <typename t_struct_type>
void visitStruct(const t_struct_type& instance, IVisitor* visitor)
{
	visitStruct(&instance, t_struct_type::staticGetArchetype(), visitor);
}

SERIALIZATION_API void visitStruct(const void* instance, rfk::Struct const& structType, IVisitor* visitor);
SERIALIZATION_API void visitStruct(void* instance, rfk::Struct const& structType, IVisitor* visitor);

SERIALIZATION_API void visitField(const void* instance, rfk::Field const& fieldType, IVisitor* visitor);
SERIALIZATION_API void visitField(void* instance, rfk::Field const& fieldType, IVisitor* visitor);

SERIALIZATION_API void visitValue(ValueAccessor const& accessor, IVisitor* visitor);

// A struct's own fields in the order they cross the wire: public and non-static, ordered by
// memory offset. Reflection returns fields in no particular order, and the binary encoding is
// the concatenation of them in this one, so this is the definition of that order.
//
// Inherited fields are not included. A parent's fields precede a child's on the wire, and the
// two callers reach that differently: the visitors below walk the parents themselves, while
// the bindings generator emits the parent as the target language's base class and lets the
// generated type inherit them. Both depend on agreeing with this function about which fields
// count and in what order, which is why it is exported rather than reimplemented per caller.
SERIALIZATION_API FieldList getStructFieldsInWireOrder(StructTypeHandle structType);
}; // namespace Serialization