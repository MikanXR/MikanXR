#pragma once

#include "GraphPage.h"
#include "MaterialCompiler/ShaderValueType.h"

#include <vector>

// One declared input of a material function, as persisted
struct MaterialFunctionInputConfig
{
	std::string name;
	std::string valueType;
	ShaderValueDefault defaultValue= {};
};

class MaterialFunctionPageConfig : public GraphPageConfig
{
public:
	MaterialFunctionPageConfig()= default;

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	std::vector<MaterialFunctionInputConfig> inputs;
	std::string outputType;
};

// One declared input of a material function: the parameter name the page's
// input node exposes, the type a call site coerces its argument to, and the
// value an unconnected call pin passes
struct MaterialFunctionInput
{
	std::string name;
	eShaderValueType type= eShaderValueType::float1;
	ShaderValueDefault defaultValue= {};
};

// A material function: a page of shader nodes with declared typed inputs and
// one typed output, compiled once per stage into a function of the shader
// language and called from any page through a call node. The page name is the
// function name. The function's source is this embedded page; a shared
// function asset would be a second source behind the same call node.
class MaterialFunctionPage : public GraphPage
{
public:
	MaterialFunctionPage()= default;

	inline static const std::string k_pageClassName= "MaterialFunctionPage";
	virtual std::string getClassName() const override { return k_pageClassName; }

	virtual bool loadFromConfig(GraphPageConfigConstPtr config) override;
	virtual void saveToConfig(GraphPageConfigPtr config) const override;

	inline const std::vector<MaterialFunctionInput>& getInputs() const { return m_inputs; }
	const MaterialFunctionInput* findInput(const std::string& name) const;
	inline eShaderValueType getOutputType() const { return m_outputType; }

	// Every edit below keeps the page's input and output nodes and every call
	// node in step, then raises the graph's page-modified delegate

	// Ignored unless the name is an identifier no other function page uses
	bool setFunctionName(const std::string& name);
	void setOutputType(eShaderValueType type);
	// Adds an input with a fresh unique name and its node on the page
	void addInput();
	void removeInput(size_t index);
	bool renameInput(size_t index, const std::string& name);
	void setInputType(size_t index, eShaderValueType type);
	void setInputDefault(size_t index, const ShaderValueDefault& value);

	std::shared_ptr<class ShaderFunctionOutputNode> getOutputNode() const;
	std::shared_ptr<class ShaderFunctionInputNode> getInputNode(const std::string& inputName) const;

	virtual void onPageCreated(const class NodeEditorState& editorState) override;
	virtual void onGraphLoaded() override;

	virtual const char* editorGetIcon() const override;
	virtual void editorRenderPropertySheet(const class NodeEditorState& editorState) override;
	virtual bool editorCanRename() const override { return true; }
	virtual bool editorRename(const std::string& name) override { return setFunctionName(name); }

protected:
	bool hasInputNamed(const std::string& name) const;
	std::string makeUniqueInputName() const;
	void createInputNode(const MaterialFunctionInput& input, const class NodeEditorState& editorState);
	void notifyModified();

	std::vector<MaterialFunctionInput> m_inputs;
	eShaderValueType m_outputType= eShaderValueType::float1;
};
using MaterialFunctionPagePtr= std::shared_ptr<MaterialFunctionPage>;

class MaterialFunctionPageFactory : public TypedGraphPageFactory<MaterialFunctionPage, MaterialFunctionPageConfig>
{
public:
	MaterialFunctionPageFactory()= default;

	virtual std::string editorGetCreateLabel() const override;
};
