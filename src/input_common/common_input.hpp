#ifndef COMMONINPUT_H
#define COMMONINPUT_H

#ifdef __linux__
#include <libevdev-1.0/libevdev/libevdev.h>

#elif WIN32
#include <Windows.h>
#include <Xinput.h>
#elif __APPLE__
#include <GCController>
#endif



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


#ifdef __linux__

struct evdev_controller
{

    std::string controller_name;
    libevdev *dev;
};
#endif


struct PAD_DATA
{
	bool input[CONTROLLER_BUTTON_MAX];
	int player_number;

	float lStickXAxis;
	float lStickYAxis;

	float rStickXAxis;
	float rStickYAxis;

	float lTriggerAxis;
	float rTriggerAxis;

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
	virtual int getEvent(int i) = 0;
	
	#ifdef __linux__
    virtual std::vector<evdev_controller *> get_interesting_devices() = 0;

	#elif WIN32
	   virtual std::vector<XINPUT_STATE> get_interesting_devices() = 0;
	#endif

};

#endif