#pragma once

#include "AssetReference.h"
#include "LocText.h"

class ModelAssetReference : public AssetReference
{
public:
	ModelAssetReference()= default;

	inline static const std::string k_assetClassName= "ModelAssetReference";
	virtual std::string getClassName() const override { return k_assetClassName; }
	virtual std::string getAssetTypeName() const override { return "Model"; }
	virtual const char* editorGetIcon() const override { return ICON_FK_CUBE; }
	virtual void editorRenderPropertySheet(const class NodeEditorState& editorState) override;

protected:
	virtual void rebuildPreview() override;
};

class ModelAssetReferenceFactory : public TypedAssetReferenceFactory<ModelAssetReference, AssetReferenceConfig>
{
public:
	ModelAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "Model"; }
	virtual char const* getFileDialogTitle() const { return "Load Model"; }
	virtual char const* const* getFilterPatterns() const
	{
		static const char* filterItems[3]= {"*.obj", "*.glb", "*.gltf"};
		return filterItems;
	}
	virtual int getFilterPatternCount() const { return 3; }
	virtual char const* getFilterDescription() const { return locText("assets.modelFilterDescription"); }
};