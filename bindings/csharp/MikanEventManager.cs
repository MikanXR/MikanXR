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
				mikanEvent = parseEventNotification(utf8ResponseString);
			}

			return mikanEvent;
		}

		// An event is a notification from the editor whose method is the event struct's name
		private MikanEvent parseEventNotification(string utf8EventString)
		{
			JObject frame= MikanJsonRpc.TryParse(utf8EventString);
			if (MikanJsonRpc.Classify(frame) != MikanJsonRpcFrameKind.Event)
			{
				_logCallback(MikanLogLevel.Error, "Not an event notification: " + utf8EventString);
				return null;
			}

			string eventTypeName= (string)frame["method"];
			if (!(MikanTypeRegistry.Create(eventTypeName) is MikanEvent eventObject))
			{
				_logCallback(MikanLogLevel.Warning, "Received an event of unknown type: " + eventTypeName);
				return null;
			}

			// A notification may leave out params, which reads as an event with no fields
			JObject parameters= frame["params"] as JObject ?? new JObject();
			if (!JsonDeserializer.deserializeFromJson(parameters, eventObject))
			{
				_logCallback(
					MikanLogLevel.Error,
					$"Failed to deserialize {eventTypeName} from JSON string: {utf8EventString}");
				return null;
			}

			eventObject.eventTypeName= eventTypeName;

			return eventObject;
		}
	}
}
