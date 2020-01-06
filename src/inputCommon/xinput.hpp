
#ifndef XINPUT_HPP
#define XINPUT_HPP

#include <iostream>
#include <Windows.h>
#include <Xinput.h>
#include "inputCommon/commonInput.h"

class xinput : commonInput
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