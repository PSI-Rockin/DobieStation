
#ifndef XINPUT_HPP
#define XINPUT_HPP

#include <iostream>
#include <Windows.h>
#include <Xinput.h>
#include "common_input.hpp"

#pragma comment (lib, "xinput.lib") 

class WinInput : CommonInput
{

private:
	int playerNumber;
	DWORD isConnected; // Is she connected captian?
	bool connected;
	XINPUT_STATE state; // General GamePad State
	inputEvent event;
public:
	void initalize();
	void sendInput(const WORD button);
	bool press(const WORD button);
};

#endif 