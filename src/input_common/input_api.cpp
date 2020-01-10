#include "input_api.hpp"

void InputApi::initalize()
{
	// create thread here.
#ifdef __linux__
	linux_input = new LinuxInput();
	linux_input->initalizeAPI();

#elif WIN32
	win_input = new WinInput();
	win_input->initalizeAPI();
#endif
}

void InputApi::update()
{
#ifdef __linux__
	linux_input->sendInput();
#elif WIN32
	win_input->sendInput();
#endif
}