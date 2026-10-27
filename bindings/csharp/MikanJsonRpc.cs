using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;

namespace MikanXR
{
	// What a frame from the editor is. The editor sends no requests, so a frame naming a method
	// is an event.
	internal enum MikanJsonRpcFrameKind
	{
		Invalid,
		Event,
		Response
	}

	// The JSON-RPC 2.0 envelope every websocket text frame between the editor and a client
	// travels in. A request names a method, carries the request struct's fields as its params and
	// carries an integer id. A response carries the id it answers and exactly one of result or
	// error. An event is a notification from the editor: a method and params with no id.
	internal static class MikanJsonRpc
	{
		public const string Version= "2.0";

		public static string FormatRequest(string method, JObject parameters, int id)
		{
			var frame= new JObject();
			frame.Add("jsonrpc", Version);
			frame.Add("method", method);
			frame.Add("params", parameters);
			frame.Add("id", id);

			return frame.ToString(Formatting.None);
		}

		// The frame as an object, or null when it is not a JSON object. A batch (an array) is
		// not supported, so it is not an object either.
		public static JObject TryParse(string utf8Frame)
		{
			try
			{
				return JToken.Parse(utf8Frame) as JObject;
			}
			catch (Exception)
			{
				return null;
			}
		}

		public static MikanJsonRpcFrameKind Classify(JObject frame)
		{
			JToken version= frame?["jsonrpc"];
			if (version == null || version.Type != JTokenType.String || (string)version != Version)
			{
				return MikanJsonRpcFrameKind.Invalid;
			}

			bool hasId= frame["id"] != null;
			JToken method= frame["method"];
			if (method != null)
			{
				return method.Type == JTokenType.String && !hasId
					? MikanJsonRpcFrameKind.Event
					: MikanJsonRpcFrameKind.Invalid;
			}

			bool hasResult= frame["result"] != null;
			bool hasError= frame["error"] != null;

			return hasId && hasResult != hasError ? MikanJsonRpcFrameKind.Response : MikanJsonRpcFrameKind.Invalid;
		}
	}
}
