#ifndef INPUTAPI_HPP
#define INPUTAPI_HPP

#include <thread>
#include <mutex>


#ifdef __linux__ 
#include "linux_input.hpp"

#elif WIN32
#include "win_input.hpp"
#endif

class InputApi{

private:
	std::unique_ptr<CommonInput> input;
	std::mutex inputMutex;
	DeviceAPI currentAPI;
	int playerNumber;
public:
	void initalize();
	virtual void update() = 0;
	virtual bool initalizeAPI() = 0;
};

#endif