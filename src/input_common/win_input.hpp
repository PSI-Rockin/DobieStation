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
	BYTE trigger;

	virtualController control;

	std::map <virtualController, DWORD> xinputMAP = {
	{virtualController::SELECT, XINPUT_GAMEPAD_BACK}, {virtualController::L3, XINPUT_GAMEPAD_LEFT_THUMB},
	{virtualController::R3, XINPUT_GAMEPAD_RIGHT_THUMB}, {virtualController::START, XINPUT_GAMEPAD_START},
	{virtualController::UP, XINPUT_GAMEPAD_DPAD_UP}, {virtualController::DOWN, XINPUT_GAMEPAD_DPAD_DOWN},
	{virtualController::LEFT, XINPUT_GAMEPAD_DPAD_LEFT}, { virtualController::RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT},
	{virtualController::L1, XINPUT_GAMEPAD_LEFT_SHOULDER}, {virtualController::R1, XINPUT_GAMEPAD_RIGHT_SHOULDER},
	{virtualController::TRIANGLE, XINPUT_GAMEPAD_Y },{virtualController::CIRCLE,XINPUT_GAMEPAD_B},
	{virtualController::CROSS, XINPUT_GAMEPAD_A}, {virtualController::SQUARE, XINPUT_GAMEPAD_X}
	};


public:
	bool reset();
	inputEvent poll();
};

#endif
