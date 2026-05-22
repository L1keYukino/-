#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vim {

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    int max_input_channels = 0;
    int default_sample_rate = 0;
};

using AudioCallback = void (*)(const float* samples, std::size_t frame_count,
                               int sample_rate, void* user_data);

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual std::vector<AudioDeviceInfo> enumerate_devices() = 0;
    virtual bool select_device(const std::string& device_id) = 0;
    virtual bool start(int sample_rate, int channels,
                       AudioCallback callback, void* user_data) = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual const char* backend_name() const = 0;
};

} // namespace vim
