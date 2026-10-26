using System;

namespace MikanXR
{
	// The client's handle on the editor: initialize, connect, send requests, pump events. Which
	// wire it rides and who owns the shared texture senders are chosen at construction through
	// IMikanTransport and IMikanRenderTargetWriter. The parameterless constructor, in Native/, is
	// the native client over MikanClientCore.dll, which is what every consumer before the managed
	// path was built against. A host that forbids P/Invoke leaves Native/ out and supplies both.
	public partial class MikanAPI : IDisposable
	{
		private MikanLogCallback _logCallback;
		private MikanLogCallback _internalLogCallback;

		private IMikanTransport _transport;
		private MikanRequestManager _requestManager;
		private MikanEventManager _eventManager;
		private MikanRenderTargetAPI _renderTargetAPI;

		private string _connectHost= "";
		private int _httpPort= 0;

		// -- API Lifecycle ----

		public MikanAPI(IMikanTransport transport, IMikanRenderTargetWriter renderTargetWriter)
		{
			_internalLogCallback= new MikanLogCallback(InternalLogCallback);
			_transport= transport;
			_requestManager = new MikanRequestManager(_internalLogCallback);
			_eventManager = new MikanEventManager(_internalLogCallback);
			_renderTargetAPI = new MikanRenderTargetAPI(_requestManager, renderTargetWriter);
		}

		public void Dispose()
		{
			Shutdown();
			_transport.Dispose();
			_requestManager= null;
			_eventManager= null;
			_renderTargetAPI= null;
		}

		public IMikanTransport Transport => _transport;

		// The writer the render target requests go to. A managed client installs its own after
		// Initialize, once the client name its sender names are built from is known.
		public IMikanRenderTargetWriter RenderTargetWriter
		{
			get { return _renderTargetAPI.Writer; }
			set { _renderTargetAPI.Writer= value; }
		}

		public MikanAPIResult Initialize(string clientName, MikanLogLevel minLogLevel)
		{
			MikanAPIResult result = _transport.Initialize(clientName, minLogLevel, _internalLogCallback);
			if (result != MikanAPIResult.Success)
			{
				return result;
			}

			result= _requestManager.Initialize(_transport);
			if (result != MikanAPIResult.Success)
			{
				return result;
			}

			_eventManager.Initialize(_transport);

			return MikanAPIResult.Success;
		}

		public bool GetIsInitialized()
		{
			return _transport.GetIsInitialized();
		}

		public MikanAPIResult Shutdown()
		{
			return _transport.Shutdown();
		}

		// -- Client Info ----

		public string GetClientUniqueID()
		{
			return _transport.ClientName;
		}

		public MikanClientInfo AllocateClientInfo()
		{
			MikanClientInfo clientInfo = new MikanClientInfo();

			// Stamp the request with the client API version and client id
			clientInfo.clientId = GetClientUniqueID();

			return clientInfo;
		}

		// -- Graphics Device ----
		// Answered by a writer whose library wants the device handed over (the native one).
		// A managed writer has no such need, and these then report InvalidParam.

		public MikanAPIResult SetGraphicsDeviceInterface(
			MikanClientGraphicsApi api,
			IntPtr graphicsDeviceInterface)
		{
			if (_renderTargetAPI.Writer is IMikanGraphicsDeviceInterfaces deviceInterfaces)
			{
				return deviceInterfaces.SetGraphicsDeviceInterface(api, graphicsDeviceInterface);
			}

			return MikanAPIResult.InvalidParam;
		}

		public MikanAPIResult GetGraphicsDeviceInterface(
			MikanClientGraphicsApi api,
			out IntPtr outGraphicsDeviceInterface)
		{
			if (_renderTargetAPI.Writer is IMikanGraphicsDeviceInterfaces deviceInterfaces)
			{
				return deviceInterfaces.GetGraphicsDeviceInterface(api, out outGraphicsDeviceInterface);
			}

			outGraphicsDeviceInterface= IntPtr.Zero;
			return MikanAPIResult.InvalidParam;
		}

		public MikanAPIResult SetGraphicsCommandQueueInterface(
			MikanClientGraphicsApi api,
			IntPtr graphicsCommandQueueInterface)
		{
			if (_renderTargetAPI.Writer is IMikanGraphicsDeviceInterfaces deviceInterfaces)
			{
				return deviceInterfaces.SetGraphicsCommandQueueInterface(api, graphicsCommandQueueInterface);
			}

			return MikanAPIResult.InvalidParam;
		}

		public MikanAPIResult GetGraphicsCommandQueueInterface(
			MikanClientGraphicsApi api,
			out IntPtr outGraphicsCommandQueueInterface)
		{
			if (_renderTargetAPI.Writer is IMikanGraphicsDeviceInterfaces deviceInterfaces)
			{
				return deviceInterfaces.GetGraphicsCommandQueueInterface(api, out outGraphicsCommandQueueInterface);
			}

			outGraphicsCommandQueueInterface= IntPtr.Zero;
			return MikanAPIResult.InvalidParam;
		}

		public IntPtr GetCameraPackDepthTextureResourcePtr(int cameraId)
		{
			if (_renderTargetAPI.Writer is IMikanGraphicsDeviceInterfaces deviceInterfaces)
			{
				return deviceInterfaces.GetCameraPackDepthTextureResourcePtr(cameraId);
			}

			return IntPtr.Zero;
		}

		// -- Connection ----

		// The editor's HTTP base, such as "http://127.0.0.1:8090". A client joins it with a
		// component's geometry_url to fetch the model. Empty until the editor's connected event
		// reports its HTTP port, and again after a disconnect.
		public string HttpBaseUrl
		{
			get
			{
				if (_httpPort == 0)
				{
					return "";
				}

				return "http://" + NormalizeHost(_connectHost) + ":" + _httpPort;
			}
		}

		private static string NormalizeHost(string host)
		{
			if (string.IsNullOrEmpty(host))
			{
				return "127.0.0.1";
			}

			foreach (string scheme in new[] { "ws://", "wss://", "http://" })
			{
				if (host.StartsWith(scheme, StringComparison.OrdinalIgnoreCase))
				{
					host= host.Substring(scheme.Length);
					break;
				}
			}

			host= host.TrimEnd('/');

			// Drop a ":port" suffix, but leave a bare IPv6 literal (several colons) alone
			int colon= host.LastIndexOf(':');
			if (colon > 0 && host.IndexOf(':') == colon)
			{
				host= host.Substring(0, colon);
			}

			return host.Length > 0 ? host : "127.0.0.1";
		}

		public MikanAPIResult Connect(string host="", string port="")
		{
			_connectHost= host;
			_httpPort= 0;
			return _transport.Connect(host, port);
		}

		public bool GetIsConnected()
		{
			return _transport.GetIsConnected();
		}

		public MikanAPIResult Disconnect()
		{
			_httpPort= 0;
			return _transport.Disconnect();
		}

		// -- Messaging ----

		public MikanResponseFuture SendRequest(MikanRequest request)
		{
			MikanResponseFuture response = _renderTargetAPI.TryProcessRequest(request);
			if (!response.IsValid())
			{
				response= _requestManager.SendRequest(request);
			}

			return response;
		}

		public MikanAPIResult FetchNextEvent(out MikanEvent outEvent)
		{
			MikanAPIResult result= _eventManager.FetchNextEvent(out outEvent);

			if (outEvent is MikanConnectedEvent connectedEvent)
			{
				_httpPort= connectedEvent.httpPort;
			}
			else if (outEvent is MikanDisconnectedEvent)
			{
				_httpPort= 0;
			}

			return result;
		}

		// -- Logging ----

		public void SetLogCallback(MikanLogCallback logCallback)
		{
			_logCallback = logCallback;
		}

		private void InternalLogCallback(MikanLogLevel logLevel, string logMessage)
		{
			_logCallback?.Invoke(logLevel, logMessage);
		}
	}
}
