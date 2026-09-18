#pragma once

#include "MkWindowExport.h"

#include <memory>
#include <string>

class IMkWindowContext;

class IMkWindowContextManager;
using IMkWindowContextManagerPtr= std::shared_ptr<IMkWindowContextManager>;

class MIKAN_WINDOW_CLASS IMkWindowContextManager
{
public:
	virtual ~IMkWindowContextManager() {}

	virtual bool startup()= 0;
	virtual void shutdown()= 0;
	virtual void pollEvents()= 0;

	virtual bool getIsInitialized() const= 0;
	virtual const std::string& getGlslVersion() const= 0;
	// The primary display's content scale (1.0 at 96 DPI). A window is created in
	// physical pixels, so a size authored for 96 DPI is multiplied by this to open
	// at the same apparent size on a high DPI display.
	virtual float getPrimaryDisplayContentScale() const= 0;
	virtual void setMouseCursor(const std::string& cursorName)= 0;

	virtual void pushCurrentWindowContext(IMkWindowContext* window)= 0;
	virtual IMkWindowContext* getCurrentWindowContext() const= 0;
	virtual void popCurrentWindowContext(IMkWindowContext* window)= 0;
};

MIKAN_WINDOW_FUNC(IMkWindowContextManagerPtr) createMkWindowContextManager();
