#ifndef LINUXINPUT_H
#define LINUXINPUT_H
#include <fcntl.h>
#include <vector>
#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unordered_map>
#include <dirent.h>
#include "common_input.hpp"
#include <map>

// These Key Codes brought to you by, Rpcs3! https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Input/evdev_joystick_handler.h

// Unique button names for the config files and our pad settings dialog
	/*const std::unordered_map<uint32_t, std::string> button_list =
	{
		// Xbox One S Controller returns some buttons as key when connected through bluetooth
		{ KEY_BACK            , "Back Key"    },
		{ KEY_HOMEPAGE        , "Homepage Key"},
		// Wii/Wii U Controller drivers use the following few keys
		{ KEY_UP              , "Up Key"      },
		{ KEY_LEFT            , "Left Key"    },
		{ KEY_RIGHT           , "Right Key"   },
		{ KEY_DOWN            , "Down Key"    },
		{ KEY_NEXT            , "Next Key"    },
		{ KEY_PREVIOUS        , "Previous Key"},
		//{ BTN_MISC            , "Misc"        }, same as BTN_0
		{ BTN_0               , "0"           },
		{ BTN_1               , "1"           },
		{ BTN_2               , "2"           },
		{ BTN_3               , "3"           },
		{ BTN_4               , "4"           },
		{ BTN_5               , "5"           },
		{ BTN_6               , "6"           },
		{ BTN_7               , "7"           },
		{ BTN_8               , "8"           },
		{ BTN_9               , "9"           },
		{ 0x10a               , "0x10a"       },
		{ 0x10b               , "0x10b"       },
		{ 0x10c               , "0x10c"       },
		{ 0x10d               , "0x10d"       },
		{ 0x10e               , "0x10e"       },
		{ 0x10f               , "0x10f"       },
		//{ BTN_MOUSE           , "Mouse"       }, same as BTN_LEFT
		{ BTN_LEFT            , "Left"        },
		{ BTN_RIGHT           , "Right"       },
		{ BTN_MIDDLE          , "Middle"      },
		{ BTN_SIDE            , "Side"        },
		{ BTN_EXTRA           , "Extra"       },
		{ BTN_FORWARD         , "Forward"     },
		{ BTN_BACK            , "Back"        },
		{ BTN_TASK            , "Task"        },
		{ 0x118               , "0x118"       },
		{ 0x119               , "0x119"       },
		{ 0x11a               , "0x11a"       },
		{ 0x11b               , "0x11b"       },
		{ 0x11c               , "0x11c"       },
		{ 0x11d               , "0x11d"       },
		{ 0x11e               , "0x11e"       },
		{ 0x11f               , "0x11f"       },
		{ BTN_JOYSTICK        , "Joystick"    },
		{ BTN_TRIGGER         , "Trigger"     },
		{ BTN_THUMB           , "Thumb"       },
		{ BTN_THUMB2          , "Thumb 2"     },
		{ BTN_TOP             , "Top"         },
		{ BTN_TOP2            , "Top 2"       },
		{ BTN_PINKIE          , "Pinkie"      },
		{ BTN_BASE            , "Base"        },
		{ BTN_BASE2           , "Base 2"      },
		{ BTN_BASE3           , "Base 3"      },
		{ BTN_BASE4           , "Base 4"      },
		{ BTN_BASE5           , "Base 5"      },
		{ BTN_BASE6           , "Base 6"      },
		{ 0x12c               , "0x12c"       },
		{ 0x12d               , "0x12d"       },
		{ 0x12e               , "0x12e"       },
		{ BTN_DEAD            , "Dead"        },
		//{ BTN_GAMEPAD         , "Gamepad"     }, same as BTN_A
		//{ BTN_SOUTH           , "South"       }, same as BTN_A
		{ BTN_A               , "A"           },
		//{ BTN_EAST            , "South"       }, same as BTN_B
		{ BTN_B               , "B"           },
		{ BTN_C               , "C"           },
		//{ BTN_NORTH           , "North"       }, same as BTN_X
		{ BTN_X               , "X"           },
		//{ BTN_WEST            , "West"        }, same as BTN_Y
		{ BTN_Y               , "Y"           },
		{ BTN_Z               , "Z"           },
		{ BTN_TL              , "TL"          },
		{ BTN_TR              , "TR"          },
		{ BTN_TL2             , "TL 2"        },
		{ BTN_TR2             , "TR 2"        },
		{ BTN_SELECT          , "Select"      },
		{ BTN_START           , "Start"       },
		{ BTN_MODE            , "Mode"        },
		{ BTN_THUMBL          , "Thumb L"     },
		{ BTN_THUMBR          , "Thumb R"     },
		{ 0x13f               , "0x13f"       },
		//{ BTN_DIGI            , "Digi"        }, same as BTN_TOOL_PEN
		{ BTN_TOOL_PEN        , "Pen"         },
		{ BTN_TOOL_RUBBER     , "Rubber"      },
		{ BTN_TOOL_BRUSH      , "Brush"       },
		{ BTN_TOOL_PENCIL     , "Pencil"      },
		{ BTN_TOOL_AIRBRUSH   , "Airbrush"    },
		{ BTN_TOOL_FINGER     , "Finger"      },
		{ BTN_TOOL_MOUSE      , "Mouse"       },
		{ BTN_TOOL_LENS       , "Lense"       },
		{ BTN_TOOL_QUINTTAP   , "Quinttap"    },
		{ 0x149               , "0x149"       },
		{ BTN_TOUCH           , "Touch"       },
		{ BTN_STYLUS          , "Stylus"      },
		{ BTN_STYLUS2         , "Stylus 2"    },
		{ BTN_TOOL_DOUBLETAP  , "Doubletap"   },
		{ BTN_TOOL_TRIPLETAP  , "Tripletap"   },
		{ BTN_TOOL_QUADTAP    , "Quadtap"     },
		//{ BTN_WHEEL           , "Wheel"       }, same as BTN_GEAR_DOWN
		{ BTN_GEAR_DOWN       , "Gear Up"     },
		{ BTN_GEAR_UP         , "Gear Down"   },
		{ BTN_DPAD_UP         , "D-Pad Up"    },
		{ BTN_DPAD_DOWN       , "D-Pad Down"  },
		{ BTN_DPAD_LEFT       , "D-Pad Left"  },
		{ BTN_DPAD_RIGHT      , "D-Pad Right" },
		{ BTN_TRIGGER_HAPPY   , "Happy"       },
		{ BTN_TRIGGER_HAPPY1  , "Happy 1"     },
		{ BTN_TRIGGER_HAPPY2  , "Happy 2"     },
		{ BTN_TRIGGER_HAPPY3  , "Happy 3"     },
		{ BTN_TRIGGER_HAPPY4  , "Happy 4"     },
		{ BTN_TRIGGER_HAPPY5  , "Happy 5"     },
		{ BTN_TRIGGER_HAPPY6  , "Happy 6"     },
		{ BTN_TRIGGER_HAPPY7  , "Happy 7"     },
		{ BTN_TRIGGER_HAPPY8  , "Happy 8"     },
		{ BTN_TRIGGER_HAPPY9  , "Happy 9"     },
		{ BTN_TRIGGER_HAPPY10 , "Happy 10"    },
		{ BTN_TRIGGER_HAPPY11 , "Happy 11"    },
		{ BTN_TRIGGER_HAPPY12 , "Happy 12"    },
		{ BTN_TRIGGER_HAPPY13 , "Happy 13"    },
		{ BTN_TRIGGER_HAPPY14 , "Happy 14"    },
		{ BTN_TRIGGER_HAPPY15 , "Happy 15"    },
		{ BTN_TRIGGER_HAPPY16 , "Happy 16"    },
		{ BTN_TRIGGER_HAPPY17 , "Happy 17"    },
		{ BTN_TRIGGER_HAPPY18 , "Happy 18"    },
		{ BTN_TRIGGER_HAPPY19 , "Happy 19"    },
		{ BTN_TRIGGER_HAPPY20 , "Happy 20"    },
		{ BTN_TRIGGER_HAPPY21 , "Happy 21"    },
		{ BTN_TRIGGER_HAPPY22 , "Happy 22"    },
		{ BTN_TRIGGER_HAPPY23 , "Happy 23"    },
		{ BTN_TRIGGER_HAPPY24 , "Happy 24"    },
		{ BTN_TRIGGER_HAPPY25 , "Happy 25"    },
		{ BTN_TRIGGER_HAPPY26 , "Happy 26"    },
		{ BTN_TRIGGER_HAPPY27 , "Happy 27"    },
		{ BTN_TRIGGER_HAPPY28 , "Happy 28"    },
		{ BTN_TRIGGER_HAPPY29 , "Happy 29"    },
		{ BTN_TRIGGER_HAPPY30 , "Happy 30"    },
		{ BTN_TRIGGER_HAPPY31 , "Happy 31"    },
		{ BTN_TRIGGER_HAPPY32 , "Happy 32"    },
		{ BTN_TRIGGER_HAPPY33 , "Happy 33"    },
		{ BTN_TRIGGER_HAPPY34 , "Happy 34"    },
		{ BTN_TRIGGER_HAPPY35 , "Happy 35"    },
		{ BTN_TRIGGER_HAPPY36 , "Happy 36"    },
		{ BTN_TRIGGER_HAPPY37 , "Happy 37"    },
		{ BTN_TRIGGER_HAPPY38 , "Happy 38"    },
		{ BTN_TRIGGER_HAPPY39 , "Happy 39"    },
		{ BTN_TRIGGER_HAPPY40 , "Happy 40"    }
	};

	// Unique positive axis names for the config files and our pad settings dialog
	const std::unordered_map<uint32_t, std::string> axis_list =
	{
		{ ABS_X              , "LX+"          },
		{ ABS_Y              , "LY+"          },
		{ ABS_Z              , "LZ+"          },
		{ ABS_RX             , "RX+"          },
		{ ABS_RY             , "RY+"          },
		{ ABS_RZ             , "RZ+"          },
		{ ABS_THROTTLE       , "Throttle+"    },
		{ ABS_RUDDER         , "Rudder+"      },
		{ ABS_WHEEL          , "Wheel+"       },
		{ ABS_GAS            , "Gas+"         },
		{ ABS_BRAKE          , "Brake+"       },
		{ ABS_HAT0X          , "Hat0 X+"      },
		{ ABS_HAT0Y          , "Hat0 Y+"      },
		{ ABS_HAT1X          , "Hat1 X+"      },
		{ ABS_HAT1Y          , "Hat1 Y+"      },
		{ ABS_HAT2X          , "Hat2 X+"      },
		{ ABS_HAT2Y          , "Hat2 Y+"      },
		{ ABS_HAT3X          , "Hat3 X+"      },
		{ ABS_HAT3Y          , "Hat3 Y+"      },
		{ ABS_PRESSURE       , "Pressure+"    },
		{ ABS_DISTANCE       , "Distance+"    },
		{ ABS_TILT_X         , "Tilt X+"      },
		{ ABS_TILT_Y         , "Tilt Y+"      },
		{ ABS_TOOL_WIDTH     , "Width+"       },
		{ ABS_VOLUME         , "Volume+"      },
		{ ABS_MISC           , "Misc+"        },
		{ ABS_MT_SLOT        , "Slot+"        },
		{ ABS_MT_TOUCH_MAJOR , "MT TMaj+"     },
		{ ABS_MT_TOUCH_MINOR , "MT TMin+"     },
		{ ABS_MT_WIDTH_MAJOR , "MT WMaj+"     },
		{ ABS_MT_WIDTH_MINOR , "MT WMin+"     },
		{ ABS_MT_ORIENTATION , "MT Orient+"   },
		{ ABS_MT_POSITION_X  , "MT PosX+"     },
		{ ABS_MT_POSITION_Y  , "MT PosY+"     },
		{ ABS_MT_TOOL_TYPE   , "MT TType+"    },
		{ ABS_MT_BLOB_ID     , "MT Blob ID+"  },
		{ ABS_MT_TRACKING_ID , "MT Track ID+" },
		{ ABS_MT_PRESSURE    , "MT Pressure+" },
		{ ABS_MT_DISTANCE    , "MT Distance+" },
		{ ABS_MT_TOOL_X      , "MT Tool X+"   },
		{ ABS_MT_TOOL_Y      , "MT Tool Y+"   },
	};

	// Unique negative axis names for the config files and our pad settings dialog
	const std::unordered_map<uint32_t, std::string> rev_axis_list =
	{
		{ ABS_X              , "LX-"          },
		{ ABS_Y              , "LY-"          },
		{ ABS_Z              , "LZ-"          },
		{ ABS_RX             , "RX-"          },
		{ ABS_RY             , "RY-"          },
		{ ABS_RZ             , "RZ-"          },
		{ ABS_THROTTLE       , "Throttle-"    },
		{ ABS_RUDDER         , "Rudder-"      },
		{ ABS_WHEEL          , "Wheel-"       },
		{ ABS_GAS            , "Gas-"         },
		{ ABS_BRAKE          , "Brake-"       },
		{ ABS_HAT0X          , "Hat0 X-"      },
		{ ABS_HAT0Y          , "Hat0 Y-"      },
		{ ABS_HAT1X          , "Hat1 X-"      },
		{ ABS_HAT1Y          , "Hat1 Y-"      },
		{ ABS_HAT2X          , "Hat2 X-"      },
		{ ABS_HAT2Y          , "Hat2 Y-"      },
		{ ABS_HAT3X          , "Hat3 X-"      },
		{ ABS_HAT3Y          , "Hat3 Y-"      },
		{ ABS_PRESSURE       , "Pressure-"    },
		{ ABS_DISTANCE       , "Distance-"    },
		{ ABS_TILT_X         , "Tilt X-"      },
		{ ABS_TILT_Y         , "Tilt Y-"      },
		{ ABS_TOOL_WIDTH     , "Width-"       },
		{ ABS_VOLUME         , "Volume-"      },
		{ ABS_MISC           , "Misc-"        },
		{ ABS_MT_SLOT        , "Slot-"        },
		{ ABS_MT_TOUCH_MAJOR , "MT TMaj-"     },
		{ ABS_MT_TOUCH_MINOR , "MT TMin-"     },
		{ ABS_MT_WIDTH_MAJOR , "MT WMaj-"     },
		{ ABS_MT_WIDTH_MINOR , "MT WMin-"     },
		{ ABS_MT_ORIENTATION , "MT Orient-"   },
		{ ABS_MT_POSITION_X  , "MT PosX-"     },
		{ ABS_MT_POSITION_Y  , "MT PosY-"     },
		{ ABS_MT_TOOL_TYPE   , "MT TType-"    },
		{ ABS_MT_BLOB_ID     , "MT Blob ID-"  },
		{ ABS_MT_TRACKING_ID , "MT Track ID-" },
		{ ABS_MT_PRESSURE    , "MT Pressure-" },
		{ ABS_MT_DISTANCE    , "MT Distance-" },
		{ ABS_MT_TOOL_X      , "MT Tool X-"   },
		{ ABS_MT_TOOL_Y      , "MT Tool Y-"   },
};*/

class LinuxInput : public CommonInput
{

private:
     input_event ev;
     evdev_controller *dev;
     PAD_DATA in;

    libevdev *temp_dev;

    DIR *dir;
    struct dirent *files;

    std::string event;

    std::map<uint, BUTTONS> button_map =
        {

            {BTN_TR, BUTTONS::SELECT},
            {BTN_THUMB, BUTTONS::L3},
            {BTN_THUMB2, BUTTONS::R3},
            {BTN_TL, BUTTONS::START},
            {BTN_TRIGGER_HAPPY3, BUTTONS::UP},
            {BTN_DPAD_RIGHT, BUTTONS::RIGHT},
            {BTN_TRIGGER_HAPPY2, BUTTONS::DOWN},
            {BTN_TRIGGER_HAPPY1, BUTTONS::LEFT},
            {BTN_WEST, BUTTONS::L1},
            {BTN_Z, BUTTONS::R1},
            {BTN_NORTH, BUTTONS::TRIANGLE},
            {BTN_EAST, BUTTONS::CIRCLE},
            {BTN_SOUTH, BUTTONS::CROSS},
            {BTN_C, BUTTONS::SQUARE},
            {ABS_Z, BUTTONS::L2},
            {ABS_RZ, BUTTONS::R2}

        };
    std::vector<evdev_controller *> interesting_devices;

    std::string events[27] = {
        "event0", "event1", "event2", "event3", "event4", "event5",
        "event6", "event7", "event8", "event9", "event10", "event11",
        "event12", "event13", "event14", "event15", "event16", "event17", "event18", "event19"
                                                                                     "event20",
        "event21", "event22", "event23", "event24", "event25", "event26", "event27"};

    std::string file_path = "/dev/input/";
    std::string current_path;

    int fd;
    int rc;
    int current_device;

public:
    std::vector<evdev_controller *> get_interesting_devices();
    int getEvent(int i);

    bool reset();
    PAD_DATA poll();
};

#endif
