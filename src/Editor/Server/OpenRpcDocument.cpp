#include "OpenRpcDocument.h"
#include "InterprocessMessageServerInterface.h"
#include "Logger.h"
#include "MikanAPITypes.h"
#include "ReflectionHandles.h"
#include "SerializableObjectPtr.h"
#include "SerializableString.h"
#include "SerializationVisitor.h"
#include "TypeRegistry.h"
#include "Version.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

using json= nlohmann::json;

const char* OpenRpcDocument::k_discoverMethod= "rpc.discover";
const char* OpenRpcDocument::k_routePath= "/openrpc.json";

namespace
{
const char* k_polymorphicSchemaName= "PolymorphicObjectPtr";

json schemaRef(const std::string& schemaName) { return json{{"$ref", "#/components/schemas/" + schemaName}}; }

json typeSchema(const char* typeName) { return json{{"type", typeName}}; }

// The component schemas the document's methods and events reach. A reference queues the
// referenced type, and drain() builds the schema of everything queued until nothing new is
// reached, so each type is described once however many fields name it.
class SchemaCollector
{
public:
	json structRef(Serialization::StructTypeHandle structType)
	{
		const std::string name= Serialization::getStructTypeName(structType);
		if (m_queuedNames.insert(name).second)
		{
			m_pendingStructs.push_back(structType);
		}

		return schemaRef(name);
	}

	json enumRef(Serialization::EnumTypeHandle enumType)
	{
		const std::string name= Serialization::getEnumTypeName(enumType);
		if (m_queuedNames.insert(name).second)
		{
			m_schemas[name]= enumSchema(enumType);
		}

		return schemaRef(name);
	}

	json polymorphicRef()
	{
		if (m_queuedNames.insert(k_polymorphicSchemaName).second)
		{
			m_bPolymorphicReached= true;
		}

		return schemaRef(k_polymorphicSchemaName);
	}

	// The struct's fields, inherited ones included, in wire order, each with its schema
	std::vector<std::pair<std::string, json>> structFields(Serialization::StructTypeHandle structType);

	void drain()
	{
		// The polymorphic schema reaches every subclass of PolymorphicStruct, which in turn
		// can reach more structs, so it is built first and the struct queue drained after
		if (m_bPolymorphicReached)
		{
			m_schemas[k_polymorphicSchemaName]= polymorphicSchema();
		}

		while (!m_pendingStructs.empty())
		{
			Serialization::StructTypeHandle structType= m_pendingStructs.front();
			m_pendingStructs.pop_front();

			m_schemas[Serialization::getStructTypeName(structType)]= structSchema(structType);

			// A struct may name a polymorphic field the first time here
			if (m_bPolymorphicReached && !m_schemas.contains(k_polymorphicSchemaName))
			{
				m_schemas[k_polymorphicSchemaName]= polymorphicSchema();
			}
		}
	}

	const json& getSchemas() const { return m_schemas; }

private:
	json structSchema(Serialization::StructTypeHandle structType)
	{
		json properties= json::object();
		json required= json::array();
		for (const auto& field : structFields(structType))
		{
			properties[field.first]= field.second;
			required.push_back(field.first);
		}

		return json{{"type", "object"}, {"properties", properties}, {"required", required}};
	}

	// The serializer writes an enum as the name of its value
	static json enumSchema(Serialization::EnumTypeHandle enumType)
	{
		std::vector<std::string> valueNames;
		Serialization::forEachEnumValue(enumType,
										[&valueNames](const char* valueName, int64_t value)
										{
											if (std::find(valueNames.begin(), valueNames.end(), valueName)
												== valueNames.end())
											{
												valueNames.push_back(valueName);
											}
										});

		return json{{"type", "string"}, {"enum", valueNames}};
	}

	// The serializer writes {class_name, value}: the runtime class's name and its fields, or an
	// empty name and an empty object when nothing is held. Any reflected subclass of
	// PolymorphicStruct may be held.
	json polymorphicSchema()
	{
		std::vector<Serialization::StructTypeHandle> subclasses;
		Serialization::forEachSubclass(&Serialization::PolymorphicStruct::staticGetArchetype(),
									   [&subclasses](Serialization::StructTypeHandle subclass)
									   {
										   // Only the client API structs cross the wire
										   const char* name= Serialization::getStructTypeName(subclass);
										   if (Serialization::TypeRegistry::getStructByName(name) != nullptr)
										   {
											   subclasses.push_back(subclass);
										   }
									   });
		std::sort(subclasses.begin(), subclasses.end(),
				  [](Serialization::StructTypeHandle a, Serialization::StructTypeHandle b)
				  { return std::string(Serialization::getStructTypeName(a)) < Serialization::getStructTypeName(b); });

		json variants= json::array();
		for (Serialization::StructTypeHandle subclass : subclasses)
		{
			variants.push_back(json{{"type", "object"},
									{"properties",
									 {{"class_name", {{"const", Serialization::getStructTypeName(subclass)}}},
									  {"value", structRef(subclass)}}}});
		}
		variants.push_back(json{{"type", "object"},
								{"properties", {{"class_name", {{"const", ""}}}, {"value", typeSchema("object")}}}});

		return json{{"type", "object"},
					{"properties", {{"class_name", typeSchema("string")}, {"value", typeSchema("object")}}},
					{"required", {"class_name", "value"}},
					{"oneOf", variants}};
	}

	std::deque<Serialization::StructTypeHandle> m_pendingStructs;
	std::set<std::string> m_queuedNames;
	bool m_bPolymorphicReached= false;
	json m_schemas= json::object();
};

// Records the schema of each field a struct visit reaches. It reads types only, never values,
// so the visit needs no instance.
class FieldSchemaVisitor : public Serialization::IVisitor
{
public:
	FieldSchemaVisitor(SchemaCollector& collector)
		: m_collector(collector)
	{
	}

	std::vector<std::pair<std::string, json>> fields;

	void visitBool(Serialization::ValueAccessor const& a) override { record(a, typeSchema("boolean")); }
	void visitByte(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitUByte(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitShort(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitUShort(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitInt(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitUInt(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitLong(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitULong(Serialization::ValueAccessor const& a) override { record(a, typeSchema("integer")); }
	void visitFloat(Serialization::ValueAccessor const& a) override { record(a, typeSchema("number")); }
	void visitDouble(Serialization::ValueAccessor const& a) override { record(a, typeSchema("number")); }
	void visitEnum(Serialization::ValueAccessor const& a) override { record(a, m_collector.enumRef(a.getEnumType())); }
	void visitStruct(Serialization::ValueAccessor const& a) override
	{
		record(a, m_collector.structRef(a.getStructType()));
	}

	void visitClass(Serialization::ValueAccessor const& a) override
	{
		if (a.isType<Serialization::String>())
		{
			record(a, typeSchema("string"));
		}
		else if (a.isType<Serialization::PolymorphicObjectPtr>())
		{
			record(a, m_collector.polymorphicRef());
		}
		else if (a.isTemplateInstantiation() && a.getTemplateName() == "List" && a.getTemplateArgumentCount() == 1)
		{
			record(a, json{{"type", "array"}, {"items", templateArgumentSchema(a, 0)}});
		}
		// The serializer writes a map as an array of key value pairs
		else if (a.isTemplateInstantiation() && a.getTemplateName() == "Map" && a.getTemplateArgumentCount() == 2)
		{
			json pairSchema= {
				{"type", "object"},
				{"properties", {{"key", templateArgumentSchema(a, 0)}, {"value", templateArgumentSchema(a, 1)}}},
				{"required", {"key", "value"}}};

			record(a, json{{"type", "array"}, {"items", pairSchema}});
		}
		else if (a.isTemplateInstantiation())
		{
			setError("Field " + a.getName() + " is an unsupported template class " + a.getTemplateName());
		}
		else
		{
			record(a, m_collector.structRef(a.getClassType()));
		}
	}

private:
	void record(Serialization::ValueAccessor const& accessor, const json& schema)
	{
		fields.emplace_back(accessor.getName(), schema);
	}

	json templateArgumentSchema(Serialization::ValueAccessor const& a, std::size_t index)
	{
		if (a.isTemplateArgumentType<bool>(index))
			return typeSchema("boolean");
		if (a.isTemplateArgumentType<int8_t>(index) || a.isTemplateArgumentType<uint8_t>(index)
			|| a.isTemplateArgumentType<int16_t>(index) || a.isTemplateArgumentType<uint16_t>(index)
			|| a.isTemplateArgumentType<int32_t>(index) || a.isTemplateArgumentType<uint32_t>(index)
			|| a.isTemplateArgumentType<int64_t>(index) || a.isTemplateArgumentType<uint64_t>(index))
			return typeSchema("integer");
		if (a.isTemplateArgumentType<float>(index) || a.isTemplateArgumentType<double>(index))
			return typeSchema("number");
		if (a.isTemplateArgumentType<Serialization::String>(index))
			return typeSchema("string");
		if (a.isTemplateArgumentType<Serialization::PolymorphicObjectPtr>(index))
			return m_collector.polymorphicRef();

		Serialization::EnumTypeHandle enumType= a.getTemplateArgumentEnumType(index);
		if (enumType != nullptr)
			return m_collector.enumRef(enumType);

		Serialization::StructTypeHandle structType= a.getTemplateArgumentStructType(index);
		if (structType != nullptr)
			return m_collector.structRef(structType);

		setError("Field " + a.getName() + " has an unsupported template argument "
				 + a.getTemplateArgumentTypeName(index));
		return json::object();
	}

	SchemaCollector& m_collector;
};

std::vector<std::pair<std::string, json>> SchemaCollector::structFields(Serialization::StructTypeHandle structType)
{
	FieldSchemaVisitor visitor(*this);
	Serialization::visitStruct(static_cast<const void*>(nullptr), *structType, &visitor);
	if (visitor.hasError())
	{
		MIKAN_LOG_WARNING("OpenRpcDocument")
			<< Serialization::getStructTypeName(structType) << ": " << visitor.getError();
	}

	return visitor.fields;
}

// A struct's fields as the content descriptors of a by-name parameter list
json contentDescriptors(SchemaCollector& collector, Serialization::StructTypeHandle structType)
{
	json descriptors= json::array();
	for (const auto& field : collector.structFields(structType))
	{
		descriptors.push_back(json{{"name", field.first}, {"required", true}, {"schema", field.second}});
	}

	return descriptors;
}

Serialization::StructTypeHandle getResponseStruct(Serialization::StructTypeHandle requestStruct)
{
	const char* responseTypeName= Serialization::getResponseTypeName(requestStruct);
	if (responseTypeName == nullptr)
	{
		return &MikanResponse::staticGetArchetype();
	}

	Serialization::StructTypeHandle responseStruct= Serialization::TypeRegistry::getStructByName(responseTypeName);
	if (responseStruct == nullptr)
	{
		MIKAN_LOG_WARNING("OpenRpcDocument") << Serialization::getStructTypeName(requestStruct)
											 << " names an unknown response type " << responseTypeName;
		return &MikanResponse::staticGetArchetype();
	}

	return responseStruct;
}

// One error per MikanAPIResult code other than Success. Two results share a value, and a code
// can only be listed once, so the first name declared for a value is the one listed.
json buildErrors(std::vector<std::string>& outErrorNames)
{
	json errors= json::object();
	std::set<int64_t> listedValues;
	Serialization::forEachEnumValue(Serialization::TypeRegistry::getEnumByName("MikanAPIResult"),
									[&](const char* valueName, int64_t value)
									{
										if (value == (int64_t)MikanAPIResult::Success
											|| !listedValues.insert(value).second)
										{
											return;
										}

										errors[valueName]= json{{"code", jsonRpcErrorCode((MikanAPIResult)value)},
																{"message", valueName},
																{"data", {{"resultCode", valueName}}}};
										outErrorNames.push_back(valueName);
									});

	return errors;
}

std::vector<Serialization::StructTypeHandle> getEventStructs()
{
	std::vector<Serialization::StructTypeHandle> events;
	Serialization::forEachSubclass(&MikanEvent::staticGetArchetype(),
								   [&events](Serialization::StructTypeHandle subclass)
								   {
									   const char* name= Serialization::getStructTypeName(subclass);
									   if (Serialization::TypeRegistry::getStructByName(name) != nullptr)
									   {
										   events.push_back(subclass);
									   }
								   });
	std::sort(events.begin(), events.end(), [](Serialization::StructTypeHandle a, Serialization::StructTypeHandle b)
			  { return std::string(Serialization::getStructTypeName(a)) < Serialization::getStructTypeName(b); });

	return events;
}
} // namespace

json OpenRpcDocument::buildDocument(const std::vector<std::string>& methodNames)
{
	SchemaCollector collector;

	std::vector<std::string> errorNames;
	json errors= buildErrors(errorNames);

	json errorRefs= json::array();
	for (const std::string& errorName : errorNames)
	{
		errorRefs.push_back(json{{"$ref", "#/components/errors/" + errorName}});
	}

	std::vector<std::string> sortedNames= methodNames;
	std::sort(sortedNames.begin(), sortedNames.end());

	json methods= json::array();
	for (const std::string& methodName : sortedNames)
	{
		if (methodName == k_discoverMethod)
		{
			continue;
		}

		Serialization::StructTypeHandle requestStruct= Serialization::TypeRegistry::getStructByName(methodName);
		if (requestStruct == nullptr)
		{
			MIKAN_LOG_WARNING("OpenRpcDocument") << "Method " << methodName << " has no reflected request struct";
			continue;
		}

		Serialization::StructTypeHandle responseStruct= getResponseStruct(requestStruct);

		methods.push_back(json{{"name", methodName},
							   {"paramStructure", "by-name"},
							   {"params", contentDescriptors(collector, requestStruct)},
							   {"result",
								{{"name", Serialization::getStructTypeName(responseStruct)},
								 {"schema", collector.structRef(responseStruct)}}},
							   {"errors", errorRefs}});
	}

	json events= json::array();
	for (Serialization::StructTypeHandle eventStruct : getEventStructs())
	{
		events.push_back(json{{"name", Serialization::getStructTypeName(eventStruct)},
							  {"paramStructure", "by-name"},
							  {"params", contentDescriptors(collector, eventStruct)}});
	}

	collector.drain();

	json document;
	document["openrpc"]= "1.3.2";
	document["info"]= json{{"title", "MikanXR"}, {"version", MIKAN_RELEASE_VERSION_STRING}};
	document["servers"]=
		json::array({json{{"name", "editor"}, {"url", WEBSOCKET_SERVER_ADDRESS ":" WEBSOCKET_SERVER_PORT}}});
	document["methods"]= methods;
	document["components"]= json{{"schemas", collector.getSchemas()}, {"errors", errors}};
	document["x-mikan-events"]= events;

	return document;
}

bool OpenRpcDocument::registerRoute(HttpInterprocessMessageServer* server, const std::string& documentText)
{
	return server->setBackgroundRouteHandler(k_routePath, [documentText](const HttpRouteRequest& request)
											 { return buildResponse(request, documentText); });
}

HttpRouteResponse OpenRpcDocument::buildResponse(const HttpRouteRequest& request, const std::string& documentText)
{
	HttpRouteResponse response;
	if (request.method != "GET")
	{
		response.statusCode= 405;
		response.body= json{{"error", "Fetch with GET"}}.dump();
		return response;
	}

	response.body= documentText;
	return response;
}
