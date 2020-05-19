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

		pad_data.input[BUTTONS::CIRCLE] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_B);
		pad_data.input[BUTTONS::CROSS] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_A);
		pad_data.input[BUTTONS::SQUARE] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_X);
		pad_data.input[BUTTONS::TRIANGLE] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_Y);


		pad_data.input[BUTTONS::START] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_START);
		pad_data.input[BUTTONS::SELECT] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);

		pad_data.input[BUTTONS::UP] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
		pad_data.input[BUTTONS::DOWN] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		pad_data.input[BUTTONS::LEFT] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		pad_data.input[BUTTONS::RIGHT] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
		pad_data.input[BUTTONS::R3] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
		pad_data.input[BUTTONS::L3] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
		pad_data.input[BUTTONS::R1] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
		pad_data.input[BUTTONS::L1] = (interesting_devices[0].dev.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);



	return pad_data;
}
