#ifndef INPUTEVENT_H
#define INUTEVENT_H

#include <thread>
#include <mutex>

#ifdef __linux__ 
#include "linux_input.hpp"

#elif WIN32
#include "win_input.hpp"
#endif



enum virtualController{CROSS, TRIANGLE, CIRCLE, SQUARE, START, SELECT, R1, R2, R3, L1, L2, L3};

enum deviceType{CONTROLLER, MOUSE, KEYBOARD, USB}; // USB is a subset of controllers like Guitar Hero stuff and for Pandubz future Eye Toy PR. Right pandubz ... you didnt forget, right?

struct inputEvent
{
	virtualController input;
	int playerNumber;

	int lStickXAxis;
	int lStickYAxis;

	int rStickXAxis;
	int rStickYAxis;

	void construct(virtualController eventI, int playerNum, int lXAxis, int lYAxis, int rXAxis, int rYAxis)
	{
		input = eventI;
		lStickXAxis = lXAxis;
		lStickYAxis = lYAxis;
		rStickXAxis = rXAxis;
		rStickYAxis = rYAxis;
		playerNumber = playerNum;
	}
};

enum DeviceAPI {
	NO_API = 0,
	WM = 2, // Windows Keyboard
	RAW = 3, // It's FUCKING RAW!!!
	XINPUT = 4,
	DS4_LIB = 5,// Potential custom option for later
	LNX_KEYBOARD = 16, // Xorg / Wayland input specific or we do some real shitz!
	EVDEV = 17,
};



class CommonInput
{

protected:
	std::thread *input;
	std::mutex inputMutex;
	DeviceAPI currentAPI;
	int playerNumber;

	#ifdef __linux__
	LinuxInput *linux_input;

	#elif WIN32
	WinInput *win_input;
	#endif


public:
	virtual bool initalizeAPI();
	virtual void sendInput();
	void initalize();
	void update();


};

#endif