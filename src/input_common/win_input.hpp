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

	std::map <virtualController, DWORD> xinputMAP = {
	{virtualController::CROSS, XINPUT_GAMEPAD_A}, {virtualController::CIRCLE,XINPUT_GAMEPAD_B},
	{virtualController::SQUARE, XINPUT_GAMEPAD_X}, {virtualController::TRIANGLE, XINPUT_GAMEPAD_Y },
	{virtualController::START, XINPUT_GAMEPAD_START}, { virtualController::SELECT, XINPUT_GAMEPAD_BACK},
	{virtualController::DPAD_UP, XINPUT_GAMEPAD_DPAD_UP}, {virtualController::DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN},
	{ virtualController::DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT}, { virtualController::DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT}
	};


public:
	bool reset();
	inputEvent poll();
};


#endif 