#include "App.h"
#include "IMkTexture.h"
#include "Logger.h"
#include "MikanTextureSourceTypes.h"
#include "CEFBrowserClient.h"
#include "CEFBrowserEditorWindow.h"
#include "CEFTextureSourceComponent.h"
#include "StringUtils.h"

#include "include/cef_browser.h"

#include <easy/profiler.h>

#include <algorithm>
#include <cstring>

namespace
{
// Windows virtual key codes for the keys that do not map straight from their SDL keycode.
// Spelled out numerically so this translation unit does not have to pull in windows.h next
// to the CEF headers.
constexpr int k_vkBack= 0x08;
constexpr int k_vkTab= 0x09;
constexpr int k_vkReturn= 0x0D;
constexpr int k_vkShift= 0x10;
constexpr int k_vkControl= 0x11;
constexpr int k_vkMenu= 0x12;
constexpr int k_vkPause= 0x13;
constexpr int k_vkEscape= 0x1B;
constexpr int k_vkSpace= 0x20;
constexpr int k_vkPageUp= 0x21;
constexpr int k_vkPageDown= 0x22;
constexpr int k_vkEnd= 0x23;
constexpr int k_vkHome= 0x24;
constexpr int k_vkLeft= 0x25;
constexpr int k_vkUp= 0x26;
constexpr int k_vkRight= 0x27;
constexpr int k_vkDown= 0x28;
constexpr int k_vkPrintScreen= 0x2C;
constexpr int k_vkInsert= 0x2D;
constexpr int k_vkDelete= 0x2E;
constexpr int k_vkLeftWin= 0x5B;
constexpr int k_vkRightWin= 0x5C;
constexpr int k_vkF1= 0x70;
constexpr int k_vkScrollLock= 0x91;
constexpr int k_vkOemSemicolon= 0xBA;
constexpr int k_vkOemPlus= 0xBB;
constexpr int k_vkOemComma= 0xBC;
constexpr int k_vkOemMinus= 0xBD;
constexpr int k_vkOemPeriod= 0xBE;
constexpr int k_vkOemSlash= 0xBF;
constexpr int k_vkOemBackquote= 0xC0;
constexpr int k_vkOemLeftBracket= 0xDB;
constexpr int k_vkOemBackslash= 0xDC;
constexpr int k_vkOemRightBracket= 0xDD;
constexpr int k_vkOemQuote= 0xDE;

// MkKeySym uses the SDL keycode encoding, so unshifted printable ASCII is its own code and
// everything else lives in the scancode-masked range.
int mkKeySymToWindowsKeyCode(MkKeySym keySym)
{
	if (keySym >= MkKey::LETTER_a && keySym <= MkKey::LETTER_z)
		return (keySym - MkKey::LETTER_a) + 'A';

	if (keySym >= MkKey::NUM_0 && keySym <= MkKey::NUM_9)
		return keySym;

	if (keySym >= MkKey::F1 && keySym <= MkKey::F12)
		return (keySym - MkKey::F1) + k_vkF1;

	switch (keySym)
	{
	case MkKey::BACKSPACE:
		return k_vkBack;
	case MkKey::TAB:
		return k_vkTab;
	case MkKey::RETURN:
		return k_vkReturn;
	case MkKey::ESCAPE:
		return k_vkEscape;
	case MkKey::SPACE:
		return k_vkSpace;
	case MkKey::PAGEUP:
		return k_vkPageUp;
	case MkKey::PAGEDOWN:
		return k_vkPageDown;
	case MkKey::END:
		return k_vkEnd;
	case MkKey::HOME:
		return k_vkHome;
	case MkKey::LEFT:
		return k_vkLeft;
	case MkKey::UP:
		return k_vkUp;
	case MkKey::RIGHT:
		return k_vkRight;
	case MkKey::DOWN:
		return k_vkDown;
	case MkKey::INSERT:
		return k_vkInsert;
	case MkKey::DELETE_KEYCODE:
		return k_vkDelete;
	case MkKey::PRINTSCREEN:
		return k_vkPrintScreen;
	case MkKey::SCROLLLOCK:
		return k_vkScrollLock;
	case MkKey::PAUSE:
		return k_vkPause;
	case MkKey::LEFT_SHIFT:
	case MkKey::RIGHT_SHIFT:
		return k_vkShift;
	case MkKey::LEFT_CTRL:
	case MkKey::RIGHT_CTRL:
		return k_vkControl;
	case MkKey::LEFT_ALT:
	case MkKey::RIGHT_ALT:
		return k_vkMenu;
	case MkKey::LEFT_GUI:
		return k_vkLeftWin;
	case MkKey::RIGHT_GUI:
		return k_vkRightWin;
	case MkKey::SEMICOLON:
		return k_vkOemSemicolon;
	case MkKey::EQUALS:
		return k_vkOemPlus;
	case ',':
		return k_vkOemComma;
	case MkKey::MINUS:
		return k_vkOemMinus;
	case MkKey::PERIOD:
		return k_vkOemPeriod;
	case MkKey::SLASH:
		return k_vkOemSlash;
	case MkKey::BACKQUOTE:
		return k_vkOemBackquote;
	case MkKey::LEFTBRACKET:
		return k_vkOemLeftBracket;
	case MkKey::BACKSLASH:
		return k_vkOemBackslash;
	case MkKey::RIGHTBRACKET:
		return k_vkOemRightBracket;
	case MkKey::QUOTE:
		return k_vkOemQuote;
	default:
		return 0;
	}
}

uint32_t makeCefEventFlags(const CEFBrowserInputState& inputState)
{
	uint32_t flags= EVENTFLAG_NONE;

	if (inputState.keyMod & MkKeyMod::SHIFT)
		flags|= EVENTFLAG_SHIFT_DOWN;
	if (inputState.keyMod & MkKeyMod::CTRL)
		flags|= EVENTFLAG_CONTROL_DOWN;
	if (inputState.keyMod & MkKeyMod::ALT)
		flags|= EVENTFLAG_ALT_DOWN;
	if (inputState.keyMod & MkKeyMod::GUI)
		flags|= EVENTFLAG_COMMAND_DOWN;

	if (inputState.bLeftButtonDown)
		flags|= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (inputState.bMiddleButtonDown)
		flags|= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if (inputState.bRightButtonDown)
		flags|= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	return flags;
}

CefMouseEvent makeCefMouseEvent(int browserX, int browserY, const CEFBrowserInputState& inputState)
{
	CefMouseEvent mouseEvent;
	mouseEvent.x= browserX;
	mouseEvent.y= browserY;
	mouseEvent.modifiers= makeCefEventFlags(inputState);

	return mouseEvent;
}

constexpr int k_bytesPerPixel= 4; // BGRA
} // namespace

// -- CEFTextureSourceDefinition ------
const std::string CEFTextureSourceDefinition::k_urlPropertyId= "url";
const std::string CEFTextureSourceDefinition::k_widthPropertyId= "width";
const std::string CEFTextureSourceDefinition::k_heightPropertyId= "height";
const std::string CEFTextureSourceDefinition::k_urlWritebackPropertyId= "url_writeback";

CEFTextureSourceDefinition::CEFTextureSourceDefinition()
	: TextureSourceDefinition()
{
}

CEFTextureSourceDefinition::CEFTextureSourceDefinition(MikanTextureSourceID textureSourceId)
	: TextureSourceDefinition(textureSourceId)
{
}

configuru::Config CEFTextureSourceDefinition::writeToJSON()
{
	configuru::Config pt= TextureSourceDefinition::writeToJSON();

	pt["cef_url"]= m_url;
	pt["cef_width"]= m_width;
	pt["cef_height"]= m_height;
	pt["cef_url_writeback"]= m_bUrlWriteback;

	return pt;
}

void CEFTextureSourceDefinition::readFromJSON(const configuru::Config& pt)
{
	TextureSourceDefinition::readFromJSON(pt);

	m_url= pt.get_or<std::string>("cef_url", m_url);
	m_width= pt.get_or<int>("cef_width", m_width);
	m_height= pt.get_or<int>("cef_height", m_height);
	m_bUrlWriteback= pt.get_or<bool>("cef_url_writeback", m_bUrlWriteback);
}

bool CEFTextureSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TextureSourceDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanCEFTextureSourceValues>();
	if (componentValues)
	{
		m_url= componentValues->url.getUtf8Value();
		m_width= componentValues->width;
		m_height= componentValues->height;
	}

	return true;
}

void CEFTextureSourceDefinition::setUrl(const std::string& url)
{
	if (url != m_url)
	{
		m_url= url;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_urlPropertyId));
	}
}

void CEFTextureSourceDefinition::setWidth(int width)
{
	if (width != m_width)
	{
		m_width= width;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_widthPropertyId));
	}
}

void CEFTextureSourceDefinition::setHeight(int height)
{
	if (height != m_height)
	{
		m_height= height;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_heightPropertyId));
	}
}

void CEFTextureSourceDefinition::setUrlWriteback(bool bWriteback)
{
	if (bWriteback != m_bUrlWriteback)
	{
		m_bUrlWriteback= bWriteback;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_urlWritebackPropertyId));
	}
}

// -- CEFTextureSourceComponent -----
CEFTextureSourceComponent::CEFTextureSourceComponent(MikanObjectWeakPtr owner)
	: TextureSourceComponent(owner)
{
	m_bWantsUpdate= true;
}

CEFTextureSourceComponent::~CEFTextureSourceComponent()= default;

// -- IEntityAccessor ----
Serialization::StructTypeHandle CEFTextureSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanCEFTextureSourceValues::staticGetArchetype();
}

void CEFTextureSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	openTextureSource();
}

void CEFTextureSourceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
														const ConfigPropertyChangeSet& changedPropertySet)
{
	TextureSourceComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	if (changedPropertySet.hasPropertyName(CEFTextureSourceDefinition::k_urlPropertyId))
	{
		// The browser is already showing this URL; it told us about it rather than the other way around
		if (m_bSuppressUrlNavigation)
			return;

		const std::string& url= getCEFTextureSourceDefinition()->getUrl();
		if (m_browser && !url.empty())
		{
			// Navigate in-place — much cheaper than tearing down and recreating the browser.
			m_browser->GetMainFrame()->LoadURL(url);
		}
		else
		{
			// No live browser yet (e.g. URL was previously empty); do a full open.
			openTextureSource();
		}
	}
	else if (changedPropertySet.hasPropertyName(CEFTextureSourceDefinition::k_widthPropertyId)
			 || changedPropertySet.hasPropertyName(CEFTextureSourceDefinition::k_heightPropertyId))
	{
		// Size changes are handled by the browser sending a new OnPaint at the updated GetViewRect size
		if (m_browser)
		{
			m_browser->GetHost()->WasResized();
		}
	}
}

void CEFTextureSourceComponent::update(float deltaSeconds)
{
	EASY_BLOCK("CEFTextureSource: upload texture");

	std::lock_guard<std::mutex> lock(m_stagingMutex);

	if (!m_dirty || m_stagingBuffer.empty())
		return;

	const int w= m_stagingWidth;
	const int h= m_stagingHeight;

	if (w <= 0 || h <= 0)
		return;

	// Reallocate GPU texture if size changed
	const int textureWidth= m_colorTexture ? m_colorTexture->getTextureWidth() : 0;
	const int textureHeight= m_colorTexture ? m_colorTexture->getTextureHeight() : 0;

	if (w != textureWidth || h != textureHeight)
	{
		if (m_colorTexture)
		{
			m_colorTexture->disposeTexture();
			m_colorTexture= nullptr;
		}

		m_colorTexture= CreateMkTexture();
		m_colorTexture->setSize((uint16_t)w, (uint16_t)h);
		m_colorTexture->setTextureFormat(MK_BGRA);
		m_colorTexture->setBufferFormat(MK_BGRA);
		// Upload straight from the staging buffer rather than through a PBO. PBO streaming uploads
		// the previous call's buffer and stages the current one, which costs nothing for a video
		// source that pushes a frame every tick. A browser only paints on damage, so the last paint
		// of a settled page would sit in the PBO unshown and the texture would hold the page before
		// it. The upload is sporadic here, so there is no per-frame stall for a PBO to hide.
		m_colorTexture->setPixelBufferObjectMode(IMkTexture::PixelBufferObjectMode::NoPBO);
		m_colorTexture->createTexture();
	}

	if (m_colorTexture)
	{
		// Composite the widget popup over a scratch copy rather than into the view buffer, so that
		// hiding the popup reveals what was underneath it without waiting for a browser repaint.
		const std::vector<uint8_t>& uploadBuffer=
			m_bPopupVisible && !m_popupBuffer.empty() ? m_compositeBuffer : m_stagingBuffer;

		if (&uploadBuffer == &m_compositeBuffer)
		{
			m_compositeBuffer= m_stagingBuffer;

			const int copyLeft= std::max(m_popupX, 0);
			const int copyTop= std::max(m_popupY, 0);
			const int copyRight= std::min(m_popupX + m_popupWidth, w);
			const int copyBottom= std::min(m_popupY + m_popupHeight, h);

			for (int y= copyTop; y < copyBottom; ++y)
			{
				const size_t srcOffset=
					((size_t)(y - m_popupY) * m_popupWidth + (copyLeft - m_popupX)) * k_bytesPerPixel;
				const size_t dstOffset= ((size_t)y * w + copyLeft) * k_bytesPerPixel;
				const size_t rowBytes= (size_t)(copyRight - copyLeft) * k_bytesPerPixel;

				memcpy(m_compositeBuffer.data() + dstOffset, m_popupBuffer.data() + srcOffset, rowBytes);
			}
		}

		m_colorTexture->copyBufferIntoTexture(uploadBuffer.data(), uploadBuffer.size());
	}

	m_dirty= false;
}

void CEFTextureSourceComponent::dispose()
{
	closeTextureSource();

	TextureSourceComponent::dispose();
}

void CEFTextureSourceComponent::closeTextureSource()
{
	if (m_browser)
	{
		m_browser->GetHost()->CloseBrowser(true);
		m_browser= nullptr;
	}

	// Release the client after the browser is gone so CEF's refcount can reach zero cleanly
	m_cefClient= nullptr;

	if (m_colorTexture)
	{
		m_colorTexture->disposeTexture();
		m_colorTexture= nullptr;
	}

	m_browserUrl.clear();
	m_browserTitle.clear();
	m_bBrowserCanGoBack= false;
	m_bBrowserCanGoForward= false;
	m_bBrowserIsLoading= false;

	{
		std::lock_guard<std::mutex> lock(m_stagingMutex);
		m_stagingBuffer.clear();
		m_stagingWidth= 0;
		m_stagingHeight= 0;
		m_dirty= false;

		m_popupBuffer.clear();
		m_compositeBuffer.clear();
		m_popupWidth= 0;
		m_popupHeight= 0;
		m_popupX= 0;
		m_popupY= 0;
		m_bPopupVisible= false;
	}
}

void CEFTextureSourceComponent::openTextureSource()
{
	closeTextureSource();

	const auto def= getCEFTextureSourceDefinition();
	if (!def)
		return;

	const std::string& url= def->getUrl();
	if (url.empty())
		return;

	m_cefClient= new CEFBrowserClient(getSelfPtr<CEFTextureSourceComponent>());

	CefWindowInfo windowInfo;
	windowInfo.SetAsWindowless(nullptr);

	CefBrowserSettings browserSettings;
	browserSettings.windowless_frame_rate= 60;

	CefBrowserHost::CreateBrowser(windowInfo, m_cefClient, url, browserSettings, nullptr, nullptr);
}

// -- Texture Source Interface ---
IMkTexturePtr CEFTextureSourceComponent::getClientColorSourceTexture(MikanCameraID cameraId,
																	 eTextureSourceColorType textureSourceColorType,
																	 int64_t frameIndex) const
{
	return m_colorTexture;
}

// -- Browser window driving ----
bool CEFTextureSourceComponent::hasBrowser() const { return m_browser != nullptr; }

void CEFTextureSourceComponent::sendMouseMove(int browserX, int browserY, bool bMouseLeave,
											  const CEFBrowserInputState& inputState)
{
	if (!m_browser)
		return;

	CefMouseEvent mouseEvent= makeCefMouseEvent(browserX, browserY, inputState);
	m_browser->GetHost()->SendMouseMoveEvent(mouseEvent, bMouseLeave);
}

void CEFTextureSourceComponent::sendMouseButton(int browserX, int browserY, int mkMouseButton, bool bMouseUp,
												int clickCount, const CEFBrowserInputState& inputState)
{
	if (!m_browser)
		return;

	CefBrowserHost::MouseButtonType buttonType;
	switch (mkMouseButton)
	{
	case MkMouseButton::LEFT:
		buttonType= MBT_LEFT;
		break;
	case MkMouseButton::MIDDLE:
		buttonType= MBT_MIDDLE;
		break;
	case MkMouseButton::RIGHT:
		buttonType= MBT_RIGHT;
		break;
	default:
		return;
	}

	CefMouseEvent mouseEvent= makeCefMouseEvent(browserX, browserY, inputState);
	m_browser->GetHost()->SendMouseClickEvent(mouseEvent, buttonType, bMouseUp, std::max(clickCount, 1));
}

void CEFTextureSourceComponent::sendMouseWheel(int browserX, int browserY, int deltaX, int deltaY,
											   const CEFBrowserInputState& inputState)
{
	if (!m_browser)
		return;

	// SDL reports wheel motion in notches; Chromium expects pixels
	constexpr int k_pixelsPerWheelNotch= 40;

	CefMouseEvent mouseEvent= makeCefMouseEvent(browserX, browserY, inputState);
	m_browser->GetHost()->SendMouseWheelEvent(mouseEvent, deltaX * k_pixelsPerWheelNotch,
											  deltaY * k_pixelsPerWheelNotch);
}

void CEFTextureSourceComponent::sendKeyEvent(MkKeySym keySym, bool bKeyDown, const CEFBrowserInputState& inputState)
{
	if (!m_browser)
		return;

	const int windowsKeyCode= mkKeySymToWindowsKeyCode(keySym);
	if (windowsKeyCode == 0)
		return;

	CefKeyEvent keyEvent;
	keyEvent.type= bKeyDown ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
	keyEvent.modifiers= makeCefEventFlags(inputState);
	keyEvent.windows_key_code= windowsKeyCode;
	keyEvent.native_key_code= 0;

	m_browser->GetHost()->SendKeyEvent(keyEvent);
}

void CEFTextureSourceComponent::sendTextInput(const char* utf8Text, const CEFBrowserInputState& inputState)
{
	if (!m_browser || utf8Text == nullptr)
		return;

	// KEYEVENT_CHAR carries one UTF-16 code unit at a time, so widen and send each in turn
	const std::wstring wideText= StringUtils::convertUTF8StringToWString(utf8Text);
	const uint32_t modifiers= makeCefEventFlags(inputState);

	for (wchar_t wideChar : wideText)
	{
		CefKeyEvent keyEvent;
		keyEvent.type= KEYEVENT_CHAR;
		keyEvent.modifiers= modifiers;
		keyEvent.windows_key_code= (int)wideChar;
		keyEvent.character= (char16_t)wideChar;
		keyEvent.unmodified_character= (char16_t)wideChar;
		keyEvent.native_key_code= 0;

		m_browser->GetHost()->SendKeyEvent(keyEvent);
	}
}

void CEFTextureSourceComponent::setBrowserFocus(bool bFocused)
{
	if (m_browser)
	{
		m_browser->GetHost()->SetFocus(bFocused);
	}
}

void CEFTextureSourceComponent::browserLoadUrl(const std::string& url)
{
	if (m_browser && !url.empty())
	{
		m_browser->GetMainFrame()->LoadURL(url);
	}
}

void CEFTextureSourceComponent::browserGoBack()
{
	if (m_browser)
	{
		m_browser->GoBack();
	}
}

void CEFTextureSourceComponent::browserGoForward()
{
	if (m_browser)
	{
		m_browser->GoForward();
	}
}

void CEFTextureSourceComponent::browserReload()
{
	if (m_browser)
	{
		m_browser->Reload();
	}
}

void CEFTextureSourceComponent::browserShowDevTools()
{
	if (!m_browser)
		return;

	CefWindowInfo windowInfo;
	windowInfo.SetAsPopup(nullptr, "DevTools");

	CefBrowserSettings browserSettings;

	// DevTools gets a real window of its own. That does not disturb the windowless browser it
	// inspects, which keeps painting into the scene texture.
	m_browser->GetHost()->ShowDevTools(windowInfo, nullptr, browserSettings, CefPoint());
}

// -- CEFBrowserClient callbacks ----
void CEFTextureSourceComponent::onCefGetViewRect(CefRect& rect)
{
	const auto def= getCEFTextureSourceDefinition();
	if (def)
	{
		rect= CefRect(0, 0, def->getWidth(), def->getHeight());
	}
	else
	{
		rect= CefRect(0, 0, 1280, 720);
	}
}

void CEFTextureSourceComponent::onCefPaint(const void* buffer, int width, int height)
{
	const size_t bufferSize= (size_t)width * height * k_bytesPerPixel;

	std::lock_guard<std::mutex> lock(m_stagingMutex);
	m_stagingBuffer.resize(bufferSize);
	memcpy(m_stagingBuffer.data(), buffer, bufferSize);
	m_stagingWidth= width;
	m_stagingHeight= height;
	m_dirty= true;
}

void CEFTextureSourceComponent::onCefPopupPaint(const void* buffer, int width, int height)
{
	const size_t bufferSize= (size_t)width * height * k_bytesPerPixel;

	std::lock_guard<std::mutex> lock(m_stagingMutex);
	m_popupBuffer.resize(bufferSize);
	memcpy(m_popupBuffer.data(), buffer, bufferSize);
	m_popupWidth= width;
	m_popupHeight= height;
	m_dirty= true;
}

void CEFTextureSourceComponent::onCefPopupShow(bool bShow)
{
	std::lock_guard<std::mutex> lock(m_stagingMutex);

	m_bPopupVisible= bShow;
	if (!bShow)
	{
		m_popupBuffer.clear();
		m_popupWidth= 0;
		m_popupHeight= 0;
	}

	// Force a re-upload so the popup appears, or so the view underneath it comes back
	m_dirty= true;
}

void CEFTextureSourceComponent::onCefPopupSize(int x, int y, int width, int height)
{
	std::lock_guard<std::mutex> lock(m_stagingMutex);

	m_popupX= x;
	m_popupY= y;
	m_popupWidth= width;
	m_popupHeight= height;
}

void CEFTextureSourceComponent::onCefBrowserCreated(CefRefPtr<CefBrowser> browser)
{
	m_browser= browser;

	// A windowless browser has no native window for Chromium to derive visibility from, and CEF
	// stops layout and OnPaint entirely while a browser is hidden. Without this the page only
	// repaints when something else pokes it, so a navigation lands in the address bar while the
	// scene texture keeps showing the previous page. This browser feeds a scene texture for its
	// whole life, so it is always visible regardless of whether any editor window is showing it.
	browser->GetHost()->WasHidden(false);
}

void CEFTextureSourceComponent::onCefBrowserClosed() { m_browser= nullptr; }

void CEFTextureSourceComponent::onCefAddressChange(const std::string& url)
{
	m_browserUrl= url;

	const auto def= getCEFTextureSourceDefinition();
	if (!def || !def->getUrlWriteback())
		return;

	// Record where browsing ended up, without bouncing the browser back through onDefinitionMarkedDirty
	m_bSuppressUrlNavigation= true;
	def->setUrl(url);
	m_bSuppressUrlNavigation= false;
}

void CEFTextureSourceComponent::onCefTitleChange(const std::string& title) { m_browserTitle= title; }

void CEFTextureSourceComponent::onCefLoadingStateChange(bool bIsLoading, bool bCanGoBack, bool bCanGoForward)
{
	m_bBrowserIsLoading= bIsLoading;
	m_bBrowserCanGoBack= bCanGoBack;
	m_bBrowserCanGoForward= bCanGoForward;

	// A navigation can swap in a new render widget, and the replacement does not inherit the
	// visibility and size the old one was told about. Left alone it never paints, so the address
	// bar moves to the new page while the scene texture keeps showing the previous one. Re-assert
	// both once the load settles and ask for the first frame.
	if (!bIsLoading && m_browser)
	{
		CefRefPtr<CefBrowserHost> browserHost= m_browser->GetHost();

		browserHost->WasHidden(false);
		browserHost->WasResized();
		browserHost->Invalidate(PET_VIEW);
	}
}

// -- IPropertyInterface ----
void CEFTextureSourceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TextureSourceComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(CEFTextureSourceDefinition::k_urlPropertyId, MikanVariantType::STRING));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(CEFTextureSourceDefinition::k_widthPropertyId, MikanVariantType::INT));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(CEFTextureSourceDefinition::k_heightPropertyId, MikanVariantType::INT));

	// Editor-only: this governs how the interactive browser window edits the definition and has no
	// meaning to a client application, so it stays out of MikanCEFTextureSourceValues.
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(CEFTextureSourceDefinition::k_urlWritebackPropertyId,
																  MikanVariantType::BOOL)
								 ->setDefaultValue(false)
								 ->setClientAPIHidden());
}

bool CEFTextureSourceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	const auto def= getCEFTextureSourceDefinition();

	if (propertyName == CEFTextureSourceDefinition::k_urlPropertyId)
	{
		outValue= def->getUrl();
		return true;
	}
	else if (propertyName == CEFTextureSourceDefinition::k_widthPropertyId)
	{
		outValue= def->getWidth();
		return true;
	}
	else if (propertyName == CEFTextureSourceDefinition::k_heightPropertyId)
	{
		outValue= def->getHeight();
		return true;
	}
	else if (propertyName == CEFTextureSourceDefinition::k_urlWritebackPropertyId)
	{
		outValue= def->getUrlWriteback();
		return true;
	}

	return TextureSourceComponent::getPropertyValue(propertyName, outValue);
}

bool CEFTextureSourceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	const auto def= getCEFTextureSourceDefinition();

	if (propertyName == CEFTextureSourceDefinition::k_urlPropertyId)
	{
		const std::string url= inValue.getUtf8Value();
		def->setUrl(url);
		return true;
	}
	else if (propertyName == CEFTextureSourceDefinition::k_widthPropertyId)
	{
		def->setWidth(inValue.getIntValue());
		return true;
	}
	else if (propertyName == CEFTextureSourceDefinition::k_heightPropertyId)
	{
		def->setHeight(inValue.getIntValue());
		return true;
	}
	else if (propertyName == CEFTextureSourceDefinition::k_urlWritebackPropertyId)
	{
		def->setUrlWriteback(inValue.getBoolValue());
		return true;
	}

	return TextureSourceComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string CEFTextureSourceComponent::k_matchResolutionToWindowFunctionId= "match_resolution_to_browser_window";

void CEFTextureSourceComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	TextureSourceComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_matchResolutionToWindowFunctionId,
																  "Match Resolution to Browser Window"));
}

bool CEFTextureSourceComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_matchResolutionToWindowFunctionId)
	{
		CEFBrowserEditorWindow* browserWindow= findBrowserWindow();
		if (browserWindow)
		{
			int pageWidth= 0;
			int pageHeight= 0;
			browserWindow->getPageAreaSize(pageWidth, pageHeight);

			if (pageWidth > 0 && pageHeight > 0)
			{
				const auto def= getCEFTextureSourceDefinition();
				def->setWidth(pageWidth);
				def->setHeight(pageHeight);
			}
		}

		return true;
	}

	return TextureSourceComponent::invokeFunction(functionName);
}

CEFBrowserEditorWindow* CEFTextureSourceComponent::findBrowserWindow() const
{
	App* app= App::getInstance();
	if (!app)
		return nullptr;

	for (CEFBrowserEditorWindow* window : app->getWindowsOfType<CEFBrowserEditorWindow>())
	{
		if (window->getBoundComponentId() == getComponentId())
			return window;
	}

	return nullptr;
}

void CEFTextureSourceComponent::showTextureSourceSettings()
{
	CEFBrowserEditorWindow* browserWindow= findBrowserWindow();
	if (!browserWindow)
	{
		App* app= App::getInstance();
		browserWindow= app->createAppWindow<CEFBrowserEditorWindow>();

		if (browserWindow)
		{
			browserWindow->bindTextureSourceComponent(getSelfPtr<CEFTextureSourceComponent>());
		}
	}
}
