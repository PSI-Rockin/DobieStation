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

	ZeroMemory(&button, sizeof(state));

	button = XInputGetState(0, &state);

	float lstickLx = state.Gamepad.sThumbLX;
	float lstickLy = state.Gamepad.sThumbLY;

	float magnitude = sqrt(lstickLy * lstickLx + lstickLy * lstickLy);

	event.lStickXAxis = lstickLx / magnitude;
	event.lStickYAxis = lstickLy / magnitude;

	std::cout << "Left X Axis: " << event.lStickXAxis << std::endl;
	std::cout << "Left Y Axis: " << event.lStickYAxis << std::endl;


	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A)
	{
		std::cout << "A is pressed" << std::endl;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
	{
		std::cout << "B is pressed" << std::endl;
	}
		
	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_X)
	{
		std::cout << "X is pressed" << std::endl;
	}

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
	{
		std::cout << "Y is pressed" << std::endl;
	}
	
	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_START)	
	{
			std::cout << "Start is pressed" << std::endl;	
	}

	return event;
}