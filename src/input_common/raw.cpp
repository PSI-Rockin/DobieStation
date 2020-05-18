#include "raw.hpp"

bool RawInput::reset()
{
	//This is Keyboard Input
	device[0].usUsagePage = 0x01;
	device[0].usUsage = 0x06;
	device[0].dwFlags = RIDEV_NOLEGACY;   // adds HID keyboard and also ignores legacy keyboard messages
	device[0].hwndTarget = (HWND)wsi->surface;
	RegisterRawInputDevices(&device[0], 0, sizeof(device[0]));

	// This is GamePad Input
	device[1].usUsagePage = 0x01;
	device[1].usUsage = 0x05;
	device[1].dwFlags = 0;
	device[1].hwndTarget = NULL; // Follows keyboard focus?
	RegisterRawInputDevices(&device[1], 1, sizeof(device[1]));

	return true;
}

PAD_DATA RawInput::poll()
{

	switch (data)
	{
	case WM_INPUT:
		RAWINPUT in;

		unsigned int size = sizeof(RAWINPUT);


		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &in, &size, sizeof(RAWINPUTHEADER)) > 0) 
			{

				uint32_t msg = in.data.keyboard.VKey;			
			}

			break;
		
	}
	return pad_data;

}