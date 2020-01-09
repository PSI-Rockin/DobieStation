#include "common_input.hpp"


bool CommonInput::initalizeAPI()
{
	//initalize thread in here
}

void CommonInput::initalize()
{
	// create thread here.
	#ifdef __linux__
	linux_input.initalizeAPI();
	#endif
		
	#ifdef _WIN32
	win_input.inializeAPI();
	#endif
}

void CommonInput::update()
{
	initalize();

while (true)
{
	// lock thread and poll for events
	#ifdef __linux__
	linux_input.sendInput();
	#endif

	#ifdef _WIN32
	win_input.sendInput();
	#endif
}

}