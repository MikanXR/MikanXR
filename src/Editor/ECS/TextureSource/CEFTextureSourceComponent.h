#pragma once

#include "CEFBrowserInput.h"
#include "TextureSourceComponent.h"

#include "include/internal/cef_ptr.h"

#include <mutex>
#include <vector>

// Forward declaration so CEFBrowserClient can reference the component
class CEFTextureSourceComponent;

// -------------------------------------------------------------------------------------------------
// CEFTextureSourceDefinition
// -------------------------------------------------------------------------------------------------
class CEFTextureSourceDefinition : public TextureSourceDefinition
{
public:
	CEFTextureSourceDefinition();
	CEFTextureSourceDefinition(MikanTextureSourceID TextureSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_urlPropertyId;
	static const std::string k_widthPropertyId;
	static const std::string k_heightPropertyId;
	static const std::string k_urlWritebackPropertyId;

	inline const std::string& getUrl() const { return m_url; }
	inline int getWidth() const { return m_width; }
	inline int getHeight() const { return m_height; }
	inline bool getUrlWriteback() const { return m_bUrlWriteback; }

	void setUrl(const std::string& url);
	void setWidth(int width);
	void setHeight(int height);
	void setUrlWriteback(bool bWriteback);

private:
	std::string m_url;
	int m_width= 1280;
	int m_height= 720;
	// When set, browsing in the interactive window updates m_url so the project records the page
	// the scene is actually showing. Off by default so casual browsing does not edit the project.
	bool m_bUrlWriteback= false;
};

// -------------------------------------------------------------------------------------------------
// CEFTextureSourceComponent
// -------------------------------------------------------------------------------------------------
class CEFTextureSourceComponent : public TextureSourceComponent
{
public:
	CEFTextureSourceComponent(MikanObjectWeakPtr owner);

	// Declared out of line, and defined in the .cpp where cef_browser.h is
	// included. The CefRefPtr members below are declared against forward
	// declarations only, and ~scoped_refptr requires the complete type - so an
	// implicit destructor would force every translation unit that destroys this
	// component to have included the full CEF browser headers. That happened to
	// hold under one CMake unity grouping and broke as soon as the grouping
	// shifted.
	virtual ~CEFTextureSourceComponent();

	inline CEFTextureSourceDefinitionPtr getCEFTextureSourceDefinition() const
	{
		return std::static_pointer_cast<CEFTextureSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;
	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "CEFTextureSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// Texture Source Interface
	virtual IMkTexturePtr getClientColorSourceTexture(MikanCameraID cameraId,
													  eTextureSourceColorType textureSourceColorType,
													  int64_t frameIndex= -1) const override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_matchResolutionToWindowFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;
	virtual void showTextureSourceSettings() override;

	// -- Browser window driving (called from CEFBrowserEditorWindow) ----
	// Input coordinates are browser view pixels, top-down from the upper left corner
	bool hasBrowser() const;
	void sendMouseMove(int browserX, int browserY, bool bMouseLeave, const CEFBrowserInputState& inputState);
	void sendMouseButton(int browserX, int browserY, int mkMouseButton, bool bMouseUp, int clickCount,
						 const CEFBrowserInputState& inputState);
	void sendMouseWheel(int browserX, int browserY, int deltaX, int deltaY, const CEFBrowserInputState& inputState);
	void sendKeyEvent(MkKeySym keySym, bool bKeyDown, const CEFBrowserInputState& inputState);
	void sendTextInput(const char* utf8Text, const CEFBrowserInputState& inputState);
	void setBrowserFocus(bool bFocused);

	void browserLoadUrl(const std::string& url);
	void browserGoBack();
	void browserGoForward();
	void browserReload();
	void browserShowDevTools();

	inline const std::string& getBrowserUrl() const { return m_browserUrl; }
	inline const std::string& getBrowserTitle() const { return m_browserTitle; }
	inline bool getBrowserCanGoBack() const { return m_bBrowserCanGoBack; }
	inline bool getBrowserCanGoForward() const { return m_bBrowserCanGoForward; }
	inline bool getBrowserIsLoading() const { return m_bBrowserIsLoading; }

	// -- CEFBrowserClient callbacks (called from CEFBrowserClient) ----
	void onCefGetViewRect(class CefRect& rect);
	void onCefPaint(const void* buffer, int width, int height);
	void onCefPopupPaint(const void* buffer, int width, int height);
	void onCefPopupShow(bool bShow);
	void onCefPopupSize(int x, int y, int width, int height);
	void onCefBrowserCreated(CefRefPtr<class CefBrowser> browser);
	void onCefBrowserClosed();
	void onCefAddressChange(const std::string& url);
	void onCefTitleChange(const std::string& title);
	void onCefLoadingStateChange(bool bIsLoading, bool bCanGoBack, bool bCanGoForward);

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;
	void closeTextureSource();
	void openTextureSource();
	class CEFBrowserEditorWindow* findBrowserWindow() const;

private:
	CefRefPtr<class CefBrowser> m_browser;
	CefRefPtr<class CEFBrowserClient> m_cefClient;
	IMkTexturePtr m_colorTexture;

	std::mutex m_stagingMutex;
	// Pristine view surface. Kept free of popup pixels so that hiding a popup restores what was
	// underneath it without waiting for the browser to repaint that region.
	std::vector<uint8_t> m_stagingBuffer;
	int m_stagingWidth= 0;
	int m_stagingHeight= 0;
	bool m_dirty= false;

	// Widget popup surface (dropdowns, autofill), composited over the view at upload time
	std::vector<uint8_t> m_popupBuffer;
	std::vector<uint8_t> m_compositeBuffer;
	int m_popupWidth= 0;
	int m_popupHeight= 0;
	int m_popupX= 0;
	int m_popupY= 0;
	bool m_bPopupVisible= false;

	std::string m_browserUrl;
	std::string m_browserTitle;
	bool m_bBrowserCanGoBack= false;
	bool m_bBrowserCanGoForward= false;
	bool m_bBrowserIsLoading= false;

	// Set while a URL write-back is editing the definition, so the resulting property change
	// does not navigate the browser back to where it already is.
	bool m_bSuppressUrlNavigation= false;
};
