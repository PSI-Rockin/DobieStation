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

	ZeroMemory(&button, sizeof(state));

	button = XInputGetState(0, &state);



	event.lStickXAxis = fmaxf(-1, (float)state.Gamepad.sThumbLX / 32767);
	event.lStickYAxis = fmaxf(-1, (float)state.Gamepad.sThumbLY / 32767);

	std::cout << "Left X Axis: " << event.lStickXAxis << std::endl;
	std::cout << "Left Y Axis: " << event.lStickYAxis << std::endl;

	// If max pressure or not pressed


	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A)
	{
		std::cout << "A is pressed" << std::endl;
		event.input[virtualController::CROSS] = button & XINPUT_GAMEPAD_A ? 255 : 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
	{
		std::cout << "B is pressed" << std::endl;
		event.input[virtualController::CIRCLE] = button & XINPUT_GAMEPAD_B ? 255 : 0;
	}
		
	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_X)
	{
		std::cout << "X is pressed" << std::endl;
		event.input[virtualController::SQUARE] = button & XINPUT_GAMEPAD_X ? 255 : 0;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
	{
		std::cout << "Y is pressed" << std::endl;
		event.input[virtualController::TRIANGLE] = button & XINPUT_GAMEPAD_Y ? 255 : 0;
	}
	
	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_START)	
	{			
		std::cout << "Start is pressed" << std::endl;
		event.input[virtualController::START] = button & XINPUT_GAMEPAD_START ? 255 : 0;
	}

	ZeroMemory(&button, sizeof(state));


	return event;
}