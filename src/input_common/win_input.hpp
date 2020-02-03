#ifndef WININPUT_HPP
#define WININPUT_HPP

#include <iostream>
#include <Windows.h>
#include <Xinput.h>
#include "common_input.hpp"
#include <map>


#pragma comment(lib, "Xinput9_1_0")

class WinInput : public CommonInput
{

private:
	int playerNumber;
	DWORD isConnected; // Is she connected captian?
	bool connected;
	XINPUT_STATE state; // General GamePad State
	inputEvent event;

	std::map <DWORD, virtualController> xinputMAP = { 
	{XINPUT_GAMEPAD_A, virtualController::CROSS}, {XINPUT_GAMEPAD_B, virtualController::CIRCLE}, 
	{XINPUT_GAMEPAD_X, virtualController::SQUARE}, {XINPUT_GAMEPAD_Y, virtualController::TRIANGLE}, 
	{XINPUT_GAMEPAD_START, virtualController::START}, {XINPUT_GAMEPAD_BACK, virtualController::SELECT}
	};


public:
	bool reset();
	inputEvent poll();
};


#endif 