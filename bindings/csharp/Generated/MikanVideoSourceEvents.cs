// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanVideoSourceAlignmentEvent : MikanEvent
	{
		public int video_source_id;
		public bool is_aligned;
		public bool marker_visible;
		public bool is_watching;

		public MikanVideoSourceAlignmentEvent()
		{
			eventTypeName = "MikanVideoSourceAlignmentEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
			json.Add("is_aligned", MikanJson.WriteBool(this.is_aligned));
			json.Add("marker_visible", MikanJson.WriteBool(this.marker_visible));
			json.Add("is_watching", MikanJson.WriteBool(this.is_watching));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
			this.is_aligned = MikanJson.ReadBool(MikanJson.Field(json, "is_aligned"));
			this.marker_visible = MikanJson.ReadBool(MikanJson.Field(json, "marker_visible"));
			this.is_watching = MikanJson.ReadBool(MikanJson.Field(json, "is_watching"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
				case "is_aligned": value = this.is_aligned; return true;
				case "marker_visible": value = this.marker_visible; return true;
				case "is_watching": value = this.is_watching; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
				case "is_aligned": return MikanJson.TryAssign(value, ref this.is_aligned);
				case "marker_visible": return MikanJson.TryAssign(value, ref this.marker_visible);
				case "is_watching": return MikanJson.TryAssign(value, ref this.is_watching);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVideoSourceClosedEvent : MikanEvent
	{

		public MikanVideoSourceClosedEvent()
		{
			eventTypeName = "MikanVideoSourceClosedEvent";
		}
	};

	public class MikanVideoSourceModeChangedEvent : MikanEvent
	{

		public MikanVideoSourceModeChangedEvent()
		{
			eventTypeName = "MikanVideoSourceModeChangedEvent";
		}
	};

	public class MikanVideoSourceOpenedEvent : MikanEvent
	{

		public MikanVideoSourceOpenedEvent()
		{
			eventTypeName = "MikanVideoSourceOpenedEvent";
		}
	};

}
