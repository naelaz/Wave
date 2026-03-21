#include "plugin/PluginHost.h"
#include "app/App.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <filesystem>

// SEH wrapper — must be in a function with no C++ destructors
typedef int (*InitFnPtr)(const WaveHostAPI*);
static int safeCallInit(InitFnPtr fn, const WaveHostAPI* api) {
    __try { return fn(api); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -999; }
}
static int safeCallVoid(void (*fn)()) {
    __try { fn(); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static int safeCallEvent(void (*fn)(WaveEventType), WaveEventType e) {
    __try { fn(e); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static int safeCallCb(void (*fn)(WaveEventType, void*), WaveEventType e, void* ud) {
    __try { fn(e, ud); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static int safeCallCmd(void (*fn)(void*), void* ud) {
    __try { fn(ud); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

namespace wave {

// ── Static host pointer for C callbacks ──────────────────────
// The C ABI callbacks need access to the PluginHost instance.
// Since there's only ever one, a static pointer is safe.
static PluginHost* s_host = nullptr;
static App* s_app = nullptr;

// ── C ABI callback implementations ──────────────────────────

static void api_play()  { if (s_app) { s_app->playTrack(s_app->library().playingIndex()); } }
static void api_pause() { if (s_app) s_app->engine().togglePause(); }
static void api_stop()  { if (s_app) s_app->engine().stop(); }
static void api_seekTo(double s)  { if (s_app) s_app->engine().seekAbsolute(s); }
static void api_setVolume(double v) { if (s_app) s_app->engine().setVolume(v); }

static void api_getPlaybackInfo(WavePlaybackInfo* out) {
    if (!out || !s_app) return;
    auto& eng = s_app->engine();
    auto& lib = s_app->library();
    out->state    = static_cast<int>(eng.state());
    out->position = eng.position();
    out->duration = eng.duration();
    out->volume   = eng.volume();

    // Point to library's current track strings (valid until library changes)
    static std::wstring s_title, s_artist, s_album, s_path;
    const auto* t = lib.current();
    if (t) {
        s_title  = t->displayTitle();
        s_artist = t->artist;
        s_album  = t->album;
        s_path   = t->fullPath;
    } else {
        s_title = s_artist = s_album = s_path = L"";
    }
    out->title    = s_title.c_str();
    out->artist   = s_artist.c_str();
    out->album    = s_album.c_str();
    out->filePath = s_path.c_str();
}

static int api_registerCommand(const char* id, const char* name,
                                void (*cb)(void*), void* ud) {
    if (!s_host || !id || !name || !cb) return -1;
    s_host->apiRegisterCommand(id, name, cb, ud, "");
    return 0;
}

static int api_subscribe(WaveEventType event,
                           void (*cb)(WaveEventType, void*), void* ud) {
    if (!s_host || !cb) return -1;
    s_host->apiSubscribe(event, cb, ud);
    return 0;
}

static void api_log(const char* level, const char* msg) {
    if (!level || !msg) return;
    std::string prefix = "[plugin] ";
    std::string full = prefix + msg;
    if (std::string(level) == "error") log::error(full);
    else if (std::string(level) == "warn") log::warn(full);
    else log::info(full);
}

// ── PluginHost implementation ────────────────────────────────

void PluginHost::buildHostAPI() {
    m_hostAPI.play             = api_play;
    m_hostAPI.pause            = api_pause;
    m_hostAPI.stop             = api_stop;
    m_hostAPI.seekTo           = api_seekTo;
    m_hostAPI.setVolume        = api_setVolume;
    m_hostAPI.getPlaybackInfo  = api_getPlaybackInfo;
    m_hostAPI.registerCommand  = api_registerCommand;
    m_hostAPI.subscribe        = api_subscribe;
    m_hostAPI.log              = api_log;
    m_apiReady = true;
}

std::wstring PluginHost::pluginsDir() const {
    // Plugins directory next to Wave.exe
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    auto slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash);
    dir += L"\\plugins";
    return dir;
}

void PluginHost::init() {
    s_host = this;
    s_app = m_app;
    buildHostAPI();
    discoverPlugins();

    for (auto& pi : m_plugins) {
        if (loadPlugin(pi)) {
            initPlugin(pi);
        }
    }

    log::info("PluginHost: " + std::to_string(m_plugins.size()) + " plugin(s) discovered");
}

void PluginHost::shutdown() {
    for (auto& pi : m_plugins) {
        shutdownPlugin(pi);
        unloadPlugin(pi);
    }
    m_plugins.clear();
    m_eventSubs.clear();
    m_commands.clear();
    s_host = nullptr;
    s_app = nullptr;
    log::info("PluginHost: shut down");
}

// ── Discovery ────────────────────────────────────────────────

void PluginHost::discoverPlugins() {
    std::wstring dir = pluginsDir();
    CreateDirectoryW(dir.c_str(), nullptr);

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return;

    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().wstring();
        // Case-insensitive .dll check
        if (ext.size() == 4) {
            for (auto& c : ext) c = towlower(c);
            if (ext != L".dll") continue;
        } else continue;

        PluginInstance pi;
        pi.dllPath = entry.path().wstring();
        pi.state = WAVE_PLUGIN_DISCOVERED;
        m_plugins.push_back(std::move(pi));
    }
}

// ── Load / Init / Shutdown / Unload ──────────────────────────

bool PluginHost::loadPlugin(PluginInstance& pi) {
    pi.hModule = LoadLibraryW(pi.dllPath.c_str());
    if (!pi.hModule) {
        log::warn("PluginHost: failed to load " + platform::toUtf8(pi.dllPath));
        pi.state = WAVE_PLUGIN_ERROR;
        return false;
    }

    pi.fnGetInfo  = reinterpret_cast<PluginInstance::GetInfoFn>(
                        GetProcAddress(pi.hModule, "wave_plugin_get_info"));
    pi.fnInit     = reinterpret_cast<PluginInstance::InitFn>(
                        GetProcAddress(pi.hModule, "wave_plugin_init"));
    pi.fnShutdown = reinterpret_cast<PluginInstance::ShutdownFn>(
                        GetProcAddress(pi.hModule, "wave_plugin_shutdown"));
    pi.fnOnEvent  = reinterpret_cast<PluginInstance::OnEventFn>(
                        GetProcAddress(pi.hModule, "wave_plugin_on_event"));

    if (!pi.fnGetInfo || !pi.fnInit || !pi.fnShutdown) {
        log::warn("PluginHost: missing required exports in " +
                  platform::toUtf8(pi.dllPath));
        FreeLibrary(pi.hModule);
        pi.hModule = nullptr;
        pi.state = WAVE_PLUGIN_ERROR;
        return false;
    }

    pi.state = WAVE_PLUGIN_LOADED;

    // Read metadata
    WavePluginInfo* info = pi.fnGetInfo();
    if (info) {
        if (info->apiVersion != WAVE_PLUGIN_API_VERSION) {
            log::warn("PluginHost: API version mismatch for " +
                      std::string(info->id ? info->id : "unknown"));
            FreeLibrary(pi.hModule);
            pi.hModule = nullptr;
            pi.state = WAVE_PLUGIN_ERROR;
            return false;
        }
        pi.id          = info->id          ? info->id          : "";
        pi.name        = info->name        ? info->name        : "";
        pi.author      = info->author      ? info->author      : "";
        pi.version     = info->version     ? info->version     : "";
        pi.description = info->description ? info->description : "";
    }

    log::info("PluginHost: loaded '" + pi.name + "' v" + pi.version);
    return true;
}

void PluginHost::initPlugin(PluginInstance& pi) {
    if (pi.state != WAVE_PLUGIN_LOADED) return;

    int result = safeCallInit(pi.fnInit, &m_hostAPI);
    if (result == -999) {
        log::error("PluginHost: crash in init for '" + pi.name + "'");
        pi.state = WAVE_PLUGIN_ERROR;
        return;
    }
    if (result != 0) {
        log::warn("PluginHost: init failed for '" + pi.name + "' (code " +
                  std::to_string(result) + ")");
        pi.state = WAVE_PLUGIN_ERROR;
        return;
    }

    pi.state = WAVE_PLUGIN_ACTIVE;
    log::info("PluginHost: initialized '" + pi.name + "'");
}

void PluginHost::shutdownPlugin(PluginInstance& pi) {
    if (pi.state != WAVE_PLUGIN_ACTIVE || !pi.fnShutdown) return;
    if (safeCallVoid(pi.fnShutdown) != 0)
        log::error("PluginHost: crash in shutdown for '" + pi.name + "'");
}

void PluginHost::unloadPlugin(PluginInstance& pi) {
    if (pi.hModule) {
        FreeLibrary(pi.hModule);
        pi.hModule = nullptr;
    }
    pi.state = WAVE_PLUGIN_UNLOADED;
}

// ── Events ───────────────────────────────────────────────────

void PluginHost::fireEvent(WaveEventType event) {
    for (auto& sub : m_eventSubs) {
        if (sub.event == event) {
            if (safeCallCb(sub.callback, event, sub.userData) != 0)
                log::error("PluginHost: crash in event callback");
        }
    }
    for (auto& pi : m_plugins) {
        if (pi.state == WAVE_PLUGIN_ACTIVE && pi.fnOnEvent) {
            if (safeCallEvent(pi.fnOnEvent, event) != 0) {
                log::error("PluginHost: crash in on_event for '" + pi.name + "'");
                pi.state = WAVE_PLUGIN_ERROR;
            }
        }
    }
}

// ── Commands ─────────────────────────────────────────────────

bool PluginHost::executeCommand(const std::string& id) {
    for (auto& cmd : m_commands) {
        if (cmd.id == id && cmd.callback) {
            if (safeCallCmd(cmd.callback, cmd.userData) != 0) {
                log::error("PluginHost: crash in command '" + id + "'");
                return false;
            }
            return true;
        }
    }
    return false;
}

void PluginHost::apiRegisterCommand(const char* id, const char* name,
                                      void (*cb)(void*), void* ud,
                                      const std::string& pluginId) {
    RegisteredCommand cmd;
    cmd.id = id;
    cmd.name = name;
    cmd.callback = cb;
    cmd.userData = ud;
    cmd.pluginId = pluginId;
    m_commands.push_back(std::move(cmd));
    log::info("PluginHost: registered command '" + std::string(id) + "'");
}

void PluginHost::apiSubscribe(WaveEventType event,
                                void (*cb)(WaveEventType, void*), void* ud) {
    EventSub sub;
    sub.event = event;
    sub.callback = cb;
    sub.userData = ud;
    m_eventSubs.push_back(sub);
}

} // namespace wave
