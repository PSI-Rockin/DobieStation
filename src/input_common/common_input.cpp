#include "common_input.hpp"


bool CommonInput::initalizeAPI()
{
	//initalize thread in here
}

void CommonInput::initalize()
{
	// create thread here.
	#ifdef __linux__
	linux_input = new LinuxInput();
	linux_input->initalizeAPI();
	#endif
		
	#ifdef WIN32
	win_input = new WinInput();
	win_input->initalizeAPI();
	#endif


}

void CommonInput::update()
{
	initalize();

while (true)
{
	win_input->sendInput();
}

}