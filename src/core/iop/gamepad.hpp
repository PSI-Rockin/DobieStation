#ifndef GAMEPAD_HPP
#define GAMEPAD_HPP
#include <cstdint>
#include <fstream>
#include "core/serialize.hpp"

enum class PAD_BUTTON
{
    SELECT,
    L3,
    R3,
    START,
    UP,
    RIGHT,
    DOWN,
    LEFT,
    L2,
    R2,
    L1,
    R1,
    TRIANGLE,
    CIRCLE,
    CROSS,
    SQUARE
};

enum class JOYSTICK
{
    RIGHT,
    LEFT
};

enum class JOYSTICK_AXIS
{
    X,
    Y
};

enum PAD_MODE
{
    DIGITAL = 0x41,
    ANALOG = 0x73,
    DS2_NATIVE = 0x79
};

class Gamepad
{
    private:
        uint8_t command_buffer[25];
        uint8_t rumble_values[8];
        uint8_t mode_lock;
        uint16_t buttons;
        uint8_t joysticks[2][2];
        uint8_t button_pressure[16];
        uint8_t command;
        int command_length;
        int data_count;

        uint8_t mask[2];
        static uint8_t mask_mode[7];
        const static uint8_t vref_param[7];
        const static uint8_t config_exit[7];
        const static uint8_t set_mode[7];
        const static uint8_t query_model_DS2[7];
        const static uint8_t query_model_DS1[7];
        const static uint8_t query_act[2][7];
        const static uint8_t query_comb[7];
        const static uint8_t query_mode[7];
        const static uint8_t native_mode[7];

        PAD_MODE pad_mode;
        bool config_mode;

        void reset_vibrate();
        void set_result(const uint8_t* result);
    public:
        Gamepad();

        void reset();
        void press_button(PAD_BUTTON button);
        void release_button(PAD_BUTTON button);
        void update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val);

        uint8_t start_transfer(uint8_t value);
        uint8_t write_SIO(uint8_t value);

        void do_state(StateSerializer &state);
};

#endif // GAMEPAD_HPP
