#include "input_api.hpp"

void InputManager::reset()
{
	// create thread here.
#ifdef __linux__
	input = std::make_unique<LinuxInput>();
#elif WIN32
	input = std::make_unique<WinInput>();
	#elif __APPLE__
	input = std::make_unique<MacInput>();
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

#ifdef __linux__
	std::vector<evdev_controller*> InputManager::get_interesting_devices()
	{
		return input->get_interesting_devices();
	}

#elif WIN32
	std::vector<XINPUT_STATE> InputManager::get_interesting_devices()
	{
		return input->get_interesting_devices();
	}
#endif