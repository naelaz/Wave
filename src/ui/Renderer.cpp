#include "ui/Renderer.h"
#include "audio/Engine.h"
#include "library/Library.h"
#include "playlist/PlaylistManager.h"
#include "core/Log.h"

#include <cstdio>
#include <string>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace wave {

static bool ptInRect(float x, float y, const D2D1_RECT_F& r) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

// ── Init / Shutdown ──────────────────────────────────────────

bool Renderer::init(HWND hwnd, Engine* engine, Library* library, PlaylistManager* playlists) {
    m_hwnd = hwnd;
    m_engine = engine;
    m_library = library;
    m_playlists = playlists;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (FAILED(hr)) { log::error("D2D1CreateFactory failed"); return false; }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                              reinterpret_cast<IUnknown**>(&m_dwFactory));
    if (FAILED(hr)) { log::error("DWriteCreateFactory failed"); return false; }

    m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &m_titleFormat);
    m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &m_bodyFormat);
    m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &m_btnFormat);
    m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &m_listFormat);
    m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &m_listSmallFormat);

    if (m_titleFormat) m_titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (m_bodyFormat)  m_bodyFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (m_btnFormat) {
        m_btnFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_btnFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
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

void Renderer::createDeviceResources() {
    if (m_renderTarget) return;
    RECT rc; GetClientRect(m_hwnd, &rc);
    auto hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(rc.right, rc.bottom)),
        &m_renderTarget);
    if (FAILED(hr)) { log::error("CreateHwndRenderTarget failed"); return; }

    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.88f, 0.88f, 0.88f), &m_textBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.45f, 0.45f, 0.45f), &m_dimBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.65f, 1.0f),  &m_accentBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.07f, 0.07f, 0.07f), &m_bgBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.15f, 0.15f), &m_barBgBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.16f, 0.16f, 0.16f), &m_btnBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.22f, 0.22f), &m_btnHoverBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.12f), &m_btnPressBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.09f, 0.09f), &m_panelBgBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.25f, 0.42f), &m_rowPlayingBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.20f, 0.20f), &m_tabActiveBrush);
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
    int row = static_cast<int>((y - m_listTop) / ROW_HEIGHT) + m_scrollOffset;
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

    bool showPanel = (m_library && !m_library->empty()) ||
                     (m_playlists && (m_playlists->playlistCount() > 0 || m_playlists->queueCount() > 0));
    float pw = showPanel ? PANEL_WIDTH : 0;
    m_playerLeft = pw;
    m_panelRect = D2D1::RectF(0, 0, pw, h);

    // Tabs at top of panel
    float tabW = pw / 3.0f;
    m_tabLibrary   = D2D1::RectF(0,          0, tabW,      TAB_HEIGHT);
    m_tabPlaylists = D2D1::RectF(tabW,       0, tabW * 2,  TAB_HEIGHT);
    m_tabQueue     = D2D1::RectF(tabW * 2,   0, pw,        TAB_HEIGHT);
    m_listTop = TAB_HEIGHT;
    m_listBottom = h;

    // Player area
    float margin = 32.0f;
    float cy = h * 0.42f;
    float bx = pw + margin, bw = w - pw - margin * 2;
    m_progressBarRect = D2D1::RectF(bx, cy + 8, bx + bw, cy + 13);

    float btnW = 64, btnH = 32, gap = 8;
    float btnY = cy + 13 + 36;
    float total = btnW * 4 + gap * 3;
    float sx = pw + (w - pw - total) * 0.5f;
    m_btnPrev      = D2D1::RectF(sx,                     btnY, sx + btnW,                     btnY + btnH);
    m_btnPlayPause = D2D1::RectF(sx + btnW + gap,        btnY, sx + btnW * 2 + gap,           btnY + btnH);
    m_btnStop      = D2D1::RectF(sx + (btnW + gap) * 2,  btnY, sx + btnW * 3 + gap * 2,       btnY + btnH);
    m_btnNext      = D2D1::RectF(sx + (btnW + gap) * 3,  btnY, sx + btnW * 4 + gap * 3,       btnY + btnH);
}

// ── Drawing Helpers ──────────────────────────────────────────

void Renderer::formatTime(double s, wchar_t* buf, int len) const {
    if (s < 0) s = 0;
    int ts = static_cast<int>(s);
    swprintf_s(buf, len, L"%d:%02d", ts / 60, ts % 60);
}

void Renderer::drawButton(D2D1_RECT_F r, const wchar_t* label, bool enabled, HitZone zone) {
    auto* bg = m_btnBrush;
    if (enabled) {
        if (m_pressed == zone) bg = m_btnPressBrush;
        else if (m_hover == zone) bg = m_btnHoverBrush;
    }
    m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(r, 4, 4), bg);
    if (m_btnFormat)
        m_renderTarget->DrawText(label, static_cast<UINT32>(wcslen(label)),
                                  m_btnFormat, r, enabled ? m_textBrush : m_dimBrush);
}

void Renderer::drawTrackRow(const TrackInfo& track, float left, float right, float y,
                             bool isPlaying, bool isActive) {
    D2D1_RECT_F rowRect = D2D1::RectF(m_panelRect.left, y, m_panelRect.right, y + ROW_HEIGHT);
    if (isPlaying)
        m_renderTarget->FillRectangle(rowRect, m_rowPlayingBrush);

    auto* titleBr = isPlaying ? m_accentBrush : m_textBrush;
    const auto& title = track.displayTitle();
    D2D1_RECT_F tR = D2D1::RectF(left, y + 4, right, y + 20);
    if (m_listFormat) {
        m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_renderTarget->DrawText(title.c_str(), static_cast<UINT32>(title.size()),
                                  m_listFormat, tR, titleBr);
    }
    auto sub = track.displayArtistAlbum();
    if (!sub.empty() && m_listSmallFormat) {
        D2D1_RECT_F sR = D2D1::RectF(left, y + 21, right, y + ROW_HEIGHT - 2);
        m_listSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_renderTarget->DrawText(sub.c_str(), static_cast<UINT32>(sub.size()),
                                  m_listSmallFormat, sR, m_dimBrush);
    }
}

// ── Panel Drawing ────────────────────────────────────────────

void Renderer::drawTabs(float panelRight, float tabY) {
    auto drawTab = [&](D2D1_RECT_F r, const wchar_t* label, PanelTab tab) {
        bool active = (m_activeTab == tab);
        if (active)
            m_renderTarget->FillRectangle(r, m_tabActiveBrush);
        if (m_btnFormat) {
            m_renderTarget->DrawText(label, static_cast<UINT32>(wcslen(label)),
                                      m_btnFormat, r, active ? m_accentBrush : m_dimBrush);
        }
    };
    drawTab(m_tabLibrary,   L"Library",   PanelTab::Library);
    drawTab(m_tabPlaylists, L"Playlists", PanelTab::Playlists);
    drawTab(m_tabQueue,     L"Queue",     PanelTab::Queue);

    // Tab underline
    m_renderTarget->DrawLine(D2D1::Point2F(0, TAB_HEIGHT - 1),
                              D2D1::Point2F(panelRight, TAB_HEIGHT - 1), m_barBgBrush);
}

void Renderer::drawLibraryList(float left, float right, float top, float bottom) {
    if (!m_library || m_library->empty()) {
        if (m_listFormat) {
            D2D1_RECT_F r = D2D1::RectF(left, top + 10, right, top + 30);
            m_listFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_renderTarget->DrawText(L"No tracks loaded", 16, m_listFormat, r, m_dimBrush);
        }
        return;
    }

    int count = m_library->count();
    if (m_scrollOffset >= count) m_scrollOffset = std::max(0, count - 1);
    int visRows = static_cast<int>((bottom - top) / ROW_HEIGHT) + 1;
    int playIdx = m_library->playingIndex();

    for (int i = 0; i < visRows && (m_scrollOffset + i) < count; i++) {
        int idx = m_scrollOffset + i;
        float y = top + i * ROW_HEIGHT;
        drawTrackRow(m_library->tracks()[idx], left, right, y, idx == playIdx, false);
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
    m_renderTarget->DrawLine(D2D1::Point2F(pw, 0), D2D1::Point2F(pw, m_panelRect.bottom), m_barBgBrush);

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
}

// ── Main Render ──────────────────────────────────────────────

void Renderer::render() {
    createDeviceResources();
    if (!m_renderTarget) return;
    computeLayout();

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0.07f, 0.07f, 0.07f));

    drawPanel();

    D2D1_SIZE_F sz = m_renderTarget->GetSize();
    float w = sz.width, margin = 32.0f;
    float centerY = sz.height * 0.42f;

    PlaybackState state = m_engine ? m_engine->state() : PlaybackState::Stopped;
    double pos = m_engine ? m_engine->position() : 0;
    double dur = m_engine ? m_engine->duration() : 0;
    double vol = m_engine ? m_engine->volume() : 100;

    // ── Now Playing ──────────────────────────────────────────
    std::wstring titleText = L"Wave";
    std::wstring subtitleText;

    if (m_engine && state != PlaybackState::Stopped) {
        const TrackInfo* playing = m_library ? m_library->current() : nullptr;
        if (playing) {
            titleText = playing->displayTitle();
            subtitleText = playing->displayArtistAlbum();
        } else {
            const auto& fn = m_engine->fileNameW();
            titleText = fn.empty() ? L"Playing" : fn;
        }
    }

    D2D1_RECT_F tR = D2D1::RectF(m_playerLeft + margin, centerY - 72, w - margin, centerY - 42);
    if (m_titleFormat) {
        m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(titleText.c_str(), static_cast<UINT32>(titleText.size()),
                                  m_titleFormat, tR, m_textBrush);
    }
    if (!subtitleText.empty() && m_bodyFormat) {
        D2D1_RECT_F sR = D2D1::RectF(m_playerLeft + margin, centerY - 42, w - margin, centerY - 22);
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(subtitleText.c_str(), static_cast<UINT32>(subtitleText.size()),
                                  m_bodyFormat, sR, m_dimBrush);
    }

    const wchar_t* stateText = L"Stopped";
    if (state == PlaybackState::Playing) stateText = L"Playing";
    else if (state == PlaybackState::Paused) stateText = L"Paused";
    D2D1_RECT_F stR = D2D1::RectF(m_playerLeft + margin, centerY - 20, w - margin, centerY - 2);
    if (m_bodyFormat) {
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_renderTarget->DrawText(stateText, static_cast<UINT32>(wcslen(stateText)),
                                  m_bodyFormat, stR, m_dimBrush);
    }

    // ── Progress Bar ─────────────────────────────────────────
    float bx = m_progressBarRect.left, by = m_progressBarRect.top;
    float bw = m_progressBarRect.right - bx, bh = m_progressBarRect.bottom - by;
    bool barHov = (m_hover == HitZone::ProgressBar || m_pressed == HitZone::ProgressBar);
    float rbh = barHov ? 7.0f : bh;
    float rby = by + (bh - rbh) * 0.5f;

    m_renderTarget->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(bx, rby, bx + bw, rby + rbh), 3, 3), m_barBgBrush);

    float prog = (dur > 0) ? std::clamp(static_cast<float>(pos / dur), 0.0f, 1.0f) : 0.0f;
    if (prog > 0) {
        float fw = std::max(bw * prog, 4.0f);
        m_renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(bx, rby, bx + fw, rby + rbh), 3, 3), m_accentBrush);
    }
    if (barHov && prog > 0) {
        float dx = bx + bw * prog;
        m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dx, rby + rbh / 2), 6, 6), m_accentBrush);
    }

    // ── Time + Volume ────────────────────────────────────────
    wchar_t pb[16], db[16], tb[64];
    formatTime(pos, pb, 16); formatTime(dur, db, 16);
    swprintf_s(tb, 64, L"%s / %s", pb, db);
    float iy = by + bh + 6;
    if (m_bodyFormat) {
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_renderTarget->DrawText(tb, static_cast<UINT32>(wcslen(tb)), m_bodyFormat,
            D2D1::RectF(bx, iy, bx + bw / 2, iy + 20), m_dimBrush);
        wchar_t vb[32]; swprintf_s(vb, 32, L"Vol: %d%%", static_cast<int>(vol));
        m_bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        m_renderTarget->DrawText(vb, static_cast<UINT32>(wcslen(vb)), m_bodyFormat,
            D2D1::RectF(bx + bw / 2, iy, bx + bw, iy + 20), m_dimBrush);
    }

    // ── Transport Buttons ────────────────────────────────────
    bool hasTrack = (state != PlaybackState::Stopped);
    bool hasPrev = m_library && m_library->hasPrev();
    bool hasNext = m_library && m_library->hasNext();
    if (m_playlists && m_playlists->queueCount() > 0) hasNext = true;

    drawButton(m_btnPrev,      L"Prev",  hasPrev,  HitZone::Prev);
    drawButton(m_btnPlayPause, (state == PlaybackState::Playing) ? L"Pause" : L"Play", hasTrack, HitZone::PlayPause);
    drawButton(m_btnStop,      L"Stop",  hasTrack, HitZone::Stop);
    drawButton(m_btnNext,      L"Next",  hasNext,  HitZone::Next);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) discardDeviceResources();
}

} // namespace wave
