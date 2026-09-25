// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetDMXData : MikanRequest
	{
		[MikanFieldOrder(0)] public List<int> dmx_universe_ids;

		public GetDMXData()
		{
			requestTypeName = "GetDMXData";
		}
	};

	public class MikanDMXDataResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanDMXData dmx_data;

		public MikanDMXDataResponse()
		{
			responseTypeName = "MikanDMXDataResponse";
		}
	};

	public class SetLightDMXDataSubcription : MikanRequest
	{
		[MikanFieldOrder(0)] public List<int> light_ids;
		[MikanFieldOrder(1)] public bool subscribe;

		public SetLightDMXDataSubcription()
		{
			requestTypeName = "SetLightDMXDataSubcription";
		}
	};

}
