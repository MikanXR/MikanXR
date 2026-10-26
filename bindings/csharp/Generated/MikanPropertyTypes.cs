// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanPropertyDescriptor : MikanSerializableObject
	{
		public string ownerSystemClass;
		public string ownerComponentClass;
		public string fieldName;
		public MikanVariantType fieldType;
		public bool isReadOnly;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystemClass", MikanJson.WriteString(this.ownerSystemClass));
			json.Add("ownerComponentClass", MikanJson.WriteString(this.ownerComponentClass));
			json.Add("fieldName", MikanJson.WriteString(this.fieldName));
			json.Add("fieldType", MikanVariantTypeJson.ToJson(this.fieldType));
			json.Add("isReadOnly", MikanJson.WriteBool(this.isReadOnly));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystemClass = MikanJson.ReadString(MikanJson.Field(json, "ownerSystemClass"));
			this.ownerComponentClass = MikanJson.ReadString(MikanJson.Field(json, "ownerComponentClass"));
			this.fieldName = MikanJson.ReadString(MikanJson.Field(json, "fieldName"));
			this.fieldType = MikanVariantTypeJson.FromJson(MikanJson.Field(json, "fieldType"));
			this.isReadOnly = MikanJson.ReadBool(MikanJson.Field(json, "isReadOnly"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystemClass": value = this.ownerSystemClass; return true;
				case "ownerComponentClass": value = this.ownerComponentClass; return true;
				case "fieldName": value = this.fieldName; return true;
				case "fieldType": value = this.fieldType; return true;
				case "isReadOnly": value = this.isReadOnly; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystemClass": return MikanJson.TryAssign(value, ref this.ownerSystemClass);
				case "ownerComponentClass": return MikanJson.TryAssign(value, ref this.ownerComponentClass);
				case "fieldName": return MikanJson.TryAssign(value, ref this.fieldName);
				case "fieldType": return MikanJson.TryAssign(value, ref this.fieldType);
				case "isReadOnly": return MikanJson.TryAssign(value, ref this.isReadOnly);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanPropertyValue : MikanSerializableObject
	{
		public string ownerSystem;
		public string ownerComponentClass;
		public int componentId;
		public string fieldName;
		public MikanVariant fieldValue;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("ownerComponentClass", MikanJson.WriteString(this.ownerComponentClass));
			json.Add("componentId", MikanJson.WriteInt(this.componentId));
			json.Add("fieldName", MikanJson.WriteString(this.fieldName));
			json.Add("fieldValue", MikanJson.WriteObject(this.fieldValue));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.ownerComponentClass = MikanJson.ReadString(MikanJson.Field(json, "ownerComponentClass"));
			this.componentId = MikanJson.ReadInt(MikanJson.Field(json, "componentId"));
			this.fieldName = MikanJson.ReadString(MikanJson.Field(json, "fieldName"));
			this.fieldValue = MikanJson.ReadObject(MikanJson.Field(json, "fieldValue"), this.fieldValue ?? new MikanVariant());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "ownerComponentClass": value = this.ownerComponentClass; return true;
				case "componentId": value = this.componentId; return true;
				case "fieldName": value = this.fieldName; return true;
				case "fieldValue": value = this.fieldValue; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "ownerComponentClass": return MikanJson.TryAssign(value, ref this.ownerComponentClass);
				case "componentId": return MikanJson.TryAssign(value, ref this.componentId);
				case "fieldName": return MikanJson.TryAssign(value, ref this.fieldName);
				case "fieldValue": return MikanJson.TryAssign(value, ref this.fieldValue);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanSystemValues : PolymorphicStruct
	{

		public override string RuntimeClassName => "MikanSystemValues";
	};

}
