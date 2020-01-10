#include "win_input.hpp"

/******************************************************* 
* i in this case is player number (Player 1, 2, 3 etc) 
* if the data structure style makes more sense to me 
* then i'll end up using playerNumer
*******************************************************/
bool WinInput::initalizeAPI()
{
	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) 
	{
		ZeroMemory(&isConnected, sizeof(state));

		isConnected = XInputGetState(i, &state);

		if (isConnected == ERROR_SUCCESS) // Controller is connected
		{			
			playerNumber = i;
			std::cout << "Xbox Controller Detected: " << playerNumber << std::endl;
			return true;
		}

		else
		{
			std::cout << "No xbox controller Detected: " << std::endl;
			return false;
		}
	}
}


bool WinInput::press(uint16_t button)
{
	return (state.Gamepad.wButtons & button) != 0;
}


void WinInput::sendInput()
{
	DWORD button;

	for (int i = 0; i < 4; i++)
	{
		if (initalizeAPI())
		{
			switch (press(state.Gamepad.wButtons))
			{
			case XINPUT_GAMEPAD_A:
				event.construct(virtualController::CROSS, playerNumber, 0, 0, 0, 0);
				std::cout << "A is pressed" << std::endl;
				break;

			case XINPUT_GAMEPAD_B:
				event.construct(virtualController::CIRCLE, playerNumber, 0, 0, 0, 0);
				std::cout << "B is pressed" << std::endl;
				break;

			case XINPUT_GAMEPAD_X:
				event.construct(virtualController::SQUARE, playerNumber, 0, 0, 0, 0);
				std::cout << "X is pressed" << std::endl;
				break;

			case XINPUT_GAMEPAD_Y:
				event.construct(virtualController::TRIANGLE, playerNumber, 0, 0, 0, 0);
				std::cout << "Y is pressed" << std::endl;
				break;

			case XINPUT_GAMEPAD_START:
				event.construct(virtualController::START, playerNumber, 0, 0, 0, 0);
				std::cout << "Start is pressed" << std::endl;
				break;
			}
		}
	}

}