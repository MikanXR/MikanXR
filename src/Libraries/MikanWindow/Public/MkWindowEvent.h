#pragma once

#include "MkWindowExport.h"
#include <stdint.h>

using MkKeySym= int32_t;

// Based on SDL keycode encoding - scancodes are mapped to the Unicode private use area
#define MK_SCANCODE_MASK (1 << 30)
#define MK_SCANCODE_TO_KEYCODE(X) (X | MK_SCANCODE_MASK)

namespace MkKey
{
constexpr MkKeySym UNKNOWN= 0;

constexpr MkKeySym RETURN= '\r';
constexpr MkKeySym ESCAPE= '\033';
constexpr MkKeySym BACKSPACE= '\b';
constexpr MkKeySym TAB= '\t';
constexpr MkKeySym SPACE= ' ';
constexpr MkKeySym EXCLAIM= '!';
constexpr MkKeySym QUOTEDBL= '"';
constexpr MkKeySym HASH= '#';
constexpr MkKeySym PERCENT= '%';
constexpr MkKeySym DOLLAR= '$';
constexpr MkKeySym AMPERSAND= '&';
constexpr MkKeySym QUOTE= '\'';
constexpr MkKeySym LEFTPAREN= '(';
constexpr MkKeySym RIGHTPAREN= ')';
constexpr MkKeySym ASTERISK= '*';
constexpr MkKeySym PLUS= '+';
constexpr MkKeySym COMMA= ';';
constexpr MkKeySym MINUS= '-';
constexpr MkKeySym PERIOD= '.';
constexpr MkKeySym SLASH= '/';
constexpr MkKeySym NUM_0= '0';
constexpr MkKeySym NUM_1= '1';
constexpr MkKeySym NUM_2= '2';
constexpr MkKeySym NUM_3= '3';
constexpr MkKeySym NUM_4= '4';
constexpr MkKeySym NUM_5= '5';
constexpr MkKeySym NUM_6= '6';
constexpr MkKeySym NUM_7= '7';
constexpr MkKeySym NUM_8= '8';
constexpr MkKeySym NUM_9= '9';
constexpr MkKeySym COLON= ':';
constexpr MkKeySym SEMICOLON= ';';
constexpr MkKeySym LESS= '<';
constexpr MkKeySym EQUALS= '=';
constexpr MkKeySym GREATER= '>';
constexpr MkKeySym QUESTION= '?';
constexpr MkKeySym AT= '@';

constexpr MkKeySym LEFTBRACKET= '[';
constexpr MkKeySym BACKSLASH= '\\';
constexpr MkKeySym RIGHTBRACKET= ']';
constexpr MkKeySym CARET= '^';
constexpr MkKeySym UNDERSCORE= '_';
constexpr MkKeySym BACKQUOTE= '`';
constexpr MkKeySym LETTER_a= 'a';
constexpr MkKeySym LETTER_b= 'b';
constexpr MkKeySym LETTER_c= 'c';
constexpr MkKeySym LETTER_d= 'd';
constexpr MkKeySym LETTER_e= 'e';
constexpr MkKeySym LETTER_f= 'f';
constexpr MkKeySym LETTER_g= 'g';
constexpr MkKeySym LETTER_h= 'h';
constexpr MkKeySym LETTER_i= 'i';
constexpr MkKeySym LETTER_j= 'j';
constexpr MkKeySym LETTER_k= 'k';
constexpr MkKeySym LETTER_l= 'l';
constexpr MkKeySym LETTER_m= 'm';
constexpr MkKeySym LETTER_n= 'n';
constexpr MkKeySym LETTER_o= 'o';
constexpr MkKeySym LETTER_p= 'p';
constexpr MkKeySym LETTER_q= 'q';
constexpr MkKeySym LETTER_r= 'r';
constexpr MkKeySym LETTER_s= 's';
constexpr MkKeySym LETTER_t= 't';
constexpr MkKeySym LETTER_u= 'u';
constexpr MkKeySym LETTER_v= 'v';
constexpr MkKeySym LETTER_w= 'w';
constexpr MkKeySym LETTER_x= 'x';
constexpr MkKeySym LETTER_y= 'y';
constexpr MkKeySym LETTER_z= 'z';

constexpr MkKeySym PRINTSCREEN= MK_SCANCODE_TO_KEYCODE(70);
constexpr MkKeySym SCROLLLOCK= MK_SCANCODE_TO_KEYCODE(71);
constexpr MkKeySym PAUSE= MK_SCANCODE_TO_KEYCODE(72);
constexpr MkKeySym INSERT= MK_SCANCODE_TO_KEYCODE(73);
constexpr MkKeySym HOME= MK_SCANCODE_TO_KEYCODE(74);
constexpr MkKeySym PAGEUP= MK_SCANCODE_TO_KEYCODE(75);
constexpr MkKeySym DELETE_KEYCODE= '\177';
constexpr MkKeySym END= MK_SCANCODE_TO_KEYCODE(77);
constexpr MkKeySym PAGEDOWN= MK_SCANCODE_TO_KEYCODE(78);
constexpr MkKeySym RIGHT= MK_SCANCODE_TO_KEYCODE(79);
constexpr MkKeySym LEFT= MK_SCANCODE_TO_KEYCODE(80);
constexpr MkKeySym DOWN= MK_SCANCODE_TO_KEYCODE(81);
constexpr MkKeySym UP= MK_SCANCODE_TO_KEYCODE(82);

constexpr MkKeySym F1= MK_SCANCODE_TO_KEYCODE(58);
constexpr MkKeySym F2= MK_SCANCODE_TO_KEYCODE(59);
constexpr MkKeySym F3= MK_SCANCODE_TO_KEYCODE(60);
constexpr MkKeySym F4= MK_SCANCODE_TO_KEYCODE(61);
constexpr MkKeySym F5= MK_SCANCODE_TO_KEYCODE(62);
constexpr MkKeySym F6= MK_SCANCODE_TO_KEYCODE(63);
constexpr MkKeySym F7= MK_SCANCODE_TO_KEYCODE(64);
constexpr MkKeySym F8= MK_SCANCODE_TO_KEYCODE(65);
constexpr MkKeySym F9= MK_SCANCODE_TO_KEYCODE(66);
constexpr MkKeySym F10= MK_SCANCODE_TO_KEYCODE(67);
constexpr MkKeySym F11= MK_SCANCODE_TO_KEYCODE(68);
constexpr MkKeySym F12= MK_SCANCODE_TO_KEYCODE(69);

constexpr MkKeySym LEFT_CTRL= MK_SCANCODE_TO_KEYCODE(224);
constexpr MkKeySym LEFT_SHIFT= MK_SCANCODE_TO_KEYCODE(225);
constexpr MkKeySym LEFT_ALT= MK_SCANCODE_TO_KEYCODE(226); /**< alt, option */
constexpr MkKeySym LEFT_GUI= MK_SCANCODE_TO_KEYCODE(227); /**< windows, command (apple), meta */
constexpr MkKeySym RIGHT_CTRL= MK_SCANCODE_TO_KEYCODE(228);
constexpr MkKeySym RIGHT_SHIFT= MK_SCANCODE_TO_KEYCODE(229);
constexpr MkKeySym RIGHT_ALT= MK_SCANCODE_TO_KEYCODE(230); /**< alt gr, option */
constexpr MkKeySym RIGHT_GUI= MK_SCANCODE_TO_KEYCODE(231); /**< windows, command (apple), meta */
}; // namespace MkKey

// Normalized modifier flags for key events: left/right variants fold
// together and lock keys are stripped
namespace MkKeyMod
{
constexpr uint16_t NONE= 0x0000;
constexpr uint16_t SHIFT= 0x0001;
constexpr uint16_t CTRL= 0x0002;
constexpr uint16_t ALT= 0x0004;
constexpr uint16_t GUI= 0x0008;
constexpr uint16_t ANY= 0xFFFF;
}; // namespace MkKeyMod

namespace MkMouseButton
{
constexpr int UNKNOWN= 0;

constexpr int LEFT= 1;
constexpr int MIDDLE= 2;
constexpr int RIGHT= 3;
}; // namespace MkMouseButton

enum class eMkWindowEventType
{
	Quit,
	KeyDown,
	KeyUp,
	TextInput,
	MouseWheel,
	MouseButtonDown,
	MouseButtonUp,
	MouseMotion,
	WindowEvent,
	Other
};

enum class eMkWindowEventID
{
	Shown,
	Hidden,
	SizeChanged,
	Enter,
	Leave,
	FocusGained,
	FocusLost,
	Minimized,
	Maximized,
	Restored,
	Close,
	Unknown
};

class MkWindowEvent
{
public:
	virtual ~MkWindowEvent() {}

	virtual eMkWindowEventType getEventType() const= 0;
	virtual eMkWindowEventID getWindowEventID() const= 0;
	// The id of the OS window the event belongs to, or 0 for an event with no window
	// (a quit). A listener that also owns child windows tells their events apart by it.
	virtual uint32_t getWindowId() const= 0;
	virtual MkKeySym getKeySym() const= 0;
	virtual uint16_t getKeyMod() const= 0; // MkKeyMod flags held during a key event
	virtual int getKeyRepeat() const= 0;
	virtual const char* getText() const= 0; // UTF-8 text committed by a TextInput event
	virtual int getMouseWheelScrollAmount() const= 0;
	virtual int getMouseWheelScrollAmountX() const= 0;
	virtual int getMouseButton() const= 0;
	virtual int getMouseClickCount() const= 0; // 1 for a single click, 2 for a double click, ...
	// Mouse position in window client pixels, top-down from the upper left corner
	virtual int getMouseX() const= 0;
	virtual int getMouseY() const= 0;
	virtual int getMouseMotionXRel() const= 0;
	virtual int getMouseMotionYRel() const= 0;
	virtual int getWindowData1() const= 0;
	virtual int getWindowData2() const= 0;
	virtual const void* getInternalWindowEvent() const= 0;
};
