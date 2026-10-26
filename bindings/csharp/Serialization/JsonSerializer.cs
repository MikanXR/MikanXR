using Newtonsoft.Json.Linq;

namespace MikanXR
{
	public static class JsonSerializer
	{
		public static string serializeToJsonString(MikanSerializableObject instance)
		{
			return serializeToJson(instance).ToString();
		}

		public static JObject serializeToJson(MikanSerializableObject instance)
		{
			var json = new JObject();
			instance.WriteFields(json);

			return json;
		}
	}
}
