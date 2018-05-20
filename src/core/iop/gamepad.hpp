#ifndef GAMEPAD_HPP
#define GAMEPAD_HPP
#include <cstdint>

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

class Gamepad
{
    private:
        uint8_t command_buffer[25];
        uint8_t rumble_values[7];
        uint16_t buttons;
        uint8_t command;
        int command_length;
        int data_count;

        const static uint8_t config_exit[7];
        const static uint8_t set_mode[7];
        const static uint8_t query_model[7];
        const static uint8_t query_act[2][7];
        const static uint8_t query_comb[7];
        const static uint8_t query_mode[7];

        uint8_t LED_value;

        bool analog_mode;
        bool config_mode;
        int halfwords_transfer;
    public:
        Gamepad();

        void set_result(const uint8_t* result);

        void reset();
        void press_button(PAD_BUTTON button);
        void release_button(PAD_BUTTON button);

        uint8_t start_transfer(uint8_t value);
        uint8_t write_SIO(uint8_t value);
};

#endif // GAMEPAD_HPP
