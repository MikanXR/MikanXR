using System;

namespace MikanXR
{
	// The client's handle on the editor: initialize, connect, send requests, pump events. Which
	// wire it rides and who owns the shared texture senders are chosen at construction through
	// IMikanTransport and IMikanRenderTargetWriter. The parameterless constructor is the native
	// client over MikanClientCore.dll, which is what every consumer before the managed path
	// was built against.
	public class MikanAPI : IDisposable
	{
		private MikanLogCallback _logCallback;
		private MikanLogCallback _internalLogCallback;

		private IMikanTransport _transport;
		private MikanRequestManager _requestManager;
		private MikanEventManager _eventManager;
		private MikanRenderTargetAPI _renderTargetAPI;

		// -- API Lifecycle ----

		public MikanAPI()
			: this(new MikanNativeTransport(), null)
		{
			_renderTargetAPI.Writer= new MikanNativeRenderTargetWriter((MikanNativeTransport)_transport);
		}

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

		public MikanAPIResult Connect(string host="", string port="")
		{
			return _transport.Connect(host, port);
		}

		public bool GetIsConnected()
		{
			return _transport.GetIsConnected();
		}

		public MikanAPIResult Disconnect()
		{
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
			return _eventManager.FetchNextEvent(out outEvent);
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
