#include "input_api.hpp"

void InputManager::initalize()
{
	// create thread here.
#ifdef __linux__
	input = std::make_unique<LinuxInput>();

#elif WIN32
	input = std::make_unique<WinInput>();
#endif
	
	input->initalizeAPI();
}

void InputManager::poll()
{
	input->poll();
}
