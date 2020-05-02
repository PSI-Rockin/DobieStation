#include "raw.hpp"

bool Raw::reset()
{
	//This is Keyboard Input
	device[0].usUsagePage = 0x01;
	device[0].usUsage = 0x06;
	device[0].dwFlags = RIDEV_NOLEGACY;   // adds HID keyboard and also ignores legacy keyboard messages
	device[0].hwndTarget = 0; //(HWND)wsi->surface;
	RegisterRawInputDevices(&device[0], 0, sizeof(device[0]));

	// This is GamePad Input
	/*device[1].usUsagePage = 0x01;
	device[1].usUsage = 0x05;
	device[1].dwFlags = 0;
	device[1].hwndTarget = NULL; // Follows keyboard focus?
	RegisterRawInputDevices(&device[1], 1, sizeof(device[1]));*/

	std::cout << "Raw Dev registered" << std::endl;


	return true;
}

PAD_DATA Raw::poll()
{
	std::cout << "Poll" << std::endl;
	switch (data)
	{
	case WM_INPUT:
		RAWINPUT in;

		unsigned int size = sizeof(RAWINPUT);

		std::cout << "WM_INPUT POLL" << std::endl;


		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &in, &size, sizeof(RAWINPUTHEADER)) > 0) 
			{

				uint32_t msg = in.data.keyboard.VKey;			


				std::cout << "vKey Code: " << unsigned(msg) << std::endl;

				switch (msg)
				{
					
				}




			}

			break;
		
	}
	return pad_data;

}

int Raw::getEvent(int i)
{
	return 0;
}
