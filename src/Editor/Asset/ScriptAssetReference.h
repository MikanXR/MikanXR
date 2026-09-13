#pragma once

#include "AssetReference.h"

class ScriptAssetReference : public AssetReference
{
public:
	ScriptAssetReference()= default;

	inline static const std::string k_assetClassName= "ScriptAssetReference";
	virtual std::string getClassName() const override { return k_assetClassName; }
	virtual std::string getAssetTypeName() const override { return "Script"; }
	virtual const char* editorGetIcon() const override { return ICON_FK_FILE_CODE_O; }

	// Scripts open in the external editor from the app settings rather than a
	// window of their own. A bundled script is copied into the project first.
	virtual bool editorCanOpen() const override { return !isEmpty(); }
	virtual void editorOpen() override;
};

class ScriptAssetReferenceFactory : public TypedAssetReferenceFactory<ScriptAssetReference, AssetReferenceConfig>
{
public:
	ScriptAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "Script"; }
	virtual char const* getFileDialogTitle() const { return "Load Script"; }
	virtual char const* const* getFilterPatterns() const
	{
		static const char* filterItems[1]= {"*.lua"};
		return filterItems;
	}
	virtual int getFilterPatternCount() const { return 1; }
	virtual char const* getFilterDescription() const { return "Script Files (*.lua)"; }
};