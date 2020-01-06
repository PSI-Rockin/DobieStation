
#ifndef XINPUT_HPP
#define XINPUT_HPP

#include <iostream>
#include <Windows.h>
#include <Xinput.h>
#include "common_input.h"

class xinput : common_input
{

private:
	DWORD isConnected; // Is she connected captian?
	XINPUT_STATE state; // General GamePad State
	inputEvent event;
public:

	void initalize();
	void sendInput();


};



#endif 