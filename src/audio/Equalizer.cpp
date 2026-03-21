#include "audio/Equalizer.h"
#include "audio/Engine.h"
#include "core/Log.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <algorithm>

namespace wave {

// ── Band frequencies ────────────────────────────────────────

static constexpr float BAND_FREQS[Equalizer::BAND_COUNT] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

static const wchar_t* BAND_LABELS[Equalizer::BAND_COUNT] = {
    L"31",  L"62",  L"125", L"250", L"500",
    L"1k",  L"2k",  L"4k",  L"8k",  L"16k"
};

float Equalizer::bandFrequency(int band) {
    if (band < 0 || band >= BAND_COUNT) return 0;
    return BAND_FREQS[band];
}

const wchar_t* Equalizer::bandLabel(int band) {
    if (band < 0 || band >= BAND_COUNT) return L"?";
    return BAND_LABELS[band];
}

// ── Presets ─────────────────────────────────────────────────

static const Equalizer::Preset PRESETS[] = {
    { L"Flat",         {{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }} },
    { L"Bass Boost",   {{ 5, 4, 3, 1, 0, 0, 0, 0, 0, 0 }} },
    { L"Treble Boost", {{ 0, 0, 0, 0, 0, 0, 1, 3, 4, 5 }} },
    { L"Vocal",        {{ -2, -1, 0, 2, 4, 4, 2, 0, -1, -2 }} },
    { L"Rock",         {{ 4, 3, 1, 0, -1, 0, 2, 3, 4, 4 }} },
    { L"Electronic",   {{ 4, 3, 1, 0, -1, 1, 0, 2, 4, 5 }} },
    { L"Jazz",         {{ 3, 2, 1, 2, -1, -1, 0, 1, 2, 3 }} },
    { L"Classical",    {{ 0, 0, 0, 0, 0, 0, -1, -2, -2, -3 }} },
    { L"Loudness",     {{ 4, 3, 0, 0, -1, 0, 0, 0, 2, 3 }} },
};

static constexpr int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

int Equalizer::presetCount() { return PRESET_COUNT; }
const Equalizer::Preset& Equalizer::preset(int index) { return PRESETS[index]; }

// ── Implementation ──────────────────────────────────────────

Equalizer::Equalizer() {
    m_gains.fill(0.0f);
}

float Equalizer::bandGain(int band) const {
    if (band < 0 || band >= BAND_COUNT) return 0;
    return m_gains[band];
}

void Equalizer::setBandGain(int band, float dB) {
    if (band < 0 || band >= BAND_COUNT) return;
    m_gains[band] = std::clamp(dB, MIN_GAIN, MAX_GAIN);
    m_presetIdx = -1; // now custom
}

void Equalizer::setGains(const std::array<float, BAND_COUNT>& gains) {
    m_gains = gains;
    for (auto& g : m_gains) g = std::clamp(g, MIN_GAIN, MAX_GAIN);
    m_presetIdx = -1;
}

void Equalizer::applyPreset(int index) {
    if (index < 0 || index >= PRESET_COUNT) return;
    m_gains = PRESETS[index].gains;
    m_presetIdx = index;
}

void Equalizer::setEnabled(bool on) {
    m_enabled = on;
}

// ── mpv filter string ───────────────────────────────────────

std::string Equalizer::buildFilterString() const {
    if (!m_enabled) return "";

    // Check if all gains are zero (flat = no filter needed)
    bool allZero = true;
    for (float g : m_gains) {
        if (std::abs(g) > 0.01f) { allZero = false; break; }
    }
    if (allZero) return "";

    // Build lavfi equalizer filter chain
    // Each band: equalizer=f=<freq>:t=o:w=1.0:g=<gain>
    // t=o means octave bandwidth, w=1.0 = 1 octave width
    std::string result = "lavfi=[";
    bool first = true;
    for (int i = 0; i < BAND_COUNT; i++) {
        if (std::abs(m_gains[i]) < 0.01f) continue;
        if (!first) result += ",";
        char buf[128];
        snprintf(buf, sizeof(buf), "equalizer=f=%.0f:t=o:w=1.0:g=%.1f",
                 BAND_FREQS[i], m_gains[i]);
        result += buf;
        first = false;
    }
    result += "]";
    return result;
}

void Equalizer::applyToEngine(Engine* engine) {
    if (!engine) return;
    std::string filter = buildFilterString();
    engine->setAudioFilter(filter);
}

// ── Serialization ───────────────────────────────────────────

std::string Equalizer::serialize() const {
    std::ostringstream ss;
    ss << (m_enabled ? "1" : "0") << ";";
    ss << m_presetIdx << ";";
    for (int i = 0; i < BAND_COUNT; i++) {
        if (i > 0) ss << ",";
        ss << static_cast<int>(m_gains[i]);
    }
    return ss.str();
}

void Equalizer::deserialize(const std::string& data) {
    if (data.empty()) return;

    // Format: "enabled;presetIdx;g0,g1,...,g9"
    size_t p1 = data.find(';');
    if (p1 == std::string::npos) return;
    m_enabled = (data[0] == '1');

    size_t p2 = data.find(';', p1 + 1);
    if (p2 == std::string::npos) return;
    m_presetIdx = std::atoi(data.substr(p1 + 1, p2 - p1 - 1).c_str());

    std::string gains = data.substr(p2 + 1);
    int band = 0;
    size_t pos = 0;
    while (pos < gains.size() && band < BAND_COUNT) {
        size_t comma = gains.find(',', pos);
        std::string val = gains.substr(pos, comma - pos);
        m_gains[band++] = std::clamp(static_cast<float>(std::atof(val.c_str())), MIN_GAIN, MAX_GAIN);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

} // namespace wave
