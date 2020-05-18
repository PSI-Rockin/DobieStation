#include <iostream>
#include <Windows.h>
#include "common_input.hpp"
#include <WinUser.h>
#include "../util/wsi.hpp"

class RawInput //: public CommonInput
{
	
private:
	RAWINPUTDEVICE device[2];
	UINT data;
	PAD_DATA pad_data;
	HWND hWnd; 
	WPARAM wParam;
	LPARAM lParam;
	LRESULT* output;
	Util::WSI* wsi;

public:
	bool reset();
	PAD_DATA poll();


};