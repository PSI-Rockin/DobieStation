#ifndef INPUTEVENT_H
#define INUTEVENT_H

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

class CommonInput
{
public:
	virtual void initalize() = 0;
	virtual void poll(int playerNumber) = 0;
	virtual void sendInput() = 0;
	virtual bool press(const uint16_t button) = 0;
};

#endif