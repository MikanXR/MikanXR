// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class ArucoMarkerImageResponse : MikanResponse
	{
		public string imageData;

		public ArucoMarkerImageResponse()
		{
			responseTypeName = "ArucoMarkerImageResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("imageData", MikanJson.WriteString(this.imageData));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.imageData = MikanJson.ReadString(MikanJson.Field(json, "imageData"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "imageData": value = this.imageData; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "imageData": return MikanJson.TryAssign(value, ref this.imageData);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class GetArucoMarkerImageRequest : MikanRequest
	{
		public int markerId;
		public int imageSize;

		public GetArucoMarkerImageRequest()
		{
			requestTypeName = "GetArucoMarkerImageRequest";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("markerId", MikanJson.WriteInt(this.markerId));
			json.Add("imageSize", MikanJson.WriteInt(this.imageSize));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.markerId = MikanJson.ReadInt(MikanJson.Field(json, "markerId"));
			this.imageSize = MikanJson.ReadInt(MikanJson.Field(json, "imageSize"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "markerId": value = this.markerId; return true;
				case "imageSize": value = this.imageSize; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "markerId": return MikanJson.TryAssign(value, ref this.markerId);
				case "imageSize": return MikanJson.TryAssign(value, ref this.imageSize);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
