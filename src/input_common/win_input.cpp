#include "win_input.hpp"

void WinInput::initalize()
{
	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
	{
		ZeroMemory(&isConnected, sizeof(state));

		isConnected = XInputGetState(i, &state);

		if (isConnected == ERROR_SUCCESS) // Controller is connected
		{
			// handle basic controller shit in here my dudes
		}
	}
}

void WinInput::sendInput()
{
	switch (state.Gamepad.wButtons)
	{
	case XINPUT_GAMEPAD_A:
		event = inputEvent::CROSS;
		break;
	
	case XINPUT_GAMEPAD_B:
		event = inputEvent::CIRCLE;
		break;

	case XINPUT_GAMEPAD_X:
		event = inputEvent::SQUARE;
		break;

	case XINPUT_GAMEPAD_Y:
		event = inputEvent::TRIANGLE;
		break;

	case XINPUT_GAMEPAD_START:
		event = inputEvent::START;
		break;
	}
}