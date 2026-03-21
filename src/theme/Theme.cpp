#include "theme/Theme.h"
#include "core/Log.h"

namespace wave {

// ── Built-in presets ─────────────────────────────────────────

static ThemePreset darkDefault() {
    ThemePreset p;
    p.id   = L"dark";
    p.name = L"Default Dark";
    auto& c = p.colors;
    c.clearColor  = { 0.07f, 0.07f, 0.07f };
    c.background  = { 0.07f, 0.07f, 0.07f };
    c.panelBg     = { 0.09f, 0.09f, 0.09f };
    c.text        = { 0.88f, 0.88f, 0.88f };
    c.textDim     = { 0.45f, 0.45f, 0.45f };
    c.accent      = { 0.40f, 0.65f, 1.00f };
    c.barBg       = { 0.15f, 0.15f, 0.15f };
    c.btnBg       = { 0.16f, 0.16f, 0.16f };
    c.btnHover    = { 0.22f, 0.22f, 0.22f };
    c.btnPressed  = { 0.12f, 0.12f, 0.12f };
    c.rowPlaying  = { 0.15f, 0.25f, 0.42f };
    c.tabActive   = { 0.20f, 0.20f, 0.20f };
    return p;
}

static ThemePreset darkBlue() {
    ThemePreset p;
    p.id   = L"darkblue";
    p.name = L"Dark Blue";
    auto& c = p.colors;
    c.clearColor  = { 0.05f, 0.06f, 0.10f };
    c.background  = { 0.05f, 0.06f, 0.10f };
    c.panelBg     = { 0.06f, 0.07f, 0.12f };
    c.text        = { 0.82f, 0.86f, 0.94f };
    c.textDim     = { 0.38f, 0.42f, 0.55f };
    c.accent      = { 0.30f, 0.55f, 0.95f };
    c.barBg       = { 0.10f, 0.12f, 0.20f };
    c.btnBg       = { 0.10f, 0.12f, 0.20f };
    c.btnHover    = { 0.15f, 0.18f, 0.28f };
    c.btnPressed  = { 0.07f, 0.09f, 0.15f };
    c.rowPlaying  = { 0.10f, 0.18f, 0.35f };
    c.tabActive   = { 0.12f, 0.14f, 0.24f };
    return p;
}

static ThemePreset light() {
    ThemePreset p;
    p.id   = L"light";
    p.name = L"Light";
    auto& c = p.colors;
    c.clearColor  = { 0.95f, 0.95f, 0.96f };
    c.background  = { 0.95f, 0.95f, 0.96f };
    c.panelBg     = { 0.90f, 0.90f, 0.92f };
    c.text        = { 0.10f, 0.10f, 0.12f };
    c.textDim     = { 0.45f, 0.45f, 0.50f };
    c.accent      = { 0.20f, 0.45f, 0.85f };
    c.barBg       = { 0.80f, 0.80f, 0.82f };
    c.btnBg       = { 0.84f, 0.84f, 0.86f };
    c.btnHover    = { 0.78f, 0.78f, 0.80f };
    c.btnPressed  = { 0.72f, 0.72f, 0.74f };
    c.rowPlaying  = { 0.78f, 0.85f, 0.96f };
    c.tabActive   = { 0.82f, 0.82f, 0.85f };
    return p;
}

static ThemePreset highContrast() {
    ThemePreset p;
    p.id   = L"hc-dark";
    p.name = L"High Contrast Dark";
    auto& c = p.colors;
    c.clearColor  = { 0.0f, 0.0f, 0.0f };
    c.background  = { 0.0f, 0.0f, 0.0f };
    c.panelBg     = { 0.04f, 0.04f, 0.04f };
    c.text        = { 1.0f, 1.0f, 1.0f };
    c.textDim     = { 0.65f, 0.65f, 0.65f };
    c.accent      = { 0.30f, 0.80f, 1.00f };
    c.barBg       = { 0.25f, 0.25f, 0.25f };
    c.btnBg       = { 0.18f, 0.18f, 0.18f };
    c.btnHover    = { 0.30f, 0.30f, 0.30f };
    c.btnPressed  = { 0.10f, 0.10f, 0.10f };
    c.rowPlaying  = { 0.10f, 0.30f, 0.50f };
    c.tabActive   = { 0.22f, 0.22f, 0.22f };
    return p;
}

// ── Accent color options ─────────────────────────────────────

static std::vector<AccentOption> builtinAccents() {
    return {
        { L"Default (theme)",  { 0, 0, 0, 0 } },  // a=0 means "use preset default"
        { L"Blue",             { 0.40f, 0.65f, 1.00f } },
        { L"Purple",           { 0.62f, 0.40f, 1.00f } },
        { L"Teal",             { 0.25f, 0.80f, 0.75f } },
        { L"Green",            { 0.35f, 0.78f, 0.40f } },
        { L"Orange",           { 0.95f, 0.60f, 0.20f } },
        { L"Red",              { 0.92f, 0.30f, 0.30f } },
        { L"Pink",             { 0.92f, 0.40f, 0.65f } },
    };
}

// ── Theme implementation ─────────────────────────────────────

Theme::Theme() {
    m_presets.push_back(darkDefault());
    m_presets.push_back(darkBlue());
    m_presets.push_back(light());
    m_presets.push_back(highContrast());
    m_accents = builtinAccents();
    rebuildActive();
}

const std::wstring& Theme::activePresetId() const {
    return m_presets[m_presetIdx].id;
}

void Theme::applyPreset(int index) {
    if (index < 0 || index >= static_cast<int>(m_presets.size())) return;
    m_presetIdx = index;
    rebuildActive();
}

void Theme::applyPresetById(const std::wstring& id) {
    for (int i = 0; i < static_cast<int>(m_presets.size()); i++) {
        if (m_presets[i].id == id) { applyPreset(i); return; }
    }
    log::warn("Unknown theme preset, using default");
    applyPreset(0);
}

void Theme::setAccent(int index) {
    if (index < 0 || index >= static_cast<int>(m_accents.size())) return;
    m_accentIdx = index;
    rebuildActive();
}

void Theme::resetToDefault() {
    m_presetIdx = 0;
    m_accentIdx = 0;
    rebuildActive();
}

std::wstring Theme::presetId() const {
    return m_presets[m_presetIdx].id;
}

void Theme::rebuildActive() {
    m_active = m_presets[m_presetIdx].colors;

    // Override accent if user picked one (index > 0, alpha > 0)
    if (m_accentIdx > 0 && m_accentIdx < static_cast<int>(m_accents.size())) {
        const auto& ac = m_accents[m_accentIdx].color;
        if (ac.a > 0) {
            m_active.accent = ac;
            // Derive playing-row tint from accent
            m_active.rowPlaying = { ac.r * 0.35f, ac.g * 0.35f, ac.b * 0.35f };
        }
    }
}

} // namespace wave
