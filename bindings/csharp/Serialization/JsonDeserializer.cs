using Newtonsoft.Json.Linq;
using System;

namespace MikanXR
{
	public static class JsonDeserializer
	{
		public static bool deserializeFromJsonString(string jsonString, MikanSerializableObject instance)
		{
			try
			{
				JObject jsonObject = JObject.Parse(jsonString);
				return deserializeFromJson(jsonObject, instance);
			}
			catch (Exception e)
			{
				Console.WriteLine("Failed to deserialize from JSON string: " + e.Message);
				return false;
			}
		}

		public static bool deserializeFromJson(JObject jsonObject, MikanSerializableObject instance)
		{
			try
			{
				instance.ReadFields(jsonObject);
				return true;
			}
			catch (Exception e)
			{
				Console.WriteLine("Failed to deserialize from JSON: " + e.Message);
				return false;
			}
		}
	}
}
