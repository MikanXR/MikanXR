using System;

namespace MikanXR
{
	public delegate void MikanLogCallback(MikanLogLevel log_level, string log_message);
	public delegate void MikanTextResponseHandler(string utf8ResponseString);

	// What MikanAPI needs from whatever carries its messages to the editor. The request and
	// event managers speak to this and nothing else, so a client picks its transport once at
	// construction: MikanNativeTransport drives MikanClientCore.dll through P/Invoke, and
	// MikanWebSocketTransport speaks the same websocket protocol from managed code alone.
	//
	// Response handlers are invoked on the transport's receive thread, never the caller's, in
	// both implementations. Events are queued and handed out by FetchNextEvent on the caller's
	// thread.
	public interface IMikanTransport : IDisposable
	{
		MikanAPIResult Initialize(string clientName, MikanLogLevel minLogLevel, MikanLogCallback logCallback);
		bool GetIsInitialized();
		MikanAPIResult Shutdown();

		// The name given to Initialize. It is the client id the editor knows this client by and
		// the prefix of its shared texture sender names.
		string ClientName { get; }

		void SetResponseHandler(MikanTextResponseHandler textHandler);

		// Begins connecting and returns at once. The editor's MikanConnectedEvent is the signal
		// that the connection is up. An empty host or port selects the default.
		MikanAPIResult Connect(string host, string port);
		MikanAPIResult Disconnect();
		bool GetIsConnected();

		MikanAPIResult SendRequestJson(string utf8RequestJson);

		// NoData when the queue is empty. A dropped connection surfaces here as the
		// "disconnect:<code>:<reason>" string MikanEventManager turns into MikanDisconnectedEvent.
		MikanAPIResult FetchNextEvent(out string utf8EventString);
	}
}
