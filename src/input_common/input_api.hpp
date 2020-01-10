#ifndef INPUTAPI_HPP
#define INPUTAPI_HPP

#ifdef __linux__ 
#include "linux_input.hpp"

#elif WIN32
#include "win_input.hpp"
#endif

class InputApi
{

private:
#ifdef __linux__
	LinuxInput* linux_input;

#elif WIN32
	WinInput* win_input;
#endif

public:
	void initalize();
	void update();
};



#endif