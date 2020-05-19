#ifndef RAWINPUT_HPP
#define RAWINPUT_HPP

#include <iostream>
#include <vector>
#include "common_input.hpp"
#include "../util/wsi.hpp"

class Raw : public CommonInput
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
	int getEvent(int i);

};

#endif