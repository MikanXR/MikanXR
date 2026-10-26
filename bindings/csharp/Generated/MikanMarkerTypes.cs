// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanMarkerDictionaryType
	{
		INVALID= -1,
		DICT_4x4= 0,
		DICT_5x5= 1,
		DICT_6x6= 2,
		DICT_7x7= 3,
	};

	internal static class MikanMarkerDictionaryTypeJson
	{
		public static JToken ToJson(MikanMarkerDictionaryType value)
		{
			switch (value)
			{
				case MikanMarkerDictionaryType.INVALID: return MikanJson.WriteString("INVALID");
				case MikanMarkerDictionaryType.DICT_4x4: return MikanJson.WriteString("DICT_4x4");
				case MikanMarkerDictionaryType.DICT_5x5: return MikanJson.WriteString("DICT_5x5");
				case MikanMarkerDictionaryType.DICT_6x6: return MikanJson.WriteString("DICT_6x6");
				case MikanMarkerDictionaryType.DICT_7x7: return MikanJson.WriteString("DICT_7x7");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanMarkerDictionaryType has no wire name for " + (long)value);
		}

		public static MikanMarkerDictionaryType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanMarkerDictionaryType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanMarkerDictionaryType.INVALID;
				case "DICT_4x4": return MikanMarkerDictionaryType.DICT_4x4;
				case "DICT_5x5": return MikanMarkerDictionaryType.DICT_5x5;
				case "DICT_6x6": return MikanMarkerDictionaryType.DICT_6x6;
				case "DICT_7x7": return MikanMarkerDictionaryType.DICT_7x7;
			}
			throw new Exception("MikanMarkerDictionaryType has no value named " + name);
		}
	};

	public class MikanMarkerComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "MarkerComponent";
		public const string k_ownerSystemName = "MarkerObjectSystem";

		public int aruco_id;
		public float length_mm;

		public override string RuntimeClassName => "MikanMarkerComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("aruco_id", MikanJson.WriteInt(this.aruco_id));
			json.Add("length_mm", MikanJson.WriteFloat(this.length_mm));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.aruco_id = MikanJson.ReadInt(MikanJson.Field(json, "aruco_id"));
			this.length_mm = MikanJson.ReadFloat(MikanJson.Field(json, "length_mm"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "aruco_id": value = this.aruco_id; return true;
				case "length_mm": value = this.length_mm; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "aruco_id": return MikanJson.TryAssign(value, ref this.aruco_id);
				case "length_mm": return MikanJson.TryAssign(value, ref this.length_mm);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanMarkerSystemValues : MikanSystemValues
	{
		public const string k_systemName = "MarkerObjectSystem";

		public List<int> aruco_id_list;
		public MikanMarkerDictionaryType aruco_dictionary_type;
		public int charuco_rows;
		public int charuco_cols;
		public float charuco_square_length_mm;
		public float charuco_marker_length_mm;
		public MikanMarkerDictionaryType charuco_dictionary_type;

		public override string RuntimeClassName => "MikanMarkerSystemValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("aruco_id_list", MikanJson.WriteList(this.aruco_id_list, e0 => MikanJson.WriteInt(e0)));
			json.Add("aruco_dictionary_type", MikanMarkerDictionaryTypeJson.ToJson(this.aruco_dictionary_type));
			json.Add("charuco_rows", MikanJson.WriteInt(this.charuco_rows));
			json.Add("charuco_cols", MikanJson.WriteInt(this.charuco_cols));
			json.Add("charuco_square_length_mm", MikanJson.WriteFloat(this.charuco_square_length_mm));
			json.Add("charuco_marker_length_mm", MikanJson.WriteFloat(this.charuco_marker_length_mm));
			json.Add("charuco_dictionary_type", MikanMarkerDictionaryTypeJson.ToJson(this.charuco_dictionary_type));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.aruco_id_list = MikanJson.ReadList(MikanJson.Field(json, "aruco_id_list"), this.aruco_id_list, e0 => MikanJson.ReadInt(e0));
			this.aruco_dictionary_type = MikanMarkerDictionaryTypeJson.FromJson(MikanJson.Field(json, "aruco_dictionary_type"));
			this.charuco_rows = MikanJson.ReadInt(MikanJson.Field(json, "charuco_rows"));
			this.charuco_cols = MikanJson.ReadInt(MikanJson.Field(json, "charuco_cols"));
			this.charuco_square_length_mm = MikanJson.ReadFloat(MikanJson.Field(json, "charuco_square_length_mm"));
			this.charuco_marker_length_mm = MikanJson.ReadFloat(MikanJson.Field(json, "charuco_marker_length_mm"));
			this.charuco_dictionary_type = MikanMarkerDictionaryTypeJson.FromJson(MikanJson.Field(json, "charuco_dictionary_type"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "aruco_id_list": value = this.aruco_id_list; return true;
				case "aruco_dictionary_type": value = this.aruco_dictionary_type; return true;
				case "charuco_rows": value = this.charuco_rows; return true;
				case "charuco_cols": value = this.charuco_cols; return true;
				case "charuco_square_length_mm": value = this.charuco_square_length_mm; return true;
				case "charuco_marker_length_mm": value = this.charuco_marker_length_mm; return true;
				case "charuco_dictionary_type": value = this.charuco_dictionary_type; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "aruco_id_list": return MikanJson.TryAssign(value, ref this.aruco_id_list);
				case "aruco_dictionary_type": return MikanJson.TryAssign(value, ref this.aruco_dictionary_type);
				case "charuco_rows": return MikanJson.TryAssign(value, ref this.charuco_rows);
				case "charuco_cols": return MikanJson.TryAssign(value, ref this.charuco_cols);
				case "charuco_square_length_mm": return MikanJson.TryAssign(value, ref this.charuco_square_length_mm);
				case "charuco_marker_length_mm": return MikanJson.TryAssign(value, ref this.charuco_marker_length_mm);
				case "charuco_dictionary_type": return MikanJson.TryAssign(value, ref this.charuco_dictionary_type);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
