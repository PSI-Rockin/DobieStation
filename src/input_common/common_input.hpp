#ifndef COMMONINPUT_H
#define COMMONINPUT_H


enum virtualController{CROSS, TRIANGLE, CIRCLE, SQUARE, START, SELECT, R1, R2, R3, L1, L2, L3};

enum deviceType{CONTROLLER, MOUSE, KEYBOARD, USB}; // USB is a subset of controllers like Guitar Hero stuff and for Pandubz future Eye Toy PR. Right pandubz ... you didnt forget, right?

struct inputEvent
{
	virtualController input;
	int playerNumber;

	float lStickXAxis;
	float lStickYAxis;

	float rStickXAxis;
	float rStickYAxis;

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
	DeviceAPI currentAPI;
	int playerNumber;

public:
	virtual bool initalizeAPI() = 0;
	virtual void poll() = 0;
};



#endif