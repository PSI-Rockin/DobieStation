#ifndef INPUTMANAGER_HPP
#define INPUTMANAGER_HPP

#ifdef __linux__
#include "linux_input.hpp"
#include <memory>
#elif WIN32
#include "win_input.hpp"
#endif

class InputManager
{

private:
	std::unique_ptr<CommonInput> input;

public:
	void reset();
	PAD_DATA poll();
};

#endif