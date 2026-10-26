using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace MikanXR
{
	// Hand-written wire types, shaped the way the generator writes them, so the runtime's JSON
	// rules are tested on a type that holds every kind of field at once

	enum SerializationTestEnum
	{
		Value1,
		Value2,
		Value3,
	};

	static class SerializationTestEnumJson
	{
		public static JToken ToJson(SerializationTestEnum value)
		{
			switch (value)
			{
				case SerializationTestEnum.Value1: return MikanJson.WriteString("Value1");
				case SerializationTestEnum.Value2: return MikanJson.WriteString("Value2");
				case SerializationTestEnum.Value3: return MikanJson.WriteString("Value3");
			}
			throw new ArgumentOutOfRangeException(nameof(value));
		}

		public static SerializationTestEnum FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (SerializationTestEnum)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Value1": return SerializationTestEnum.Value1;
				case "Value2": return SerializationTestEnum.Value2;
				case "Value3": return SerializationTestEnum.Value3;
			}
			throw new Exception("SerializationTestEnum has no value named " + name);
		}
	}

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

		public override string RuntimeClassName => "SerializationPoint2d";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x_field", MikanJson.WriteFloat(this.x_field));
			json.Add("y_field", MikanJson.WriteFloat(this.y_field));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x_field = MikanJson.ReadFloat(MikanJson.Field(json, "x_field"));
			this.y_field = MikanJson.ReadFloat(MikanJson.Field(json, "y_field"));
		}
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

		public override string RuntimeClassName => "SerializationPoint3d";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x_field", MikanJson.WriteFloat(this.x_field));
			json.Add("y_field", MikanJson.WriteFloat(this.y_field));
			json.Add("z_field", MikanJson.WriteFloat(this.z_field));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x_field = MikanJson.ReadFloat(MikanJson.Field(json, "x_field"));
			this.y_field = MikanJson.ReadFloat(MikanJson.Field(json, "y_field"));
			this.z_field = MikanJson.ReadFloat(MikanJson.Field(json, "z_field"));
		}
	};

	class SerializationTestObject : MikanSerializableObject
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

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("bool_field", MikanJson.WriteBool(this.bool_field));
			json.Add("byte_field", MikanJson.WriteSByte(this.byte_field));
			json.Add("ubyte_field", MikanJson.WriteByte(this.ubyte_field));
			json.Add("short_field", MikanJson.WriteShort(this.short_field));
			json.Add("ushort_field", MikanJson.WriteUShort(this.ushort_field));
			json.Add("int_field", MikanJson.WriteInt(this.int_field));
			json.Add("uint_field", MikanJson.WriteUInt(this.uint_field));
			json.Add("long_field", MikanJson.WriteLong(this.long_field));
			json.Add("float_field", MikanJson.WriteFloat(this.float_field));
			json.Add("double_field", MikanJson.WriteDouble(this.double_field));
			json.Add("string_field", MikanJson.WriteString(this.string_field));
			json.Add("enum_field", SerializationTestEnumJson.ToJson(this.enum_field));
			json.Add("point2d_field", MikanJson.WriteObject(this.point2d_field));
			json.Add("point_ptr_field", MikanJson.WritePolymorphic(this.point_ptr_field));
			json.Add("null_ptr_field", MikanJson.WritePolymorphic(this.null_ptr_field));
			json.Add("bool_array", MikanJson.WriteList(this.bool_array, e0 => MikanJson.WriteBool(e0)));
			json.Add("int_array", MikanJson.WriteList(this.int_array, e0 => MikanJson.WriteInt(e0)));
			json.Add("float_array", MikanJson.WriteList(this.float_array, e0 => MikanJson.WriteFloat(e0)));
			json.Add("string_array", MikanJson.WriteList(this.string_array, e0 => MikanJson.WriteString(e0)));
			json.Add("point2d_array", MikanJson.WriteList(this.point2d_array, e0 => MikanJson.WriteObject(e0)));
			json.Add("int_point_map", MikanJson.WriteMap(this.int_point_map, k0 => MikanJson.WriteInt(k0), v0 => MikanJson.WriteObject(v0)));
			json.Add("string_point_map", MikanJson.WriteMap(this.string_point_map, k0 => MikanJson.WriteString(k0), v0 => MikanJson.WriteObject(v0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.bool_field = MikanJson.ReadBool(MikanJson.Field(json, "bool_field"));
			this.byte_field = MikanJson.ReadSByte(MikanJson.Field(json, "byte_field"));
			this.ubyte_field = MikanJson.ReadByte(MikanJson.Field(json, "ubyte_field"));
			this.short_field = MikanJson.ReadShort(MikanJson.Field(json, "short_field"));
			this.ushort_field = MikanJson.ReadUShort(MikanJson.Field(json, "ushort_field"));
			this.int_field = MikanJson.ReadInt(MikanJson.Field(json, "int_field"));
			this.uint_field = MikanJson.ReadUInt(MikanJson.Field(json, "uint_field"));
			this.long_field = MikanJson.ReadLong(MikanJson.Field(json, "long_field"));
			this.float_field = MikanJson.ReadFloat(MikanJson.Field(json, "float_field"));
			this.double_field = MikanJson.ReadDouble(MikanJson.Field(json, "double_field"));
			this.string_field = MikanJson.ReadString(MikanJson.Field(json, "string_field"));
			this.enum_field = SerializationTestEnumJson.FromJson(MikanJson.Field(json, "enum_field"));
			this.point2d_field = MikanJson.ReadObject(MikanJson.Field(json, "point2d_field"), this.point2d_field ?? new SerializationPoint2d());
			this.point_ptr_field = MikanJson.ReadPolymorphic(MikanJson.Field(json, "point_ptr_field"), this.point_ptr_field);
			this.null_ptr_field = MikanJson.ReadPolymorphic(MikanJson.Field(json, "null_ptr_field"), this.null_ptr_field);
			this.bool_array = MikanJson.ReadList(MikanJson.Field(json, "bool_array"), this.bool_array, e0 => MikanJson.ReadBool(e0));
			this.int_array = MikanJson.ReadList(MikanJson.Field(json, "int_array"), this.int_array, e0 => MikanJson.ReadInt(e0));
			this.float_array = MikanJson.ReadList(MikanJson.Field(json, "float_array"), this.float_array, e0 => MikanJson.ReadFloat(e0));
			this.string_array = MikanJson.ReadList(MikanJson.Field(json, "string_array"), this.string_array, e0 => MikanJson.ReadString(e0));
			this.point2d_array = MikanJson.ReadList(MikanJson.Field(json, "point2d_array"), this.point2d_array, e0 => MikanJson.ReadObject(e0, new SerializationPoint2d()));
			this.int_point_map = MikanJson.ReadMap(MikanJson.Field(json, "int_point_map"), this.int_point_map, k0 => MikanJson.ReadInt(k0), v0 => MikanJson.ReadObject(v0, new SerializationPoint2d()));
			this.string_point_map = MikanJson.ReadMap(MikanJson.Field(json, "string_point_map"), this.string_point_map, k0 => MikanJson.ReadString(k0), v0 => MikanJson.ReadObject(v0, new SerializationPoint2d()));
		}
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

		public bool TestJsonRoundTrip()
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

			Console.WriteLine("    TestJsonRoundTrip - " + (success ? "PASSED" : "FAILED"));
			return success;
		}

		// Every registered wire type, filled with distinct values, reads back what it wrote
		public bool TestGeneratedTypesRoundTrip()
		{
			var filler = new WireTypeFiller();
			int tested = 0, skipped = 0;
			bool success = true;

			List<string> typeNames = MikanTypeRegistry.GetTypeNames();
			typeNames.Sort(StringComparer.Ordinal);
			foreach (string typeName in typeNames)
			{
				MikanSerializableObject expected = MikanTypeRegistry.Create(typeName);
				if (!WireTypeFiller.IsSerializable(expected.GetType()))
				{
					skipped++;
					continue;
				}

				try
				{
					filler.Fill(expected);
					string expectedJson = JsonSerializer.serializeToJsonString(expected);

					MikanSerializableObject actual = MikanTypeRegistry.Create(typeName);
					if (!JsonDeserializer.deserializeFromJsonString(expectedJson, actual) ||
						JsonSerializer.serializeToJsonString(actual) != expectedJson)
					{
						Console.WriteLine("    " + typeName + " did not read back what it wrote");
						success = false;
					}
				}
				catch (Exception e)
				{
					Console.WriteLine("    " + typeName + " exception: " + e.Message);
					success = false;
				}

				tested++;
			}

			Console.WriteLine($"    TestGeneratedTypesRoundTrip ({tested} types, {skipped} with pointer or ulong fields skipped) - " +
				(success ? "PASSED" : "FAILED"));
			return success;
		}

		// Fields read and written by wire name, converting where the field type allows
		public bool TestFieldAccessByName()
		{
			bool success = true;

			var floatValue = new MikanFloatValue();
			success &= floatValue.TrySetField("value", 2.5f) && floatValue.value == 2.5f;
			success &= floatValue.TrySetField("value", 3) && floatValue.value == 3.0f;
			success &= !floatValue.TrySetField("value", "not a number");
			success &= !floatValue.TrySetField("no_such_field", 1.0f);
			success &= floatValue.TryGetField("value", out object boxedValue) && boxedValue is float f && f == 3.0f;
			success &= !floatValue.TryGetField("no_such_field", out object _);

			// A field declared on a base class is reached through the derived type
			var response = new MikanCameraPropertiesResponse();
			success &= response.TrySetField("resultCode", (int)MikanAPIResult.Timeout) &&
				response.resultCode == MikanAPIResult.Timeout;
			success &= response.TrySetField("responseTypeName", null) && response.responseTypeName == null;

			Console.WriteLine("    TestFieldAccessByName - " + (success ? "PASSED" : "FAILED"));
			return success;
		}

		public bool RunAllTests()
		{
			bool success = true;
			Console.WriteLine("[SerializationUnitTests]");

			// The hand-written polymorphic type has to be known before it is read
			MikanTypeRegistry.Register("SerializationPoint3d", () => new SerializationPoint3d());

			success &= TestJsonRoundTrip();
			success &= TestGeneratedTypesRoundTrip();
			success &= TestFieldAccessByName();

			Console.WriteLine("  SerializationUnitTests module - " + (success ? "PASSED" : "FAILED"));
			return success;
		}
	}
}