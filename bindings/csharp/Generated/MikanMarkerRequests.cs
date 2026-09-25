// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class ArucoMarkerImageResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public string imageData;

		public ArucoMarkerImageResponse()
		{
			responseTypeName = "ArucoMarkerImageResponse";
		}
	};

	public class GetArucoMarkerImageRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public int markerId;
		[MikanFieldOrder(1)] public int imageSize;

		public GetArucoMarkerImageRequest()
		{
			requestTypeName = "GetArucoMarkerImageRequest";
		}
	};

}
