using Newtonsoft.Json.Linq;

namespace MikanXR
{
	public class MikanEventManager
	{
		private static readonly string WEBSOCKET_DISCONNECT_EVENT = "disconnect";

		private MikanLogCallback _logCallback;
		private IMikanTransport _transport;

		public MikanEventManager(MikanLogCallback logCallback)
		{
			_logCallback= logCallback;
		}

		public void Initialize(IMikanTransport transport)
		{
			_transport = transport;
		}

		public MikanAPIResult FetchNextEvent(out MikanEvent outEvent)
		{
			outEvent= null;

			var result = _transport.FetchNextEvent(out string utf8EventString);
			if (result == MikanAPIResult.Success)
			{
				outEvent = parseEventString(utf8EventString);
				if (outEvent == null)
				{
					_logCallback(MikanLogLevel.Error,
						"fetchNextEvent() - failed to parse event string: " + utf8EventString);
					result = MikanAPIResult.MalformedResponse;
				}
			}

			return result;
		}

		private MikanEvent parseEventString(string utf8ResponseString)
		{
			MikanEvent mikanEvent= null;

			if (utf8ResponseString.StartsWith(WEBSOCKET_DISCONNECT_EVENT))
			{
				int disconnectCode = 0;
				string disconnectReason = "";

				string[] tokens = utf8ResponseString.Split(new char[] {':'});
				if (tokens.Length >= 3)
				{
					int.TryParse(tokens[1], out disconnectCode);
					disconnectReason = tokens[2];
				}

				var disconnectEvent = new MikanDisconnectedEvent();
				disconnectEvent.eventTypeName = nameof(MikanDisconnectedEvent);
				disconnectEvent.code = (MikanDisconnectCode)disconnectCode;
				disconnectEvent.reason= disconnectReason;

				mikanEvent = disconnectEvent;
			}
			else
			{
				var root = JObject.Parse(utf8ResponseString);

				// Check if the "eventTypeName" key exists
				if (root.TryGetValue("eventTypeName", out JToken eventTypeNameElement))
				{
					// Check if the value of eventType keys
					if (eventTypeNameElement.Type == JTokenType.String)
					{
						// Get the string value of "eventTypeName"
						string eventTypeName = (string)eventTypeNameElement;

						// Attempt to create the event object by class name
						if (MikanTypeRegistry.Create(eventTypeName) is MikanEvent eventObject)
						{
							// Deserialize the event object from the already parsed JSON
							if (JsonDeserializer.deserializeFromJson(root, eventObject))
							{
								mikanEvent = eventObject;
							}
							else
							{
								_logCallback(
									MikanLogLevel.Error,
									"Failed to deserialize event object from JSON string: " + utf8ResponseString);
							}
						}
						else
						{
							_logCallback(MikanLogLevel.Error,
								"Unknown event type: " + eventTypeName +
								" (className: " + eventTypeNameElement + ")");
						}
					}
					else
					{
						_logCallback(MikanLogLevel.Error, "eventTypes not of expected types.");
					}
				}
				else
				{
					_logCallback(MikanLogLevel.Error, "eventType keys not found.");
				}
			}

			return mikanEvent;
		}
	}
}
