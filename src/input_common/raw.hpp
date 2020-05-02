#include <iostream>
#include <Windows.h>
#include "common_input.hpp"
#include <WinUser.h>


class RawInput //: public CommonInput
{
	
private:
	RAWINPUTDEVICE device[2];
	UINT data;
public:
	bool reset();
	PAD_DATA poll();


};