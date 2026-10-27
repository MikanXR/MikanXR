using System;
using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace MikanXR
{
	// The transport that speaks the editor's websocket protocol from managed code alone, for a
	// host that cannot or would rather not load MikanClientCore.dll. It mirrors the native
	// client's rules: the Mikan-<version> sub-protocol on the handshake, JSON-RPC text frames
	// where a notification (a method with no id) is an event and a frame with an id and a result
	// or an error is a response, binary frames are unexpected and dropped, and a close becomes the
	// disconnect event string.
	//
	// Connect returns at once with the handshake in flight, as the native transport does. A
	// connect that fails is logged and reported through GetIsConnected; the caller retries on
	// its own schedule, which is what the native client's consumers already do.
	public class MikanWebSocketTransport : IMikanTransport
	{
		public const string DefaultHost= "ws://127.0.0.1";
		public const string DefaultPort= "8080";

		private const string k_subProtocolPrefix= "Mikan-";
		private const string k_disconnectEventPrefix= "disconnect";
		private const int k_receiveChunkBytes= 64 * 1024;
		private const int k_closeTimeoutMilliseconds= 1000;

		private string _clientName= "";
		private MikanLogLevel _minLogLevel= MikanLogLevel.Info;
		private MikanLogCallback _logCallback;
		private bool _isInitialized= false;

		private ClientWebSocket _socket;
		private CancellationTokenSource _cancellation;
		private Task _connectTask;
		private Task _receiveTask;
		private int _disconnectEventQueued= 0;
		private readonly object _sendLock= new object();

		private readonly ConcurrentQueue<string> _eventQueue= new ConcurrentQueue<string>();
		private MikanTextResponseHandler _textResponseHandler;

		public string ClientName => _clientName;

		public void Dispose()
		{
			Shutdown();
		}

		public MikanAPIResult Initialize(string clientName, MikanLogLevel minLogLevel, MikanLogCallback logCallback)
		{
			if (string.IsNullOrEmpty(clientName))
			{
				return MikanAPIResult.InvalidParam;
			}

			_clientName= clientName;
			_minLogLevel= minLogLevel;
			_logCallback= logCallback;
			_isInitialized= true;

			return MikanAPIResult.Success;
		}

		public bool GetIsInitialized()
		{
			return _isInitialized;
		}

		public MikanAPIResult Shutdown()
		{
			if (!_isInitialized)
			{
				return MikanAPIResult.Uninitialized;
			}

			Disconnect();
			DisposeSocket();
			_isInitialized= false;

			return MikanAPIResult.Success;
		}

		public void SetResponseHandler(MikanTextResponseHandler textHandler)
		{
			_textResponseHandler= textHandler;
		}

		public MikanAPIResult Connect(string host, string port)
		{
			if (!_isInitialized)
			{
				return MikanAPIResult.Uninitialized;
			}

			if (GetIsConnected())
			{
				return MikanAPIResult.AlreadyConnected;
			}

			// An attempt already in flight is left to finish
			if (_connectTask != null && !_connectTask.IsCompleted)
			{
				return MikanAPIResult.Success;
			}

			string url= (string.IsNullOrEmpty(host) ? DefaultHost : host) + ":" +
						(string.IsNullOrEmpty(port) ? DefaultPort : port);
			if (!Uri.TryCreate(url, UriKind.Absolute, out Uri uri))
			{
				Log(MikanLogLevel.Error, $"MikanWebSocketTransport::Connect - invalid url: {url}");
				return MikanAPIResult.InvalidParam;
			}

			DisposeSocket();

			var socket= new ClientWebSocket();
			socket.Options.AddSubProtocol(k_subProtocolPrefix + (int)MikanConstants.ClientAPIVersion);

			_socket= socket;
			_cancellation= new CancellationTokenSource();
			_disconnectEventQueued= 0;

			CancellationToken token= _cancellation.Token;
			_connectTask= ConnectAsync(socket, uri, token);

			return MikanAPIResult.Success;
		}

		public MikanAPIResult Disconnect()
		{
			ClientWebSocket socket= _socket;
			if (socket == null)
			{
				return MikanAPIResult.NotConnected;
			}

			if (socket.State == WebSocketState.Open)
			{
				try
				{
					// The close handshake completes on the receive loop, which is what queues the
					// disconnect event, so this only has to start it and bound the wait
					Task closeTask= socket.CloseOutputAsync(
						WebSocketCloseStatus.NormalClosure, "", CancellationToken.None);
					closeTask.Wait(k_closeTimeoutMilliseconds);
					_receiveTask?.Wait(k_closeTimeoutMilliseconds);
				}
				catch (Exception e)
				{
					Log(MikanLogLevel.Debug, $"MikanWebSocketTransport::Disconnect - close failed: {e.Message}");
				}
			}

			QueueDisconnectEvent(MikanDisconnectCode.Normal, "");
			_cancellation?.Cancel();

			return MikanAPIResult.Success;
		}

		public bool GetIsConnected()
		{
			ClientWebSocket socket= _socket;

			return socket != null && socket.State == WebSocketState.Open;
		}

		public MikanAPIResult SendRequestJson(string utf8RequestJson)
		{
			ClientWebSocket socket= _socket;
			if (socket == null || socket.State != WebSocketState.Open)
			{
				return MikanAPIResult.NotConnected;
			}

			byte[] bytes= Encoding.UTF8.GetBytes(utf8RequestJson);

			try
			{
				// ClientWebSocket allows one send in flight. Requests are small control messages
				// over loopback, so waiting for each is cheaper than a send queue.
				lock (_sendLock)
				{
					socket.SendAsync(
						new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, CancellationToken.None)
						.GetAwaiter().GetResult();
				}
			}
			catch (Exception e)
			{
				Log(MikanLogLevel.Error, $"MikanWebSocketTransport::SendRequestJson - send failed: {e.Message}");
				return MikanAPIResult.SocketError;
			}

			return MikanAPIResult.Success;
		}

		public MikanAPIResult FetchNextEvent(out string utf8EventString)
		{
			if (_eventQueue.TryDequeue(out utf8EventString))
			{
				return MikanAPIResult.Success;
			}

			return MikanAPIResult.NoData;
		}

		private async Task ConnectAsync(ClientWebSocket socket, Uri uri, CancellationToken token)
		{
			try
			{
				await socket.ConnectAsync(uri, token).ConfigureAwait(false);
			}
			catch (Exception e)
			{
				// Expected while the editor is not running. The caller polls GetIsConnected and
				// retries, so this is noise above debug.
				Log(MikanLogLevel.Debug, $"MikanWebSocketTransport::Connect - {uri}: {e.Message}");
				return;
			}

			Log(MikanLogLevel.Info, $"MikanWebSocketTransport::Connect - connected to {uri}, protocol: {socket.SubProtocol}");

			_receiveTask= ReceiveLoopAsync(socket, token);
		}

		private async Task ReceiveLoopAsync(ClientWebSocket socket, CancellationToken token)
		{
			// A message is received straight into this buffer, which doubles whenever less than a
			// chunk of room is left, so it ends up sized to the largest message seen
			var message= new byte[k_receiveChunkBytes];

			try
			{
				while (!token.IsCancellationRequested && socket.State == WebSocketState.Open)
				{
					int messageLength= 0;

					WebSocketReceiveResult result;
					do
					{
						if (message.Length - messageLength < k_receiveChunkBytes)
						{
							Array.Resize(ref message, message.Length * 2);
						}

						var room= new ArraySegment<byte>(message, messageLength, message.Length - messageLength);
						result= await socket.ReceiveAsync(room, token).ConfigureAwait(false);

						if (result.MessageType == WebSocketMessageType.Close)
						{
							int code= result.CloseStatus.HasValue
								? (int)result.CloseStatus.Value
								: (int)MikanDisconnectCode.NoStatus;
							QueueDisconnectEvent((MikanDisconnectCode)code, result.CloseStatusDescription ?? "");

							// Answer the server's close so the handshake completes on both ends
							if (socket.State == WebSocketState.CloseReceived)
							{
								await socket.CloseOutputAsync(
									WebSocketCloseStatus.NormalClosure, "", CancellationToken.None).ConfigureAwait(false);
							}

							return;
						}

						messageLength+= result.Count;
					} while (!result.EndOfMessage);

					if (result.MessageType == WebSocketMessageType.Text)
					{
						HandleTextMessage(Encoding.UTF8.GetString(message, 0, messageLength));
					}
					else
					{
						// The protocol is text only: geometry is served over HTTP
						Log(MikanLogLevel.Warning, $"MikanWebSocketTransport::ReceiveLoop - dropped unexpected binary frame ({messageLength} bytes)");
					}
				}
			}
			catch (OperationCanceledException)
			{
			}
			catch (Exception e)
			{
				Log(MikanLogLevel.Error, $"MikanWebSocketTransport::ReceiveLoop - {e.Message}");
				QueueDisconnectEvent(MikanDisconnectCode.AbnormalClose, e.Message);
			}
		}

		private void HandleTextMessage(string message)
		{
			switch (MikanJsonRpc.Classify(MikanJsonRpc.TryParse(message)))
			{
				case MikanJsonRpcFrameKind.Event:
					_eventQueue.Enqueue(message);
					break;
				case MikanJsonRpcFrameKind.Response:
					_textResponseHandler?.Invoke(message);
					break;
				default:
					Log(MikanLogLevel.Error, $"MikanWebSocketTransport::ReceiveLoop - unrecognized message: {message}");
					break;
			}
		}

		// Queued once per connection, whichever of the close paths gets there first
		private void QueueDisconnectEvent(MikanDisconnectCode code, string reason)
		{
			if (Interlocked.Exchange(ref _disconnectEventQueued, 1) == 0)
			{
				_eventQueue.Enqueue($"{k_disconnectEventPrefix}:{(int)code}:{reason}");
			}
		}

		private void DisposeSocket()
		{
			_cancellation?.Cancel();
			_cancellation?.Dispose();
			_cancellation= null;

			_socket?.Dispose();
			_socket= null;
			_connectTask= null;
			_receiveTask= null;
		}

		private void Log(MikanLogLevel level, string message)
		{
			if (level >= _minLogLevel)
			{
				_logCallback?.Invoke(level, message);
			}
		}
	}
}
