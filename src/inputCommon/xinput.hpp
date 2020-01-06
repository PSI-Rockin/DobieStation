
#ifndef XINPUT_HPP
#define XINPUT_HPP

#include <iostream>
#include <Windows.h>

#include "../core/iop/gamepad.hpp"

class xinput
{

private:

	DWORD isConnected; // Is she connected captian?
	Gamepad virualController;

public:

	void initalize();



};



#endif 