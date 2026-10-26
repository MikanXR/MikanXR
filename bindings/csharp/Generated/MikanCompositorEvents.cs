// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCompositorStartedEvent : MikanEvent
	{
		public int compositor_id;
		public int camera_id;

		public MikanCompositorStartedEvent()
		{
			eventTypeName = "MikanCompositorStartedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("compositor_id", MikanJson.WriteInt(this.compositor_id));
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.compositor_id = MikanJson.ReadInt(MikanJson.Field(json, "compositor_id"));
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "compositor_id": value = this.compositor_id; return true;
				case "camera_id": value = this.camera_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "compositor_id": return MikanJson.TryAssign(value, ref this.compositor_id);
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanCompositorStoppedEvent : MikanEvent
	{
		public int compositor_id;
		public int camera_id;

		public MikanCompositorStoppedEvent()
		{
			eventTypeName = "MikanCompositorStoppedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("compositor_id", MikanJson.WriteInt(this.compositor_id));
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.compositor_id = MikanJson.ReadInt(MikanJson.Field(json, "compositor_id"));
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "compositor_id": value = this.compositor_id; return true;
				case "camera_id": value = this.camera_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "compositor_id": return MikanJson.TryAssign(value, ref this.compositor_id);
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
