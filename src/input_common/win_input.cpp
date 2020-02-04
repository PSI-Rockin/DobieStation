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
	DWORD button;

	uint16_t currentButton;

	ZeroMemory(&state, sizeof(state));

	button = XInputGetState(0, &state);

	if (float regX = fmaxf(-1, (float)state.Gamepad.sThumbLX / 32767))
	{
		event.lStickXAxis = (regX * (128 + 128));

	}


	if (float regY = fmaxf(-1, (float)state.Gamepad.sThumbLY / 32767))
	{
		event.lStickYAxis = (regY * (128 + 128));

	}



	std::cout << "Left X Axis: " << event.lStickXAxis << std::endl;
	std::cout << "Left Y Axis: " << event.lStickYAxis << std::endl;

	// If max pressure or not pressed


	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A)
	{
		std::cout << "A is pressed" << std::endl;
		printf("Like a CHAD");
		event.input[virtualController::CROSS] = state.Gamepad.wButtons & XINPUT_GAMEPAD_A ? 255 : 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
	{
		std::cout << "B is pressed" << std::endl;
		event.input[virtualController::CIRCLE] = state.Gamepad.wButtons & XINPUT_GAMEPAD_B ? 255 : 0;
	}
		
	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_X)
	{
		std::cout << "X is pressed" << std::endl;
		event.input[virtualController::SQUARE] = state.Gamepad.wButtons & button & XINPUT_GAMEPAD_X ? 255 : 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
	{
		std::cout << "Y is pressed" << std::endl;
		event.input[virtualController::TRIANGLE] = state.Gamepad.wButtons & XINPUT_GAMEPAD_Y ? 255 : 0;
	}
	
	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_START)	
	{			
		std::cout << "Start is pressed" << std::endl;
		event.input[virtualController::START] = state.Gamepad.wButtons & XINPUT_GAMEPAD_START ? 255 : 0;
	}


	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
	{
		std::cout << "DPAD UP" << std::endl;
		event.input[virtualController::DPAD_UP] = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP ? 255: 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
	{
		event.input[virtualController::DPAD_DOWN] = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN ? 255 : 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
	{
		event.input[virtualController::DPAD_LEFT] = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT ? 255 : 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
	{
		event.input[virtualController::DPAD_RIGHT] = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 255 : 0;
	}

	return event;
}