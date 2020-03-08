#ifndef COMMONINPUT_H
#define COMMONINPUT_H

enum BUTTONS
{
	SELECT,
	L3,
	R3,
	START,
	UP,
	RIGHT,
	DOWN,
	LEFT,
	L2,
	R2,
	L1,
	R1,
	TRIANGLE,
	CIRCLE,
	CROSS,
	SQUARE,
	CONTROLLER_BUTTON_MAX
};

enum DEVICE_TYPE
{
	CONTROLLER,
	MOUSE,
	KEYBOARD,
	USB
}; // USB is a subset of controllers like Guitar Hero stuff and for Pandubz future Eye Toy PR. Right pandubz ... you didnt forget, right?

struct PAD_DATA
{
	bool input[CONTROLLER_BUTTON_MAX];
	int player_number;

	float lStickXAxis;
	float lStickYAxis;

	float rStickXAxis;
	float rStickYAxis;
};

enum DEVICE_API
{
	NO_API = 0,
	WM = 2,	 // Windows Keyboard
	RAW = 3, // It's FUCKING RAW!!!
	XINPUT = 4,
	DS4_LIB = 5,	   // Potential custom option for later
	LNX_KEYBOARD = 16, // Xorg / Wayland input specific or we do some real shitz!
	EVDEV = 17,
};

class CommonInput
{

protected:
	DEVICE_API device_api;
	int player_number;

public:
	virtual bool reset() = 0;
	virtual PAD_DATA poll() = 0;
};

#endif