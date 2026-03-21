#include "ui/Renderer.h"
#include "audio/Engine.h"
#include "library/Library.h"
#include "playlist/PlaylistManager.h"
#include "layout/Layout.h"
#include "theme/Theme.h"
#include "visualizer/Visualizer.h"
#include "art/AlbumArt.h"
#include "audio/WaveformCache.h"
#include "coverflow/CoverFlow.h"
#include "core/Log.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace wave {

static bool ptInRect(float x, float y, const D2D1_RECT_F& r) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

// ── Init / Shutdown ──────────────────────────────────────────

bool Renderer::init(HWND hwnd, Engine* engine, Library* library, PlaylistManager* playlists, Layout* layout, Theme* theme, Visualizer* visualizer, AlbumArt* albumArt, WaveformCache* waveform) {
    m_hwnd = hwnd;
    m_engine = engine;
    m_library = library;
    m_playlists = playlists;
    m_layout = layout;
    m_theme = theme;
    m_visualizer = visualizer;
    m_albumArt = albumArt;
    m_waveform = waveform;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (FAILED(hr)) { log::error("D2D1CreateFactory failed"); return false; }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                              reinterpret_cast<IUnknown**>(&m_dwFactory));
    if (FAILED(hr)) { log::error("DWriteCreateFactory failed"); return false; }

    // Typography: Segoe UI Variable if available (Win11), fallback Segoe UI
    const wchar_t* uiFont = L"Segoe UI Variable Display";

    m_dwFactory->CreateTextFormat(uiFont, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &m_titleFormat);
    if (!m_titleFormat) // fallback if Variable not available
        m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &m_titleFormat);

    m_dwFactory->CreateTextFormat(uiFont, nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &m_bodyFormat);
    if (!m_bodyFormat)
        m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &m_bodyFormat);

    m_dwFactory->CreateTextFormat(uiFont, nullptr, DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &m_btnFormat);
    if (!m_btnFormat)
        m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &m_btnFormat);

    m_dwFactory->CreateTextFormat(L"Segoe MDL2 Assets", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &m_iconFormat);

    m_dwFactory->CreateTextFormat(uiFont, nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"en-us", &m_listFormat);
    if (!m_listFormat)
        m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"en-us", &m_listFormat);

    m_dwFactory->CreateTextFormat(uiFont, nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"en-us", &m_listSmallFormat);
    if (!m_listSmallFormat)
        m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"en-us", &m_listSmallFormat);

    if (m_titleFormat) m_titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (m_bodyFormat)  m_bodyFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (m_btnFormat) {
        m_btnFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_btnFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_iconFormat) {
        m_iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_iconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.0f, L"en-us", &m_badgeFormat);
    if (m_badgeFormat) {
        m_badgeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_badgeFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_listFormat)      { m_listFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                             m_listFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP); }
    if (m_listSmallFormat) { m_listSmallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                             m_listSmallFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP); }

    createDeviceResources();
    log::info("Renderer initialized");
    return true;
}

void Renderer::shutdown() {
    discardDeviceResources();
    if (m_badgeFormat)     { m_badgeFormat->Release();     m_badgeFormat = nullptr; }
    if (m_iconFormat)      { m_iconFormat->Release();      m_iconFormat = nullptr; }
    if (m_listSmallFormat) { m_listSmallFormat->Release(); m_listSmallFormat = nullptr; }
    if (m_listFormat)  { m_listFormat->Release();  m_listFormat = nullptr; }
    if (m_btnFormat)   { m_btnFormat->Release();   m_btnFormat = nullptr; }
    if (m_bodyFormat)  { m_bodyFormat->Release();  m_bodyFormat = nullptr; }
    if (m_titleFormat) { m_titleFormat->Release(); m_titleFormat = nullptr; }
    if (m_dwFactory)   { m_dwFactory->Release();   m_dwFactory = nullptr; }
    if (m_d2dFactory)  { m_d2dFactory->Release();  m_d2dFactory = nullptr; }
    log::info("Renderer shut down");
}

// ── Device Resources ─────────────────────────────────────────

static D2D1::ColorF toD2D(const ThemeColor& c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

void Renderer::createDeviceResources() {
    if (m_renderTarget) return;
    RECT rc; GetClientRect(m_hwnd, &rc);
    auto hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(rc.right, rc.bottom)),
        &m_renderTarget);
    if (FAILED(hr)) { log::error("CreateHwndRenderTarget failed"); return; }

    // Crisp text rendering
    m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    applyTheme();
}

void Renderer::applyTheme() {
    if (!m_renderTarget) return;

    // Release old brushes
    auto release = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    release(m_textBrush); release(m_dimBrush); release(m_accentBrush);
    release(m_bgBrush); release(m_barBgBrush); release(m_btnBrush);
    release(m_btnHoverBrush); release(m_btnPressBrush); release(m_panelBgBrush);
    release(m_rowPlayingBrush); release(m_tabActiveBrush);

    // Fallback colors if no theme
    ThemeColors c;
    if (m_theme) c = m_theme->colors();
    else {
        c.text       = { 0.88f, 0.88f, 0.88f };
        c.textDim    = { 0.45f, 0.45f, 0.45f };
        c.accent     = { 0.40f, 0.65f, 1.00f };
        c.background = { 0.07f, 0.07f, 0.07f };
        c.barBg      = { 0.15f, 0.15f, 0.15f };
        c.btnBg      = { 0.16f, 0.16f, 0.16f };
        c.btnHover   = { 0.22f, 0.22f, 0.22f };
        c.btnPressed = { 0.12f, 0.12f, 0.12f };
        c.panelBg    = { 0.09f, 0.09f, 0.09f };
        c.rowPlaying = { 0.15f, 0.25f, 0.42f };
        c.tabActive  = { 0.20f, 0.20f, 0.20f };
    }

    m_renderTarget->CreateSolidColorBrush(toD2D(c.text),       &m_textBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.textDim),    &m_dimBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.accent),     &m_accentBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.background), &m_bgBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.barBg),      &m_barBgBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.btnBg),      &m_btnBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.btnHover),   &m_btnHoverBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.btnPressed), &m_btnPressBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.panelBg),    &m_panelBgBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.rowPlaying), &m_rowPlayingBrush);
    m_renderTarget->CreateSolidColorBrush(toD2D(c.tabActive),  &m_tabActiveBrush);
}

void Renderer::discardDeviceResources() {
    auto release = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    release(m_tabActiveBrush); release(m_rowPlayingBrush); release(m_panelBgBrush);
    release(m_btnPressBrush); release(m_btnHoverBrush); release(m_btnBrush);
    release(m_barBgBrush); release(m_bgBrush); release(m_accentBrush);
    release(m_dimBrush); release(m_textBrush); release(m_renderTarget);
}

void Renderer::resize() {
    if (!m_renderTarget) return;
    RECT rc; GetClientRect(m_hwnd, &rc);
    m_renderTarget->Resize(D2D1::SizeU(rc.right, rc.bottom));
}

// ── Hit Testing ──────────────────────────────────────────────

HitZone Renderer::hitTest(float x, float y) const {
    // Tabs
    if (ptInRect(x, y, m_tabLibrary))   return HitZone::TabLibrary;
    if (ptInRect(x, y, m_tabPlaylists)) return HitZone::TabPlaylists;
    if (ptInRect(x, y, m_tabQueue))     return HitZone::TabQueue;

    // Panel rows
    if (x >= m_panelRect.left && x <= m_panelRect.right &&
        y >= m_listTop && y <= m_listBottom) {
        return HitZone::PanelRow;
    }

    // Progress bar (expanded hit area)
    D2D1_RECT_F barHit = m_progressBarRect;
    barHit.top -= 12.0f; barHit.bottom += 12.0f;
    if (ptInRect(x, y, barHit)) return HitZone::ProgressBar;

    if (ptInRect(x, y, m_btnPlayPause)) return HitZone::PlayPause;
    if (ptInRect(x, y, m_btnStop))      return HitZone::Stop;
    if (ptInRect(x, y, m_btnPrev))      return HitZone::Prev;
    if (ptInRect(x, y, m_btnNext))      return HitZone::Next;
    return HitZone::None;
}

float Renderer::progressBarFraction(float x) const {
    float bw = m_progressBarRect.right - m_progressBarRect.left;
    if (bw <= 0) return 0;
    return std::clamp((x - m_progressBarRect.left) / bw, 0.0f, 1.0f);
}

int Renderer::panelRowAt(float y) const {
    if (y < m_listTop || y > m_listBottom) return -1;

    float rowStart = (m_activeTab == PanelTab::Library) ? m_libListTop : m_listTop;
    if (y < rowStart) return -2; // search bar area

    if (m_activeTab == PanelTab::Library && m_library) {
        // Variable-height rows: walk from scrollOffset
        float curY = rowStart;
        int cnt = m_library->count();
        for (int viewIdx = m_scrollOffset; viewIdx < cnt; viewIdx++) {
            const ViewRow* row = m_library->viewRowAt(viewIdx);
            if (!row) break;
            float rowH;
            if (row->isHeader) {
                rowH = 44; // tall header
                if (viewIdx > 0) rowH += 10; // gap above non-first headers
            } else {
                rowH = ROW_HEIGHT;
            }
            if (y >= curY && y < curY + rowH) return viewIdx;
            curY += rowH;
            if (curY > m_listBottom) break;
        }
        return -1;
    }

    // Other tabs: fixed row height
    float relY = y - rowStart;
    int row = static_cast<int>(relY / ROW_HEIGHT) + m_scrollOffset;
    if (row < 0) row = 0;
    return row;
}

void Renderer::scrollPanel(int rows) {
    m_scrollOffset += rows;
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    // Upper bound clamping happens during render (depends on list size)
}

// ── Layout ───────────────────────────────────────────────────

void Renderer::computeLayout() {
    D2D1_SIZE_F sz = m_renderTarget->GetSize();
    float w = sz.width, h = sz.height;

    bool panelAllowed = !m_layout || m_layout->isPanelVisible(PanelId::SidePanel);
    bool hasContent = (m_library && m_library->totalCount() > 0) ||
                      (m_playlists && (m_playlists->playlistCount() > 0 || m_playlists->queueCount() > 0));
    float pw = (panelAllowed && hasContent) ? PANEL_WIDTH : 0;
    m_playerLeft = pw;
    m_panelRect = D2D1::RectF(0, 0, pw, h);

    // Tabs at top of panel
    float tabW = pw / 3.0f;
    m_tabLibrary   = D2D1::RectF(0,          0, tabW,      TAB_HEIGHT);
    m_tabPlaylists = D2D1::RectF(tabW,       0, tabW * 2,  TAB_HEIGHT);
    m_tabQueue     = D2D1::RectF(tabW * 2,   0, pw,        TAB_HEIGHT);
    m_listTop = TAB_HEIGHT;
    m_listBottom = h;

    // Player area — responsive layout
    // Transport zone: progress bar + time text + buttons + visualizer at bottom
    // Progress bar is positioned from the bottom up, giving now-playing max space
    float margin = 40.0f;
    float bx = pw + margin, bw = w - pw - margin * 2;

    float btnW = 44, btnH = 44, playW = 52, gap = 16;
    float total = btnW * 2 + playW + btnW + gap * 3;

    // Fixed bottom zone: buttons(44) + gap(28) + bar(6) + timetext(26) + viz(variable)
    // Position progress bar so there's space for buttons below and content above
    float transportH = 44 + 28 + 6 + 26; // buttons + gap + bar + time = 104
    float vizMinH = 50.0f;

    // Progress bar: placed so buttons fit below it, leaving room for viz at very bottom
    float barY = h - transportH - vizMinH - 16;
    // Don't let it go too high (max 55% of window height)
    barY = std::max(barY, h * 0.35f);
    // Don't let it go too low either
    barY = std::min(barY, h * 0.55f);

    m_progressBarRect = D2D1::RectF(bx, barY, bx + bw, barY + 6);

    float btnY = barY + 6 + 26 + 10; // below bar + time text + gap
    float sx = pw + (w - pw - total) * 0.5f;
    m_btnPrev      = D2D1::RectF(sx,                            btnY, sx + btnW,                            btnY + btnH);
    m_btnPlayPause = D2D1::RectF(sx + btnW + gap,               btnY, sx + btnW + gap + playW,              btnY + btnH);
    m_btnStop      = D2D1::RectF(sx + btnW + gap + playW + gap, btnY, sx + btnW + gap + playW + gap + btnW, btnY + btnH);
    m_btnNext      = D2D1::RectF(sx + btnW + gap + playW + gap + btnW + gap, btnY,
                                  sx + btnW + gap + playW + gap + btnW + gap + btnW, btnY + btnH);

    // Visualizer region: below buttons, above bottom
    float vizTop = btnY + btnH + 12;
    float vizH = h - vizTop - 8;
    if (vizH < 30) vizH = 30;
    m_vizRect = D2D1::RectF(pw + margin, vizTop, w - margin, vizTop + vizH);
}

// ── Drawing Helpers ──────────────────────────────────────────

void Renderer::formatTime(double s, wchar_t* buf, int len) const {
    if (s < 0) s = 0;
    int ts = static_cast<int>(s);
    swprintf_s(buf, len, L"%d:%02d", ts / 60, ts % 60);
}

void Renderer::drawButton(D2D1_RECT_F r, const wchar_t* label, bool enabled, HitZone zone) {
    bool isPlay = (zone == HitZone::PlayPause);

    auto* bg = m_btnBrush;
    auto* brush = enabled ? m_textBrush : m_dimBrush;

    if (enabled) {
        if (m_pressed == zone) bg = m_btnPressBrush;
        else if (m_hover == zone) bg = m_btnHoverBrush;
    }

    // Play/Pause: circular, accent-tinted when playing
    if (isPlay) {
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        float rad = (r.right - r.left) * 0.5f;
        m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), bg);
        // Accent ring on hover
        if (enabled && m_hover == zone)
            m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), m_accentBrush, 1.5f);
    } else {
        // Other buttons: rounded rect
        m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(r, 8, 8), bg);
    }

    auto* fmt = m_iconFormat ? m_iconFormat : m_btnFormat;
    if (fmt)
        m_renderTarget->DrawText(label, static_cast<UINT32>(wcslen(label)), fmt, r, brush);
}

void Renderer::drawTrackRow(const TrackInfo& track, float left, float right, float y,
                             bool isPlaying, bool /*isActive*/) {
    D2D1_RECT_F rowRect = D2D1::RectF(m_panelRect.left, y, m_panelRect.right, y + ROW_HEIGHT);

    if (isPlaying) {
        m_renderTarget->FillRectangle(rowRect, m_rowPlayingBrush);
        // Accent bar on the left edge
        D2D1_RECT_F bar = D2D1::RectF(m_panelRect.left, y + 4, m_panelRect.left + 3, y + ROW_HEIGHT - 4);
        m_renderTarget->FillRectangle(bar, m_accentBrush);
    }

    float textLeft = left + (isPlaying ? 4 : 0); // indent playing row slightly
    auto* titleBr = isPlaying ? m_accentBrush : m_textBrush;
    const auto& title = track.displayTitle();

    D2D1_RECT_F tR = D2D1::RectF(textLeft, y + 5, right, y + 21);
    if (m_listFormat) {
        m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_renderTarget->DrawText(title.c_str(), static_cast<UINT32>(title.size()),
                                  m_listFormat, tR, titleBr);
    }

    auto sub = track.displayArtistAlbum();
    if (!sub.empty() && m_listSmallFormat) {
        D2D1_RECT_F sR = D2D1::RectF(textLeft, y + 22, right, y + ROW_HEIGHT - 2);
        m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_renderTarget->DrawText(sub.c_str(), static_cast<UINT32>(sub.size()),
                                  m_listSmallFormat, sR, m_dimBrush);
    }

    // Subtle bottom separator
    m_renderTarget->DrawLine(
        D2D1::Point2F(left, y + ROW_HEIGHT - 0.5f),
        D2D1::Point2F(right, y + ROW_HEIGHT - 0.5f),
        m_barBgBrush, 0.5f);
}

// ── Panel Drawing ────────────────────────────────────────────

void Renderer::drawTabs(float panelRight, float /*tabY*/) {
    // Full tab bar background
    D2D1_RECT_F tabBg = D2D1::RectF(0, 0, panelRight, TAB_HEIGHT);
    m_renderTarget->FillRectangle(tabBg, m_panelBgBrush);

    auto drawTab = [&](D2D1_RECT_F r, const wchar_t* label, PanelTab tab) {
        bool active = (m_activeTab == tab);
        if (m_btnFormat) {
            m_renderTarget->DrawText(label, static_cast<UINT32>(wcslen(label)),
                                      m_btnFormat, r, active ? m_textBrush : m_dimBrush);
        }
        // Active tab: accent underline
        if (active) {
            float cx = (r.left + r.right) * 0.5f;
            float lineW = (r.right - r.left) * 0.5f;
            m_renderTarget->DrawLine(
                D2D1::Point2F(cx - lineW * 0.5f, TAB_HEIGHT - 2),
                D2D1::Point2F(cx + lineW * 0.5f, TAB_HEIGHT - 2),
                m_accentBrush, 2.5f);
        }
    };
    drawTab(m_tabLibrary,   L"Library",   PanelTab::Library);
    drawTab(m_tabPlaylists, L"Playlists", PanelTab::Playlists);
    drawTab(m_tabQueue,     L"Queue",     PanelTab::Queue);

    // Subtle separator
    m_renderTarget->DrawLine(D2D1::Point2F(0, TAB_HEIGHT - 0.5f),
                              D2D1::Point2F(panelRight, TAB_HEIGHT - 0.5f), m_barBgBrush, 0.5f);
}

void Renderer::drawLibraryList(float left, float right, float top, float bottom) {
    // Search bar
    float searchH = 28;
    D2D1_RECT_F searchRect = D2D1::RectF(left, top + 2, right, top + searchH);
    m_renderTarget->FillRoundedRectangle(
        D2D1::RoundedRect(searchRect, 4, 4), m_barBgBrush);

    if (m_library && !m_library->searchQuery().empty()) {
        const auto& q = m_library->searchQuery();
        if (m_listFormat) {
            D2D1_RECT_F tR = D2D1::RectF(left + 8, top + 2, right - 4, top + searchH);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_renderTarget->DrawText(q.c_str(), static_cast<UINT32>(q.size()),
                                      m_listFormat, tR, m_textBrush);
        }
    } else {
        // Placeholder
        if (m_listFormat) {
            D2D1_RECT_F tR = D2D1::RectF(left + 8, top + 2, right - 4, top + searchH);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            static const wchar_t PH[] = L"\xE721 Search..."; // Segoe MDL2 search icon
            m_renderTarget->DrawText(PH, static_cast<UINT32>(wcslen(PH)),
                                      m_listFormat, tR, m_dimBrush);
        }
    }

    float listTop = top + searchH + 4;
    m_libListTop = listTop;

    if (!m_library || m_library->empty()) {
        if (m_listFormat) {
            const wchar_t* emptyMsg = (m_library && m_library->totalCount() > 0)
                ? L"No matches" : L"No tracks loaded";
            D2D1_RECT_F r = D2D1::RectF(left, listTop + 10, right, listTop + 30);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(emptyMsg, static_cast<UINT32>(wcslen(emptyMsg)), m_listFormat, r, m_dimBrush);
        }
        return;
    }

    int cnt = m_library->count();
    if (m_scrollOffset >= cnt) m_scrollOffset = std::max(0, cnt - 1);
    int playViewIdx = m_library->playingViewIndex();

    // Variable-height rows: headers are shorter than tracks
    // We need to walk from scrollOffset and accumulate Y positions
    float y = listTop;
    for (int viewIdx = m_scrollOffset; viewIdx < cnt && y < bottom; viewIdx++) {
        const ViewRow* row = m_library->viewRowAt(viewIdx);
        if (!row) break;

        if (row->isHeader) {
            // Album header — prominent visual separator
            float hH = 44; // taller header

            // Gap + accent separator line above (except first)
            if (viewIdx > 0) {
                y += 6; // extra gap between albums
                m_accentBrush->SetOpacity(0.3f);
                m_renderTarget->DrawLine(
                    D2D1::Point2F(left + 4, y),
                    D2D1::Point2F(right - 4, y),
                    m_accentBrush, 1.0f);
                m_accentBrush->SetOpacity(1.0f);
                y += 4;
            }

            // Dark background for header row
            D2D1_RECT_F headerBg = D2D1::RectF(left, y, right, y + hH);
            m_panelBgBrush->SetOpacity(0.5f);
            m_renderTarget->FillRectangle(headerBg, m_panelBgBrush);
            m_panelBgBrush->SetOpacity(1.0f);

            // Small album art thumbnail from CoverFlow cache
            float thumbSize = hH - 8;
            float thumbX = left + 4;
            float thumbY = y + 4;
            D2D1_RECT_F thumbR = D2D1::RectF(thumbX, thumbY, thumbX + thumbSize, thumbY + thumbSize);
            bool drewArt = false;

            if (m_coverFlow) {
                ID2D1Bitmap* art = m_coverFlow->getArtByName(row->albumName, m_renderTarget);
                if (art) {
                    m_renderTarget->PushAxisAlignedClip(thumbR, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    m_renderTarget->DrawBitmap(art, thumbR, 1.0f,
                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                    m_renderTarget->PopAxisAlignedClip();
                    drewArt = true;
                }
            }

            if (!drewArt) {
                // Placeholder square
                m_renderTarget->FillRoundedRectangle(
                    D2D1::RoundedRect(thumbR, 3, 3), m_barBgBrush);
                static const wchar_t NOTE[] = L"\xE8D6";
                if (m_iconFormat) {
                    m_dimBrush->SetOpacity(0.3f);
                    m_renderTarget->DrawText(NOTE, 1, m_iconFormat, thumbR, m_dimBrush);
                    m_dimBrush->SetOpacity(1.0f);
                }
            }

            // Album name (bold) + artist below
            float textLeft = thumbX + thumbSize + 6;
            if (m_btnFormat) {
                D2D1_RECT_F nameR = D2D1::RectF(textLeft, y + 4, right - 36, y + 22);
                m_btnFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                m_btnFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                m_renderTarget->DrawText(row->albumName.c_str(),
                    static_cast<UINT32>(row->albumName.size()), m_btnFormat, nameR, m_textBrush);
            }

            // Artist name below album name
            if (m_listSmallFormat && !row->artistName.empty()) {
                D2D1_RECT_F artR = D2D1::RectF(textLeft, y + 23, right - 36, y + hH - 2);
                m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                m_renderTarget->DrawText(row->artistName.c_str(),
                    static_cast<UINT32>(row->artistName.size()),
                    m_listSmallFormat, artR, m_dimBrush);
            }

            // Track count badge on the right
            if (m_listSmallFormat) {
                wchar_t countBuf[16];
                swprintf_s(countBuf, 16, L"%d", row->trackCount);
                D2D1_RECT_F cR = D2D1::RectF(right - 34, y + 4, right - 4, y + hH - 4);
                m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                m_listSmallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                m_renderTarget->DrawText(countBuf, static_cast<UINT32>(wcslen(countBuf)),
                    m_listSmallFormat, cR, m_dimBrush);
            }

            y += hH;
        } else {
            // Track row
            const TrackInfo* t = m_library->trackAt(row->masterIndex);
            if (t) drawTrackRow(*t, left, right, y, viewIdx == playViewIdx, false);
            y += ROW_HEIGHT;
        }
    }
}

void Renderer::drawPlaylistList(float left, float right, float top, float bottom) {
    if (!m_playlists || m_playlists->playlistCount() == 0) {
        if (m_listFormat) {
            D2D1_RECT_F r = D2D1::RectF(left, top + 10, right, top + 30);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(L"No playlists", 12, m_listFormat, r, m_dimBrush);
        }
        return;
    }

    int count = m_playlists->playlistCount();
    if (m_scrollOffset >= count) m_scrollOffset = std::max(0, count - 1);
    int visRows = static_cast<int>((bottom - top) / ROW_HEIGHT) + 1;

    for (int i = 0; i < visRows && (m_scrollOffset + i) < count; i++) {
        int idx = m_scrollOffset + i;
        float y = top + i * ROW_HEIGHT;
        const auto& pl = m_playlists->playlists()[idx];

        D2D1_RECT_F nameR = D2D1::RectF(left, y + 4, right, y + 20);
        if (m_listFormat) {
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_renderTarget->DrawText(pl.name.c_str(), static_cast<UINT32>(pl.name.size()),
                                      m_listFormat, nameR, m_textBrush);
        }

        wchar_t countBuf[32];
        swprintf_s(countBuf, 32, L"%d tracks", static_cast<int>(pl.tracks.size()));
        D2D1_RECT_F countR = D2D1::RectF(left, y + 21, right, y + ROW_HEIGHT - 2);
        if (m_listSmallFormat) {
            m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_renderTarget->DrawText(countBuf, static_cast<UINT32>(wcslen(countBuf)),
                                      m_listSmallFormat, countR, m_dimBrush);
        }
    }
}

void Renderer::drawPlaylistTracks(float left, float right, float top, float bottom) {
    auto* pl = m_playlists ? m_playlists->playlist(m_viewedPlaylist) : nullptr;
    if (!pl) { m_viewedPlaylist = -1; return; }

    // Header: "< Back | Playlist Name"
    D2D1_RECT_F headerR = D2D1::RectF(left, top, right, top + ROW_HEIGHT);
    std::wstring header = L"\u25C0 " + pl->name;
    if (m_listFormat) {
        m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_renderTarget->DrawText(header.c_str(), static_cast<UINT32>(header.size()),
                                  m_listFormat, headerR, m_accentBrush);
    }

    float trackTop = top + ROW_HEIGHT;
    int count = static_cast<int>(pl->tracks.size());
    if (count == 0) {
        if (m_listFormat) {
            D2D1_RECT_F r = D2D1::RectF(left, trackTop + 10, right, trackTop + 30);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(L"Empty playlist", 14, m_listFormat, r, m_dimBrush);
        }
        return;
    }

    if (m_scrollOffset >= count) m_scrollOffset = std::max(0, count - 1);
    int visRows = static_cast<int>((bottom - trackTop) / ROW_HEIGHT) + 1;

    for (int i = 0; i < visRows && (m_scrollOffset + i) < count; i++) {
        int idx = m_scrollOffset + i;
        float y = trackTop + i * ROW_HEIGHT;
        drawTrackRow(pl->tracks[idx], left, right, y, idx == pl->playingIndex, false);
    }
}

void Renderer::drawQueueList(float left, float right, float top, float bottom) {
    if (!m_playlists || m_playlists->queueCount() == 0) {
        if (m_listFormat) {
            D2D1_RECT_F r = D2D1::RectF(left, top + 10, right, top + 30);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(L"Queue is empty", 14, m_listFormat, r, m_dimBrush);
        }
        return;
    }

    int count = m_playlists->queueCount();
    if (m_scrollOffset >= count) m_scrollOffset = std::max(0, count - 1);
    int visRows = static_cast<int>((bottom - top) / ROW_HEIGHT) + 1;

    for (int i = 0; i < visRows && (m_scrollOffset + i) < count; i++) {
        int idx = m_scrollOffset + i;
        float y = top + i * ROW_HEIGHT;
        drawTrackRow(m_playlists->queue()[idx], left, right, y, false, false);
    }
}

void Renderer::drawPanel() {
    float pw = m_panelRect.right;
    if (pw <= 0) return;

    m_renderTarget->FillRectangle(m_panelRect, m_panelBgBrush);

    // Subtle right edge — thin line with slight opacity
    m_barBgBrush->SetOpacity(0.6f);
    m_renderTarget->DrawLine(D2D1::Point2F(pw - 0.5f, 0), D2D1::Point2F(pw - 0.5f, m_panelRect.bottom), m_barBgBrush, 1.0f);
    m_barBgBrush->SetOpacity(1.0f);

    drawTabs(pw, 0);

    float left = PAD, right = pw - PAD;

    m_renderTarget->PushAxisAlignedClip(
        D2D1::RectF(0, m_listTop, pw, m_listBottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    switch (m_activeTab) {
        case PanelTab::Library:
            drawLibraryList(left, right, m_listTop, m_listBottom);
            break;
        case PanelTab::Playlists:
            if (m_viewedPlaylist >= 0)
                drawPlaylistTracks(left, right, m_listTop, m_listBottom);
            else
                drawPlaylistList(left, right, m_listTop, m_listBottom);
            break;
        case PanelTab::Queue:
            drawQueueList(left, right, m_listTop, m_listBottom);
            break;
    }

    m_renderTarget->PopAxisAlignedClip();

    // ── Minimal scrollbar ──
    if (m_activeTab == PanelTab::Library && m_library && m_library->count() > 0) {
        int totalRows = m_library->count();
        float listH = m_listBottom - m_listTop;
        // Rough estimate of visible rows (mix of headers and tracks)
        float avgRowH = (ROW_HEIGHT + 44) * 0.5f; // average of track and header heights
        float visibleRows = listH / avgRowH;
        if (totalRows > visibleRows) {
            float scrollbarW = 3.0f;
            float scrollbarX = pw - scrollbarW - 1;
            float trackH = listH;
            float thumbRatio = std::min(visibleRows / totalRows, 1.0f);
            float thumbH = std::max(trackH * thumbRatio, 20.0f);
            float scrollRatio = static_cast<float>(m_scrollOffset) / std::max(1.0f, static_cast<float>(totalRows) - visibleRows);
            float thumbY = m_listTop + scrollRatio * (trackH - thumbH);

            // Track background
            m_barBgBrush->SetOpacity(0.15f);
            m_renderTarget->FillRectangle(
                D2D1::RectF(scrollbarX, m_listTop, scrollbarX + scrollbarW, m_listBottom),
                m_barBgBrush);
            m_barBgBrush->SetOpacity(1.0f);

            // Thumb
            m_dimBrush->SetOpacity(0.4f);
            m_renderTarget->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(scrollbarX, thumbY, scrollbarX + scrollbarW, thumbY + thumbH), 1.5f, 1.5f),
                m_dimBrush);
            m_dimBrush->SetOpacity(1.0f);
        }
    }
}

// ── Welcome / Empty State ─────────────────────────────────────

void Renderer::drawWelcome(float w, float h) {
    float cx = m_playerLeft + (w - m_playerLeft) * 0.5f;

    // All welcome content must stay above the progress bar
    float barTop = m_progressBarRect.top;
    // Total content: icon(60) + gap(8) + title(28) + subtitle(18) + gap(20) + 4 hints(96) = ~230
    float contentH = 230.0f;
    float startY = (barTop - contentH) * 0.4f; // bias upward
    if (startY < 30) startY = 30;

    float y = startY;

    // App icon placeholder
    static const wchar_t ICON[] = L"\xE8D6"; // music note
    if (m_iconFormat) {
        D2D1_RECT_F iconR = D2D1::RectF(cx - 30, y, cx + 30, y + 60);
        m_iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_iconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_accentBrush->SetOpacity(0.4f);
        m_renderTarget->DrawText(ICON, 1, m_iconFormat, iconR, m_accentBrush);
        m_accentBrush->SetOpacity(1.0f);
        y += 68;
    }

    // Title
    if (m_titleFormat) {
        D2D1_RECT_F tR = D2D1::RectF(cx - 200, y, cx + 200, y + 28);
        m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(L"Wave", 4, m_titleFormat, tR, m_textBrush);
        y += 30;
    }

    // Subtitle
    if (m_bodyFormat) {
        D2D1_RECT_F sR = D2D1::RectF(cx - 250, y, cx + 250, y + 18);
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(L"Lightweight audio player for local music",
                                  41, m_bodyFormat, sR, m_dimBrush);
        y += 28;
    }

    // Hints — only draw if enough space above the bar
    if (m_listFormat && y + 96 < barTop - 10) {
        m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

        const wchar_t* hints[] = {
            L"File \x2192 Open Folder  to load your music library",
            L"File \x2192 Open File  to play a single track",
            L"Ctrl+O  open file  |  Ctrl+Shift+O  open folder",
            L"Space  play/pause  |  \x2190\x2192  seek  |  \x2191\x2193  volume",
        };

        for (int i = 0; i < 4; i++) {
            D2D1_RECT_F r = D2D1::RectF(cx - 280, y, cx + 280, y + 20);
            auto* brush = (i < 2) ? m_dimBrush : m_barBgBrush;
            m_renderTarget->DrawText(hints[i], static_cast<UINT32>(wcslen(hints[i])),
                                      m_listFormat, r, brush);
            y += 24;
        }
    }

    // Version — bottom of window
    if (m_listSmallFormat) {
        D2D1_RECT_F vR = D2D1::RectF(cx - 100, h - 30, cx + 100, h - 12);
        m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(L"v0.1.0-beta", 11, m_listSmallFormat, vR, m_barBgBrush);
    }
}

// ── Now Playing (polished) ────────────────────────────────────

static std::wstring getFileExtUpper(const std::wstring& path) {
    auto dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return {};
    std::wstring ext = path.substr(dot + 1);
    for (auto& c : ext) c = towupper(c);
    return ext;
}

void Renderer::drawNowPlaying(float /*centerY*/, float w, float margin) {
    PlaybackState state = m_engine ? m_engine->state() : PlaybackState::Stopped;
    const TrackInfo* playing = nullptr;
    if (m_engine && state != PlaybackState::Stopped)
        playing = m_library ? m_library->current() : nullptr;

    // ── Album art ─────────────────────────────────────────
    // Hard boundary: everything must fit above the progress bar
    float barTop = m_progressBarRect.top;
    float topMargin = 16.0f;
    float textBlockH = 70.0f; // title + subtitle + state/badge
    float bottomGap = 12.0f;  // gap between text block and progress bar
    float artGap = 12.0f;     // gap between art and text
    float maxArtSpace = barTop - topMargin - textBlockH - artGap - bottomGap;
    float artSize = std::clamp(maxArtSpace, 80.0f, 340.0f);
    float playerCX = m_playerLeft + (w - m_playerLeft) * 0.5f;
    float artX = playerCX - artSize * 0.5f;
    // Center the art+text block in available space above progress bar
    float totalH = artSize + artGap + textBlockH;
    float availH = barTop - topMargin - bottomGap;
    float artY = topMargin + (availH - totalH) * 0.4f; // bias upward
    if (artY < topMargin) artY = topMargin;

    // Load/cache art — only when track path changes (loadForTrack checks internally)
    if (m_albumArt && m_renderTarget) {
        if (playing) {
            if (playing->fullPath != m_albumArt->cachedPath())
                m_albumArt->loadForTrack(playing->fullPath, m_renderTarget);
        } else if (state == PlaybackState::Stopped && m_albumArt->hasArt()) {
            m_albumArt->clear();
        }
    }

    // Draw art container (rounded rect background)
    D2D1_RECT_F artRect = D2D1::RectF(artX, artY, artX + artSize, artY + artSize);
    m_renderTarget->FillRoundedRectangle(
        D2D1::RoundedRect(artRect, 8, 8), m_barBgBrush);

    if (m_albumArt && m_albumArt->hasArt()) {
        // Draw the actual album art, clipped to rounded rect
        m_renderTarget->PushAxisAlignedClip(artRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        m_renderTarget->DrawBitmap(m_albumArt->bitmap(), artRect,
                                    1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        m_renderTarget->PopAxisAlignedClip();
    } else {
        // Placeholder: music note icon
        static const wchar_t MUSIC_NOTE[] = L"\xE8D6"; // Segoe MDL2 Assets
        if (m_iconFormat)
            m_renderTarget->DrawText(MUSIC_NOTE, 1, m_iconFormat, artRect, m_dimBrush);
    }

    // ── Text info below art ───────────────────────────────
    float textLeft = m_playerLeft + margin;
    float textRight = w - margin;
    float textY = artY + artSize + artGap;
    // Clamp: never let text extend past the progress bar
    float maxTextY = barTop - bottomGap - textBlockH;
    if (textY > maxTextY) textY = maxTextY;

    std::wstring titleText = L"Wave";
    std::wstring subtitleText;
    std::wstring formatText;

    if (playing) {
        titleText = playing->displayTitle();
        subtitleText = playing->displayArtistAlbum();
        formatText = getFileExtUpper(playing->fullPath);
    } else if (m_engine && state != PlaybackState::Stopped) {
        const auto& fn = m_engine->fileNameW();
        titleText = fn.empty() ? L"Playing" : fn;
    }

    // Title
    if (m_titleFormat) {
        D2D1_RECT_F tR = D2D1::RectF(textLeft, textY, textRight, textY + 26);
        m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(titleText.c_str(), static_cast<UINT32>(titleText.size()),
                                  m_titleFormat, tR, m_textBrush);
    }

    // Artist — Album
    if (!subtitleText.empty() && m_bodyFormat) {
        D2D1_RECT_F sR = D2D1::RectF(textLeft, textY + 26, textRight, textY + 44);
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(subtitleText.c_str(), static_cast<UINT32>(subtitleText.size()),
                                  m_bodyFormat, sR, m_dimBrush);
    }

    // Format badge (e.g., "FLAC") — small pill next to state
    float badgeY = subtitleText.empty() ? textY + 26 : textY + 46;
    const wchar_t* stateText = L"Stopped";
    if (state == PlaybackState::Playing) stateText = L"Playing";
    else if (state == PlaybackState::Paused) stateText = L"Paused";

    if (m_bodyFormat) {
        float stateW = 60;
        float cx = m_playerLeft + (w - m_playerLeft) * 0.5f;
        D2D1_RECT_F stR = D2D1::RectF(cx - stateW, badgeY, cx + stateW, badgeY + 18);
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(stateText, static_cast<UINT32>(wcslen(stateText)),
                                  m_bodyFormat, stR, m_dimBrush);
    }

    // Format pill badge
    if (!formatText.empty() && m_badgeFormat) {
        float cx = m_playerLeft + (w - m_playerLeft) * 0.5f;
        float pillW = 36, pillH = 16;
        float pillX = cx + 50;
        D2D1_RECT_F pill = D2D1::RectF(pillX, badgeY + 1, pillX + pillW, badgeY + 1 + pillH);
        m_renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(pill, 3, 3), m_accentBrush);
        m_renderTarget->DrawText(formatText.c_str(), static_cast<UINT32>(formatText.size()),
                                  m_badgeFormat, pill, m_bgBrush);
    }
}

int Renderer::coverFlowHitTest(float x, float y) const {
    // Check back-to-front (last drawn = on top = check first)
    for (int i = static_cast<int>(m_cfHitRects.size()) - 1; i >= 0; i--) {
        if (ptInRect(x, y, m_cfHitRects[i].rect))
            return m_cfHitRects[i].albumIndex;
    }
    return -1;
}

// ── Cover Flow (iPod-style 3D) ──────────────────────────────

void Renderer::drawCoverFlow() {
    if (!m_coverFlow || m_coverFlow->empty() || !m_renderTarget) return;

    D2D1_SIZE_F sz = m_renderTarget->GetSize();
    float w = sz.width;
    float barTop = m_progressBarRect.top;

    float areaLeft = m_playerLeft;
    float areaW = w - areaLeft;
    float cx = areaLeft + areaW * 0.5f;

    // Art size: large center cover
    float textZone = 60.0f; // room for album title below
    float reflectionH = 40.0f; // reflection space
    float availH = barTop - 20 - textZone - reflectionH;
    float artSize = std::clamp(availH * 0.7f, 120.0f, 320.0f);
    float artTop = 20 + (availH - artSize) * 0.35f; // bias upward

    float so = m_coverFlow->smoothOffset();
    int albumCount = m_coverFlow->albumCount();

    // iPod-style layout: center is full-size face-on, sides are tilted (narrower)
    float sideW = artSize * 0.28f;       // side covers appear as narrow slivers
    float sideH = artSize * 0.85f;       // slightly shorter
    float centerGap = artSize * 0.52f;   // gap between center and first side
    float sideSpacing = sideW * 0.72f;   // tight packing of side items
    int visibleRange = 6;

    struct DrawItem {
        int index;
        float absD, d;
        float x, y, itemW, itemH, opacity;
    };
    std::vector<DrawItem> items;

    for (int i = 0; i < albumCount; i++) {
        float d = static_cast<float>(i) - so;
        float ad = std::abs(d);
        if (ad > visibleRange + 0.5f) continue;

        DrawItem item;
        item.index = i;
        item.absD = ad;
        item.d = d;

        if (ad <= 0.5f) {
            // Center item: full size, face-on
            float t = ad * 2.0f; // 0 at center, 1 at edge
            item.itemW = artSize * (1.0f - t * (1.0f - sideW / artSize));
            item.itemH = artSize * (1.0f - t * (1.0f - sideH / artSize));
            item.x = cx + d * centerGap * 2.0f - item.itemW * 0.5f;
            item.y = artTop + (artSize - item.itemH) * 0.5f;
            item.opacity = 1.0f;
        } else {
            // Side items: narrow, tilted look
            item.itemW = sideW;
            item.itemH = sideH;
            float sign = (d > 0) ? 1.0f : -1.0f;
            float sideOff = centerGap + (ad - 1.0f) * sideSpacing;
            item.x = cx + sign * sideOff - sideW * 0.5f;
            // Slightly shift away from center to avoid overlap
            if (d > 0) item.x += sideW * 0.1f;
            else item.x -= sideW * 0.1f;
            item.y = artTop + (artSize - sideH) * 0.5f;
            item.opacity = std::clamp(1.0f - (ad - 1.0f) * 0.12f, 0.25f, 0.9f);
        }

        items.push_back(item);
    }

    // Sort: farthest first, center last (on top)
    std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        return a.absD > b.absD;
    });

    // Clear hit-test rects and rebuild during drawing
    m_cfHitRects.clear();

    // Draw each cover
    for (auto& item : items) {
        D2D1_RECT_F rect = D2D1::RectF(item.x, item.y, item.x + item.itemW, item.y + item.itemH);
        bool isCenter = (item.absD < 0.5f);

        // Shadow behind cover (subtle drop shadow)
        if (isCenter) {
            D2D1_RECT_F shadow = D2D1::RectF(item.x + 4, item.y + 6,
                                               item.x + item.itemW + 4, item.y + item.itemH + 6);
            m_bgBrush->SetOpacity(0.5f);
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(shadow, 4, 4), m_bgBrush);
            m_bgBrush->SetOpacity(1.0f);
        }

        // Cover background
        m_barBgBrush->SetOpacity(item.opacity);
        m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(rect, isCenter ? 4.0f : 2.0f, isCenter ? 4.0f : 2.0f), m_barBgBrush);
        m_barBgBrush->SetOpacity(1.0f);

        // Album art
        ID2D1Bitmap* art = m_coverFlow->getArt(item.index, m_renderTarget);
        if (art) {
            m_renderTarget->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            m_renderTarget->DrawBitmap(art, rect, item.opacity,
                                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            m_renderTarget->PopAxisAlignedClip();
        } else {
            static const wchar_t NOTE[] = L"\xE8D6";
            if (m_iconFormat) {
                m_dimBrush->SetOpacity(item.opacity * 0.4f);
                m_renderTarget->DrawText(NOTE, 1, m_iconFormat, rect, m_dimBrush);
                m_dimBrush->SetOpacity(1.0f);
            }
        }

        // Reflection: faded mirror below center cover (iPod style)
        if (isCenter && art) {
            float refH = item.itemH * 0.25f;
            float refTop = item.y + item.itemH + 1;
            D2D1_RECT_F refDest = D2D1::RectF(item.x, refTop, item.x + item.itemW, refTop + refH);
            // Source: bottom 25% of the bitmap (in bitmap pixel coords)
            auto bmpSize = art->GetSize();
            D2D1_RECT_F refSrc = D2D1::RectF(0, bmpSize.height * 0.75f, bmpSize.width, bmpSize.height);
            m_renderTarget->DrawBitmap(art, refDest, 0.12f,
                                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, refSrc);
            // Fade overlay to simulate reflection fade
            m_bgBrush->SetOpacity(0.75f);
            m_renderTarget->FillRectangle(refDest, m_bgBrush);
            m_bgBrush->SetOpacity(1.0f);
        }

        // Subtle edge highlight for center
        if (isCenter) {
            m_accentBrush->SetOpacity(0.5f);
            m_renderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rect, 4, 4), m_accentBrush, 1.5f);
            m_accentBrush->SetOpacity(1.0f);
        }

        // Store rect for click hit-testing
        m_cfHitRects.push_back({ item.index, rect });
    }

    // ── Focused album info ──
    auto* focused = m_coverFlow->focusedAlbum();
    float textY = artTop + artSize + reflectionH + 4;
    if (textY + textZone > barTop - 10) textY = barTop - textZone - 10;

    if (focused) {
        float textLeft = areaLeft + 40, textRight = w - 40;
        if (m_titleFormat) {
            D2D1_RECT_F tR = D2D1::RectF(textLeft, textY, textRight, textY + 24);
            m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(focused->name.c_str(),
                static_cast<UINT32>(focused->name.size()), m_titleFormat, tR, m_textBrush);
        }
        if (m_bodyFormat) {
            std::wstring sub = focused->artist + L"  \xB7  " +
                std::to_wstring(focused->trackIndices.size()) +
                ((focused->trackIndices.size() == 1) ? L" track" : L" tracks");
            D2D1_RECT_F sR = D2D1::RectF(textLeft, textY + 24, textRight, textY + 42);
            m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(sub.c_str(), static_cast<UINT32>(sub.size()),
                                      m_bodyFormat, sR, m_dimBrush);
        }
    }

    // ── Counter + hint ──
    if (m_listSmallFormat) {
        int focusNum = m_coverFlow->focusedIndex() + 1;
        int totalAlbums = m_coverFlow->albumCount();
        wchar_t cb[32]; swprintf_s(cb, 32, L"%d / %d", focusNum, totalAlbums);
        D2D1_RECT_F cR = D2D1::RectF(cx - 100, barTop - 34, cx + 100, barTop - 18);
        m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(cb, static_cast<UINT32>(wcslen(cb)), m_listSmallFormat, cR, m_dimBrush);

        D2D1_RECT_F hR = D2D1::RectF(cx - 200, barTop - 18, cx + 200, barTop - 4);
        m_renderTarget->DrawText(L"\x2190\x2192 browse  |  Enter play  |  Esc exit",
                                  40, m_listSmallFormat, hR, m_barBgBrush);
    }
}

// ── Scrub Preview ────────────────────────────────────────────

static constexpr float SCRUB_UPWARD_THRESHOLD = 30.0f;
static constexpr float SCRUB_DOWNWARD_DISMISS = 10.0f; // px below anchor to dismiss
static constexpr float SCRUB_PREVIEW_HEIGHT = 44.0f;   // compact height
static constexpr float SCRUB_ANIM_SPEED = 12.0f;       // alpha per second

void Renderer::beginScrub(float anchorY) {
    m_scrubAnchorY = anchorY;
    m_scrubMouseY = anchorY;
    m_scrubPreview = false;
    m_scrubActive = true;
    m_scrubFrac = 0;
}

void Renderer::updateScrub(float mx, float my) {
    m_scrubFrac = progressBarFraction(mx);
    m_scrubMouseY = my;

    float upDist = m_scrubAnchorY - my;

    if (!m_scrubPreview && upDist > SCRUB_UPWARD_THRESHOLD) {
        m_scrubPreview = true;
    }
    // Dismiss when cursor drops back below the bar
    if (m_scrubPreview && my > m_scrubAnchorY + SCRUB_DOWNWARD_DISMISS) {
        m_scrubPreview = false;
    }
}

void Renderer::endScrub() {
    m_scrubPreview = false;
    m_scrubActive = false;
}

void Renderer::drawScrubPreview() {
    if (!m_waveform) return;

    // Animate alpha
    float targetAlpha = m_scrubPreview ? 1.0f : 0.0f;
    float dt = 0.033f; // ~30fps
    if (m_scrubAlpha < targetAlpha)
        m_scrubAlpha = std::min(m_scrubAlpha + SCRUB_ANIM_SPEED * dt, 1.0f);
    else if (m_scrubAlpha > targetAlpha)
        m_scrubAlpha = std::max(m_scrubAlpha - SCRUB_ANIM_SPEED * dt, 0.0f);

    if (m_scrubAlpha < 0.01f) return; // fully hidden

    float barLeft  = m_progressBarRect.left;
    float barRight = m_progressBarRect.right;
    float barTop   = m_progressBarRect.top;
    float barW     = barRight - barLeft;
    if (barW <= 0) return;

    float alpha = m_scrubAlpha;

    // Compact preview: slim panel just above the progress bar
    float pvH = SCRUB_PREVIEW_HEIGHT * alpha; // slides in
    float gap = 6.0f;
    float pvBot = barTop - gap;
    float pvTop = pvBot - pvH;

    // Background with animated opacity
    if (m_renderTarget && m_panelBgBrush) {
        m_panelBgBrush->SetOpacity(alpha * 0.92f);
        D2D1_RECT_F pvRect = D2D1::RectF(barLeft, pvTop, barRight, pvBot);
        m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(pvRect, 4, 4), m_panelBgBrush);
        m_panelBgBrush->SetOpacity(1.0f);
    }

    // Waveform
    std::vector<float> envelope;
    m_waveform->getEnvelope(envelope);

    if (!envelope.empty() && pvH > 4) {
        int envCount = static_cast<int>(envelope.size());
        float midY = (pvTop + pvBot) * 0.5f;
        float halfH = pvH * 0.42f;

        // Draw thin waveform bars
        float barStep = barW / envCount;
        float barWidth = std::max(barStep - 0.8f, 1.0f);

        m_dimBrush->SetOpacity(alpha * 0.5f);
        m_accentBrush->SetOpacity(alpha);

        for (int i = 0; i < envCount; i++) {
            float x = barLeft + i * barStep;
            float amp = envelope[i] * halfH;
            if (amp < 0.3f) amp = 0.3f;

            D2D1_RECT_F wbar = D2D1::RectF(x, midY - amp, x + barWidth, midY + amp);
            float dist = std::abs(static_cast<float>(i) / envCount - m_scrubFrac);
            m_renderTarget->FillRectangle(wbar, dist < 0.04f ? m_accentBrush : m_dimBrush);
        }

        m_dimBrush->SetOpacity(1.0f);
        m_accentBrush->SetOpacity(1.0f);
    }

    // Playhead
    if (pvH > 4) {
        float phX = barLeft + barW * m_scrubFrac;
        m_accentBrush->SetOpacity(alpha);
        m_renderTarget->DrawLine(D2D1::Point2F(phX, pvTop), D2D1::Point2F(phX, pvBot),
                                  m_accentBrush, 1.5f);
        m_accentBrush->SetOpacity(1.0f);

        // Time tooltip
        if (m_engine && m_badgeFormat) {
            double previewTime = m_scrubFrac * m_engine->duration();
            wchar_t tb[16];
            formatTime(previewTime, tb, 16);

            float tw = 40, th = 16;
            float tx = std::clamp(phX - tw * 0.5f, barLeft, barRight - tw);
            D2D1_RECT_F pill = D2D1::RectF(tx, pvTop - th - 2, tx + tw, pvTop - 2);
            m_btnBrush->SetOpacity(alpha);
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(pill, 3, 3), m_btnBrush);
            m_btnBrush->SetOpacity(1.0f);
            m_textBrush->SetOpacity(alpha);
            m_renderTarget->DrawText(tb, static_cast<UINT32>(wcslen(tb)), m_badgeFormat, pill, m_textBrush);
            m_textBrush->SetOpacity(1.0f);
        }
    }
}

// ── Visualizer ───────────────────────────────────────────────

void Renderer::drawVisualizer() {
    if (!m_visualizer || m_visualizer->mode() == VisMode::Off) return;
    if (!m_layout || !m_layout->isPanelVisible(PanelId::Visualizer)) return;

    const auto& bars = m_visualizer->bars();
    int count = Visualizer::BAR_COUNT;

    float left = m_vizRect.left, right = m_vizRect.right;
    float top = m_vizRect.top, bottom = m_vizRect.bottom;
    float totalW = right - left;
    float maxH = bottom - top;
    if (totalW <= 0 || maxH <= 0) return;

    float gap = 3.0f;
    float barW = (totalW - gap * (count - 1)) / count;
    if (barW < 2.0f) barW = 2.0f;

    for (int i = 0; i < count; i++) {
        float h = bars[i] * maxH;
        if (h < 1.0f) h = 1.0f;
        float x = left + i * (barW + gap);
        float y = bottom - h;

        // Gradient effect: bar fades from accent to dimmer at bottom
        D2D1_RECT_F r = D2D1::RectF(x, y, x + barW, bottom);
        float radius = barW * 0.3f;
        m_renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(r, radius, radius), m_accentBrush);

        // Subtle cap at the top of each bar
        if (bars[i] > 0.02f) {
            D2D1_RECT_F cap = D2D1::RectF(x, y, x + barW, y + 2.0f);
            m_renderTarget->FillRectangle(cap, m_textBrush);
        }
    }
}

// ── Main Render ──────────────────────────────────────────────

void Renderer::render() {
    // Skip rendering when minimized
    if (IsIconic(m_hwnd)) return;

    createDeviceResources();
    if (!m_renderTarget) return;
    computeLayout();

    m_renderTarget->BeginDraw();
    if (m_theme) {
        auto& cc = m_theme->colors().clearColor;
        m_renderTarget->Clear(D2D1::ColorF(cc.r, cc.g, cc.b));
    } else {
        m_renderTarget->Clear(D2D1::ColorF(0.07f, 0.07f, 0.07f));
    }

    drawPanel();

    D2D1_SIZE_F sz = m_renderTarget->GetSize();
    float w = sz.width, margin = 40.0f;
    float centerY = sz.height * 0.42f;

    PlaybackState state = m_engine ? m_engine->state() : PlaybackState::Stopped;
    double pos = m_engine ? m_engine->position() : 0;
    double dur = m_engine ? m_engine->duration() : 0;
    double vol = m_engine ? m_engine->volume() : 100;

    bool showNP   = !m_layout || m_layout->isPanelVisible(PanelId::NowPlaying);
    bool showBar  = !m_layout || m_layout->isPanelVisible(PanelId::TransportBar);
    bool showBtns = !m_layout || m_layout->isPanelVisible(PanelId::TransportBtns);

    // ── Now Playing or Welcome ───────────────────────────────
    bool hasLibrary = m_library && m_library->totalCount() > 0;
    bool isIdle = (state == PlaybackState::Stopped && !hasLibrary);

    if (isIdle) {
        drawWelcome(w, sz.height);
    } else if (m_coverFlowMode && m_coverFlow && hasLibrary) {
        m_coverFlow->rebuild(m_library); // ensure albums are up to date
        m_coverFlow->update(0.033f);
        drawCoverFlow();
    } else if (showNP) {
        drawNowPlaying(centerY, w, margin);
    }

    // ── Progress Bar ─────────────────────────────────────────
    if (showBar) {
        float bx = m_progressBarRect.left, by = m_progressBarRect.top;
        float bw = m_progressBarRect.right - bx, bh = m_progressBarRect.bottom - by;
        bool barHov = (m_hover == HitZone::ProgressBar || m_pressed == HitZone::ProgressBar);
        float rbh = barHov ? 7.0f : bh;
        float rby = by + (bh - rbh) * 0.5f;

        m_renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(bx, rby, bx + bw, rby + rbh), 3, 3), m_barBgBrush);

        // During drag: show drag position. Otherwise: show playback position.
        float prog;
        if (m_scrubActive) {
            prog = m_scrubFrac;
        } else {
            prog = (dur > 0.5) ? std::clamp(static_cast<float>(pos / dur), 0.0f, 1.0f) : 0.0f;
        }

        if (prog > 0) {
            float fw = roundf(bw * prog);
            if (fw < 4.0f) fw = 4.0f;
            m_renderTarget->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(bx, rby, bx + fw, rby + rbh), 3, 3), m_accentBrush);
        }
        if ((barHov || m_scrubActive) && prog > 0) {
            float dx = roundf(bx + bw * prog);
            m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dx, rby + rbh / 2), 7, 7), m_accentBrush);
        }

        // Time + Volume
        wchar_t pb[16], db[16], tb[64];
        double displayPos = m_scrubActive ? (m_scrubFrac * dur) : pos;
        formatTime(displayPos, pb, 16); formatTime(dur, db, 16);
        swprintf_s(tb, 64, L"%s / %s", pb, db);
        float iy = by + bh + 6;
        if (m_bodyFormat) {
            m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_renderTarget->DrawText(tb, static_cast<UINT32>(wcslen(tb)), m_bodyFormat,
                D2D1::RectF(bx, iy, bx + bw / 2, iy + 20), m_dimBrush);
            // Volume + mode indicators
            std::wstring rightText;
            if (m_showShuffle) rightText += L"\xE8B1 "; // shuffle icon
            if (m_showRepeat == 1) rightText += L"\xE8EE "; // repeat all icon
            else if (m_showRepeat == 2) rightText += L"\xE8ED "; // repeat one icon
            wchar_t vb[32]; swprintf_s(vb, 32, L"Vol: %d%%", static_cast<int>(vol));
            rightText += vb;
            m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            m_renderTarget->DrawText(rightText.c_str(), static_cast<UINT32>(rightText.size()), m_bodyFormat,
                D2D1::RectF(bx + bw / 2, iy, bx + bw, iy + 20), m_dimBrush);
        }
    }

    // ── Scrub Preview Overlay ─────────────────────────────────
    drawScrubPreview();

    // ── Transport Buttons ────────────────────────────────────
    if (showBtns) {
        bool hasPrev = m_library && m_library->hasPrev();
        bool hasNext = m_library && m_library->hasNext();
        if (m_playlists && m_playlists->queueCount() > 0) hasNext = true;

        // Segoe MDL2 Assets icons
        static const wchar_t ICO_PREV[]  = L"\xE892"; // Previous
        static const wchar_t ICO_PLAY[]  = L"\xE768"; // Play
        static const wchar_t ICO_PAUSE[] = L"\xE769"; // Pause
        static const wchar_t ICO_STOP[]  = L"\xE71A"; // Stop
        static const wchar_t ICO_NEXT[]  = L"\xE893"; // Next

        bool isPlaying = (state == PlaybackState::Playing);

        drawButton(m_btnPrev,      ICO_PREV,                     hasPrev, HitZone::Prev);
        drawButton(m_btnPlayPause, isPlaying ? ICO_PAUSE : ICO_PLAY, true, HitZone::PlayPause);
        drawButton(m_btnStop,      ICO_STOP,                     isPlaying || state == PlaybackState::Paused, HitZone::Stop);
        drawButton(m_btnNext,      ICO_NEXT,                     hasNext, HitZone::Next);
    }

    // ── Visualizer ─────────────────────────────────────────────
    if (m_visualizer) {
        bool playing = (state == PlaybackState::Playing);
        m_visualizer->update(playing, pos, 0.033f); // ~30fps
        drawVisualizer();
    }

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) discardDeviceResources();
}

} // namespace wave
