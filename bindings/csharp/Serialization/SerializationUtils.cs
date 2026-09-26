using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;

namespace MikanXR
{
	internal class ValueAccessor
	{
		public ValueAccessor(object instance, Type type)
		{
			_instance = instance;
			_type = type;
			_name = type.Name;
		}

		public ValueAccessor(object instance, FieldInfo field)
		{
			_instance = instance;
			_fieldInfo = field;
			_type = field.FieldType;
			_name = field.Name;
		}

		public void ensureValueAllocated()
		{
			if (_fieldInfo != null)
			{
				// See if the field doesn't have an instance allocated to it
				object fieldValue= _fieldInfo.GetValue(_instance);
				if (fieldValue == null)
				{
					// Allocate an instance of the field type and assign it
					fieldValue = Activator.CreateInstance(_type);
					_fieldInfo.SetValue(_instance, fieldValue);
				}
			}
			else if (_instance == null)
			{
				_instance = Activator.CreateInstance(_type);
			}
		}

		public object getValueObject()
		{
			Debug.Assert(_instance != null);

			if (_fieldInfo != null)
			{
				return _fieldInfo.GetValue(_instance);
			}
			else
			{
				return _instance;
			}
		}

		public void setValueObject(object value)
		{
			Debug.Assert(_type == value.GetType());

			if (_fieldInfo != null)
			{
				Debug.Assert(_instance != null);
				_fieldInfo.SetValue(_instance, value);
			}
			else
			{
				_instance = value;
			}
		}

		public virtual T getValue<T>()
		{
			Debug.Assert(_type == typeof(T));
			Debug.Assert(_instance != null);
			
			if (_fieldInfo != null)
			{
				return (T)_fieldInfo.GetValue(_instance);
			}
			else
			{
				return (T)_instance;
			}
		}

		public virtual void setValue<T>(T value)
		{
			Debug.Assert(_type == typeof(T));

			if (_fieldInfo != null)
			{
				Debug.Assert(_instance != null);
				_fieldInfo.SetValue(_instance, value);
			}
			else
			{
				_instance= value;
			}
		}

		protected object _instance;
		public object ValueInstance => _instance;

		protected FieldInfo _fieldInfo;
		public FieldInfo ValueField => _fieldInfo;

		protected Type _type;
		public Type ValueType => _type;

		protected string _name;
		public string ValueName => _name;
	}

	internal interface IVisitor
	{
		void visitClass(ValueAccessor accessor);
		void visitList(ValueAccessor accessor);
		void visitDictionary(ValueAccessor accessor);
		void visitPolymorphicObject(ValueAccessor accessor);
		void visitEnum(ValueAccessor accessor);
		void visitBool(ValueAccessor accessor);
		void visitByte(ValueAccessor accessor);
		void visitUByte(ValueAccessor accessor);
		void visitShort(ValueAccessor accessor);
		void visitUShort(ValueAccessor accessor);
		void visitInt(ValueAccessor accessor);
		void visitUInt(ValueAccessor accessor);
		void visitLong(ValueAccessor accessor);
		void visitULong(ValueAccessor accessor);
		void visitFloat(ValueAccessor accessor);
		void visitDouble(ValueAccessor accessor);
		void visitString(ValueAccessor accessor);
	}

	internal static class Utils
	{
        public static void memoryOffsetSortStructFields(Type classType, ref List<FieldInfo> outFields)
		{
			// Add fields from the parent classes first
			if (classType.BaseType != null)
			{
				memoryOffsetSortStructFields(classType.BaseType, ref outFields);
			}

			// Add fields from the current class, in the wire order the generated bindings state.
			// Type.GetFields() is documented as returning fields in no particular order, so the
			// order is read off MikanFieldOrderAttribute rather than assumed from the array.
			// OrderBy is a stable sort, so a type carrying no such attribute (nothing generated,
			// but a hand-written serializable class would) keeps the order it came back in.
			FieldInfo[] fields = classType.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly);
			outFields.AddRange(
				fields.OrderBy(field => field.GetCustomAttribute<MikanFieldOrderAttribute>()?.Order ?? int.MaxValue));
		}

		public static void visitObject<T>(T instance, IVisitor visitor) where T : struct
		{
			visitObject(instance, typeof(T), visitor);
		}

		public static void visitObject(object instance, Type classType, IVisitor visitor)
		{
			List<FieldInfo> fields = new List<FieldInfo>();
			memoryOffsetSortStructFields(classType, ref fields);

			foreach (FieldInfo field in fields)
			{
				visitField(instance, field, visitor);
			}
		}

		public static void visitField(object instance, FieldInfo fieldInfo, IVisitor visitor)
		{
			visitValue(new ValueAccessor(instance, fieldInfo), visitor);
		}

		public static void visitValue(ValueAccessor accessor, IVisitor visitor)
		{
			Type type = accessor.ValueType;
			string name = accessor.ValueName;

			if (type.IsClass)
			{
				Type fieldType = accessor.ValueType;

				if (fieldType.IsGenericType)
				{
					if (fieldType.Name.StartsWith("List") &&
						fieldType.GenericTypeArguments.Length == 1)
					{
						visitor.visitList(accessor);
					}
					else if (fieldType.Name.StartsWith("Dictionary") &&
							fieldType.GenericTypeArguments.Length == 2)
					{
						visitor.visitDictionary(accessor);
					}
					else
					{
						visitor.visitClass(accessor);
					}
				}
				else if (fieldType.Name.StartsWith("PolymorphicObject"))
				{
					visitor.visitPolymorphicObject(accessor);
				}
				else if (fieldType == typeof(string))
				{
					visitor.visitString(accessor);
				}
				else
				{
					visitor.visitClass(accessor);
				}
			}
			else if (type.IsEnum)
			{
				visitor.visitEnum(accessor);
			}
			else if (type.IsPrimitive)
			{
				switch (Type.GetTypeCode(type))
				{
					case TypeCode.Boolean:
						visitor.visitBool(accessor);
						break;
					case TypeCode.Byte:
						visitor.visitUByte(accessor);
						break;
					case TypeCode.SByte:
						visitor.visitByte(accessor);
						break;
					case TypeCode.Int16:
						visitor.visitShort(accessor);
						break;
					case TypeCode.UInt16:
						visitor.visitUShort(accessor);
						break;
					case TypeCode.Int32:
						visitor.visitInt(accessor);
						break;
					case TypeCode.UInt32:
						visitor.visitUInt(accessor);
						break;
					case TypeCode.Int64:
						visitor.visitLong(accessor);
						break;
					case TypeCode.UInt64:
						visitor.visitULong(accessor);
						break;
					case TypeCode.Single:
						visitor.visitFloat(accessor);
						break;
					case TypeCode.Double:
						visitor.visitDouble(accessor);
						break;
					default:
						throw new NotImplementedException("Accessor " + name + " has unsupported type");
				}
			}
			else
			{
				throw new NotImplementedException("Accessor " + name + " has unsupported type");
			}
		}
	}
}
