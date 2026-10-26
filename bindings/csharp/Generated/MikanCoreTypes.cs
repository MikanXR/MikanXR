// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanClientAPIVersion : MikanSerializableObject
	{
		public int version;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("version", MikanJson.WriteInt(this.version));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.version = MikanJson.ReadInt(MikanJson.Field(json, "version"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "version": value = this.version; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "version": return MikanJson.TryAssign(value, ref this.version);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRenderTargetDescriptor : MikanSerializableObject
	{
		public MikanColorBufferType color_buffer_type;
		public MikanDepthBufferType depth_buffer_type;
		public MikanShadowBufferType shadow_buffer_type;
		public uint width;
		public uint height;
		public uint aux_width;
		public uint aux_height;
		public MikanClientGraphicsApi graphicsAPI;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("color_buffer_type", MikanColorBufferTypeJson.ToJson(this.color_buffer_type));
			json.Add("depth_buffer_type", MikanDepthBufferTypeJson.ToJson(this.depth_buffer_type));
			json.Add("shadow_buffer_type", MikanShadowBufferTypeJson.ToJson(this.shadow_buffer_type));
			json.Add("width", MikanJson.WriteUInt(this.width));
			json.Add("height", MikanJson.WriteUInt(this.height));
			json.Add("aux_width", MikanJson.WriteUInt(this.aux_width));
			json.Add("aux_height", MikanJson.WriteUInt(this.aux_height));
			json.Add("graphicsAPI", MikanClientGraphicsApiJson.ToJson(this.graphicsAPI));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.color_buffer_type = MikanColorBufferTypeJson.FromJson(MikanJson.Field(json, "color_buffer_type"));
			this.depth_buffer_type = MikanDepthBufferTypeJson.FromJson(MikanJson.Field(json, "depth_buffer_type"));
			this.shadow_buffer_type = MikanShadowBufferTypeJson.FromJson(MikanJson.Field(json, "shadow_buffer_type"));
			this.width = MikanJson.ReadUInt(MikanJson.Field(json, "width"));
			this.height = MikanJson.ReadUInt(MikanJson.Field(json, "height"));
			this.aux_width = MikanJson.ReadUInt(MikanJson.Field(json, "aux_width"));
			this.aux_height = MikanJson.ReadUInt(MikanJson.Field(json, "aux_height"));
			this.graphicsAPI = MikanClientGraphicsApiJson.FromJson(MikanJson.Field(json, "graphicsAPI"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "color_buffer_type": value = this.color_buffer_type; return true;
				case "depth_buffer_type": value = this.depth_buffer_type; return true;
				case "shadow_buffer_type": value = this.shadow_buffer_type; return true;
				case "width": value = this.width; return true;
				case "height": value = this.height; return true;
				case "aux_width": value = this.aux_width; return true;
				case "aux_height": value = this.aux_height; return true;
				case "graphicsAPI": value = this.graphicsAPI; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "color_buffer_type": return MikanJson.TryAssign(value, ref this.color_buffer_type);
				case "depth_buffer_type": return MikanJson.TryAssign(value, ref this.depth_buffer_type);
				case "shadow_buffer_type": return MikanJson.TryAssign(value, ref this.shadow_buffer_type);
				case "width": return MikanJson.TryAssign(value, ref this.width);
				case "height": return MikanJson.TryAssign(value, ref this.height);
				case "aux_width": return MikanJson.TryAssign(value, ref this.aux_width);
				case "aux_height": return MikanJson.TryAssign(value, ref this.aux_height);
				case "graphicsAPI": return MikanJson.TryAssign(value, ref this.graphicsAPI);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
