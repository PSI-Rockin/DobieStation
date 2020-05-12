#ifndef INPUTMANAGER_HPP
#define INPUTMANAGER_HPP

#ifdef __linux__
#include "linux_input.hpp"
#include <memory>
#elif WIN32
#include "win_input.hpp"
#elif __APPLE__
#include "mac_input.hpp"
#endif

class InputManager
{

private:
	std::unique_ptr<CommonInput> input;

public:
	void reset();
	PAD_DATA poll();
	int getEvent(int i); // For configuration not DobieStation ingame
<<<<<<< HEAD

	#ifdef __linux__
    	std::vector<evdev_controller *> get_interesting_devices();

	#elif WIN32
	    std::vector<xinput_controller> get_interesting_devices();
	#endif
=======
>>>>>>> a77f949... Work on configureator, not sure how to extend that to windows stuff
};

#endif