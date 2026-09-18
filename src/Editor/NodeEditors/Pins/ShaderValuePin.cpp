#include "ShaderValuePin.h"
#include "MkGuiDrawUtils.h"
#include "NodeEditorState.h"

#include "imgui.h"

// -- ShaderValuePinConfig -----
configuru::Config ShaderValuePinConfig::writeToJSON()
{
	configuru::Config pt= NodePinConfig::writeToJSON();

	pt["value_type"]= valueType;
	pt["default_value"]= configuru::Config::array({defaultValue[0], defaultValue[1], defaultValue[2], defaultValue[3]});

	return pt;
}

void ShaderValuePinConfig::readFromJSON(const configuru::Config& pt)
{
	NodePinConfig::readFromJSON(pt);

	valueType= pt.get_or<std::string>("value_type", "wildcard");
	if (pt.has_key("default_value") && pt["default_value"].is_array())
	{
		const configuru::Config& valueArray= pt["default_value"];
		for (size_t index= 0; index < valueArray.array_size() && index < defaultValue.size(); ++index)
		{
			defaultValue[index]= (float)valueArray[index].as_double();
		}
	}
}

// -- ShaderValuePin -----
ShaderValuePin::ShaderValuePin()
	: NodePin()
{
	// Every unconnected input compiles to its default literal, so no input is required
	setHasDefaultValue(true);
}

bool ShaderValuePin::loadFromConfig(NodeGraphPtr ownerGraph, NodePinConfigConstPtr config)
{
	if (!NodePin::loadFromConfig(ownerGraph, config))
		return false;

	auto pinConfig= std::static_pointer_cast<const ShaderValuePinConfig>(config);
	m_declaredType= ShaderValueTypeUtils::fromString(pinConfig->valueType);
	if (m_declaredType == eShaderValueType::INVALID)
	{
		m_declaredType= eShaderValueType::wildcard;
	}
	m_defaultValue= pinConfig->defaultValue;

	return true;
}

void ShaderValuePin::saveToConfig(NodePinConfigPtr config) const
{
	auto pinConfig= std::static_pointer_cast<ShaderValuePinConfig>(config);

	pinConfig->valueType= ShaderValueTypeUtils::toString(m_declaredType);
	pinConfig->defaultValue= m_defaultValue;

	NodePin::saveToConfig(config);
}

eShaderValueType ShaderValuePin::getResolvedType() const
{
	return (m_declaredType != eShaderValueType::wildcard) ? m_declaredType : m_resolvedType;
}

bool ShaderValuePin::canPinsBeConnected(NodePinPtr otherPinPtr) const
{
	// The base rule demands equal data sizes, which a broadcast link breaks by design
	if (!otherPinPtr || otherPinPtr.get() == this)
		return false;

	if (otherPinPtr->getClassName() != k_pinClassName)
		return false;

	if (getDirection() == otherPinPtr->getDirection())
		return false;

	if (!isOnSamePage(otherPinPtr))
		return false;

	auto otherPin= std::static_pointer_cast<ShaderValuePin>(otherPinPtr);
	const ShaderValuePin* sourcePin= (getDirection() == eNodePinDirection::OUTPUT) ? this : otherPin.get();
	const ShaderValuePin* targetPin= (getDirection() == eNodePinDirection::INPUT) ? this : otherPin.get();

	return ShaderValueTypeUtils::canConvert(sourcePin->getDeclaredType(), targetPin->getDeclaredType());
}

float ShaderValuePin::editorComputeInputWidth() const
{
	if (m_connectedLinks.empty())
	{
		const int componentCount= ShaderValueTypeUtils::getComponentCount(m_declaredType);
		if (componentCount > 0)
		{
			return ImGui::CalcTextSize(m_name.c_str()).x + (11.f + 50.f * (float)componentCount) * MkGui::getUiScale();
		}
		if (m_declaredType == eShaderValueType::wildcard)
		{
			return ImGui::CalcTextSize(m_name.c_str()).x + (11.f + 50.f) * MkGui::getUiScale();
		}
	}

	return NodePin::editorComputeInputWidth();
}

void ShaderValuePin::editorRenderInputTextEntry(const NodeEditorState& editorState)
{
	if (!m_connectedLinks.empty())
		return;

	// A wildcard input edits one scalar, which broadcasts to whatever the other operands resolve to
	int componentCount= ShaderValueTypeUtils::getComponentCount(m_declaredType);
	if (m_declaredType == eShaderValueType::wildcard)
	{
		componentCount= 1;
	}
	if (componentCount <= 0)
		return;

	ImGui::SameLine();
	ImGui::SetNextItemWidth(50.f * (float)componentCount);
	ImGui::InputScalarN("", ImGuiDataType_Float, m_defaultValue.data(), componentCount);
}

MkCanvas::PinIcon ShaderValuePin::editorGetPinIcon() const
{
	return (getResolvedType() == eShaderValueType::texture2D) ? MkCanvas::PinIcon::Square : MkCanvas::PinIcon::Circle;
}

ImVec4 ShaderValuePin::getColorForType(eShaderValueType type)
{
	switch (type)
	{
	case eShaderValueType::float1:
		return ImVec4(156.f / 255.f, 253.f / 255.f, 65.f / 255.f, 1.f);
	case eShaderValueType::float2:
		return ImVec4(120.f / 255.f, 220.f / 255.f, 220.f / 255.f, 1.f);
	case eShaderValueType::float3:
		return ImVec4(252.f / 255.f, 200.f / 255.f, 35.f / 255.f, 1.f);
	case eShaderValueType::float4:
		return ImVec4(230.f / 255.f, 120.f / 255.f, 230.f / 255.f, 1.f);
	case eShaderValueType::texture2D:
		return ImVec4(160.f / 255.f, 160.f / 255.f, 255.f / 255.f, 1.f);
	default:
		return ImVec4(200.f / 255.f, 200.f / 255.f, 200.f / 255.f, 1.f);
	}
}

ImVec4 ShaderValuePin::editorGetPinColor() const { return getColorForType(getResolvedType()); }

ImU32 ShaderValuePin::editorGetLinkStyleColor() const
{
	const ImVec4 color= getColorForType(getResolvedType());
	return IM_COL32((int)(color.x * 255.f), (int)(color.y * 255.f), (int)(color.z * 255.f), 255);
}
