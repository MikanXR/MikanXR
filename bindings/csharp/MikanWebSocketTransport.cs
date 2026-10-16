using System;
using System.Collections.Concurrent;
using System.IO;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace MikanXR
{
	// The transport that speaks the editor's websocket protocol from managed code alone, for a
	// host that cannot or would rather not load MikanClientCore.dll. It mirrors the native
	// client's rules: the Mikan-<version> sub-protocol on the handshake, text frames carrying
	// eventTypeName are events and those carrying responseTypeName are responses, binary frames
	// are always responses, and a close becomes the disconnect event string.
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
		private const string k_eventTypeKey= "\"eventTypeName\"";
		private const string k_responseTypeKey= "\"responseTypeName\"";
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
		private MikanBinaryResponseHandler _binaryResponseHandler;

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

		public void SetResponseHandlers(MikanTextResponseHandler textHandler, MikanBinaryResponseHandler binaryHandler)
		{
			_textResponseHandler= textHandler;
			_binaryResponseHandler= binaryHandler;
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
			var chunk= new byte[k_receiveChunkBytes];
			var message= new MemoryStream();

			try
			{
				while (!token.IsCancellationRequested && socket.State == WebSocketState.Open)
				{
					message.SetLength(0);

					WebSocketReceiveResult result;
					do
					{
						result= await socket.ReceiveAsync(new ArraySegment<byte>(chunk), token).ConfigureAwait(false);

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

						message.Write(chunk, 0, result.Count);
					} while (!result.EndOfMessage);

					if (result.MessageType == WebSocketMessageType.Text)
					{
						HandleTextMessage(Encoding.UTF8.GetString(message.GetBuffer(), 0, (int)message.Length));
					}
					else
					{
						// Binary frames are always responses, never events
						_binaryResponseHandler?.Invoke(message.ToArray());
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
			// The type name keys only ever appear at the top level of a message, since no wire
			// type carries a field by either name, so a substring test is the whole check
			if (message.Contains(k_eventTypeKey))
			{
				_eventQueue.Enqueue(message);
			}
			else if (message.Contains(k_responseTypeKey))
			{
				_textResponseHandler?.Invoke(message);
			}
			else
			{
				Log(MikanLogLevel.Error, $"MikanWebSocketTransport::ReceiveLoop - unrecognized message: {message}");
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
