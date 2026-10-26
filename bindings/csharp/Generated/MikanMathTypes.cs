// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanMatrix3d : MikanSerializableObject
	{
		public double x0;
		public double x1;
		public double x2;
		public double y0;
		public double y1;
		public double y2;
		public double z0;
		public double z1;
		public double z2;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x0", MikanJson.WriteDouble(this.x0));
			json.Add("x1", MikanJson.WriteDouble(this.x1));
			json.Add("x2", MikanJson.WriteDouble(this.x2));
			json.Add("y0", MikanJson.WriteDouble(this.y0));
			json.Add("y1", MikanJson.WriteDouble(this.y1));
			json.Add("y2", MikanJson.WriteDouble(this.y2));
			json.Add("z0", MikanJson.WriteDouble(this.z0));
			json.Add("z1", MikanJson.WriteDouble(this.z1));
			json.Add("z2", MikanJson.WriteDouble(this.z2));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x0 = MikanJson.ReadDouble(MikanJson.Field(json, "x0"));
			this.x1 = MikanJson.ReadDouble(MikanJson.Field(json, "x1"));
			this.x2 = MikanJson.ReadDouble(MikanJson.Field(json, "x2"));
			this.y0 = MikanJson.ReadDouble(MikanJson.Field(json, "y0"));
			this.y1 = MikanJson.ReadDouble(MikanJson.Field(json, "y1"));
			this.y2 = MikanJson.ReadDouble(MikanJson.Field(json, "y2"));
			this.z0 = MikanJson.ReadDouble(MikanJson.Field(json, "z0"));
			this.z1 = MikanJson.ReadDouble(MikanJson.Field(json, "z1"));
			this.z2 = MikanJson.ReadDouble(MikanJson.Field(json, "z2"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x0": value = this.x0; return true;
				case "x1": value = this.x1; return true;
				case "x2": value = this.x2; return true;
				case "y0": value = this.y0; return true;
				case "y1": value = this.y1; return true;
				case "y2": value = this.y2; return true;
				case "z0": value = this.z0; return true;
				case "z1": value = this.z1; return true;
				case "z2": value = this.z2; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x0": return MikanJson.TryAssign(value, ref this.x0);
				case "x1": return MikanJson.TryAssign(value, ref this.x1);
				case "x2": return MikanJson.TryAssign(value, ref this.x2);
				case "y0": return MikanJson.TryAssign(value, ref this.y0);
				case "y1": return MikanJson.TryAssign(value, ref this.y1);
				case "y2": return MikanJson.TryAssign(value, ref this.y2);
				case "z0": return MikanJson.TryAssign(value, ref this.z0);
				case "z1": return MikanJson.TryAssign(value, ref this.z1);
				case "z2": return MikanJson.TryAssign(value, ref this.z2);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanMatrix4d : MikanSerializableObject
	{
		public double x0;
		public double x1;
		public double x2;
		public double x3;
		public double y0;
		public double y1;
		public double y2;
		public double y3;
		public double z0;
		public double z1;
		public double z2;
		public double z3;
		public double w0;
		public double w1;
		public double w2;
		public double w3;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x0", MikanJson.WriteDouble(this.x0));
			json.Add("x1", MikanJson.WriteDouble(this.x1));
			json.Add("x2", MikanJson.WriteDouble(this.x2));
			json.Add("x3", MikanJson.WriteDouble(this.x3));
			json.Add("y0", MikanJson.WriteDouble(this.y0));
			json.Add("y1", MikanJson.WriteDouble(this.y1));
			json.Add("y2", MikanJson.WriteDouble(this.y2));
			json.Add("y3", MikanJson.WriteDouble(this.y3));
			json.Add("z0", MikanJson.WriteDouble(this.z0));
			json.Add("z1", MikanJson.WriteDouble(this.z1));
			json.Add("z2", MikanJson.WriteDouble(this.z2));
			json.Add("z3", MikanJson.WriteDouble(this.z3));
			json.Add("w0", MikanJson.WriteDouble(this.w0));
			json.Add("w1", MikanJson.WriteDouble(this.w1));
			json.Add("w2", MikanJson.WriteDouble(this.w2));
			json.Add("w3", MikanJson.WriteDouble(this.w3));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x0 = MikanJson.ReadDouble(MikanJson.Field(json, "x0"));
			this.x1 = MikanJson.ReadDouble(MikanJson.Field(json, "x1"));
			this.x2 = MikanJson.ReadDouble(MikanJson.Field(json, "x2"));
			this.x3 = MikanJson.ReadDouble(MikanJson.Field(json, "x3"));
			this.y0 = MikanJson.ReadDouble(MikanJson.Field(json, "y0"));
			this.y1 = MikanJson.ReadDouble(MikanJson.Field(json, "y1"));
			this.y2 = MikanJson.ReadDouble(MikanJson.Field(json, "y2"));
			this.y3 = MikanJson.ReadDouble(MikanJson.Field(json, "y3"));
			this.z0 = MikanJson.ReadDouble(MikanJson.Field(json, "z0"));
			this.z1 = MikanJson.ReadDouble(MikanJson.Field(json, "z1"));
			this.z2 = MikanJson.ReadDouble(MikanJson.Field(json, "z2"));
			this.z3 = MikanJson.ReadDouble(MikanJson.Field(json, "z3"));
			this.w0 = MikanJson.ReadDouble(MikanJson.Field(json, "w0"));
			this.w1 = MikanJson.ReadDouble(MikanJson.Field(json, "w1"));
			this.w2 = MikanJson.ReadDouble(MikanJson.Field(json, "w2"));
			this.w3 = MikanJson.ReadDouble(MikanJson.Field(json, "w3"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x0": value = this.x0; return true;
				case "x1": value = this.x1; return true;
				case "x2": value = this.x2; return true;
				case "x3": value = this.x3; return true;
				case "y0": value = this.y0; return true;
				case "y1": value = this.y1; return true;
				case "y2": value = this.y2; return true;
				case "y3": value = this.y3; return true;
				case "z0": value = this.z0; return true;
				case "z1": value = this.z1; return true;
				case "z2": value = this.z2; return true;
				case "z3": value = this.z3; return true;
				case "w0": value = this.w0; return true;
				case "w1": value = this.w1; return true;
				case "w2": value = this.w2; return true;
				case "w3": value = this.w3; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x0": return MikanJson.TryAssign(value, ref this.x0);
				case "x1": return MikanJson.TryAssign(value, ref this.x1);
				case "x2": return MikanJson.TryAssign(value, ref this.x2);
				case "x3": return MikanJson.TryAssign(value, ref this.x3);
				case "y0": return MikanJson.TryAssign(value, ref this.y0);
				case "y1": return MikanJson.TryAssign(value, ref this.y1);
				case "y2": return MikanJson.TryAssign(value, ref this.y2);
				case "y3": return MikanJson.TryAssign(value, ref this.y3);
				case "z0": return MikanJson.TryAssign(value, ref this.z0);
				case "z1": return MikanJson.TryAssign(value, ref this.z1);
				case "z2": return MikanJson.TryAssign(value, ref this.z2);
				case "z3": return MikanJson.TryAssign(value, ref this.z3);
				case "w0": return MikanJson.TryAssign(value, ref this.w0);
				case "w1": return MikanJson.TryAssign(value, ref this.w1);
				case "w2": return MikanJson.TryAssign(value, ref this.w2);
				case "w3": return MikanJson.TryAssign(value, ref this.w3);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanMatrix4f : MikanSerializableObject
	{
		public float x0;
		public float x1;
		public float x2;
		public float x3;
		public float y0;
		public float y1;
		public float y2;
		public float y3;
		public float z0;
		public float z1;
		public float z2;
		public float z3;
		public float w0;
		public float w1;
		public float w2;
		public float w3;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x0", MikanJson.WriteFloat(this.x0));
			json.Add("x1", MikanJson.WriteFloat(this.x1));
			json.Add("x2", MikanJson.WriteFloat(this.x2));
			json.Add("x3", MikanJson.WriteFloat(this.x3));
			json.Add("y0", MikanJson.WriteFloat(this.y0));
			json.Add("y1", MikanJson.WriteFloat(this.y1));
			json.Add("y2", MikanJson.WriteFloat(this.y2));
			json.Add("y3", MikanJson.WriteFloat(this.y3));
			json.Add("z0", MikanJson.WriteFloat(this.z0));
			json.Add("z1", MikanJson.WriteFloat(this.z1));
			json.Add("z2", MikanJson.WriteFloat(this.z2));
			json.Add("z3", MikanJson.WriteFloat(this.z3));
			json.Add("w0", MikanJson.WriteFloat(this.w0));
			json.Add("w1", MikanJson.WriteFloat(this.w1));
			json.Add("w2", MikanJson.WriteFloat(this.w2));
			json.Add("w3", MikanJson.WriteFloat(this.w3));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x0 = MikanJson.ReadFloat(MikanJson.Field(json, "x0"));
			this.x1 = MikanJson.ReadFloat(MikanJson.Field(json, "x1"));
			this.x2 = MikanJson.ReadFloat(MikanJson.Field(json, "x2"));
			this.x3 = MikanJson.ReadFloat(MikanJson.Field(json, "x3"));
			this.y0 = MikanJson.ReadFloat(MikanJson.Field(json, "y0"));
			this.y1 = MikanJson.ReadFloat(MikanJson.Field(json, "y1"));
			this.y2 = MikanJson.ReadFloat(MikanJson.Field(json, "y2"));
			this.y3 = MikanJson.ReadFloat(MikanJson.Field(json, "y3"));
			this.z0 = MikanJson.ReadFloat(MikanJson.Field(json, "z0"));
			this.z1 = MikanJson.ReadFloat(MikanJson.Field(json, "z1"));
			this.z2 = MikanJson.ReadFloat(MikanJson.Field(json, "z2"));
			this.z3 = MikanJson.ReadFloat(MikanJson.Field(json, "z3"));
			this.w0 = MikanJson.ReadFloat(MikanJson.Field(json, "w0"));
			this.w1 = MikanJson.ReadFloat(MikanJson.Field(json, "w1"));
			this.w2 = MikanJson.ReadFloat(MikanJson.Field(json, "w2"));
			this.w3 = MikanJson.ReadFloat(MikanJson.Field(json, "w3"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x0": value = this.x0; return true;
				case "x1": value = this.x1; return true;
				case "x2": value = this.x2; return true;
				case "x3": value = this.x3; return true;
				case "y0": value = this.y0; return true;
				case "y1": value = this.y1; return true;
				case "y2": value = this.y2; return true;
				case "y3": value = this.y3; return true;
				case "z0": value = this.z0; return true;
				case "z1": value = this.z1; return true;
				case "z2": value = this.z2; return true;
				case "z3": value = this.z3; return true;
				case "w0": value = this.w0; return true;
				case "w1": value = this.w1; return true;
				case "w2": value = this.w2; return true;
				case "w3": value = this.w3; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x0": return MikanJson.TryAssign(value, ref this.x0);
				case "x1": return MikanJson.TryAssign(value, ref this.x1);
				case "x2": return MikanJson.TryAssign(value, ref this.x2);
				case "x3": return MikanJson.TryAssign(value, ref this.x3);
				case "y0": return MikanJson.TryAssign(value, ref this.y0);
				case "y1": return MikanJson.TryAssign(value, ref this.y1);
				case "y2": return MikanJson.TryAssign(value, ref this.y2);
				case "y3": return MikanJson.TryAssign(value, ref this.y3);
				case "z0": return MikanJson.TryAssign(value, ref this.z0);
				case "z1": return MikanJson.TryAssign(value, ref this.z1);
				case "z2": return MikanJson.TryAssign(value, ref this.z2);
				case "z3": return MikanJson.TryAssign(value, ref this.z3);
				case "w0": return MikanJson.TryAssign(value, ref this.w0);
				case "w1": return MikanJson.TryAssign(value, ref this.w1);
				case "w2": return MikanJson.TryAssign(value, ref this.w2);
				case "w3": return MikanJson.TryAssign(value, ref this.w3);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanMatrix4x3d : MikanSerializableObject
	{
		public double x0;
		public double x1;
		public double x2;
		public double x3;
		public double y0;
		public double y1;
		public double y2;
		public double y3;
		public double z0;
		public double z1;
		public double z2;
		public double z3;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x0", MikanJson.WriteDouble(this.x0));
			json.Add("x1", MikanJson.WriteDouble(this.x1));
			json.Add("x2", MikanJson.WriteDouble(this.x2));
			json.Add("x3", MikanJson.WriteDouble(this.x3));
			json.Add("y0", MikanJson.WriteDouble(this.y0));
			json.Add("y1", MikanJson.WriteDouble(this.y1));
			json.Add("y2", MikanJson.WriteDouble(this.y2));
			json.Add("y3", MikanJson.WriteDouble(this.y3));
			json.Add("z0", MikanJson.WriteDouble(this.z0));
			json.Add("z1", MikanJson.WriteDouble(this.z1));
			json.Add("z2", MikanJson.WriteDouble(this.z2));
			json.Add("z3", MikanJson.WriteDouble(this.z3));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x0 = MikanJson.ReadDouble(MikanJson.Field(json, "x0"));
			this.x1 = MikanJson.ReadDouble(MikanJson.Field(json, "x1"));
			this.x2 = MikanJson.ReadDouble(MikanJson.Field(json, "x2"));
			this.x3 = MikanJson.ReadDouble(MikanJson.Field(json, "x3"));
			this.y0 = MikanJson.ReadDouble(MikanJson.Field(json, "y0"));
			this.y1 = MikanJson.ReadDouble(MikanJson.Field(json, "y1"));
			this.y2 = MikanJson.ReadDouble(MikanJson.Field(json, "y2"));
			this.y3 = MikanJson.ReadDouble(MikanJson.Field(json, "y3"));
			this.z0 = MikanJson.ReadDouble(MikanJson.Field(json, "z0"));
			this.z1 = MikanJson.ReadDouble(MikanJson.Field(json, "z1"));
			this.z2 = MikanJson.ReadDouble(MikanJson.Field(json, "z2"));
			this.z3 = MikanJson.ReadDouble(MikanJson.Field(json, "z3"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x0": value = this.x0; return true;
				case "x1": value = this.x1; return true;
				case "x2": value = this.x2; return true;
				case "x3": value = this.x3; return true;
				case "y0": value = this.y0; return true;
				case "y1": value = this.y1; return true;
				case "y2": value = this.y2; return true;
				case "y3": value = this.y3; return true;
				case "z0": value = this.z0; return true;
				case "z1": value = this.z1; return true;
				case "z2": value = this.z2; return true;
				case "z3": value = this.z3; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x0": return MikanJson.TryAssign(value, ref this.x0);
				case "x1": return MikanJson.TryAssign(value, ref this.x1);
				case "x2": return MikanJson.TryAssign(value, ref this.x2);
				case "x3": return MikanJson.TryAssign(value, ref this.x3);
				case "y0": return MikanJson.TryAssign(value, ref this.y0);
				case "y1": return MikanJson.TryAssign(value, ref this.y1);
				case "y2": return MikanJson.TryAssign(value, ref this.y2);
				case "y3": return MikanJson.TryAssign(value, ref this.y3);
				case "z0": return MikanJson.TryAssign(value, ref this.z0);
				case "z1": return MikanJson.TryAssign(value, ref this.z1);
				case "z2": return MikanJson.TryAssign(value, ref this.z2);
				case "z3": return MikanJson.TryAssign(value, ref this.z3);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanQuatd : MikanSerializableObject
	{
		public double w;
		public double x;
		public double y;
		public double z;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("w", MikanJson.WriteDouble(this.w));
			json.Add("x", MikanJson.WriteDouble(this.x));
			json.Add("y", MikanJson.WriteDouble(this.y));
			json.Add("z", MikanJson.WriteDouble(this.z));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.w = MikanJson.ReadDouble(MikanJson.Field(json, "w"));
			this.x = MikanJson.ReadDouble(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadDouble(MikanJson.Field(json, "y"));
			this.z = MikanJson.ReadDouble(MikanJson.Field(json, "z"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "w": value = this.w; return true;
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
				case "z": value = this.z; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "w": return MikanJson.TryAssign(value, ref this.w);
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
				case "z": return MikanJson.TryAssign(value, ref this.z);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanQuatf : MikanSerializableObject
	{
		public float w;
		public float x;
		public float y;
		public float z;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("w", MikanJson.WriteFloat(this.w));
			json.Add("x", MikanJson.WriteFloat(this.x));
			json.Add("y", MikanJson.WriteFloat(this.y));
			json.Add("z", MikanJson.WriteFloat(this.z));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.w = MikanJson.ReadFloat(MikanJson.Field(json, "w"));
			this.x = MikanJson.ReadFloat(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadFloat(MikanJson.Field(json, "y"));
			this.z = MikanJson.ReadFloat(MikanJson.Field(json, "z"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "w": value = this.w; return true;
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
				case "z": value = this.z; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "w": return MikanJson.TryAssign(value, ref this.w);
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
				case "z": return MikanJson.TryAssign(value, ref this.z);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRotator3f : MikanSerializableObject
	{
		public float x_angle;
		public float y_angle;
		public float z_angle;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x_angle", MikanJson.WriteFloat(this.x_angle));
			json.Add("y_angle", MikanJson.WriteFloat(this.y_angle));
			json.Add("z_angle", MikanJson.WriteFloat(this.z_angle));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x_angle = MikanJson.ReadFloat(MikanJson.Field(json, "x_angle"));
			this.y_angle = MikanJson.ReadFloat(MikanJson.Field(json, "y_angle"));
			this.z_angle = MikanJson.ReadFloat(MikanJson.Field(json, "z_angle"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x_angle": value = this.x_angle; return true;
				case "y_angle": value = this.y_angle; return true;
				case "z_angle": value = this.z_angle; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x_angle": return MikanJson.TryAssign(value, ref this.x_angle);
				case "y_angle": return MikanJson.TryAssign(value, ref this.y_angle);
				case "z_angle": return MikanJson.TryAssign(value, ref this.z_angle);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanTransform : MikanSerializableObject
	{
		public MikanVector3f scale;
		public MikanQuatf rotation;
		public MikanVector3f position;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("scale", MikanJson.WriteObject(this.scale));
			json.Add("rotation", MikanJson.WriteObject(this.rotation));
			json.Add("position", MikanJson.WriteObject(this.position));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.scale = MikanJson.ReadObject(MikanJson.Field(json, "scale"), this.scale ?? new MikanVector3f());
			this.rotation = MikanJson.ReadObject(MikanJson.Field(json, "rotation"), this.rotation ?? new MikanQuatf());
			this.position = MikanJson.ReadObject(MikanJson.Field(json, "position"), this.position ?? new MikanVector3f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "scale": value = this.scale; return true;
				case "rotation": value = this.rotation; return true;
				case "position": value = this.position; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "scale": return MikanJson.TryAssign(value, ref this.scale);
				case "rotation": return MikanJson.TryAssign(value, ref this.rotation);
				case "position": return MikanJson.TryAssign(value, ref this.position);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector2d : MikanSerializableObject
	{
		public double x;
		public double y;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteDouble(this.x));
			json.Add("y", MikanJson.WriteDouble(this.y));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadDouble(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadDouble(MikanJson.Field(json, "y"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector2f : MikanSerializableObject
	{
		public float x;
		public float y;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteFloat(this.x));
			json.Add("y", MikanJson.WriteFloat(this.y));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadFloat(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadFloat(MikanJson.Field(json, "y"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector2i : MikanSerializableObject
	{
		public int x;
		public int y;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteInt(this.x));
			json.Add("y", MikanJson.WriteInt(this.y));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadInt(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadInt(MikanJson.Field(json, "y"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector3d : MikanSerializableObject
	{
		public double x;
		public double y;
		public double z;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteDouble(this.x));
			json.Add("y", MikanJson.WriteDouble(this.y));
			json.Add("z", MikanJson.WriteDouble(this.z));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadDouble(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadDouble(MikanJson.Field(json, "y"));
			this.z = MikanJson.ReadDouble(MikanJson.Field(json, "z"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
				case "z": value = this.z; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
				case "z": return MikanJson.TryAssign(value, ref this.z);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector3f : MikanSerializableObject
	{
		public float x;
		public float y;
		public float z;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteFloat(this.x));
			json.Add("y", MikanJson.WriteFloat(this.y));
			json.Add("z", MikanJson.WriteFloat(this.z));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadFloat(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadFloat(MikanJson.Field(json, "y"));
			this.z = MikanJson.ReadFloat(MikanJson.Field(json, "z"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
				case "z": value = this.z; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
				case "z": return MikanJson.TryAssign(value, ref this.z);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector4d : MikanSerializableObject
	{
		public double x;
		public double y;
		public double z;
		public double w;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteDouble(this.x));
			json.Add("y", MikanJson.WriteDouble(this.y));
			json.Add("z", MikanJson.WriteDouble(this.z));
			json.Add("w", MikanJson.WriteDouble(this.w));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadDouble(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadDouble(MikanJson.Field(json, "y"));
			this.z = MikanJson.ReadDouble(MikanJson.Field(json, "z"));
			this.w = MikanJson.ReadDouble(MikanJson.Field(json, "w"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
				case "z": value = this.z; return true;
				case "w": value = this.w; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
				case "z": return MikanJson.TryAssign(value, ref this.z);
				case "w": return MikanJson.TryAssign(value, ref this.w);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVector4f : MikanSerializableObject
	{
		public float x;
		public float y;
		public float z;
		public float w;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("x", MikanJson.WriteFloat(this.x));
			json.Add("y", MikanJson.WriteFloat(this.y));
			json.Add("z", MikanJson.WriteFloat(this.z));
			json.Add("w", MikanJson.WriteFloat(this.w));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.x = MikanJson.ReadFloat(MikanJson.Field(json, "x"));
			this.y = MikanJson.ReadFloat(MikanJson.Field(json, "y"));
			this.z = MikanJson.ReadFloat(MikanJson.Field(json, "z"));
			this.w = MikanJson.ReadFloat(MikanJson.Field(json, "w"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "x": value = this.x; return true;
				case "y": value = this.y; return true;
				case "z": value = this.z; return true;
				case "w": value = this.w; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "x": return MikanJson.TryAssign(value, ref this.x);
				case "y": return MikanJson.TryAssign(value, ref this.y);
				case "z": return MikanJson.TryAssign(value, ref this.z);
				case "w": return MikanJson.TryAssign(value, ref this.w);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
