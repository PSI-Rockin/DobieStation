#include "win_input.hpp"

/******************************************************* 
* i in this case is player number (Player 1, 2, 3 etc) 
* if the data structure style makes more sense to me 
* then i'll end up using playerNumer
*******************************************************/

bool WinInput::reset()
{
	ZeroMemory(&is_connected, sizeof(state.dev));

	device_api = DEVICE_API::XINPUT;

	is_connected = XInputGetState(0, &state.dev);

	if (is_connected == ERROR_SUCCESS) // Controller is connected
	{
		state.player_number = 0;
		std::cout << "Xbox Controller Detected: " << player_number << std::endl;
		interesting_devices.push_back(state);
		return true;
	}

	else
	{
		std::cout << "No xbox controller Detected: " << std::endl;
		return false;
	}
}

std::vector<xinput_controller> WinInput::get_interesting_devices()
{
	return interesting_devices;
}




int WinInput::getEvent(int i)
{
	XInputGetState(i, &interesting_devices[i].dev);
	return interesting_devices[i].dev.Gamepad.wButtons;
}


PAD_DATA WinInput::poll()
{
	DWORD controller;

	uint16_t current_button;
	ZeroMemory(&interesting_devices[0].dev, sizeof(interesting_devices[0].dev));

	controller = XInputGetState(0, &interesting_devices[0].dev);

	float leftX = fmaxf(-1, (float)interesting_devices[0].dev.Gamepad.sThumbLX / 32767);
	pad_data.lStickXAxis = leftX;

	float leftY = fmaxf(-1, (float)interesting_devices[0].dev.Gamepad.sThumbLY / 32767);
	pad_data.lStickYAxis = leftY;

	float rightX = fmaxf((float)interesting_devices[0].dev.Gamepad.sThumbRX / 32767, -1);
	pad_data.rStickXAxis = rightX;

	float rightY = fmaxf(-1, (float)interesting_devices[0].dev.Gamepad.sThumbRY / 32767);
	pad_data.rStickYAxis = rightY;

	float lTrigger = (float)interesting_devices[0].dev.Gamepad.bLeftTrigger;
	pad_data.lTriggerAxis = lTrigger / 255;

	float rTrigger =  (float)interesting_devices[0].dev.Gamepad.bRightTrigger;
	pad_data.rTriggerAxis = rTrigger / 255;

	//std::cout << "Left X Axis: " << pad_data.lStickXAxis << std::endl;
	//std::cout << "Left Y Axis: " << pad_data.lStickYAxis << std::endl;

	//std::cout << "Left L Trigger: " << (float)state.Gamepad.bLeftTrigger << std::endl;
	//std::cout << "Left R Trigger: " << (float)state.Gamepad.bRightTrigger << std::endl;


	// If max pressure or not pressed

		pad_data.button[BUTTONS::CIRCLE].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_B);
		pad_data.button[BUTTONS::CROSS].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_A);
		pad_data.button[BUTTONS::SQUARE].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_X);
		pad_data.button[BUTTONS::TRIANGLE].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_Y);


		pad_data.button[BUTTONS::START].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_START);
		pad_data.button[BUTTONS::SELECT].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);

		pad_data.button[BUTTONS::UP].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
		pad_data.button[BUTTONS::DOWN].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		pad_data.button[BUTTONS::LEFT].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		pad_data.button[BUTTONS::RIGHT].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
		pad_data.button[BUTTONS::R3].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
		pad_data.button[BUTTONS::L3].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
		pad_data.button[BUTTONS::R1].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
		pad_data.button[BUTTONS::L1].input = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);

		for (int i = 0; i < CONTROLLER_BUTTON_MAX; i++)
		{
			if (pad_data.button[i].input)
			{
				pad_data.button[i].pressure = 255;
			}

			else
			{
				pad_data.button[i].pressure = 0;
			}

		}



		//std::cout << "Pressure: " << pad_data.pressure << std::endl;

	return pad_data;
}
