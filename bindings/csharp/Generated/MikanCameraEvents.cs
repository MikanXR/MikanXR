// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCameraNewFrameEvent : MikanEvent
	{
		public int camera_id;
		public MikanVector3f camera_forward;
		public MikanVector3f camera_up;
		public MikanVector3f camera_position;
		public MikanVector2i pixel_size;
		public MikanVector2i aux_pixel_size;
		public MikanVector2d focal_length;
		public MikanVector2d principal_point;
		public MikanVector2d z_bounds;
		public long frame;

		public MikanCameraNewFrameEvent()
		{
			eventTypeName = "MikanCameraNewFrameEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("camera_forward", MikanJson.WriteObject(this.camera_forward));
			json.Add("camera_up", MikanJson.WriteObject(this.camera_up));
			json.Add("camera_position", MikanJson.WriteObject(this.camera_position));
			json.Add("pixel_size", MikanJson.WriteObject(this.pixel_size));
			json.Add("aux_pixel_size", MikanJson.WriteObject(this.aux_pixel_size));
			json.Add("focal_length", MikanJson.WriteObject(this.focal_length));
			json.Add("principal_point", MikanJson.WriteObject(this.principal_point));
			json.Add("z_bounds", MikanJson.WriteObject(this.z_bounds));
			json.Add("frame", MikanJson.WriteLong(this.frame));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.camera_forward = MikanJson.ReadObject(MikanJson.Field(json, "camera_forward"), this.camera_forward ?? new MikanVector3f());
			this.camera_up = MikanJson.ReadObject(MikanJson.Field(json, "camera_up"), this.camera_up ?? new MikanVector3f());
			this.camera_position = MikanJson.ReadObject(MikanJson.Field(json, "camera_position"), this.camera_position ?? new MikanVector3f());
			this.pixel_size = MikanJson.ReadObject(MikanJson.Field(json, "pixel_size"), this.pixel_size ?? new MikanVector2i());
			this.aux_pixel_size = MikanJson.ReadObject(MikanJson.Field(json, "aux_pixel_size"), this.aux_pixel_size ?? new MikanVector2i());
			this.focal_length = MikanJson.ReadObject(MikanJson.Field(json, "focal_length"), this.focal_length ?? new MikanVector2d());
			this.principal_point = MikanJson.ReadObject(MikanJson.Field(json, "principal_point"), this.principal_point ?? new MikanVector2d());
			this.z_bounds = MikanJson.ReadObject(MikanJson.Field(json, "z_bounds"), this.z_bounds ?? new MikanVector2d());
			this.frame = MikanJson.ReadLong(MikanJson.Field(json, "frame"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "camera_forward": value = this.camera_forward; return true;
				case "camera_up": value = this.camera_up; return true;
				case "camera_position": value = this.camera_position; return true;
				case "pixel_size": value = this.pixel_size; return true;
				case "aux_pixel_size": value = this.aux_pixel_size; return true;
				case "focal_length": value = this.focal_length; return true;
				case "principal_point": value = this.principal_point; return true;
				case "z_bounds": value = this.z_bounds; return true;
				case "frame": value = this.frame; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "camera_forward": return MikanJson.TryAssign(value, ref this.camera_forward);
				case "camera_up": return MikanJson.TryAssign(value, ref this.camera_up);
				case "camera_position": return MikanJson.TryAssign(value, ref this.camera_position);
				case "pixel_size": return MikanJson.TryAssign(value, ref this.pixel_size);
				case "aux_pixel_size": return MikanJson.TryAssign(value, ref this.aux_pixel_size);
				case "focal_length": return MikanJson.TryAssign(value, ref this.focal_length);
				case "principal_point": return MikanJson.TryAssign(value, ref this.principal_point);
				case "z_bounds": return MikanJson.TryAssign(value, ref this.z_bounds);
				case "frame": return MikanJson.TryAssign(value, ref this.frame);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanCameraNewPropertiesEvent : MikanEvent
	{
		public int camera_id;
		public MikanVector3f camera_forward;
		public MikanVector3f camera_up;
		public MikanVector3f camera_position;
		public MikanVector2i pixel_size;
		public MikanVector2i aux_pixel_size;
		public MikanVector2d focal_length;
		public MikanVector2d principal_point;
		public MikanVector2d z_bounds;

		public MikanCameraNewPropertiesEvent()
		{
			eventTypeName = "MikanCameraNewPropertiesEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("camera_forward", MikanJson.WriteObject(this.camera_forward));
			json.Add("camera_up", MikanJson.WriteObject(this.camera_up));
			json.Add("camera_position", MikanJson.WriteObject(this.camera_position));
			json.Add("pixel_size", MikanJson.WriteObject(this.pixel_size));
			json.Add("aux_pixel_size", MikanJson.WriteObject(this.aux_pixel_size));
			json.Add("focal_length", MikanJson.WriteObject(this.focal_length));
			json.Add("principal_point", MikanJson.WriteObject(this.principal_point));
			json.Add("z_bounds", MikanJson.WriteObject(this.z_bounds));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.camera_forward = MikanJson.ReadObject(MikanJson.Field(json, "camera_forward"), this.camera_forward ?? new MikanVector3f());
			this.camera_up = MikanJson.ReadObject(MikanJson.Field(json, "camera_up"), this.camera_up ?? new MikanVector3f());
			this.camera_position = MikanJson.ReadObject(MikanJson.Field(json, "camera_position"), this.camera_position ?? new MikanVector3f());
			this.pixel_size = MikanJson.ReadObject(MikanJson.Field(json, "pixel_size"), this.pixel_size ?? new MikanVector2i());
			this.aux_pixel_size = MikanJson.ReadObject(MikanJson.Field(json, "aux_pixel_size"), this.aux_pixel_size ?? new MikanVector2i());
			this.focal_length = MikanJson.ReadObject(MikanJson.Field(json, "focal_length"), this.focal_length ?? new MikanVector2d());
			this.principal_point = MikanJson.ReadObject(MikanJson.Field(json, "principal_point"), this.principal_point ?? new MikanVector2d());
			this.z_bounds = MikanJson.ReadObject(MikanJson.Field(json, "z_bounds"), this.z_bounds ?? new MikanVector2d());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "camera_forward": value = this.camera_forward; return true;
				case "camera_up": value = this.camera_up; return true;
				case "camera_position": value = this.camera_position; return true;
				case "pixel_size": value = this.pixel_size; return true;
				case "aux_pixel_size": value = this.aux_pixel_size; return true;
				case "focal_length": value = this.focal_length; return true;
				case "principal_point": value = this.principal_point; return true;
				case "z_bounds": value = this.z_bounds; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "camera_forward": return MikanJson.TryAssign(value, ref this.camera_forward);
				case "camera_up": return MikanJson.TryAssign(value, ref this.camera_up);
				case "camera_position": return MikanJson.TryAssign(value, ref this.camera_position);
				case "pixel_size": return MikanJson.TryAssign(value, ref this.pixel_size);
				case "aux_pixel_size": return MikanJson.TryAssign(value, ref this.aux_pixel_size);
				case "focal_length": return MikanJson.TryAssign(value, ref this.focal_length);
				case "principal_point": return MikanJson.TryAssign(value, ref this.principal_point);
				case "z_bounds": return MikanJson.TryAssign(value, ref this.z_bounds);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
