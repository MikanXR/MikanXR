#pragma once

//-- includes -----
#include "IMkWindowContextManager.h"
#include "SdlFwd.h"

#include <string>
#include <vector>

//-- definitions -----
class SdlWindowContextManager : public IMkWindowContextManager
{
public:
	SdlWindowContextManager();
	~SdlWindowContextManager();

	// -- IMkWindowContextManager --
	virtual bool startup() override;
	virtual void shutdown() override;
	virtual void pollEvents() override;

	virtual bool getIsInitialized() const override { return m_sdlInitialized; }
	virtual const std::string& getGlslVersion() const override { return m_glslVersion; }
	virtual float getPrimaryDisplayContentScale() const override;
	virtual void setMouseCursor(const std::string& cursorName) override;

	virtual void pushCurrentWindowContext(IMkWindowContext* window) override;
	virtual IMkWindowContext* getCurrentWindowContext() const override;
	virtual void popCurrentWindowContext(IMkWindowContext* window) override;

	std::vector<SDL_Event>& getEvents() { return m_events; }

private:
	bool m_sdlInitialized= false;
	std::string m_glslVersion;

	std::vector<IMkWindowContext*> m_mkWindowContextStack;

	SDL_Cursor* cursor_default= nullptr;
	SDL_Cursor* cursor_move= nullptr;
	SDL_Cursor* cursor_pointer= nullptr;
	SDL_Cursor* cursor_resize= nullptr;
	SDL_Cursor* cursor_cross= nullptr;
	SDL_Cursor* cursor_text= nullptr;
	SDL_Cursor* cursor_unavailable= nullptr;

	std::vector<SDL_Event> m_events;
};
