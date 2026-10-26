// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanTransformComponentValues : MikanComponentValues
	{
		public int parent_transform_id;
		public MikanVector3f relative_scale;
		public MikanQuatf relative_quaternion;
		public MikanVector3f relative_position;

		public override string RuntimeClassName => "MikanTransformComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("parent_transform_id", MikanJson.WriteInt(this.parent_transform_id));
			json.Add("relative_scale", MikanJson.WriteObject(this.relative_scale));
			json.Add("relative_quaternion", MikanJson.WriteObject(this.relative_quaternion));
			json.Add("relative_position", MikanJson.WriteObject(this.relative_position));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.parent_transform_id = MikanJson.ReadInt(MikanJson.Field(json, "parent_transform_id"));
			this.relative_scale = MikanJson.ReadObject(MikanJson.Field(json, "relative_scale"), this.relative_scale ?? new MikanVector3f());
			this.relative_quaternion = MikanJson.ReadObject(MikanJson.Field(json, "relative_quaternion"), this.relative_quaternion ?? new MikanQuatf());
			this.relative_position = MikanJson.ReadObject(MikanJson.Field(json, "relative_position"), this.relative_position ?? new MikanVector3f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "parent_transform_id": value = this.parent_transform_id; return true;
				case "relative_scale": value = this.relative_scale; return true;
				case "relative_quaternion": value = this.relative_quaternion; return true;
				case "relative_position": value = this.relative_position; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "parent_transform_id": return MikanJson.TryAssign(value, ref this.parent_transform_id);
				case "relative_scale": return MikanJson.TryAssign(value, ref this.relative_scale);
				case "relative_quaternion": return MikanJson.TryAssign(value, ref this.relative_quaternion);
				case "relative_position": return MikanJson.TryAssign(value, ref this.relative_position);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
