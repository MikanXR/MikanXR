using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace MikanXR
{
	enum SerializationTestEnum
	{
		Value1,
		Value2,
		Value3,
	};

	class SerializationPoint : PolymorphicStruct
	{
	};

	class SerializationPoint2d : SerializationPoint
	{
		public SerializationPoint2d()
		{
		}

		public SerializationPoint2d(float x, float y)
		{
			x_field = x;
			y_field = y;
		}

		public float x_field = 0;
		public float y_field = 0;
	};

	class SerializationPoint3d : SerializationPoint
	{
		public SerializationPoint3d()
		{
		}

		public SerializationPoint3d(float x, float y, float z)
		{
			x_field = x;
			y_field = y;
			z_field = z;
		}

		public float x_field = 0;
		public float y_field = 0;
		public float z_field = 0;
	};

	class SerializationTestObject
	{
		public bool bool_field;
		public sbyte byte_field;
		public byte ubyte_field;
		public short short_field;
		public ushort ushort_field;
		public int int_field;
		public uint uint_field;
		public long long_field;
		public float float_field;
		public double double_field;
		public string string_field;
		public SerializationTestEnum enum_field;
		public SerializationPoint2d point2d_field;
		public PolymorphicObject point_ptr_field;
		public PolymorphicObject null_ptr_field;
		public List<bool> bool_array;
		public List<int> int_array;
		public List<float> float_array;
		public List<string> string_array;
		public List<SerializationPoint2d> point2d_array;
		public Dictionary<int, SerializationPoint2d> int_point_map;
		public Dictionary<string, SerializationPoint2d> string_point_map;
	};

	public class SerializationUnitTests
	{
		SerializationTestObject buildSerializationTestObject()
		{
			SerializationTestObject testObject = new SerializationTestObject();
			var boolArray = new List<bool>() { true, false, true };
			var intArray = new List<int>() {1, 2, 3};
			var floatArray = new List<float>() {1.2345f, 5.4321f, 9.8765f};
			var stringArray = new List<string>() {"hello", "world", "!"};

			var pointArray = new List<SerializationPoint2d>();
			pointArray.Add( new SerializationPoint2d(1.2345f, 5.4321f) );
			pointArray.Add( new SerializationPoint2d(5.4321f, 1.2345f) );

			var intPointMap = new Dictionary<int, SerializationPoint2d>();
			intPointMap.Add(1, new SerializationPoint2d(1.2345f, 5.4321f));
			intPointMap.Add(2, new SerializationPoint2d(5.4321f, 1.2345f));

			var stringPointMap = new Dictionary<string, SerializationPoint2d>();
			stringPointMap.Add("key1", new SerializationPoint2d(1.2345f, 5.4321f));
			stringPointMap.Add("key2", new SerializationPoint2d(5.4321f, 1.2345f));

			testObject.bool_field= true;
			testObject.byte_field= -123;
			testObject.ubyte_field= 123;
			testObject.short_field= -1234;
			testObject.ushort_field= 1234;
			testObject.int_field= -123456;
			testObject.uint_field= 123456;
			testObject.long_field= -123456789;
			testObject.float_field= 1.2345f;
			testObject.double_field= 1.23456789;
			testObject.string_field= "hello";
			testObject.enum_field= SerializationTestEnum.Value2;
			testObject.point2d_field= new SerializationPoint2d(1.2345f, 5.4321f);
			testObject.point_ptr_field = new PolymorphicObject();
			testObject.point_ptr_field.setInstance(new SerializationPoint3d(1.2345f, 5.4321f, 9.8765f));
			testObject.null_ptr_field = new PolymorphicObject();
			testObject.bool_array= boolArray;
			testObject.int_array= intArray;
			testObject.float_array = floatArray;
			testObject.string_array = stringArray;
			testObject.point2d_array= pointArray;
			testObject.int_point_map= intPointMap;
			testObject.string_point_map= stringPointMap;

			return testObject;
		}

		bool verifySerializationTestObject(
			SerializationTestObject actual,
			SerializationTestObject expected)
		{
			bool success;

			success = (actual.bool_field == expected.bool_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.byte_field == expected.byte_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.ubyte_field == expected.ubyte_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.short_field == expected.short_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.ushort_field == expected.ushort_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.int_field == expected.int_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.uint_field == expected.uint_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.long_field == expected.long_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (Math.Abs(actual.float_field - expected.float_field) <= float.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			success = (Math.Abs(actual.double_field - expected.double_field) <= double.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.string_field == expected.string_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.enum_field == expected.enum_field);
			Debug.Assert(success);
			if (!success) return false;

			success = (Math.Abs(actual.point2d_field.x_field - expected.point2d_field.x_field) <= float.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			success = (Math.Abs(actual.point2d_field.y_field - expected.point2d_field.y_field) <= float.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			var expected_point3d= (SerializationPoint3d)expected.point_ptr_field.Instance;
			var actual_point3d= (SerializationPoint3d)actual.point_ptr_field.Instance;

			success = (Math.Abs(expected_point3d.x_field - actual_point3d.x_field) <= float.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			success = (Math.Abs(expected_point3d.y_field - actual_point3d.y_field) <= float.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			success = (Math.Abs(expected_point3d.z_field - actual_point3d.z_field) <= float.Epsilon);
			Debug.Assert(success);
			if (!success) return false;

			success = (expected.null_ptr_field.Instance == null);
			Debug.Assert(success);
			if (!success) return false;

			success = (expected.null_ptr_field.RuntimeClassName == string.Empty);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.null_ptr_field.Instance == null);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.null_ptr_field.RuntimeClassName == string.Empty);
			Debug.Assert(success);
			if (!success) return false;

			success = (actual.bool_array.Count == expected.bool_array.Count);
			Debug.Assert(success);
			if (!success) return false;

			for (int i = 0; i < actual.bool_array.Count; ++i)
			{
				success = (actual.bool_array[i] == expected.bool_array[i]);
				Debug.Assert(success);
				if (!success) return false;
			}

			success = (actual.int_array.Count == expected.int_array.Count);
			Debug.Assert(success);
			if (!success) return false;

			for (int i = 0; i < actual.int_array.Count; ++i)
			{
				success = (actual.int_array[i] == expected.int_array[i]);
				Debug.Assert(success);
				if (!success) return false;
			}

			success = (actual.float_array.Count == expected.float_array.Count);
			Debug.Assert(success);
			if (!success) return false;

			for (int i = 0; i < actual.float_array.Count; ++i)
			{
				success = (Math.Abs(actual.float_array[i] - expected.float_array[i]) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;
			}

			success = (actual.string_array.Count == expected.string_array.Count);
			Debug.Assert(success);
			if (!success) return false;

			for (int i = 0; i < actual.string_array.Count; ++i)
			{
				success = (actual.string_array[i] == expected.string_array[i]);
				Debug.Assert(success);
				if (!success) return false;
			}

			success = (actual.point2d_array.Count == expected.point2d_array.Count);
			Debug.Assert(success);
			if (!success) return false;

			for (int i = 0; i < 2; ++i)
			{
				var actualPoint = actual.point2d_array[i];
				var expectedPoint = expected.point2d_array[i];

				success = (Math.Abs(actualPoint.x_field - expectedPoint.x_field) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;

				success = (Math.Abs(actualPoint.y_field - expectedPoint.y_field) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;
			}

			success = (actual.int_point_map.Count == expected.int_point_map.Count);
			Debug.Assert(success);
			if (!success) return false;

			foreach (var pair in actual.int_point_map)
			{
				var key = pair.Key;
				var actualPoint = pair.Value;
				var expectedPoint = expected.int_point_map[key];

				success = (Math.Abs(actualPoint.x_field - expectedPoint.x_field) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;

				success = (Math.Abs(actualPoint.y_field - expectedPoint.y_field) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;
			}

			success = (actual.string_point_map.Count == expected.string_point_map.Count);
			Debug.Assert(success);
			if (!success) return false;

			foreach (var pair in actual.string_point_map)
			{
				var key = pair.Key;
				var actualPoint = pair.Value;
				var expectedPoint = expected.string_point_map[key];

				success = (Math.Abs(actualPoint.x_field - expectedPoint.x_field) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;

				success = (Math.Abs(actualPoint.y_field - expectedPoint.y_field) <= float.Epsilon);
				Debug.Assert(success);
				if (!success) return false;
			}

			return true;
		}

		public bool TestReflectionFromJson()
		{
			bool success = true;

			try
			{
				var expected = buildSerializationTestObject();

				string jsonString = JsonSerializer.serializeToJsonString(expected);
				var actual = new SerializationTestObject();
				bool bCanDeserialize= JsonDeserializer.deserializeFromJsonString(jsonString, actual);

				if (!bCanDeserialize)
				{
					success = false;
				}
				else
				{
					success = verifySerializationTestObject(actual, expected);
				}
			}
			catch (Exception e)
			{
				Console.WriteLine("    Exception: " + e.Message);
				success = false;
			}

			Console.WriteLine("    TestReflectionFromJson - " + (success ? "PASSED" : "FAILED"));
			return success;
		}

		public bool RunAllTests()
		{
			bool success = true;
			Console.WriteLine("[SerializationUnitTests]");

			success &= TestReflectionFromJson();

			Console.WriteLine("  SerializationUnitTests module - " + (success ? "PASSED" : "FAILED"));
			return success;
		}
	}
}