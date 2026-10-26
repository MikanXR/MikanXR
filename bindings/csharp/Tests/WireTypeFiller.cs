using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;

namespace MikanXR
{
	// Fills any wire type with distinct non-default values, through reflection. Test code only:
	// the runtime itself never reflects, but a test that covers every generated type has to.
	class WireTypeFiller
	{
		// Containers stop growing sooner than objects, which have to be filled all the way down
		// since a null nested object cannot be serialized
		private const int k_maxContainerDepth = 4;
		private const int k_maxObjectDepth = 8;
		private int _seed = 0;

		// False when the type, or a type it holds, has a field the wire cannot carry
		public static bool IsSerializable(Type type)
		{
			return IsSerializable(type, 0);
		}

		public void Fill(object instance)
		{
			Fill(instance, instance.GetType(), 0);
		}

		private static bool IsSerializable(Type type, int depth)
		{
			if (type == typeof(IntPtr) || type == typeof(ulong))
			{
				return false;
			}

			if (depth > k_maxObjectDepth)
			{
				return true;
			}

			if (type.IsGenericType)
			{
				foreach (Type argument in type.GetGenericArguments())
				{
					if (!IsSerializable(argument, depth + 1))
					{
						return false;
					}
				}
			}
			else if (typeof(MikanSerializableObject).IsAssignableFrom(type))
			{
				foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public))
				{
					if (!IsSerializable(field.FieldType, depth + 1))
					{
						return false;
					}
				}
			}

			return true;
		}

		private void Fill(object instance, Type type, int depth)
		{
			foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public))
			{
				field.SetValue(instance, MakeValue(field.FieldType, field.Name, depth));
			}
		}

		private object MakeValue(Type type, string name, int depth)
		{
			_seed++;

			if (type == typeof(bool)) return (_seed % 2) == 0;
			if (type == typeof(sbyte)) return (sbyte)(-(_seed % 100));
			if (type == typeof(byte)) return (byte)(_seed % 200);
			if (type == typeof(short)) return (short)(-_seed * 3);
			if (type == typeof(ushort)) return (ushort)(_seed * 5);
			if (type == typeof(int)) return -_seed * 7;
			if (type == typeof(uint)) return (uint)(_seed * 11);
			if (type == typeof(long)) return -1234567890123L - _seed;
			if (type == typeof(float)) return 0.1f * _seed + 1.1f;
			if (type == typeof(double)) return 0.1 * _seed + 3.141592653589793;
			if (type == typeof(string)) return name + "_" + _seed;

			if (type.IsEnum)
			{
				Array values = Enum.GetValues(type);
				return values.Length > 0 ? values.GetValue(_seed % values.Length) : Activator.CreateInstance(type);
			}

			if (type == typeof(PolymorphicObject))
			{
				var floatValue = new MikanFloatValue();
				floatValue.value = 0.1f * _seed + 2.5f;
				return new PolymorphicObject(floatValue);
			}

			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>))
			{
				var list = (IList)Activator.CreateInstance(type);
				if (depth < k_maxContainerDepth)
				{
					Type elementType = type.GetGenericArguments()[0];
					list.Add(MakeValue(elementType, name, depth + 1));
					list.Add(MakeValue(elementType, name, depth + 1));
				}
				return list;
			}

			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Dictionary<,>))
			{
				var map = (IDictionary)Activator.CreateInstance(type);
				if (depth < k_maxContainerDepth)
				{
					Type keyType = type.GetGenericArguments()[0];
					Type valueType = type.GetGenericArguments()[1];
					object firstKey = MakeValue(keyType, name, depth + 1);
					object secondKey = MakeValue(keyType, name, depth + 1);
					map[firstKey] = MakeValue(valueType, name, depth + 1);
					if (!secondKey.Equals(firstKey))
					{
						map[secondKey] = MakeValue(valueType, name, depth + 1);
					}
				}
				return map;
			}

			if (typeof(MikanSerializableObject).IsAssignableFrom(type))
			{
				object nested = Activator.CreateInstance(type);
				if (depth < k_maxObjectDepth)
				{
					Fill(nested, type, depth + 1);
				}
				return nested;
			}

			throw new Exception("No test value for " + type.FullName);
		}
	}
}
