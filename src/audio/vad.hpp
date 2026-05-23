#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vim {

// Simple energy-based Voice Activity Detector.
// Thread-safe for a single calling thread.
class EnergyVAD {
public:
    struct Config {
        float silence_threshold_db = -40.0f;   // below this = silence
        float speech_threshold_db  = -30.0f;   // above this = speech
        int   silence_timeout_ms   = 800;       // ms of silence before end-of-utterance
        int   speech_confirm_ms    = 100;       // ms of speech before triggering start
        int   sample_rate          = 16000;
    };

    enum class State { Silence, Speech };

    explicit EnergyVAD(const Config& cfg) : cfg_(cfg) {}

    // Process a chunk of float samples [-1.0, +1.0], return current state.
    State process(const float* samples, std::size_t count) {
        if (count == 0) return state_;

        float rms = compute_rms(samples, count);
        float db = 20.0f * std::log10(rms + 1e-10f);

        int frame_ms = static_cast<int>(count * 1000 / cfg_.sample_rate);

        if (state_ == State::Silence) {
            if (db > cfg_.speech_threshold_db) {
                speech_frames_ms_ += frame_ms;
                silence_frames_ms_ = 0;
                if (speech_frames_ms_ >= cfg_.speech_confirm_ms) {
                    state_ = State::Speech;
                    speech_frames_ms_ = 0;
                }
            } else {
                speech_frames_ms_ = 0;
            }
        } else { // Speech
            if (db < cfg_.silence_threshold_db) {
                silence_frames_ms_ += frame_ms;
                if (silence_frames_ms_ >= cfg_.silence_timeout_ms) {
                    state_ = State::Silence;
                    silence_frames_ms_ = 0;
                    return State::Silence; // transition happened this frame
                }
            } else {
                silence_frames_ms_ = 0;
            }
        }

        return state_;
    }

    State state() const { return state_; }
    bool is_speech() const { return state_ == State::Speech; }

    void set_silence_threshold(float db) { cfg_.silence_threshold_db = db; }
    void set_speech_threshold(float db)  { cfg_.speech_threshold_db  = db; }
    void set_silence_timeout(int ms)     { cfg_.silence_timeout_ms   = ms; }

    void reset() {
        state_ = State::Silence;
        speech_frames_ms_ = 0;
        silence_frames_ms_ = 0;
    }

private:
    static float compute_rms(const float* samples, std::size_t count) {
        float sum = 0.0f;
        for (std::size_t i = 0; i < count; ++i)
            sum += samples[i] * samples[i];
        return std::sqrt(sum / static_cast<float>(count));
    }

    Config cfg_;
    State state_ = State::Silence;
    int speech_frames_ms_ = 0;
    int silence_frames_ms_ = 0;
};

} // namespace vim
