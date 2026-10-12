// Calls into each DLL the client SDK ships an import library for, so linking this proves the
// SDK's lib/ is complete.
#include "MikanAPI.h"
#include "MikanCoreCAPI.h"
#include "SerializableString.h"

int main(int argc, char** argv)
{
	IMikanAPIPtr api= IMikanAPI::createMikanAPI();
	Serialization::String clientName(argc > 0 ? argv[0] : "ClientSdkConsumer");
	const bool coreInitialized= Mikan_GetIsInitialized(nullptr);

	return (api && !clientName.isEmpty() && !coreInitialized) ? 0 : 1;
}
