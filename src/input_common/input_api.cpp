#include "input_api.hpp"

void InputManager::reset()
{
	// create thread here.
#ifdef __linux__
	input = std::make_unique<LinuxInput>();
#elif WIN32
	input = std::make_unique<WinInput>();
#endif
	input->reset();
}

PAD_DATA InputManager::poll()
{
	return input->poll();
}

int InputManager::getEvent(int i)
{
	return input->getEvent(i);
}
