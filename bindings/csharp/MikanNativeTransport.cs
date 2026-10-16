using System;
using System.Runtime.InteropServices;
using System.Text;

namespace MikanXR
{
	// The transport over MikanClientCore.dll. The native client owns the websocket, the event
	// queue, and the shared texture writers, and this class is the P/Invoke edge of all of it.
	public class MikanNativeTransport : IMikanTransport
	{
		private IntPtr _mikanContext= IntPtr.Zero;
		private string _clientName= "";
		private MikanLogCallback _logCallback;

		// The native side keeps raw pointers to these delegates for the life of the context, so
		// they are held here to keep the garbage collector from moving or freeing them.
		private MikanCoreNative.NativeLogCallback _nativeLogCallback;
		private MikanCoreNative.NativeTextResponseCallback _nativeTextResponseCallback;
		private MikanCoreNative.NativeBinaryResponseCallback _nativeBinaryResponseCallback;

		private MikanTextResponseHandler _textResponseHandler;
		private MikanBinaryResponseHandler _binaryResponseHandler;

		// Grown on demand: an event larger than this is re-fetched into a buffer sized for it
		// rather than left wedged at the head of the native queue.
		private byte[] _eventBuffer= new byte[1024];

		public IntPtr Context => _mikanContext;
		public string ClientName => _clientName;

		public MikanNativeTransport()
		{
			_nativeLogCallback= new MikanCoreNative.NativeLogCallback(InternalLogCallback);
			_nativeTextResponseCallback= new MikanCoreNative.NativeTextResponseCallback(InternalTextResponseCallback);
			_nativeBinaryResponseCallback= new MikanCoreNative.NativeBinaryResponseCallback(InternalBinaryResponseCallback);
		}

		public void Dispose()
		{
			Shutdown();
		}

		public MikanAPIResult Initialize(string clientName, MikanLogLevel minLogLevel, MikanLogCallback logCallback)
		{
			_logCallback= logCallback;

			MikanAPIResult result=
				(MikanAPIResult)MikanCoreNative.Mikan_Initialize(
					clientName, minLogLevel, _nativeLogCallback, out _mikanContext);
			if (result != MikanAPIResult.Success)
			{
				return result;
			}

			_clientName= clientName;

			result= (MikanAPIResult)MikanCoreNative.Mikan_SetTextResponseCallback(
				_mikanContext, _nativeTextResponseCallback, IntPtr.Zero);
			if (result != MikanAPIResult.Success)
			{
				return result;
			}

			return (MikanAPIResult)MikanCoreNative.Mikan_SetBinaryResponseCallback(
				_mikanContext, _nativeBinaryResponseCallback, IntPtr.Zero);
		}

		public bool GetIsInitialized()
		{
			return _mikanContext != IntPtr.Zero && MikanCoreNative.Mikan_GetIsInitialized(_mikanContext);
		}

		public MikanAPIResult Shutdown()
		{
			MikanAPIResult result= MikanAPIResult.NotConnected;

			if (_mikanContext != IntPtr.Zero)
			{
				result= (MikanAPIResult)MikanCoreNative.Mikan_Shutdown(_mikanContext);
				_mikanContext= IntPtr.Zero;
			}

			return result;
		}

		public void SetResponseHandlers(MikanTextResponseHandler textHandler, MikanBinaryResponseHandler binaryHandler)
		{
			_textResponseHandler= textHandler;
			_binaryResponseHandler= binaryHandler;
		}

		public MikanAPIResult Connect(string host, string port)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_Connect(_mikanContext, host, port);
		}

		public MikanAPIResult Disconnect()
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_Disconnect(_mikanContext, 0, "");
		}

		public bool GetIsConnected()
		{
			return _mikanContext != IntPtr.Zero && MikanCoreNative.Mikan_GetIsConnected(_mikanContext);
		}

		public MikanAPIResult SendRequestJson(string utf8RequestJson)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_SendRequestJSON(_mikanContext, utf8RequestJson);
		}

		public MikanAPIResult FetchNextEvent(out string utf8EventString)
		{
			utf8EventString= null;

			var result= (MikanAPIResult)MikanCoreNative.Mikan_FetchNextEvent(
				_mikanContext, (UIntPtr)_eventBuffer.Length, _eventBuffer, out UIntPtr bytesWritten);

			if (result == MikanAPIResult.BufferTooSmall)
			{
				// A null buffer asks for the size of the event at the head of the queue
				result= (MikanAPIResult)MikanCoreNative.Mikan_FetchNextEvent(
					_mikanContext, UIntPtr.Zero, IntPtr.Zero, out UIntPtr bytesNeeded);
				if (result != MikanAPIResult.Success)
				{
					return result;
				}

				_eventBuffer= new byte[(int)bytesNeeded.ToUInt32()];
				result= (MikanAPIResult)MikanCoreNative.Mikan_FetchNextEvent(
					_mikanContext, (UIntPtr)_eventBuffer.Length, _eventBuffer, out bytesWritten);
			}

			if (result == MikanAPIResult.Success)
			{
				// bytesWritten counts the null terminator the native side appends
				int byteCount= Math.Max((int)bytesWritten.ToUInt32() - 1, 0);
				utf8EventString= Encoding.UTF8.GetString(_eventBuffer, 0, byteCount);
			}

			return result;
		}

		private void InternalLogCallback(int logLevel, string logMessage)
		{
			_logCallback?.Invoke((MikanLogLevel)logLevel, logMessage);
		}

		private void InternalTextResponseCallback(int requestId, string utf8ResponseString, IntPtr userData)
		{
			_textResponseHandler?.Invoke(utf8ResponseString);
		}

		private void InternalBinaryResponseCallback(IntPtr buffer, UIntPtr bufferSize, IntPtr userData)
		{
			int byteCount= (int)bufferSize.ToUInt32();
			byte[] managedBuffer= new byte[byteCount];
			Marshal.Copy(buffer, managedBuffer, 0, byteCount);

			_binaryResponseHandler?.Invoke(managedBuffer);
		}
	}
}
