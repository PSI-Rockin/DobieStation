#include "win_input.hpp"

/******************************************************* 
* i in this case is player number (Player 1, 2, 3 etc) 
* if the data structure style makes more sense to me 
* then i'll end up using playerNumer
*******************************************************/

bool WinInput::reset()
{
	ZeroMemory(&is_connected, sizeof(state));

	is_connected = XInputGetState(0, &state);

	if (is_connected == ERROR_SUCCESS) // Controller is connected
	{
		player_number = 0;
		std::cout << "Xbox Controller Detected: " << player_number << std::endl;
		return true;
	}

	else
	{
		std::cout << "No xbox controller Detected: " << std::endl;
		return false;
	}
}


int WinInput::getEvent(int i)
{
	return XInputGetState(0, &state);
}


PAD_DATA WinInput::poll()
{
	DWORD controller;

	uint16_t current_button;
	ZeroMemory(&state, sizeof(state));

	controller = XInputGetState(0, &state);

	float leftX = fmaxf(-1, (float)state.Gamepad.sThumbLX / 32767);
	pad_data.lStickXAxis = leftX;

	float leftY = fmaxf(-1, (float)state.Gamepad.sThumbLY / 32767);
	pad_data.lStickYAxis = leftY;

	float rightX = fmaxf((float)state.Gamepad.sThumbRX / 32767, -1);
	pad_data.rStickXAxis = rightX;

	float rightY = fmaxf(-1, (float)state.Gamepad.sThumbRY / 32767);
	pad_data.rStickYAxis = rightY;

	float lTrigger = (float)state.Gamepad.bLeftTrigger;
	pad_data.lTriggerAxis = lTrigger;

	float rTrigger =  (float)state.Gamepad.bRightTrigger;
	pad_data.rTriggerAxis = rTrigger;

	//std::cout << "Left X Axis: " << pad_data.lStickXAxis << std::endl;
	//std::cout << "Left Y Axis: " << pad_data.lStickYAxis << std::endl;

	//std::cout << "Left L Trigger: " << (float)state.Gamepad.bLeftTrigger << std::endl;
	//std::cout << "Left R Trigger: " << (float)state.Gamepad.bRightTrigger << std::endl;


	// If max pressure or not pressed

		pad_data.input[BUTTONS::CIRCLE] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
		pad_data.input[BUTTONS::CROSS] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
		pad_data.input[BUTTONS::SQUARE] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
		pad_data.input[BUTTONS::TRIANGLE] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);


		pad_data.input[BUTTONS::START] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
		pad_data.input[BUTTONS::SELECT] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);

		pad_data.input[BUTTONS::UP] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
		pad_data.input[BUTTONS::DOWN] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		pad_data.input[BUTTONS::LEFT] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		pad_data.input[BUTTONS::RIGHT] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
		pad_data.input[BUTTONS::R3] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
		pad_data.input[BUTTONS::L3] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
		pad_data.input[BUTTONS::R1] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
		pad_data.input[BUTTONS::L1] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);


	return pad_data;
}
