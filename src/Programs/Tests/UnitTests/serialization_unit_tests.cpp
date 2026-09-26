//-- includes -----
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "MikanRemoteControlRequests.h"

#include "BinarySerializer.h"
#include "BinaryDeserializer.h"
#include "BinaryUtility.h"
#include "JsonSerializer.h"
#include "JsonDeserializer.h"
#include "MathUtility.h"
#include "SerializableList.h"
#include "SerializableMap.h"
#include "TypeRegistry.h"

#include "serialization_unit_tests.h"
#include "serialization_unit_tests.rfks.h"
#include "unit_test.h"

//-- public interface -----
bool run_serialization_unit_tests()
{
	Serialization::TypeRegistry::build();

	UNIT_TEST_MODULE_BEGIN("serialization")
	UNIT_TEST_MODULE_CALL_TEST(serialization_utility_test_endian_swap);
	UNIT_TEST_MODULE_CALL_TEST(serialization_utility_test_reflection_from_json);
	UNIT_TEST_MODULE_CALL_TEST(serialization_utility_test_reflection_from_bytes);
	UNIT_TEST_MODULE_CALL_TEST(serialization_utility_test_remote_control);
	UNIT_TEST_MODULE_END()
}

//-- private functions -----
void test_endian_mode(Serialization::Endian endianMode)
{
	uint8_t scratch[32];

	int16_t expectedValue16= 0x1234;
	Serialization::write_int16((uint8_t*)&scratch, expectedValue16, endianMode);
	int16_t actualValue16= Serialization::read_int16((uint8_t*)&scratch, endianMode);
	assert(actualValue16 == expectedValue16);

	int32_t expectedValue24= 0x123456;
	Serialization::write_int24((uint8_t*)&scratch, expectedValue24, endianMode);
	int32_t actualValue24= Serialization::read_int24((uint8_t*)&scratch, endianMode);
	assert(actualValue24 == expectedValue24);

	int32_t expectedValue32= 0x12345678;
	Serialization::write_int32((uint8_t*)&scratch, expectedValue32, endianMode);
	int32_t actualValue32= Serialization::read_int32((uint8_t*)&scratch, endianMode);
	assert(actualValue32 == expectedValue32);

	int64_t expectedValue64= 0x1234567890abcdef;
	Serialization::write_int64((uint8_t*)&scratch, expectedValue64, endianMode);
	int64_t actualValue64= Serialization::read_int64((uint8_t*)&scratch, endianMode);
	assert(actualValue64 == expectedValue64);

	float expectedFloat= k_real_pi;
	Serialization::write_float((uint8_t*)&scratch, expectedFloat, endianMode);
	float actualFloat= Serialization::read_float((uint8_t*)&scratch, endianMode);
	assert(is_nearly_equal(expectedFloat, actualFloat, k_real_epsilon));

	double expectedDouble= k_real64_pi;
	Serialization::write_double((uint8_t*)&scratch, expectedDouble, endianMode);
	double actualDouble= Serialization::read_double((uint8_t*)&scratch, endianMode);
	assert(is_double_nearly_equal(expectedDouble, actualDouble, k_real64_epsilon));
}

bool serialization_utility_test_endian_swap()
{
	UNIT_TEST_BEGIN("endian swap")

	test_endian_mode(Serialization::Endian::Little);
	test_endian_mode(Serialization::Endian::Big);

	UNIT_TEST_COMPLETE()
}

void build_serialization_test_struct(SerializationTestStruct& outStruct)
{

	Serialization::List<bool> boolArray;
	boolArray.push_back(true);
	boolArray.push_back(false);
	boolArray.push_back(true);

	Serialization::List<int> intArray({{1, 2, 3}});

	Serialization::List<float> floatArray({{1.2345f, 5.4321f, 9.8765f}});

	Serialization::List<Serialization::String> stringArray;
	stringArray.push_back("hello");
	stringArray.push_back("world");
	stringArray.push_back("!");

	Serialization::List<SerializationPoint2dStruct> pointArray;
	pointArray.push_back({1.2345f, 5.4321f});
	pointArray.push_back({5.4321f, 1.2345f});

	Serialization::Map<int, SerializationPoint2dStruct> intPointMap;
	intPointMap.insert({1, {1.2345f, 5.4321f}});
	intPointMap.insert({2, {5.4321f, 1.2345f}});

	Serialization::Map<Serialization::String, SerializationPoint2dStruct> stringPointMap;
	stringPointMap.insert({Serialization::String("key1"), {1.2345f, 5.4321f}});
	stringPointMap.insert({Serialization::String("key2"), {5.4321f, 1.2345f}});

	SerializationPoint3dStruct point3d= {1.2345f, 5.4321f, 9.8765f};

	outStruct.point_ptr_field.allocatedByType<SerializationPoint3dStruct>();
	auto* point3d_ptr= outStruct.point_ptr_field.getTypedPointerMutable<SerializationPoint3dStruct>();
	point3d_ptr->x_field= 1.2345f;
	point3d_ptr->y_field= 5.4321f;
	point3d_ptr->z_field= 9.8765f;

	outStruct.null_ptr_field.reset();

	outStruct.bool_field= true;
	outStruct.byte_field= -123;
	outStruct.ubyte_field= 123;
	outStruct.short_field= -1234;
	outStruct.ushort_field= 1234;
	outStruct.int_field= -123456;
	outStruct.uint_field= 123456;
	outStruct.long_field= -123456789;
	outStruct.float_field= 1.2345f;
	outStruct.double_field= 1.23456789;
	outStruct.string_field= Serialization::String("hello");
	outStruct.enum_field= SerializationTestEnum_Value2;
	outStruct.point2d_field= {1.2345f, 5.4321f};
	outStruct.bool_array= boolArray;
	outStruct.int_array= intArray;
	outStruct.float_array= floatArray;
	outStruct.string_array= stringArray;
	outStruct.point2d_array= pointArray;
	outStruct.int_point_map= intPointMap;
	outStruct.string_point_map= stringPointMap;
}

void verify_serialization_test_struct(const SerializationTestStruct& actual, const SerializationTestStruct& expected)
{
	assert(actual.bool_field == expected.bool_field);
	assert(actual.byte_field == expected.byte_field);
	assert(actual.ubyte_field == expected.ubyte_field);
	assert(actual.short_field == expected.short_field);
	assert(actual.ushort_field == expected.ushort_field);
	assert(actual.int_field == expected.int_field);
	assert(actual.uint_field == expected.uint_field);
	assert(actual.long_field == expected.long_field);
	assert(is_nearly_equal(actual.float_field, expected.float_field, k_real_epsilon));
	assert(is_double_nearly_equal(actual.double_field, expected.double_field, k_real64_epsilon));
	assert(actual.string_field == expected.string_field);
	assert(actual.enum_field == expected.enum_field);
	assert(is_nearly_equal(actual.point2d_field.x_field, expected.point2d_field.x_field, k_real_epsilon));
	assert(is_nearly_equal(actual.point2d_field.y_field, expected.point2d_field.y_field, k_real_epsilon));

	auto* expected_point3d= expected.point_ptr_field.getTypedPointer<SerializationPoint3dStruct>();
	auto* actual_point3d= actual.point_ptr_field.getTypedPointer<SerializationPoint3dStruct>();
	assert(expected_point3d);
	assert(actual_point3d);
	assert(is_nearly_equal(expected_point3d->x_field, actual_point3d->x_field, k_real_epsilon));
	assert(is_nearly_equal(expected_point3d->y_field, actual_point3d->y_field, k_real_epsilon));
	assert(is_nearly_equal(expected_point3d->z_field, actual_point3d->z_field, k_real_epsilon));

	assert(expected.null_ptr_field.getRawPtr() == nullptr);
	assert(expected.null_ptr_field.getRuntimeClassName().empty());
	assert(actual.null_ptr_field.getRawPtr() == nullptr);
	assert(actual.null_ptr_field.getRuntimeClassName().empty());

	assert(actual.bool_array.size() == expected.bool_array.size());
	for (size_t i= 0; i < actual.bool_array.size(); ++i)
	{
		assert(actual.bool_array[i] == expected.bool_array[i]);
	}

	assert(actual.int_array.size() == expected.int_array.size());
	for (size_t i= 0; i < actual.int_array.size(); ++i)
	{
		assert(actual.int_array[i] == expected.int_array[i]);
	}

	assert(actual.float_array.size() == expected.float_array.size());
	for (size_t i= 0; i < actual.float_array.size(); ++i)
	{
		assert(is_nearly_equal(actual.float_array[i], expected.float_array[i], k_real_epsilon));
	}

	assert(actual.string_array.size() == expected.string_array.size());
	for (size_t i= 0; i < actual.string_array.size(); ++i)
	{
		assert(actual.string_array[i] == expected.string_array[i]);
	}

	assert(actual.point2d_array.size() == expected.point2d_array.size());
	for (size_t i= 0; i < 2; ++i)
	{
		const auto& actualPoint= actual.point2d_array[i];
		const auto& expectedPoint= expected.point2d_array[i];

		assert(is_nearly_equal(actualPoint.x_field, expectedPoint.x_field, k_real_epsilon));
		assert(is_nearly_equal(actualPoint.y_field, expectedPoint.y_field, k_real_epsilon));
	}

	assert(actual.int_point_map.size() == expected.int_point_map.size());
	for (const auto& entry : actual.int_point_map)
	{
		const int key= entry.key;
		const auto& actualPoint= entry.value;
		const auto* expectedValuePtr= expected.int_point_map.findValue(key);
		assert(expectedValuePtr != nullptr);
		const auto& expectedPoint= *expectedValuePtr;

		assert(is_nearly_equal(actualPoint.x_field, expectedPoint.x_field, k_real_epsilon));
		assert(is_nearly_equal(actualPoint.y_field, expectedPoint.y_field, k_real_epsilon));
	}

	assert(actual.string_point_map.size() == expected.string_point_map.size());
	for (const auto& entry : actual.string_point_map)
	{
		const auto& key= entry.key;
		const auto& actualPoint= entry.value;
		const auto* expectedValuePtr= expected.string_point_map.findValue(key);
		assert(expectedValuePtr != nullptr);
		const auto& expectedPoint= *expectedValuePtr;

		assert(is_nearly_equal(actualPoint.x_field, expectedPoint.x_field, k_real_epsilon));
		assert(is_nearly_equal(actualPoint.y_field, expectedPoint.y_field, k_real_epsilon));
	}
}

bool serialization_utility_test_reflection_from_json()
{
	UNIT_TEST_BEGIN("reflect from json")
	SerializationTestStruct expected= {};
	build_serialization_test_struct(expected);

	std::string jsonString;
	std::string serializeError;
	bool bCanSerialize= Serialization::serializeToJsonString(expected, jsonString, serializeError);
	assert(bCanSerialize);

	SerializationTestStruct actual= {};
	std::string deserializeError;
	bool bCanDeserialize= Serialization::deserializeFromJsonString(jsonString, actual, deserializeError);

	verify_serialization_test_struct(actual, expected);

	UNIT_TEST_COMPLETE()
}

bool serialization_utility_test_reflection_from_bytes()
{
	UNIT_TEST_BEGIN("reflect from bytes")
	SerializationTestStruct expected;
	build_serialization_test_struct(expected);

	std::vector<uint8_t> bytes;
	std::string serializeError;
	bool bCanSerialize= Serialization::serializeToBytes(expected, bytes, serializeError);
	assert(bCanSerialize);

	std::string parseError;
	SerializationTestStruct actual= {};
	bool bCanDeserialize= Serialization::deserializeFromBytes(bytes, actual, parseError);
	assert(bCanDeserialize);
	assert(parseError.empty());

	verify_serialization_test_struct(actual, expected);
	UNIT_TEST_COMPLETE()
}

bool serialization_utility_test_remote_control()
{
	UNIT_TEST_BEGIN("remote control")
	MikanRemoteControlCommand expected= {};
	expected.command= "test_command";
	expected.parameters.push_back("param1");
	expected.parameters.push_back("param2");

	std::string jsonString;
	std::string serializeError;
	bool bCanSerialize= Serialization::serializeToJsonString(expected, jsonString, serializeError);
	assert(bCanSerialize);

	MikanRemoteControlCommand actual= {};
	std::string deserializeError;
	bool bCanDeserialize= Serialization::deserializeFromJsonString(jsonString, actual, deserializeError);
	assert(bCanDeserialize);
	assert(deserializeError.empty());

	assert(actual.command == expected.command);

	assert(actual.parameters.size() == expected.parameters.size());
	for (int i= 0; i < expected.parameters.size(); ++i)
	{
		const Serialization::String& actualValue= actual.parameters[i];
		const Serialization::String& expectedValue= expected.parameters[i];

		assert(actualValue == expectedValue);
	}
	UNIT_TEST_COMPLETE()
}