#include "ServiceAdvertiserTests.h"
#include "ServiceAdvertiser.h"
#include "unit_test.h"

#include <cassert>
#include <string>

namespace
{
std::string findTxtValue(const ServiceAdvertiser::TxtRecord& record, const std::string& key, bool& outFound)
{
	for (const auto& entry : record)
	{
		if (entry.first == key)
		{
			outFound= true;
			return entry.second;
		}
	}

	outFound= false;
	return std::string();
}
} // namespace

bool service_advertiser_test_txt_version()
{
	UNIT_TEST_BEGIN("the TXT record carries its version")

	bool bFound= false;
	const std::string version= findTxtValue(ServiceAdvertiser::buildTxtRecord({27015}), "txtvers", bFound);
	success&= bFound;
	success&= (version == std::to_string(ServiceAdvertiser::k_txtVersion));

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool service_advertiser_test_txt_ports_sorted()
{
	UNIT_TEST_BEGIN("ARKit base ports are listed sorted and once each")

	bool bFound= false;
	const std::string ports=
		findTxtValue(ServiceAdvertiser::buildTxtRecord({27020, 27015, 27030, 27015}), "arkit", bFound);
	success&= bFound;
	// The phone takes the lowest, so the order is part of the contract
	success&= (ports == "27015,27020,27030");

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool service_advertiser_test_txt_no_ports()
{
	UNIT_TEST_BEGIN("the arkit key is present and empty with no ARKit source")

	bool bFound= false;
	const std::string ports= findTxtValue(ServiceAdvertiser::buildTxtRecord({}), "arkit", bFound);
	success&= bFound;
	success&= ports.empty();

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool service_advertiser_test_instance_label()
{
	UNIT_TEST_BEGIN("the instance label names the computer")

	success&= (ServiceAdvertiser::buildInstanceLabel("DESKTOP-1") == "MikanXR (DESKTOP-1)");

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool run_service_advertiser_tests()
{
	UNIT_TEST_MODULE_BEGIN("service_advertiser")
	UNIT_TEST_MODULE_CALL_TEST(service_advertiser_test_txt_version);
	UNIT_TEST_MODULE_CALL_TEST(service_advertiser_test_txt_ports_sorted);
	UNIT_TEST_MODULE_CALL_TEST(service_advertiser_test_txt_no_ports);
	UNIT_TEST_MODULE_CALL_TEST(service_advertiser_test_instance_label);
	UNIT_TEST_MODULE_END()
}
