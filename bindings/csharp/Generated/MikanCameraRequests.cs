// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class AllocateCameraRenderTargetTextures : MikanRequest
	{
		public int camera_id;
		public MikanRenderTargetDescriptor descriptor;

		public AllocateCameraRenderTargetTextures()
		{
			requestTypeName = "AllocateCameraRenderTargetTextures";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("descriptor", MikanJson.WriteObject(this.descriptor));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.descriptor = MikanJson.ReadObject(MikanJson.Field(json, "descriptor"), this.descriptor ?? new MikanRenderTargetDescriptor());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "descriptor": value = this.descriptor; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "descriptor": return MikanJson.TryAssign(value, ref this.descriptor);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class FreeCameraRenderTargetTextures : MikanRequest
	{
		public int camera_id;

		public FreeCameraRenderTargetTextures()
		{
			requestTypeName = "FreeCameraRenderTargetTextures";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class GetCameraProperties : MikanRequest
	{
		public int camera_id;

		public GetCameraProperties()
		{
			requestTypeName = "GetCameraProperties";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanCameraPropertiesResponse : MikanResponse
	{
		public int camera_id;
		public bool compositor_running;
		public MikanVector3f camera_forward;
		public MikanVector3f camera_up;
		public MikanVector3f camera_position;
		public MikanVector2i pixel_size;
		public MikanVector2i aux_pixel_size;
		public MikanVector2d focal_length;
		public MikanVector2d principal_point;
		public MikanVector2d z_bounds;

		public MikanCameraPropertiesResponse()
		{
			responseTypeName = "MikanCameraPropertiesResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("compositor_running", MikanJson.WriteBool(this.compositor_running));
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
			this.compositor_running = MikanJson.ReadBool(MikanJson.Field(json, "compositor_running"));
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
				case "compositor_running": value = this.compositor_running; return true;
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
				case "compositor_running": return MikanJson.TryAssign(value, ref this.compositor_running);
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

	public class PublishCameraRenderTargetTextures : MikanRequest
	{
		public int camera_id;
		public long frame_index;

		public PublishCameraRenderTargetTextures()
		{
			requestTypeName = "PublishCameraRenderTargetTextures";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("frame_index", MikanJson.WriteLong(this.frame_index));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.frame_index = MikanJson.ReadLong(MikanJson.Field(json, "frame_index"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "frame_index": value = this.frame_index; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "frame_index": return MikanJson.TryAssign(value, ref this.frame_index);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class WriteCameraColorRenderTargetTexture : MikanRequest
	{
		public int camera_id;
		public IntPtr api_color_texture_ptr;

		public WriteCameraColorRenderTargetTexture()
		{
			requestTypeName = "WriteCameraColorRenderTargetTexture";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("api_color_texture_ptr", MikanJson.WritePointer(this.api_color_texture_ptr));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.api_color_texture_ptr = MikanJson.ReadPointer(MikanJson.Field(json, "api_color_texture_ptr"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "api_color_texture_ptr": value = this.api_color_texture_ptr; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "api_color_texture_ptr": return MikanJson.TryAssign(value, ref this.api_color_texture_ptr);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class WriteCameraDepthRenderTargetTexture : MikanRequest
	{
		public int camera_id;
		public IntPtr api_depth_texture_ptr;
		public float z_near;
		public float z_far;

		public WriteCameraDepthRenderTargetTexture()
		{
			requestTypeName = "WriteCameraDepthRenderTargetTexture";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("api_depth_texture_ptr", MikanJson.WritePointer(this.api_depth_texture_ptr));
			json.Add("z_near", MikanJson.WriteFloat(this.z_near));
			json.Add("z_far", MikanJson.WriteFloat(this.z_far));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.api_depth_texture_ptr = MikanJson.ReadPointer(MikanJson.Field(json, "api_depth_texture_ptr"));
			this.z_near = MikanJson.ReadFloat(MikanJson.Field(json, "z_near"));
			this.z_far = MikanJson.ReadFloat(MikanJson.Field(json, "z_far"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "api_depth_texture_ptr": value = this.api_depth_texture_ptr; return true;
				case "z_near": value = this.z_near; return true;
				case "z_far": value = this.z_far; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "api_depth_texture_ptr": return MikanJson.TryAssign(value, ref this.api_depth_texture_ptr);
				case "z_near": return MikanJson.TryAssign(value, ref this.z_near);
				case "z_far": return MikanJson.TryAssign(value, ref this.z_far);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class WriteCameraShadowRenderTargetTexture : MikanRequest
	{
		public int camera_id;
		public IntPtr api_shadow_texture_ptr;

		public WriteCameraShadowRenderTargetTexture()
		{
			requestTypeName = "WriteCameraShadowRenderTargetTexture";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("api_shadow_texture_ptr", MikanJson.WritePointer(this.api_shadow_texture_ptr));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.api_shadow_texture_ptr = MikanJson.ReadPointer(MikanJson.Field(json, "api_shadow_texture_ptr"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "camera_id": value = this.camera_id; return true;
				case "api_shadow_texture_ptr": value = this.api_shadow_texture_ptr; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "api_shadow_texture_ptr": return MikanJson.TryAssign(value, ref this.api_shadow_texture_ptr);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
