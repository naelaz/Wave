#pragma once

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace wave {

// Pre-computes a downsampled amplitude envelope for waveform preview.
// Scans the track once in a background thread using a temporary mpv instance.
class WaveformCache {
public:
    static constexpr int SAMPLES_PER_SEC = 20; // resolution: 20 samples/second

    ~WaveformCache();

    // Start scanning a new track. Cancels any in-progress scan.
    void scanTrack(const std::wstring& trackPath, double duration);

    // Is data ready for the current track?
    bool ready() const { return m_ready; }
    bool scanning() const { return m_scanning; }
    const std::wstring& cachedPath() const { return m_cachedPath; }

    // Get the envelope data (0.0–1.0 per sample). Thread-safe.
    void getEnvelope(std::vector<float>& out) const;

    // Get amplitude at a specific time (interpolated). Thread-safe.
    float amplitudeAt(double seconds) const;

    // Number of samples in the envelope
    int sampleCount() const;

    void cancel();

private:
    void scanThread(std::wstring path, double dur);

    mutable std::mutex m_mutex;
    std::vector<float> m_envelope;
    std::wstring m_cachedPath;
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_scanning{false};
    std::atomic<bool> m_cancelRequested{false};
    std::thread m_thread;
};

} // namespace wave
