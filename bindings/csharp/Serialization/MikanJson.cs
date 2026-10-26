using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	// The wire's JSON rules, called by the generated WriteFields and ReadFields. Containers take
	// the conversion of their elements as delegates, so nested containers compose without the
	// runtime inspecting a type.
	//
	// Values are boxed into JValue the same way for every type, so a float is written with float
	// precision rather than widened to a double first.
	public static class MikanJson
	{
		// -- Writing -----

		public static JToken WriteBool(bool value) => new JValue((object)value);
		public static JToken WriteSByte(sbyte value) => new JValue((object)value);
		public static JToken WriteByte(byte value) => new JValue((object)value);
		public static JToken WriteShort(short value) => new JValue((object)value);
		public static JToken WriteUShort(ushort value) => new JValue((object)value);
		public static JToken WriteInt(int value) => new JValue((object)value);
		public static JToken WriteUInt(uint value) => new JValue((object)value);
		public static JToken WriteLong(long value) => new JValue((object)value);
		public static JToken WriteFloat(float value) => new JValue((object)value);
		public static JToken WriteDouble(double value) => new JValue((object)value);

		public static JToken WriteULong(ulong value)
		{
			throw new NotSupportedException("ulong values are not supported by all JSON libraries");
		}

		// Pointers name memory in the client's own process, so a request carrying one is handled
		// locally and never serialized
		public static JToken WritePointer(IntPtr value)
		{
			throw new NotSupportedException("pointer values do not cross the wire");
		}

		public static JToken WriteString(string value) => new JValue(value ?? string.Empty);

		public static JToken WriteObject(MikanSerializableObject value)
		{
			if (value == null)
			{
				throw new ArgumentNullException(nameof(value), "Cannot serialize a null object");
			}

			var json = new JObject();
			value.WriteFields(json);
			return json;
		}

		public static JToken WriteList<T>(List<T> list, Func<T, JToken> writeElement)
		{
			if (list == null)
			{
				throw new ArgumentNullException(nameof(list), "Cannot serialize a null list");
			}

			var jsonArray = new JArray();
			foreach (T element in list)
			{
				jsonArray.Add(writeElement(element));
			}

			return jsonArray;
		}

		// A map is an array of {key, value} pairs, since a JSON object key can only be a string
		public static JToken WriteMap<K, V>(Dictionary<K, V> map, Func<K, JToken> writeKey, Func<V, JToken> writeValue)
		{
			if (map == null)
			{
				throw new ArgumentNullException(nameof(map), "Cannot serialize a null map");
			}

			var jsonPairArray = new JArray();
			foreach (KeyValuePair<K, V> pair in map)
			{
				var jsonPair = new JObject();
				jsonPair["key"] = writeKey(pair.Key);
				jsonPair["value"] = writeValue(pair.Value);
				jsonPairArray.Add(jsonPair);
			}

			return jsonPairArray;
		}

		// {class_name, value}, with an empty class_name and value for an empty object
		public static JToken WritePolymorphic(PolymorphicObject value)
		{
			if (value == null)
			{
				throw new ArgumentNullException(nameof(value), "Cannot serialize a null polymorphic object");
			}

			var jsonInstance = new JObject();
			if (!string.IsNullOrEmpty(value.RuntimeClassName) && value.Instance != null)
			{
				value.Instance.WriteFields(jsonInstance);
			}

			var json = new JObject();
			json["class_name"] = value.RuntimeClassName;
			json["value"] = jsonInstance;
			return json;
		}

		// -- Reading -----

		public static JToken Field(JObject json, string fieldName)
		{
			if (json.TryGetValue(fieldName, out JToken token))
			{
				return token;
			}

			throw new Exception("Field " + fieldName + " not found in json");
		}

		public static bool ReadBool(JToken token)
		{
			ExpectType(token, JTokenType.Boolean, "bool");
			return (bool)token;
		}

		public static sbyte ReadSByte(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "sbyte");
			return (sbyte)(short)token;
		}

		public static byte ReadByte(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "byte");
			return (byte)(ushort)token;
		}

		public static short ReadShort(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "short");
			return (short)token;
		}

		public static ushort ReadUShort(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "ushort");
			return (ushort)token;
		}

		public static int ReadInt(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "int");
			return (int)token;
		}

		public static uint ReadUInt(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "uint");
			return (uint)token;
		}

		public static long ReadLong(JToken token)
		{
			ExpectType(token, JTokenType.Integer, "long");
			return (long)token;
		}

		public static ulong ReadULong(JToken token)
		{
			throw new NotSupportedException("ulong values are not supported by all JSON libraries");
		}

		public static IntPtr ReadPointer(JToken token)
		{
			throw new NotSupportedException("pointer values do not cross the wire");
		}

		public static float ReadFloat(JToken token)
		{
			ExpectNumber(token, "float");
			return (float)token;
		}

		public static double ReadDouble(JToken token)
		{
			ExpectNumber(token, "double");
			return (double)token;
		}

		public static string ReadString(JToken token)
		{
			ExpectType(token, JTokenType.String, "string");
			return (string)token;
		}

		// The name an enum value was written as, for the generated enum readers
		public static string ReadEnumName(JToken token)
		{
			ExpectType(token, JTokenType.String, "enum");
			return (string)token;
		}

		// Fills instance from the token and returns it
		public static T ReadObject<T>(JToken token, T instance) where T : MikanSerializableObject
		{
			ExpectType(token, JTokenType.Object, "object");
			instance.ReadFields((JObject)token);
			return instance;
		}

		// Refills list, or a new list when it is null, and returns it
		public static List<T> ReadList<T>(JToken token, List<T> list, Func<JToken, T> readElement)
		{
			ExpectType(token, JTokenType.Array, "list");

			if (list == null)
			{
				list = new List<T>();
			}

			list.Clear();
			foreach (JToken jsonElement in (JArray)token)
			{
				list.Add(readElement(jsonElement));
			}

			return list;
		}

		// Refills map, or a new map when it is null, and returns it
		public static Dictionary<K, V> ReadMap<K, V>(
			JToken token, Dictionary<K, V> map, Func<JToken, K> readKey, Func<JToken, V> readValue)
		{
			ExpectType(token, JTokenType.Array, "map");

			if (map == null)
			{
				map = new Dictionary<K, V>();
			}

			map.Clear();
			foreach (JToken jsonPair in (JArray)token)
			{
				map.Add(readKey(jsonPair["key"]), readValue(jsonPair["value"]));
			}

			return map;
		}

		// Fills value, or a new object when it is null, with an instance of the type class_name
		// names, and returns it. An empty class_name leaves the object as it was.
		public static PolymorphicObject ReadPolymorphic(JToken token, PolymorphicObject value)
		{
			ExpectType(token, JTokenType.Object, "polymorphic object");

			if (value == null)
			{
				value = new PolymorphicObject();
			}

			string runtimeClassName = (string)token["class_name"];
			if (string.IsNullOrEmpty(runtimeClassName))
			{
				return value;
			}

			var instance = MikanTypeRegistry.Create(runtimeClassName) as PolymorphicStruct;
			if (instance == null)
			{
				throw new Exception("Polymorphic object used an unknown runtime class_name: " + runtimeClassName);
			}

			ReadObject(token["value"], instance);
			value.setInstance(instance);
			return value;
		}

		// -- Field access by name -----

		// Assigns value to field when it is a T, an int for an enum T, or converts to a T
		public static bool TryAssign<T>(object value, ref T field)
		{
			if (value == null)
			{
				if (default(T) == null)
				{
					field = default(T);
					return true;
				}

				return false;
			}

			if (value is T typedValue)
			{
				field = typedValue;
				return true;
			}

			if (typeof(T).IsEnum && value is int enumValue)
			{
				field = (T)Enum.ToObject(typeof(T), enumValue);
				return true;
			}

			try
			{
				field = (T)Convert.ChangeType(value, typeof(T));
				return true;
			}
			catch (Exception)
			{
				return false;
			}
		}

		private static void ExpectType(JToken token, JTokenType expectedType, string valueKind)
		{
			if (token == null || token.Type != expectedType)
			{
				throw new Exception(
					"Expected a json " + expectedType + " for a " + valueKind + " value, found " +
					(token == null ? "nothing" : token.Type.ToString()));
			}
		}

		private static void ExpectNumber(JToken token, string valueKind)
		{
			if (token == null || (token.Type != JTokenType.Integer && token.Type != JTokenType.Float))
			{
				throw new Exception(
					"Expected a json number for a " + valueKind + " value, found " +
					(token == null ? "nothing" : token.Type.ToString()));
			}
		}
	}
}
