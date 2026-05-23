#include "src/audio/portaudio_capture.hpp"
#include <cstdio>
#include <stdexcept>

#ifdef VIM_HAS_PORTAUDIO
  #include <portaudio.h>
#endif

namespace vim {

// ─── PIMPL ──────────────────────────────────────────────

struct PortAudioCapture::Impl {
#ifdef VIM_HAS_PORTAUDIO
    PaStream* stream = nullptr;
#endif
};

// ─── Constructor / Destructor ───────────────────────────

PortAudioCapture::PortAudioCapture()
    : ring_buffer_(44100 * 10)  // 10 seconds at 44.1kHz mono
    , impl_(std::make_unique<Impl>())
{
}

PortAudioCapture::~PortAudioCapture() {
    stop();
}

// ─── Device enumeration ─────────────────────────────────

std::vector<AudioDeviceInfo> PortAudioCapture::enumerate_devices() {
    std::vector<AudioDeviceInfo> result;
#ifdef VIM_HAS_PORTAUDIO
    Pa_Initialize();
    int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info->maxInputChannels > 0) {
            AudioDeviceInfo dev;
            dev.id = std::to_string(i);
            dev.name = info->name;
            dev.max_input_channels = info->maxInputChannels;
            dev.default_sample_rate = static_cast<int>(info->defaultSampleRate);
            result.push_back(dev);
        }
    }
    Pa_Terminate();
#else
    // Stub: return a default device so the API doesn't break
    AudioDeviceInfo dev;
    dev.id = "0";
    dev.name = "Default Input (PortAudio not linked)";
    dev.max_input_channels = 1;
    dev.default_sample_rate = 16000;
    result.push_back(dev);
#endif
    return result;
}

bool PortAudioCapture::select_device(const std::string& device_id) {
    selected_device_id_ = device_id;
    return true;
}

// ─── PortAudio callback ─────────────────────────────────

#ifdef VIM_HAS_PORTAUDIO
static int pa_callback(const void* input, void* /*output*/,
                       unsigned long frame_count,
                       const PaStreamCallbackTimeInfo* /*time*/,
                       PaStreamCallbackFlags flags,
                       void* user_data)
{
    auto* self = static_cast<PortAudioCapture*>(user_data);
    const float* samples = static_cast<const float*>(input);

    if (flags & paInputUnderflow) {
        std::fprintf(stderr, "[PortAudio] input underflow\n");
    }
    if (flags & paInputOverflow) {
        std::fprintf(stderr, "[PortAudio] input overflow\n");
    }

    if (samples && frame_count > 0) {
        std::size_t total = frame_count * static_cast<std::size_t>(self->channels());
        std::size_t written = self->ring_buffer().write(samples, total);
        static int call_count = 0;
        if (++call_count <= 3) {
            std::fprintf(stderr, "[PortAudio] callback #%d: %lu frames, %zu/%zu samples written\n",
                         call_count, frame_count, written, total);
        }
    }

    return paContinue;
}
#endif

// ─── Start / Stop ───────────────────────────────────────

bool PortAudioCapture::start(int sample_rate, int channels,
                              AudioCallback callback, void* user_data) {
    if (running_.load()) return true;

    sample_rate_ = sample_rate;
    channels_ = channels;
    user_callback_ = callback;
    user_data_ = user_data;

#ifdef VIM_HAS_PORTAUDIO
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::fprintf(stderr, "[PortAudio] Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
        return false;
    }

    PaStreamParameters params;
    params.device = selected_device_id_.empty()
        ? Pa_GetDefaultInputDevice()
        : std::stoi(selected_device_id_);
    params.channelCount = channels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&impl_->stream, &params, nullptr,
                        static_cast<double>(sample_rate),
                        0, // frames per buffer (0 = automatic)
                        paNoFlag, pa_callback, this);
    if (err != paNoError) {
        std::fprintf(stderr, "[PortAudio] Pa_OpenStream failed: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(impl_->stream);
    if (err != paNoError) {
        std::fprintf(stderr, "[PortAudio] Pa_StartStream failed: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(impl_->stream);
        Pa_Terminate();
        return false;
    }
#endif

    running_.store(true);
    return true;
}

void PortAudioCapture::stop() {
    if (!running_.exchange(false)) return;

#ifdef VIM_HAS_PORTAUDIO
    if (impl_->stream) {
        Pa_StopStream(impl_->stream);
        Pa_CloseStream(impl_->stream);
        impl_->stream = nullptr;
    }
    Pa_Terminate();
#endif

    // ring buffer intentionally NOT reset — caller drains it after stop()
}

bool PortAudioCapture::is_running() const {
    return running_.load();
}

} // namespace vim
