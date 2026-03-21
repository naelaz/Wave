#include "audio/AudioCapture.h"
#include "core/Log.h"

#include <functiondiscoverykeys_devpkey.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace wave {

AudioCapture::AudioCapture() {
    // Precompute Hann window
    for (int i = 0; i < FFT_SIZE; i++) {
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
    }
}

AudioCapture::~AudioCapture() {
    stop();
}

bool AudioCapture::start() {
    if (m_running) return true;
    m_stopRequested = false;
    m_thread = std::thread(&AudioCapture::captureThread, this);
    return true;
}

void AudioCapture::stop() {
    if (!m_running && !m_thread.joinable()) return;
    m_stopRequested = true;
    if (m_thread.joinable()) m_thread.join();
}

void AudioCapture::getSpectrum(float* out, int count) const {
    std::lock_guard<std::mutex> lock(m_specMutex);
    int n = std::min(count, static_cast<int>(m_spectrum.size()));
    for (int i = 0; i < n; i++) out[i] = m_spectrum[i];
    for (int i = n; i < count; i++) out[i] = 0.0f;
}

// ── WASAPI loopback capture thread ───────────────────────────

void AudioCapture::captureThread() {
    m_running = true;
    HRESULT hr;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { m_running = false; return; }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) { CoUninitialize(); m_running = false; return; }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        log::warn("AudioCapture: no default audio device");
        enumerator->Release();
        CoUninitialize();
        m_running = false;
        return;
    }

    IAudioClient* audioClient = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&audioClient));
    if (FAILED(hr)) {
        device->Release(); enumerator->Release(); CoUninitialize();
        m_running = false; return;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        audioClient->Release(); device->Release(); enumerator->Release();
        CoUninitialize(); m_running = false; return;
    }

    // Initialize in loopback mode
    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        200000, // 20ms buffer in 100ns units
        0, mixFormat, nullptr);
    if (FAILED(hr)) {
        log::warn("AudioCapture: Initialize failed");
        CoTaskMemFree(mixFormat);
        audioClient->Release(); device->Release(); enumerator->Release();
        CoUninitialize(); m_running = false; return;
    }

    IAudioCaptureClient* captureClient = nullptr;
    hr = audioClient->GetService(__uuidof(IAudioCaptureClient),
                                  reinterpret_cast<void**>(&captureClient));
    if (FAILED(hr)) {
        CoTaskMemFree(mixFormat);
        audioClient->Release(); device->Release(); enumerator->Release();
        CoUninitialize(); m_running = false; return;
    }

    int sampleRate = mixFormat->nSamplesPerSec;
    int channels = mixFormat->nChannels;
    bool isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        isFloat = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    CoTaskMemFree(mixFormat);

    hr = audioClient->Start();
    if (FAILED(hr)) {
        captureClient->Release(); audioClient->Release();
        device->Release(); enumerator->Release();
        CoUninitialize(); m_running = false; return;
    }

    log::info("AudioCapture: started (rate=" + std::to_string(sampleRate) +
              " ch=" + std::to_string(channels) + ")");

    // Accumulation buffer for FFT input
    std::vector<float> accumBuf;
    accumBuf.reserve(FFT_SIZE * 2);

    while (!m_stopRequested) {
        Sleep(10); // ~100 captures/sec

        UINT32 packetLen = 0;
        hr = captureClient->GetNextPacketSize(&packetLen);
        if (FAILED(hr)) break;

        while (packetLen > 0) {
            BYTE* data = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            hr = captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && isFloat) {
                auto* fdata = reinterpret_cast<const float*>(data);

                // Mix down to mono and accumulate
                for (UINT32 f = 0; f < numFrames; f++) {
                    float mono = 0;
                    for (int c = 0; c < channels; c++)
                        mono += fdata[f * channels + c];
                    mono /= channels;
                    accumBuf.push_back(mono);
                }

                // When we have enough samples, run FFT
                while (accumBuf.size() >= FFT_SIZE) {
                    processBuffer(accumBuf.data(), FFT_SIZE, 1);
                    // Overlap: advance by half
                    accumBuf.erase(accumBuf.begin(), accumBuf.begin() + FFT_SIZE / 2);
                }
            }

            captureClient->ReleaseBuffer(numFrames);
            hr = captureClient->GetNextPacketSize(&packetLen);
            if (FAILED(hr)) { packetLen = 0; break; }
        }
    }

    audioClient->Stop();
    captureClient->Release();
    audioClient->Release();
    device->Release();
    enumerator->Release();
    CoUninitialize();

    log::info("AudioCapture: stopped");
    m_running = false;
}

// ── FFT processing ───────────────────────────────────────────

void AudioCapture::processBuffer(const float* data, int frames, int /*channels*/) {
    // Apply Hann window and copy to FFT buffer
    for (int i = 0; i < FFT_SIZE; i++) {
        m_fftReal[i] = (i < frames) ? data[i] * m_window[i] : 0.0f;
        m_fftImag[i] = 0.0f;
    }

    // In-place FFT
    fft(m_fftReal.data(), m_fftImag.data(), FFT_SIZE);

    // Convert to magnitude spectrum (only first half is useful)
    int halfN = FFT_SIZE / 2;

    // Map FFT bins to SPECTRUM_BINS using logarithmic frequency scaling
    // This gives more resolution to bass/mids and less to treble,
    // matching human perception
    std::array<float, SPECTRUM_BINS> newSpec{};

    for (int i = 0; i < SPECTRUM_BINS; i++) {
        // Logarithmic mapping: bin i covers frequency range [f0, f1]
        float t0 = static_cast<float>(i) / SPECTRUM_BINS;
        float t1 = static_cast<float>(i + 1) / SPECTRUM_BINS;
        // Map [0,1] to [1, halfN] logarithmically
        int bin0 = static_cast<int>(std::pow(halfN, t0));
        int bin1 = static_cast<int>(std::pow(halfN, t1));
        if (bin0 < 1) bin0 = 1;
        if (bin1 <= bin0) bin1 = bin0 + 1;
        if (bin1 > halfN) bin1 = halfN;

        float sum = 0;
        int count = 0;
        for (int b = bin0; b < bin1; b++) {
            float mag = std::sqrt(m_fftReal[b] * m_fftReal[b] + m_fftImag[b] * m_fftImag[b]);
            // Normalize by FFT size so magnitudes are independent of window length
            mag /= (FFT_SIZE / 2);
            sum += mag;
            count++;
        }
        if (count > 0) sum /= count;

        // Convert to dB scale, then map to 0–1
        // Typical normalized magnitudes: silence ~1e-7, quiet ~0.001, loud ~0.5
        // dB range: -80dB (silence) to 0dB (clipping)
        // We show -70dB to -6dB as the useful range (64dB dynamic range)
        float db = 20.0f * std::log10(sum + 1e-10f);
        float normalized = (db + 70.0f) / 64.0f; // -70dB→0, -6dB→1
        newSpec[i] = std::clamp(normalized, 0.0f, 1.0f);
    }

    // Update shared spectrum
    {
        std::lock_guard<std::mutex> lock(m_specMutex);
        m_spectrum = newSpec;
    }
}

// ── Cooley-Tukey radix-2 FFT ─────────────────────────────────

void AudioCapture::fft(float* real, float* imag, int n) {
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

    // Butterfly operations
    for (int len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * static_cast<float>(M_PI) / len;
        float wR = std::cos(angle), wI = std::sin(angle);

        for (int i = 0; i < n; i += len) {
            float curR = 1.0f, curI = 0.0f;
            for (int j = 0; j < len / 2; j++) {
                int u = i + j, v = i + j + len / 2;
                float tR = curR * real[v] - curI * imag[v];
                float tI = curR * imag[v] + curI * real[v];
                real[v] = real[u] - tR;
                imag[v] = imag[u] - tI;
                real[u] += tR;
                imag[u] += tI;
                float newR = curR * wR - curI * wI;
                curI = curR * wI + curI * wR;
                curR = newR;
            }
        }
    }
}

} // namespace wave
