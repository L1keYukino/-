#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vim {

// Lock-free single-producer single-consumer ring buffer for float samples.
// Producer: audio callback thread. Consumer: ASR processing thread.
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(int capacity_samples)
        : capacity_(static_cast<std::size_t>(capacity_samples) + 1)
        , buf_(capacity_)
        , write_pos_(0)
        , read_pos_(0)
    {}

    // Producer: write interleaved float samples. Returns bytes written.
    std::size_t write(const float* samples, std::size_t count) {
        std::size_t available = write_available();
        std::size_t to_write = (count < available) ? count : available;
        if (to_write == 0) return 0;

        std::size_t w = write_pos_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < to_write; ++i) {
            buf_[w] = samples[i];
            w = (w + 1) % capacity_;
        }
        write_pos_.store(w, std::memory_order_release);
        return to_write;
    }

    // Consumer: read float samples. Returns bytes actually read.
    std::size_t read(float* dst, std::size_t count) {
        std::size_t available = read_available();
        std::size_t to_read = (count < available) ? count : available;
        if (to_read == 0) return 0;

        std::size_t r = read_pos_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < to_read; ++i) {
            dst[i] = buf_[r];
            r = (r + 1) % capacity_;
        }
        read_pos_.store(r, std::memory_order_release);
        return to_read;
    }

    // Consumer: drop samples without copying.
    std::size_t skip(std::size_t count) {
        std::size_t available = read_available();
        std::size_t to_skip = (count < available) ? count : available;
        if (to_skip == 0) return 0;

        std::size_t r = read_pos_.load(std::memory_order_relaxed);
        r = (r + to_skip) % capacity_;
        read_pos_.store(r, std::memory_order_release);
        return to_skip;
    }

    // Consumer: peek at samples without consuming.
    std::size_t peek(float* dst, std::size_t count) const {
        std::size_t available = read_available();
        std::size_t n = (count < available) ? count : available;
        std::size_t r = read_pos_.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < n; ++i) {
            dst[i] = buf_[r];
            r = (r + 1) % capacity_;
        }
        return n;
    }

    std::size_t read_available() const {
        std::size_t w = write_pos_.load(std::memory_order_acquire);
        std::size_t r = read_pos_.load(std::memory_order_acquire);
        if (w >= r) return w - r;
        return capacity_ - r + w;
    }

    std::size_t write_available() const {
        return capacity_ - read_available() - 1;
    }

    int capacity_ms(int sample_rate) const {
        return static_cast<int>((capacity_ * 1000) / static_cast<std::size_t>(sample_rate));
    }

    void reset() {
        write_pos_.store(0, std::memory_order_release);
        read_pos_.store(0, std::memory_order_release);
    }

private:
    std::size_t capacity_;
    std::vector<float> buf_;
    std::atomic<std::size_t> write_pos_;
    std::atomic<std::size_t> read_pos_;
};

} // namespace vim
