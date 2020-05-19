#ifndef WININPUT_HPP
#define WININPUT_HPP

#include <iostream>
#include <vector.h>
#include <Windows.h>
#include <Xinput.h>
#include "common_input.hpp"
#include <map>

#pragma comment(lib, "Xinput9_1_0")

class WinInput : public CommonInput
{

private:
	int player_number;
	DWORD is_connected; // Is she connected captian?
	bool connected;
	XINPUT_STATE state; // General GamePad State
	PAD_DATA pad_data;
	BYTE trigger;

	BUTTONS control;

	std::map <BUTTONS, DWORD> button_map = {
	{BUTTONS::SELECT, XINPUT_GAMEPAD_BACK}, {BUTTONS::L3, XINPUT_GAMEPAD_LEFT_THUMB},
	{BUTTONS::R3, XINPUT_GAMEPAD_RIGHT_THUMB}, {BUTTONS::START, XINPUT_GAMEPAD_START},
	{BUTTONS::UP, XINPUT_GAMEPAD_DPAD_UP}, {BUTTONS::DOWN, XINPUT_GAMEPAD_DPAD_DOWN},
	{BUTTONS::LEFT, XINPUT_GAMEPAD_DPAD_LEFT}, {BUTTONS::RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT},
	{BUTTONS::L1, XINPUT_GAMEPAD_LEFT_SHOULDER}, {BUTTONS::R1, XINPUT_GAMEPAD_RIGHT_SHOULDER},
	{BUTTONS::TRIANGLE, XINPUT_GAMEPAD_Y },{BUTTONS::CIRCLE,XINPUT_GAMEPAD_B},
	{BUTTONS::CROSS, XINPUT_GAMEPAD_A}, {BUTTONS::SQUARE, XINPUT_GAMEPAD_X}
	};


public:
	bool reset();
	PAD_DATA poll();
	int getEvent(int i);
	std::vector<XINPUT_STATE> get_xinput_devices();

};

#endif
