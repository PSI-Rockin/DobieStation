#include "common_input.hpp"


bool CommonInput::initalizeAPI(DeviceAPI api)
{
	currentAPI = api;
	//initalize thread in here
}

void CommonInput::initalize()
{
	// create thread here.

	input = new std::thread(update, 1); 

}

void update()
{
	// lock thread and poll for events
}