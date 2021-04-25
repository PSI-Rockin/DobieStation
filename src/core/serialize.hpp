#ifndef __SERIALIZE_H_
#define __SERIALIZE_H_
#include <cstring>
#include <fstream>
#include <list>
#include <memory>
#include <queue>
#include <vector>

class StateSerializer
{
  public:
    enum class Mode
    {
        Read,
        Write,
    };

    StateSerializer(std::fstream& stream, Mode mode) : m_stream(stream), m_mode(mode)
    {
    }

    //~StateSerializer();

    Mode GetMode() { return m_mode; };

    template <typename T>
    void Do(T* ptr)
    {
        if (m_mode == Mode::Read)
        {
            m_stream.read((char*)ptr, sizeof(T));
        }
        else
        {
            m_stream.write((char*)ptr, sizeof(T));
        }
    }

    template <typename T>
    void DoBytes(T* ptr, size_t len)
    {
        if (m_mode == Mode::Read)
        {
            m_stream.read((char*)ptr, len);
        }
        else
        {
            m_stream.write((char*)ptr, len);
        }
    }

    template <typename T>
    void Do(std::queue<T>* data)
    {
        uint32_t length = static_cast<uint32_t>(data->size());
        Do(&length);
        if (m_mode == Mode::Read)
        {
            // Doesn't reset
            for (uint32_t i = 0; i < length; i++)
            {
                T value;
                Do(&value);
                data->push(value);
            }
        }
        else
        {
            for (uint32_t i = 0; i < length; i++)
                Do(&data[i]);
        }
    }

    template <typename T>
    void Do(std::list<T>* data)
    {
        uint32_t length = static_cast<uint32_t>(data->size());
        Do(&length);
        if (m_mode == Mode::Read)
        {
            data->clear();
            for (uint32_t i = 0; i < length; i++)
            {
                T value;
                Do(&value);
                data->push_back(value);
            }
        }
        else
        {
            for (auto it = data->begin(); it != data->end(); it++)
            {
                T value = *it;
                Do((&value));
            }
        }
    }

    template <typename T>
    void Do(std::vector<T>* data)
    {
        uint32_t length = static_cast<uint32_t>(data->size());
        Do(&length);
        if (m_mode == Mode::Read)
            data->resize(length);
        DoArray(data->data(), data->size());
    }

    template <typename T>
    void DoArray(T* ptr, size_t count)
    {
        for (size_t i = 0; i < count; i++)
        {
            Do(&ptr[i]);
        }
    }

    bool DoMarker(const char* marker)
    {
        auto len = strlen(marker);
        auto fileVal = std::make_unique<char[]>(len);
        memcpy(fileVal.get(), marker, len);
        DoArray(fileVal.get(), len);

        if (m_mode == Mode::Write || memcmp(marker, fileVal.get(), len) == 0)
            return true;

        fprintf(stderr, "Savestate marker mismatch: found '%s' expected '%s'\n", fileVal.get(), marker);

        return false;
    }

  private:
    Mode m_mode;
    std::fstream& m_stream;
};

#endif // __SERIALIZE_H_
