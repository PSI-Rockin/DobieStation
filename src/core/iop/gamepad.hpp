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
        uint8_t rumble_values[7];
        uint16_t buttons;
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

        uint8_t LED_value;

        PAD_MODE pad_mode;
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
