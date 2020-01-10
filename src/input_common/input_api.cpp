
#include "input_api.hpp"

void InputApi::initalize()
{
	// create thread here.
#ifdef __linux__
	input = std::make_unique<new LinuxInput>();
	input->initalizeAPI();

#elif WIN32
	input = std::make_unique<WinInput>();
	input->initalizeAPI();
#endif
}

void InputApi::update()
{
#ifdef __linux__
	input->sendInput();
#elif WIN32
	input->sendInput();
#endif
}
