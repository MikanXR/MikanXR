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
	// The command may carry {project}, {file}, {line} placeholders.
	virtual bool editorCanOpen() const override { return !isEmpty(); }
	virtual void editorOpen() override;

	// Substitutes {project}/{file}/{line} in an editor command with the quoted
	// generic-string paths and a 1-based line number. Pure.
	static std::string expandEditorCommand(const std::string& command, const std::filesystem::path& projectDir,
										   const std::filesystem::path& scriptPath, int line);

	// Opens scriptPath in the configured script editor, at line when non-zero,
	// copying a read-only bundled script into the project first if needed.
	static bool openScriptInEditor(const std::filesystem::path& scriptPath, int line= 0);
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