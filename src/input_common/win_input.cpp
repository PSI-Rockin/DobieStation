#include "win_input.hpp"

/******************************************************* 
* i in this case is player number (Player 1, 2, 3 etc) 
* if the data structure style makes more sense to me 
* then i'll end up using playerNumer
*******************************************************/

bool WinInput::reset()
{
	ZeroMemory(&isConnected, sizeof(state));

	isConnected = XInputGetState(0, &state);

	if (isConnected == ERROR_SUCCESS) // Controller is connected
	{
		playerNumber = 0;
		std::cout << "Xbox Controller Detected: " << playerNumber << std::endl;
		return true;
	}

	else
	{
		std::cout << "No xbox controller Detected: " << std::endl;
		return false;
	}
}

inputEvent WinInput::poll()
{
	event = {};

	DWORD controller;

	uint16_t currentButton;
	ZeroMemory(&state, sizeof(state));

	controller = XInputGetState(0, &state);

	float leftX = fmaxf(-1, (float)state.Gamepad.sThumbLX / 32767);
	event.lStickXAxis = leftX;

	float leftY = fmaxf(-1, (float)state.Gamepad.sThumbLY / 32767);
	event.lStickYAxis = leftY;

	float rightX = fmaxf((float)state.Gamepad.sThumbRX / 32767, -1);
	event.rStickXAxis = rightX;

	float rightY = fmaxf(-1, (float)state.Gamepad.sThumbRY / 32767);
	event.rStickYAxis = rightY;


	//std::cout << "Left X Axis: " << event.lStickXAxis << std::endl;
	//std::cout << "Left Y Axis: " << event.lStickYAxis << std::endl;

	// If max pressure or not pressed

		event.input[virtualController::CIRCLE] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
		event.input[virtualController::CROSS] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
		event.input[virtualController::SQUARE] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
		event.input[virtualController::TRIANGLE] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);


		event.input[virtualController::START] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
		event.input[virtualController::SELECT] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);

		event.input[virtualController::UP] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
		event.input[virtualController::DOWN] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		event.input[virtualController::LEFT] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		event.input[virtualController::RIGHT] = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

	return event;
}
