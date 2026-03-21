#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>

namespace wave {

// Captures system audio via WASAPI loopback and runs FFT to produce
// frequency-domain spectrum data for the visualizer.
class AudioCapture {
public:
    static constexpr int FFT_SIZE = 2048;
    static constexpr int SPECTRUM_BINS = 64; // output bins (we downsample to 32 for display)

    AudioCapture();
    ~AudioCapture();

    bool start();
    void stop();
    bool running() const { return m_running; }

    // Get current spectrum — thread-safe, lock-free read
    // Values are 0.0–1.0 magnitude per bin, logarithmically scaled
    void getSpectrum(float* out, int count) const;

private:
    void captureThread();
    void processBuffer(const float* data, int frames, int channels);

    // FFT (Cooley-Tukey radix-2 in-place, real input)
    void fft(float* real, float* imag, int n);

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    // Double-buffered spectrum output
    mutable std::mutex m_specMutex;
    std::array<float, SPECTRUM_BINS> m_spectrum{};

    // FFT working buffers
    std::array<float, FFT_SIZE> m_fftReal{};
    std::array<float, FFT_SIZE> m_fftImag{};
    std::array<float, FFT_SIZE> m_window{}; // Hann window, precomputed
};

} // namespace wave
