#ifndef GAMEPAD_HPP
#define GAMEPAD_HPP
#include <cstdint>

enum class GAMEPAD_COMMAND
{
    NONE,
    WAITING,
    READ_DATA,
    CONFIG,
    QUERY_MODEL,
    QUERY_ACT,
    QUERY_COMB,
    QUERY_MODE,
    ENABLE_VIBRATION
};

class Gamepad
{
    private:
        int data_sent;
        int command_length;
        int config_count;
        GAMEPAD_COMMAND command;

        void get_command(uint8_t value);
        uint8_t read_data(uint8_t value);
        uint8_t config(uint8_t value);
        uint8_t query_model(uint8_t value);
        uint8_t query_act(uint8_t value);
        uint8_t query_comb(uint8_t value);
        uint8_t query_mode(uint8_t value);
        uint8_t enable_vibration(uint8_t value);
    public:
        Gamepad();

        void reset();

        uint8_t write_SIO(uint8_t value);
};

#endif // GAMEPAD_HPP
