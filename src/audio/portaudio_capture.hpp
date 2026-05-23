#pragma once
#include "src/audio/i_audio_capture.hpp"
#include "src/audio/audio_buffer.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

// Forward declare PortAudio types to avoid header dependency here
struct PaUtilRingBuffer;

namespace vim {

class PortAudioCapture : public IAudioCapture {
public:
    PortAudioCapture();
    ~PortAudioCapture() override;

    std::vector<AudioDeviceInfo> enumerate_devices() override;
    bool select_device(const std::string& device_id) override;
    bool start(int sample_rate, int channels,
               AudioCallback callback, void* user_data) override;
    void stop() override;
    bool is_running() const override;
    const char* backend_name() const override { return "PortAudio"; }

    // Expose the ring buffer for direct access (used by ASR thread)
    AudioRingBuffer& ring_buffer() { return ring_buffer_; }
    int channels() const { return channels_; }
    int sample_rate() const { return sample_rate_; }

private:
    AudioRingBuffer ring_buffer_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool pa_initialized_ = false;
    AudioCallback user_callback_ = nullptr;
    void* user_data_ = nullptr;
    std::string selected_device_id_;
    int sample_rate_ = 16000;
    int channels_ = 1;
    std::atomic<bool> running_{false};
};

} // namespace vim
