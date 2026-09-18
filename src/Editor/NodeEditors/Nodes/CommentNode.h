#pragma once

#include "LocText.h"
#include "Node.h"

#include <array>
#include <string>

class CommentNodeConfig : public NodeConfig
{
public:
	CommentNodeConfig()= default;

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	std::string text;
	std::array<float, 4> color= {0.35f, 0.35f, 0.35f, 1.f};
	std::array<float, 2> size= {320.f, 200.f};
};

// A comment box: a colored, resizable region with a title that drags the nodes
// inside it along. It has no pins, so no graph ever evaluates or compiles it,
// and every graph type registers it.
class CommentNode : public Node
{
public:
	CommentNode()= default;

	inline static const std::string k_nodeClassName= "CommentNode";
	virtual std::string getClassName() const override { return k_nodeClassName; }

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	inline const std::string& getText() const { return m_text; }
	inline void setText(const std::string& text) { m_text= text; }
	inline const std::array<float, 4>& getColor() const { return m_color; }
	inline void setColor(const std::array<float, 4>& color) { m_color= color; }
	inline const std::array<float, 2>& getSize() const { return m_size; }
	// Sets the box size and pushes it to the canvas on the next draw
	void setSize(const std::array<float, 2>& size);
	// Pushes the size the box already has again, for when the canvas scale
	// changed underneath it
	inline void applySizeOnNextDraw() { m_bApplySizeOnDraw= true; }

	// Never reached: a comment has no pins for a walk to arrive through
	virtual bool evaluateNode(NodeEvaluator& evaluator) override { return true; }

	// The first line of the text, or the localized default
	virtual std::string editorGetTitle() const override;
	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	virtual ImVec4 editorGetHeaderColor() const override;

	std::string m_text;
	std::array<float, 4> m_color= {0.35f, 0.35f, 0.35f, 1.f};
	std::array<float, 2> m_size= {320.f, 200.f};
	// The canvas owns the live size once the box exists; a loaded or edited size is pushed once
	bool m_bApplySizeOnDraw= true;

	// Editor scratch for the multiline text editor, synced from and to m_text
	inline static const size_t k_textBufferSize= 1024;
	char m_textBuffer[k_textBufferSize]= {};
};
using CommentNodePtr= std::shared_ptr<CommentNode>;

// No pins to add: the box is the whole node
class CommentNodeFactory : public TypedNodeFactory<CommentNode, CommentNodeConfig>
{
public:
	CommentNodeFactory()= default;
};
