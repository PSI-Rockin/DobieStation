#ifndef INPUTAPI_HPP
#define INPUTAPI_HPP

#ifdef __linux__ 
#include "linux_input.hpp"

#elif WIN32
#include "win_input.hpp"
#endif

class InputApi
{

private:
	std::unique_ptr<CommonInput> input;
public:
	void initalize();
	void update();
};



#endif