
#include "input_api.hpp"

void InputApi::initalize()
{
	// create thread here.
#ifdef __linux__
	input = std::make_unique<LinuxInput>();
	input->initalizeAPI();

#elif WIN32
	input = std::make_unique<WinInput>();
	input->initalizeAPI();
#endif
}

void InputApi::update()
{
	input->sendInput();
}
