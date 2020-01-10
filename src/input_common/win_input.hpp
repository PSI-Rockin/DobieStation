
#ifndef WININPUT_HPP
#define WININPUT_HPP

#include <iostream>
#include <Windows.h>
#include <Xinput.h>
#include "common_input.hpp"

#pragma comment(lib, "Xinput9_1_0")

class WinInput : public CommonInput
{

private:
	int playerNumber;
	DWORD isConnected; // Is she connected captian?
	bool connected;
	XINPUT_STATE state; // General GamePad State
	inputEvent event;

public:
	bool initalizeAPI();
	void update();
	bool press(const uint16_t button);
};

#endif 