#include "xinput.hpp"

void xinput::initalize()
{
	XINPUT_STATE state;
	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
	{
		ZeroMemory(&isConnected, sizeof(state))

			isConnected = XInputGetState(i, state);

		if (isConnected == ERROR_SUCCESS) // Controller is connected
		{
			// handle basic controller shit in here my dudes
		}

	}
}