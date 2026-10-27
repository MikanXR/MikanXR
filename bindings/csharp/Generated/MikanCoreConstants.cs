// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanClientGraphicsApi
	{
		UNKNOWN= -1,
		Direct3D9= 0,
		Direct3D11= 1,
		Direct3D12= 2,
		OpenGL= 3,
		Metal= 4,
		Vulkan= 5,
	};

	internal static class MikanClientGraphicsApiJson
	{
		public static JToken ToJson(MikanClientGraphicsApi value)
		{
			switch (value)
			{
				case MikanClientGraphicsApi.UNKNOWN: return MikanJson.WriteString("UNKNOWN");
				case MikanClientGraphicsApi.Direct3D9: return MikanJson.WriteString("Direct3D9");
				case MikanClientGraphicsApi.Direct3D11: return MikanJson.WriteString("Direct3D11");
				case MikanClientGraphicsApi.Direct3D12: return MikanJson.WriteString("Direct3D12");
				case MikanClientGraphicsApi.OpenGL: return MikanJson.WriteString("OpenGL");
				case MikanClientGraphicsApi.Metal: return MikanJson.WriteString("Metal");
				case MikanClientGraphicsApi.Vulkan: return MikanJson.WriteString("Vulkan");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanClientGraphicsApi has no wire name for " + (long)value);
		}

		public static MikanClientGraphicsApi FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanClientGraphicsApi)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "UNKNOWN": return MikanClientGraphicsApi.UNKNOWN;
				case "Direct3D9": return MikanClientGraphicsApi.Direct3D9;
				case "Direct3D11": return MikanClientGraphicsApi.Direct3D11;
				case "Direct3D12": return MikanClientGraphicsApi.Direct3D12;
				case "OpenGL": return MikanClientGraphicsApi.OpenGL;
				case "Metal": return MikanClientGraphicsApi.Metal;
				case "Vulkan": return MikanClientGraphicsApi.Vulkan;
			}
			throw new Exception("MikanClientGraphicsApi has no value named " + name);
		}
	};

	public enum MikanColorBufferType
	{
		NOCOLOR= 0,
		RGB24= 1,
		RGBA32= 2,
		BGRA32= 3,
		RGBA16F= 4,
	};

	internal static class MikanColorBufferTypeJson
	{
		public static JToken ToJson(MikanColorBufferType value)
		{
			switch (value)
			{
				case MikanColorBufferType.NOCOLOR: return MikanJson.WriteString("NOCOLOR");
				case MikanColorBufferType.RGB24: return MikanJson.WriteString("RGB24");
				case MikanColorBufferType.RGBA32: return MikanJson.WriteString("RGBA32");
				case MikanColorBufferType.BGRA32: return MikanJson.WriteString("BGRA32");
				case MikanColorBufferType.RGBA16F: return MikanJson.WriteString("RGBA16F");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanColorBufferType has no wire name for " + (long)value);
		}

		public static MikanColorBufferType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanColorBufferType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "NOCOLOR": return MikanColorBufferType.NOCOLOR;
				case "RGB24": return MikanColorBufferType.RGB24;
				case "RGBA32": return MikanColorBufferType.RGBA32;
				case "BGRA32": return MikanColorBufferType.BGRA32;
				case "RGBA16F": return MikanColorBufferType.RGBA16F;
			}
			throw new Exception("MikanColorBufferType has no value named " + name);
		}
	};

	public enum MikanConstants
	{
		InvalidMikanID= -1,
		ClientAPIVersion= 1,
	};

	internal static class MikanConstantsJson
	{
		public static JToken ToJson(MikanConstants value)
		{
			switch (value)
			{
				case MikanConstants.InvalidMikanID: return MikanJson.WriteString("InvalidMikanID");
				case MikanConstants.ClientAPIVersion: return MikanJson.WriteString("ClientAPIVersion");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanConstants has no wire name for " + (long)value);
		}

		public static MikanConstants FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanConstants)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "InvalidMikanID": return MikanConstants.InvalidMikanID;
				case "ClientAPIVersion": return MikanConstants.ClientAPIVersion;
			}
			throw new Exception("MikanConstants has no value named " + name);
		}
	};

	public enum MikanCoreResult
	{
		Success= 0,
		GeneralError= 1,
		Uninitialized= 2,
		NullParam= 3,
		InvalidParam= 4,
		RequestFailed= 5,
		NotConnected= 6,
		AlreadyConnected= 7,
		SocketError= 8,
		Timeout= 9,
		Canceled= 10,
		NoData= 11,
		BufferTooSmall= 12,
		UnknownClient= 13,
		UnknownFunction= 14,
		MalformedParameters= 15,
		MalformedResponse= 16,
	};

	internal static class MikanCoreResultJson
	{
		public static JToken ToJson(MikanCoreResult value)
		{
			switch (value)
			{
				case MikanCoreResult.Success: return MikanJson.WriteString("Success");
				case MikanCoreResult.GeneralError: return MikanJson.WriteString("GeneralError");
				case MikanCoreResult.Uninitialized: return MikanJson.WriteString("Uninitialized");
				case MikanCoreResult.NullParam: return MikanJson.WriteString("NullParam");
				case MikanCoreResult.InvalidParam: return MikanJson.WriteString("InvalidParam");
				case MikanCoreResult.RequestFailed: return MikanJson.WriteString("RequestFailed");
				case MikanCoreResult.NotConnected: return MikanJson.WriteString("NotConnected");
				case MikanCoreResult.AlreadyConnected: return MikanJson.WriteString("AlreadyConnected");
				case MikanCoreResult.SocketError: return MikanJson.WriteString("SocketError");
				case MikanCoreResult.Timeout: return MikanJson.WriteString("Timeout");
				case MikanCoreResult.Canceled: return MikanJson.WriteString("Canceled");
				case MikanCoreResult.NoData: return MikanJson.WriteString("NoData");
				case MikanCoreResult.BufferTooSmall: return MikanJson.WriteString("BufferTooSmall");
				case MikanCoreResult.UnknownClient: return MikanJson.WriteString("UnknownClient");
				case MikanCoreResult.UnknownFunction: return MikanJson.WriteString("UnknownFunction");
				case MikanCoreResult.MalformedParameters: return MikanJson.WriteString("MalformedParameters");
				case MikanCoreResult.MalformedResponse: return MikanJson.WriteString("MalformedResponse");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanCoreResult has no wire name for " + (long)value);
		}

		public static MikanCoreResult FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanCoreResult)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Success": return MikanCoreResult.Success;
				case "GeneralError": return MikanCoreResult.GeneralError;
				case "Uninitialized": return MikanCoreResult.Uninitialized;
				case "NullParam": return MikanCoreResult.NullParam;
				case "InvalidParam": return MikanCoreResult.InvalidParam;
				case "RequestFailed": return MikanCoreResult.RequestFailed;
				case "NotConnected": return MikanCoreResult.NotConnected;
				case "AlreadyConnected": return MikanCoreResult.AlreadyConnected;
				case "SocketError": return MikanCoreResult.SocketError;
				case "Timeout": return MikanCoreResult.Timeout;
				case "Canceled": return MikanCoreResult.Canceled;
				case "NoData": return MikanCoreResult.NoData;
				case "BufferTooSmall": return MikanCoreResult.BufferTooSmall;
				case "UnknownClient": return MikanCoreResult.UnknownClient;
				case "UnknownFunction": return MikanCoreResult.UnknownFunction;
				case "MalformedParameters": return MikanCoreResult.MalformedParameters;
				case "MalformedResponse": return MikanCoreResult.MalformedResponse;
			}
			throw new Exception("MikanCoreResult has no value named " + name);
		}
	};

	public enum MikanDepthBufferType
	{
		NODEPTH= 0,
		FLOAT_DEVICE_DEPTH= 1,
		FLOAT_SCENE_DEPTH= 2,
		PACK_DEPTH_RGBA= 3,
	};

	internal static class MikanDepthBufferTypeJson
	{
		public static JToken ToJson(MikanDepthBufferType value)
		{
			switch (value)
			{
				case MikanDepthBufferType.NODEPTH: return MikanJson.WriteString("NODEPTH");
				case MikanDepthBufferType.FLOAT_DEVICE_DEPTH: return MikanJson.WriteString("FLOAT_DEVICE_DEPTH");
				case MikanDepthBufferType.FLOAT_SCENE_DEPTH: return MikanJson.WriteString("FLOAT_SCENE_DEPTH");
				case MikanDepthBufferType.PACK_DEPTH_RGBA: return MikanJson.WriteString("PACK_DEPTH_RGBA");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanDepthBufferType has no wire name for " + (long)value);
		}

		public static MikanDepthBufferType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanDepthBufferType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "NODEPTH": return MikanDepthBufferType.NODEPTH;
				case "FLOAT_DEVICE_DEPTH": return MikanDepthBufferType.FLOAT_DEVICE_DEPTH;
				case "FLOAT_SCENE_DEPTH": return MikanDepthBufferType.FLOAT_SCENE_DEPTH;
				case "PACK_DEPTH_RGBA": return MikanDepthBufferType.PACK_DEPTH_RGBA;
			}
			throw new Exception("MikanDepthBufferType has no value named " + name);
		}
	};

	public enum MikanDisconnectCode
	{
		Normal= 1000,
		ProtocolError= 1002,
		NoStatus= 1005,
		AbnormalClose= 1006,
		InvalidFramePayload= 1007,
		InternalError= 1011,
		IncompatibleVersion= 2000,
	};

	internal static class MikanDisconnectCodeJson
	{
		public static JToken ToJson(MikanDisconnectCode value)
		{
			switch (value)
			{
				case MikanDisconnectCode.Normal: return MikanJson.WriteString("Normal");
				case MikanDisconnectCode.ProtocolError: return MikanJson.WriteString("ProtocolError");
				case MikanDisconnectCode.NoStatus: return MikanJson.WriteString("NoStatus");
				case MikanDisconnectCode.AbnormalClose: return MikanJson.WriteString("AbnormalClose");
				case MikanDisconnectCode.InvalidFramePayload: return MikanJson.WriteString("InvalidFramePayload");
				case MikanDisconnectCode.InternalError: return MikanJson.WriteString("InternalError");
				case MikanDisconnectCode.IncompatibleVersion: return MikanJson.WriteString("IncompatibleVersion");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanDisconnectCode has no wire name for " + (long)value);
		}

		public static MikanDisconnectCode FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanDisconnectCode)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Normal": return MikanDisconnectCode.Normal;
				case "ProtocolError": return MikanDisconnectCode.ProtocolError;
				case "NoStatus": return MikanDisconnectCode.NoStatus;
				case "AbnormalClose": return MikanDisconnectCode.AbnormalClose;
				case "InvalidFramePayload": return MikanDisconnectCode.InvalidFramePayload;
				case "InternalError": return MikanDisconnectCode.InternalError;
				case "IncompatibleVersion": return MikanDisconnectCode.IncompatibleVersion;
			}
			throw new Exception("MikanDisconnectCode has no value named " + name);
		}
	};

	public enum MikanLogLevel
	{
		Trace= 0,
		Debug= 1,
		Info= 2,
		Warning= 3,
		Error= 4,
		Fatal= 5,
	};

	internal static class MikanLogLevelJson
	{
		public static JToken ToJson(MikanLogLevel value)
		{
			switch (value)
			{
				case MikanLogLevel.Trace: return MikanJson.WriteString("Trace");
				case MikanLogLevel.Debug: return MikanJson.WriteString("Debug");
				case MikanLogLevel.Info: return MikanJson.WriteString("Info");
				case MikanLogLevel.Warning: return MikanJson.WriteString("Warning");
				case MikanLogLevel.Error: return MikanJson.WriteString("Error");
				case MikanLogLevel.Fatal: return MikanJson.WriteString("Fatal");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanLogLevel has no wire name for " + (long)value);
		}

		public static MikanLogLevel FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanLogLevel)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Trace": return MikanLogLevel.Trace;
				case "Debug": return MikanLogLevel.Debug;
				case "Info": return MikanLogLevel.Info;
				case "Warning": return MikanLogLevel.Warning;
				case "Error": return MikanLogLevel.Error;
				case "Fatal": return MikanLogLevel.Fatal;
			}
			throw new Exception("MikanLogLevel has no value named " + name);
		}
	};

	public enum MikanShadowBufferType
	{
		NOSHADOW= 0,
		RGB24= 1,
		RGBA32= 2,
		BGRA32= 3,
		RGBA16F= 4,
	};

	internal static class MikanShadowBufferTypeJson
	{
		public static JToken ToJson(MikanShadowBufferType value)
		{
			switch (value)
			{
				case MikanShadowBufferType.NOSHADOW: return MikanJson.WriteString("NOSHADOW");
				case MikanShadowBufferType.RGB24: return MikanJson.WriteString("RGB24");
				case MikanShadowBufferType.RGBA32: return MikanJson.WriteString("RGBA32");
				case MikanShadowBufferType.BGRA32: return MikanJson.WriteString("BGRA32");
				case MikanShadowBufferType.RGBA16F: return MikanJson.WriteString("RGBA16F");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanShadowBufferType has no wire name for " + (long)value);
		}

		public static MikanShadowBufferType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanShadowBufferType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "NOSHADOW": return MikanShadowBufferType.NOSHADOW;
				case "RGB24": return MikanShadowBufferType.RGB24;
				case "RGBA32": return MikanShadowBufferType.RGBA32;
				case "BGRA32": return MikanShadowBufferType.BGRA32;
				case "RGBA16F": return MikanShadowBufferType.RGBA16F;
			}
			throw new Exception("MikanShadowBufferType has no value named " + name);
		}
	};

}
