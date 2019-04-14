#include <assert.h> 
#include <cstring>

#include "gsdump.hpp"

namespace GSDump
{
    Replayer::Replayer()
        : current_advance(FRAME),
        current_draw_count(0),
        current_frame_count(0),
        current_message({}),
        skip_draws(false)
    {
        fprintf(stderr, "[GSDUMP Replay] Init!\n");
    }

    Replayer::~Replayer()
    {
        fprintf(stderr, "[GSDUMP Replay] Shutdown\n");
        file.close();
    }

    bool Replayer::check_version()
    {
        char magic_number[4] = {};
        file.read(magic_number, 4);

        if (strcmp(magic_number, "GSD") != 0)
        {
            fprintf(stderr, "[GSDUMP Replay] Not a valid GSD\n");
            return false;
        }

        uint8_t version_major;
        file.read((char*)&version_major, sizeof(uint8_t));

        if (version_major != VERSION_MAJ)
        {
            fprintf(stderr, "[GSDUMP Replay] Version mismatch\n");
            return false;
        }

        uint8_t version_minor;
        file.read((char*)&version_minor, sizeof(uint8_t));

        fprintf(stderr, "[GSDUMP Replay] Version: %d:%d\n", version_major, version_minor);

        file.ignore(PADDING_SIZE);

        return true;
    }

    bool Replayer::open(const char* name)
    {
        fprintf(stderr, "[GSDUMP Replay] Load file %s\n", name);
        if (file.is_open())
            file.close();

        file.open(name, std::ifstream::binary);

        if (!file.is_open())
            return false;

        //if (!check_version())
        //	return false;

        return true;
    }

    void Replayer::close()
    {
        file.close();
    }

    bool Replayer::is_open() const
    {
        return file.is_open();
    }

    void Replayer::set_breakpoint(ADVANCE advance, int location, bool is_goto)
    {
        // different breakpoints depending on params
        // if it's a GOTO, we want an absolute location
        // otherwise we want a relative one (next frame, next draw)
        current_advance = advance;
        switch (advance)
        {
        case DRAW:
            set_location(current_draw_count, location, is_goto);
            break;
        case FRAME:
        case FRAME_DRAW:
            set_location(current_frame_count, location, is_goto);
            break;
        default:
            assert(0);
        }
        
    }

    void Replayer::set_location(int relative_location, int location, bool is_goto)
    {
        if (is_goto)
            current_goto = location;
        else
            current_goto = relative_location + location;

        skip_draws = is_goto;
    }

    bool Replayer::is_draw(const GSMessage& message)
    {
        bool draw = false;

        switch (message.type)
        {
        case set_xyz_t:
        {
            GSMessagePayload payload = message.payload;
            draw = payload.xyz_payload.drawing_kick;
            break;
        }
        case set_xyzf_t:
        {
            GSMessagePayload payload = message.payload;
            draw = payload.xyzf_payload.drawing_kick;
            break;
        }
        default:
            break;
        }

        return draw;
    }

    bool Replayer::is_frame(const GSMessage& message)
    {
        return message.type == render_crt_t;
    }

    void Replayer::next()
    {
        file.read((char*)&current_message, sizeof(GSMessage));

        if (is_draw(current_message))
            current_draw_count++;

        if (is_frame(current_message))
        {
            // We want to reset the goto location
            // in the event that the goto draw location
            // is higher than the total draw count
            // otherwise we win an infinite loop
            current_goto = 0;
            current_draw_count = 0;
            current_frame_count++;
        }
    }

    bool Replayer::breakpoint_hit()
    {
        bool breakpoint = false;

        switch (current_advance)
        {
        case DRAW:
            breakpoint = is_draw(current_message);
            breakpoint &= (current_draw_count >= current_goto);
            break;
        case FRAME:
        case FRAME_DRAW:
            breakpoint = is_frame(current_message);
            break;
        default:
            assert(0);
        }

        if(breakpoint)
            fprintf(stderr, "[GSDUMP Replay] BREAK\n");

        return breakpoint;
    }

    bool Replayer::draw()
    {
        bool draw = false;

        switch (current_advance)
        {
        case DRAW:
        case FRAME_DRAW:
            draw = is_draw(current_message);
        default:
            break;
        }

        return draw && (!skip_draws | breakpoint_hit());
    }

    Recorder::Recorder()
        : recording(false)
    {
        fprintf(stderr, "[GSDUMP Record] Init!\n");
    }

    Recorder::~Recorder()
    {
        fprintf(stderr, "[GSDUMP Record] Shutdown\n");
        file.close();
    }

    void Recorder::write_header()
    {
        file.write("GSD", 4);
        file.write((const char*)&VERSION_MAJ, sizeof(uint8_t));
        file.write((const char*)&VERSION_MIN, sizeof(uint8_t));

        uint8_t padding[PADDING_SIZE] = {};
        file.write((const char*)&padding, PADDING_SIZE);
    }

    void Recorder::record_message(const GSMessage& message)
    {
        if (recording && file.is_open())
            file.write((const char*)&message, sizeof(message));
    }

    void Recorder::toggle_recording()
    {
        recording = !recording;

        if (recording)
        {
            fprintf(stderr, "[GSDUMP Record] START\n");
            file.open("gsdump.gsd", std::ios::binary);

            if (!file.is_open())
            {
                fprintf(stderr, "[GSDUMP Record] Couldn't open output file\n");
                fprintf(stderr, "[GSDUMP Record] Output will be ignored!\n");

                return;
            }

            write_header();
        }
        else
        {
            fprintf(stderr, "[GSDUMP Record] END\n");
            file.close();
        }
    }
}