#include "win_input.hpp"

/******************************************************* 
* i in this case is player number (Player 1, 2, 3 etc) 
* if the data structure style makes more sense to me 
* then i'll end up using playerNumer
*******************************************************/
void WinInput::initalize()
{
	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) 
	{
		ZeroMemory(&isConnected, sizeof(state));

		isConnected = XInputGetState(i, &state);

		if (isConnected == ERROR_SUCCESS) // Controller is connected
		{
			connected = true;
			std::cout << "Xbox Controller Detected!" << std::endl;
		}

		else
		{
			connected = false;
			std::cout << "Ripparoini and cheese" << std::endl;
		}
	}
}


bool WinInput::press(uint16_t button)
{
	return (state.Gamepad.wButtons & button) != 0;
}


void WinInput::poll(int playerNumber)
{

}

void WinInput::sendInput()
{
	initalize(); // This is for constant polling if the controller lives

	uint16_t button;

	if (connected)
	{
			switch (press(button))
			{
			case XINPUT_GAMEPAD_A:
				event.construct(virtualController::CROSS, playerNumber, 0, 0, 0, 0);
				break;

			case XINPUT_GAMEPAD_B:
				event.construct(virtualController::CIRCLE, playerNumber, 0, 0, 0, 0);
				break;

			case XINPUT_GAMEPAD_X:
				event.construct(virtualController::SQUARE, playerNumber, 0, 0, 0, 0);
				break;

			case XINPUT_GAMEPAD_Y:
				event.construct(virtualController::TRIANGLE, playerNumber, 0, 0, 0, 0);
				break;

			case XINPUT_GAMEPAD_START:
				event.construct(virtualController::START, playerNumber, 0, 0, 0, 0);
				break;
			}
	}


}