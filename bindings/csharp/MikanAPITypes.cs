namespace MikanXR
{
	// The bases of every request, response and event. The JSON-RPC envelope carries their
	// members rather than the struct's own JSON, so these are written here instead of generated:
	// none of their members is a wire field, and they read and write no fields of their own.
	//
	// The generated subclasses stamp their class name into the type name member on construction,
	// and that name is what the type registry creates each one by.
	public class MikanRequest : MikanSerializableObject
	{
		// The JSON-RPC method
		public string requestTypeName;
		// The JSON-RPC id, stamped by MikanRequestManager when the request is sent
		public int requestId;

		// The name of the response struct this request is answered with, which a result is read
		// into since it carries no type name. A name rather than a Type, since the runtime
		// compiles into hosts that forbid reading a Type's members.
		public virtual string ResponseTypeName => nameof(MikanResponse);

		public MikanRequest()
		{
			requestTypeName= nameof(MikanRequest);
		}
	}

	public class MikanResponse : MikanSerializableObject
	{
		// The struct the result was read into
		public string responseTypeName;
		// The id of the request this answers
		public int requestId;
		// Success for a result, and the error's data.resultCode for an error
		public MikanAPIResult resultCode;

		public MikanResponse()
		{
			responseTypeName= nameof(MikanResponse);
		}
	}

	public class MikanEvent : MikanSerializableObject
	{
		// The JSON-RPC method of the notification the event arrived in
		public string eventTypeName;

		public MikanEvent()
		{
			eventTypeName= nameof(MikanEvent);
		}
	}
}
