#ifndef GSDUMP_HPP
#define GSDUMP_HPP

#include <fstream>
#include <iostream>
#include <sstream>
#include "../core/gsthread.hpp"

// HEADER INFORMATION
// 4 bytes - Magic number
// 1 bytes - Version major
// 1 bytes - Version minor
// 10 bytes - Reserved 
namespace GSDump
{
    // Haiku for future nerds
    //
    // Subtract from this number
    // and update the VERSION const
    // if you add something
    static const int PADDING_SIZE = 10;
    static const uint8_t VERSION_MAJ = 0;
    static const uint8_t VERSION_MIN = 0;
    static const uint16_t VERSION = (VERSION_MAJ << 8) | VERSION_MIN;

    class Replayer
    {
    public:
        enum ADVANCE
        {
            DRAW,
            FRAME,
            FRAME_DRAW
        };

        Replayer();
        ~Replayer();

        bool open(const char* file_name);
        bool is_open() const;
        bool breakpoint_hit();
        bool is_draw(const GSMessage& message);
        bool is_frame(const GSMessage& message);
        bool draw();

        int current_draw() const { return current_draw_count; };
        int current_frame() const { return current_frame_count; };

        void next();
        void close();
        void set_breakpoint(ADVANCE advance, int location, bool is_goto);

        std::ifstream& stream() { return file; };

        const GSMessage& message() { return current_message; };
    private:
        int current_draw_count;
        int current_frame_count;
        int current_goto;

        bool skip_draws;

        std::ifstream file;

        GSMessage current_message;
        ADVANCE current_advance;

        bool check_version();

        void set_location(int relative_location, int location, bool is_goto);
    };

    class Recorder
    {
    public:
        Recorder();
        ~Recorder();

        void toggle_recording();
        void record_message(const GSMessage& message);
        
        bool is_recording() const { return recording; };

        std::ofstream* stream() { return &file; };
        
    private:
        bool recording;

        std::ofstream file;

        void write_header();
    };
}
#endif