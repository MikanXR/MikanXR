#include "ScriptHttpRouteTests.h"
#include "unit_test.h"

#include "ScriptHttpRouteTable.h"

#include <assert.h>
#include <stdio.h>
#include <string>

namespace
{
ScriptHttpRouteTable makeRouteTestTable()
{
	ScriptHttpRouteTable table;
	table.addRoute(ScriptHttpRoute{"gift_sub", 101, "GiftSub"});
	table.addRoute(ScriptHttpRoute{"compositor/desk_camera", 202, "DeskCamera"});
	return table;
}

bool tablesEqual(const ScriptHttpRouteTable& a, const ScriptHttpRouteTable& b)
{
	if (a.getRouteCount() != b.getRouteCount())
		return false;

	for (size_t i= 0; i < a.getRouteCount(); ++i)
	{
		if (!(a.getRoutes()[i] == b.getRoutes()[i]))
			return false;
	}

	return true;
}
} // namespace

bool run_script_http_route_tests()
{
	UNIT_TEST_MODULE_BEGIN("script_http_route")
	UNIT_TEST_MODULE_CALL_TEST(script_http_route_test_valid_route_names);
	UNIT_TEST_MODULE_CALL_TEST(script_http_route_test_add_set_remove);
	UNIT_TEST_MODULE_CALL_TEST(script_http_route_test_json_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(script_http_route_test_json_string_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(script_http_route_test_malformed_entries);
	UNIT_TEST_MODULE_END()
}

bool script_http_route_test_valid_route_names()
{
	UNIT_TEST_BEGIN("isValidRoute accepts a plain segmented path and rejects malformed ones")

	success= ScriptHttpRouteTable::isValidRoute("gift_sub");
	assert(success);
	success&= ScriptHttpRouteTable::isValidRoute("compositor/desk_camera");
	assert(success);

	success&= !ScriptHttpRouteTable::isValidRoute("");
	assert(success);
	success&= !ScriptHttpRouteTable::isValidRoute("/x");
	assert(success);
	success&= !ScriptHttpRouteTable::isValidRoute("x/");
	assert(success);
	success&= !ScriptHttpRouteTable::isValidRoute("a b");
	assert(success);
	success&= !ScriptHttpRouteTable::isValidRoute("a//b");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_http_route_test_add_set_remove()
{
	UNIT_TEST_BEGIN("add/set/remove keep the table free of duplicate and out-of-range routes")

	ScriptHttpRouteTable table;
	success= table.addRoute(ScriptHttpRoute{"a", 1, "A"});
	assert(success);
	success&= table.addRoute(ScriptHttpRoute{"b", 2, "B"});
	assert(success);
	success&= table.addRoute(ScriptHttpRoute{"c", 3, "C"});
	assert(success);
	success&= (table.getRouteCount() == 3);
	assert(success);

	// A duplicate route name is refused
	success&= !table.addRoute(ScriptHttpRoute{"a", 4, "A2"});
	assert(success);
	success&= (table.getRouteCount() == 3);
	assert(success);

	// Renaming index 0 to an existing route collides
	success&= !table.setRoute(0, ScriptHttpRoute{"b", 1, "A"});
	assert(success);

	// Renaming index 0 to a fresh route succeeds
	success&= table.setRoute(0, ScriptHttpRoute{"a2", 1, "A"});
	assert(success);
	success&= (table.findRouteIndex("a2") == 0);
	assert(success);
	success&= (table.findRouteIndex("a") == -1);
	assert(success);

	// Remove the middle entry
	success&= table.removeRoute(1);
	assert(success);
	success&= (table.getRouteCount() == 2);
	assert(success);
	success&= (table.findRouteIndex("b") == -1);
	assert(success);
	success&= (table.findRouteIndex("c") == 1);
	assert(success);

	// Out-of-range operations are refused
	success&= !table.removeRoute(5);
	assert(success);
	success&= !table.setRoute(5, ScriptHttpRoute{"d", 5, "D"});
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_http_route_test_json_round_trip()
{
	UNIT_TEST_BEGIN("table round-trips through writeToJSON / readFromJSON")

	const ScriptHttpRouteTable source= makeRouteTestTable();
	success= (source.getRouteCount() == 2);
	assert(success);

	ScriptHttpRouteTable restored;
	restored.readFromJSON(source.writeToJSON());
	success&= tablesEqual(source, restored);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_http_route_test_json_string_round_trip()
{
	UNIT_TEST_BEGIN("single-line JSON text round-trips")

	const ScriptHttpRouteTable source= makeRouteTestTable();
	const std::string text= source.toJsonString();

	// One line, since the transaction log and the automation protocol carry it as one value
	success= (text.find('\n') == std::string::npos);
	assert(success);

	ScriptHttpRouteTable restored;
	success&= restored.fromJsonString(text);
	assert(success);
	success&= tablesEqual(source, restored);
	assert(success);

	// An empty table is still a valid array
	ScriptHttpRouteTable empty;
	success&= (empty.toJsonString() == "[]");
	assert(success);
	success&= restored.fromJsonString("[]") && restored.empty();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_http_route_test_malformed_entries()
{
	UNIT_TEST_BEGIN("malformed text is rejected and bad entries are skipped")

	ScriptHttpRouteTable table= makeRouteTestTable();

	// A parse failure is refused and the table stays untouched
	success= !table.fromJsonString("not json");
	assert(success);
	success&= tablesEqual(table, makeRouteTestTable());
	assert(success);

	// A non-object element, an entry missing route, an entry with an invalid
	// route, and a duplicate route are skipped, the good entry survives
	const std::string mixed= "[{\"route\": \"good\", \"script_component_id\": 7, \"function\": \"Good\"},"
							 " 12,"
							 " {\"script_component_id\": 8, \"function\": \"Missing\"},"
							 " {\"route\": \"/bad\", \"script_component_id\": 9, \"function\": \"Bad\"},"
							 " {\"route\": \"good\", \"script_component_id\": 10, \"function\": \"Dup\"}]";
	success&= table.fromJsonString(mixed);
	assert(success);
	success&= (table.getRouteCount() == 1);
	assert(success);
	success&= (table.getRoutes()[0] == ScriptHttpRoute{"good", 7, "Good"});
	assert(success);

	UNIT_TEST_COMPLETE()
}
