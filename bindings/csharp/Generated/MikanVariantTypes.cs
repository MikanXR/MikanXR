// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanVariantType
	{
		INVALID_TYPE= 0,
		BOOL_TYPE= 1,
		UBYTE_TYPE= 2,
		USHORT_TYPE= 3,
		INT_TYPE= 4,
		LONG_TYPE= 5,
		FLOAT_TYPE= 6,
		DOUBLE_TYPE= 7,
		MK_STRING_TYPE= 8,
		VECTOR2F_TYPE= 9,
		VECTOR3F_TYPE= 10,
		VECTOR4F_TYPE= 11,
		QUATERNIONF_TYPE= 12,
		MATRIX4F_TYPE= 13,
		VECTOR2D_TYPE= 14,
		VECTOR3D_TYPE= 15,
		VECTOR4D_TYPE= 16,
		QUATERNIOND_TYPE= 17,
		BOOL_ARRAY_TYPE= 18,
		UBYTE_ARRAY_TYPE= 19,
		INT_ARRAY_TYPE= 20,
		FLOAT_ARRAY_TYPE= 21,
		STRING_ARRAY_TYPE= 22,
		STRING_MAP_TYPE= 23,
		POLYMORPHIC_OBJECT_TYPE= 24,
	};

	internal static class MikanVariantTypeJson
	{
		public static JToken ToJson(MikanVariantType value)
		{
			switch (value)
			{
				case MikanVariantType.INVALID_TYPE: return MikanJson.WriteString("INVALID_TYPE");
				case MikanVariantType.BOOL_TYPE: return MikanJson.WriteString("BOOL_TYPE");
				case MikanVariantType.UBYTE_TYPE: return MikanJson.WriteString("UBYTE_TYPE");
				case MikanVariantType.USHORT_TYPE: return MikanJson.WriteString("USHORT_TYPE");
				case MikanVariantType.INT_TYPE: return MikanJson.WriteString("INT_TYPE");
				case MikanVariantType.LONG_TYPE: return MikanJson.WriteString("LONG_TYPE");
				case MikanVariantType.FLOAT_TYPE: return MikanJson.WriteString("FLOAT_TYPE");
				case MikanVariantType.DOUBLE_TYPE: return MikanJson.WriteString("DOUBLE_TYPE");
				case MikanVariantType.MK_STRING_TYPE: return MikanJson.WriteString("MK_STRING_TYPE");
				case MikanVariantType.VECTOR2F_TYPE: return MikanJson.WriteString("VECTOR2F_TYPE");
				case MikanVariantType.VECTOR3F_TYPE: return MikanJson.WriteString("VECTOR3F_TYPE");
				case MikanVariantType.VECTOR4F_TYPE: return MikanJson.WriteString("VECTOR4F_TYPE");
				case MikanVariantType.QUATERNIONF_TYPE: return MikanJson.WriteString("QUATERNIONF_TYPE");
				case MikanVariantType.MATRIX4F_TYPE: return MikanJson.WriteString("MATRIX4F_TYPE");
				case MikanVariantType.VECTOR2D_TYPE: return MikanJson.WriteString("VECTOR2D_TYPE");
				case MikanVariantType.VECTOR3D_TYPE: return MikanJson.WriteString("VECTOR3D_TYPE");
				case MikanVariantType.VECTOR4D_TYPE: return MikanJson.WriteString("VECTOR4D_TYPE");
				case MikanVariantType.QUATERNIOND_TYPE: return MikanJson.WriteString("QUATERNIOND_TYPE");
				case MikanVariantType.BOOL_ARRAY_TYPE: return MikanJson.WriteString("BOOL_ARRAY_TYPE");
				case MikanVariantType.UBYTE_ARRAY_TYPE: return MikanJson.WriteString("UBYTE_ARRAY_TYPE");
				case MikanVariantType.INT_ARRAY_TYPE: return MikanJson.WriteString("INT_ARRAY_TYPE");
				case MikanVariantType.FLOAT_ARRAY_TYPE: return MikanJson.WriteString("FLOAT_ARRAY_TYPE");
				case MikanVariantType.STRING_ARRAY_TYPE: return MikanJson.WriteString("STRING_ARRAY_TYPE");
				case MikanVariantType.STRING_MAP_TYPE: return MikanJson.WriteString("STRING_MAP_TYPE");
				case MikanVariantType.POLYMORPHIC_OBJECT_TYPE: return MikanJson.WriteString("POLYMORPHIC_OBJECT_TYPE");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanVariantType has no wire name for " + (long)value);
		}

		public static MikanVariantType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanVariantType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID_TYPE": return MikanVariantType.INVALID_TYPE;
				case "BOOL_TYPE": return MikanVariantType.BOOL_TYPE;
				case "UBYTE_TYPE": return MikanVariantType.UBYTE_TYPE;
				case "USHORT_TYPE": return MikanVariantType.USHORT_TYPE;
				case "INT_TYPE": return MikanVariantType.INT_TYPE;
				case "LONG_TYPE": return MikanVariantType.LONG_TYPE;
				case "FLOAT_TYPE": return MikanVariantType.FLOAT_TYPE;
				case "DOUBLE_TYPE": return MikanVariantType.DOUBLE_TYPE;
				case "MK_STRING_TYPE": return MikanVariantType.MK_STRING_TYPE;
				case "VECTOR2F_TYPE": return MikanVariantType.VECTOR2F_TYPE;
				case "VECTOR3F_TYPE": return MikanVariantType.VECTOR3F_TYPE;
				case "VECTOR4F_TYPE": return MikanVariantType.VECTOR4F_TYPE;
				case "QUATERNIONF_TYPE": return MikanVariantType.QUATERNIONF_TYPE;
				case "MATRIX4F_TYPE": return MikanVariantType.MATRIX4F_TYPE;
				case "VECTOR2D_TYPE": return MikanVariantType.VECTOR2D_TYPE;
				case "VECTOR3D_TYPE": return MikanVariantType.VECTOR3D_TYPE;
				case "VECTOR4D_TYPE": return MikanVariantType.VECTOR4D_TYPE;
				case "QUATERNIOND_TYPE": return MikanVariantType.QUATERNIOND_TYPE;
				case "BOOL_ARRAY_TYPE": return MikanVariantType.BOOL_ARRAY_TYPE;
				case "UBYTE_ARRAY_TYPE": return MikanVariantType.UBYTE_ARRAY_TYPE;
				case "INT_ARRAY_TYPE": return MikanVariantType.INT_ARRAY_TYPE;
				case "FLOAT_ARRAY_TYPE": return MikanVariantType.FLOAT_ARRAY_TYPE;
				case "STRING_ARRAY_TYPE": return MikanVariantType.STRING_ARRAY_TYPE;
				case "STRING_MAP_TYPE": return MikanVariantType.STRING_MAP_TYPE;
				case "POLYMORPHIC_OBJECT_TYPE": return MikanVariantType.POLYMORPHIC_OBJECT_TYPE;
			}
			throw new Exception("MikanVariantType has no value named " + name);
		}
	};

	public class MikanBoolArrayValue : MikanVariantBase
	{
		public List<bool> value;

		public override string RuntimeClassName => "MikanBoolArrayValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteList(this.value, e0 => MikanJson.WriteBool(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadList(MikanJson.Field(json, "value"), this.value, e0 => MikanJson.ReadBool(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanBoolValue : MikanVariantBase
	{
		public bool value;

		public override string RuntimeClassName => "MikanBoolValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteBool(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadBool(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDoubleValue : MikanVariantBase
	{
		public double value;

		public override string RuntimeClassName => "MikanDoubleValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteDouble(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadDouble(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanFloatArrayValue : MikanVariantBase
	{
		public List<float> value;

		public override string RuntimeClassName => "MikanFloatArrayValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteList(this.value, e0 => MikanJson.WriteFloat(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadList(MikanJson.Field(json, "value"), this.value, e0 => MikanJson.ReadFloat(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanFloatValue : MikanVariantBase
	{
		public float value;

		public override string RuntimeClassName => "MikanFloatValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteFloat(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadFloat(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanIntArrayValue : MikanVariantBase
	{
		public List<int> value;

		public override string RuntimeClassName => "MikanIntArrayValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteList(this.value, e0 => MikanJson.WriteInt(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadList(MikanJson.Field(json, "value"), this.value, e0 => MikanJson.ReadInt(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanIntValue : MikanVariantBase
	{
		public int value;

		public override string RuntimeClassName => "MikanIntValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteInt(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadInt(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanLongValue : MikanVariantBase
	{
		public long value;

		public override string RuntimeClassName => "MikanLongValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteLong(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadLong(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanMatrix4fValue : MikanVariantBase
	{
		public MikanMatrix4f value;

		public override string RuntimeClassName => "MikanMatrix4fValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanMatrix4f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanQuatdValue : MikanVariantBase
	{
		public MikanQuatd value;

		public override string RuntimeClassName => "MikanQuatdValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanQuatd());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanQuatfValue : MikanVariantBase
	{
		public MikanQuatf value;

		public override string RuntimeClassName => "MikanQuatfValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanQuatf());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanStringArrayValue : MikanVariantBase
	{
		public List<string> value;

		public override string RuntimeClassName => "MikanStringArrayValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteList(this.value, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadList(MikanJson.Field(json, "value"), this.value, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanStringMapValue : MikanVariantBase
	{
		public Dictionary<string, string> value;

		public override string RuntimeClassName => "MikanStringMapValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteMap(this.value, k0 => MikanJson.WriteString(k0), v0 => MikanJson.WriteString(v0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadMap(MikanJson.Field(json, "value"), this.value, k0 => MikanJson.ReadString(k0), v0 => MikanJson.ReadString(v0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanStringValue : MikanVariantBase
	{
		public string value;

		public override string RuntimeClassName => "MikanStringValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteString(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadString(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanUByteArrayValue : MikanVariantBase
	{
		public List<byte> value;

		public override string RuntimeClassName => "MikanUByteArrayValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteList(this.value, e0 => MikanJson.WriteByte(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadList(MikanJson.Field(json, "value"), this.value, e0 => MikanJson.ReadByte(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanUByteValue : MikanVariantBase
	{
		public byte value;

		public override string RuntimeClassName => "MikanUByteValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteByte(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadByte(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanUShortValue : MikanVariantBase
	{
		public ushort value;

		public override string RuntimeClassName => "MikanUShortValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteUShort(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadUShort(MikanJson.Field(json, "value"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVariant : MikanSerializableObject
	{
		public MikanVariantType value_type;
		public PolymorphicObject value_ptr;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value_type", MikanVariantTypeJson.ToJson(this.value_type));
			json.Add("value_ptr", MikanJson.WritePolymorphic(this.value_ptr));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value_type = MikanVariantTypeJson.FromJson(MikanJson.Field(json, "value_type"));
			this.value_ptr = MikanJson.ReadPolymorphic(MikanJson.Field(json, "value_ptr"), this.value_ptr);
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value_type": value = this.value_type; return true;
				case "value_ptr": value = this.value_ptr; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value_type": return MikanJson.TryAssign(value, ref this.value_type);
				case "value_ptr": return MikanJson.TryAssign(value, ref this.value_ptr);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVariantBase : PolymorphicStruct
	{

		public override string RuntimeClassName => "MikanVariantBase";
	};

	public class MikanVector2dValue : MikanVariantBase
	{
		public MikanVector2d value;

		public override string RuntimeClassName => "MikanVector2dValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanVector2d());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector2fValue : MikanVariantBase
	{
		public MikanVector2f value;

		public override string RuntimeClassName => "MikanVector2fValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanVector2f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector3dValue : MikanVariantBase
	{
		public MikanVector3d value;

		public override string RuntimeClassName => "MikanVector3dValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanVector3d());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector3fValue : MikanVariantBase
	{
		public MikanVector3f value;

		public override string RuntimeClassName => "MikanVector3fValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanVector3f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector4dValue : MikanVariantBase
	{
		public MikanVector4d value;

		public override string RuntimeClassName => "MikanVector4dValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanVector4d());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector4fValue : MikanVariantBase
	{
		public MikanVector4f value;

		public override string RuntimeClassName => "MikanVector4fValue";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("value", MikanJson.WriteObject(this.value));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.value = MikanJson.ReadObject(MikanJson.Field(json, "value"), this.value ?? new MikanVector4f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "value": value = this.value; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "value": return MikanJson.TryAssign(value, ref this.value);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
