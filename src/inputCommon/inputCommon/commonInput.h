#pragma once

enum inputEvent{CROSS, TRIANGLE, CIRCLE, SQUARE, START, SELECT, R1, R2, R3, L1, L2, L3};

class commonInput
{
	virtual void initalize();
	virtual void sendInput();
};