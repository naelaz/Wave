#pragma once

#include "plugin/PluginTypes.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <functional>

namespace wave {

class App;

// Represents one loaded plugin
struct PluginInstance {
    std::wstring dllPath;
    std::string  id;
    std::string  name;
    std::string  author;
    std::string  version;
    std::string  description;
    WavePluginState state = WAVE_PLUGIN_DISCOVERED;

    // DLL handles
    HMODULE hModule = nullptr;

    // Resolved function pointers
    using GetInfoFn   = WavePluginInfo* (*)();
    using InitFn      = int (*)(const WaveHostAPI*);
    using ShutdownFn  = void (*)();
    using OnEventFn   = void (*)(WaveEventType);

    GetInfoFn   fnGetInfo   = nullptr;
    InitFn      fnInit      = nullptr;
    ShutdownFn  fnShutdown  = nullptr;
    OnEventFn   fnOnEvent   = nullptr;  // optional
};

// Internal event subscription record
struct EventSub {
    WaveEventType event;
    void (*callback)(WaveEventType, void*);
    void* userData;
};

// Internal command record
struct RegisteredCommand {
    std::string id;
    std::string name;
    void (*callback)(void*);
    void* userData;
    std::string pluginId; // which plugin registered it
};

class PluginHost {
public:
    // Set app pointer — must be called before init
    void setApp(App* app) { m_app = app; }

    // Discover plugins in the plugins directory, load and init them
    void init();

    // Shutdown all plugins, unload DLLs
    void shutdown();

    // Fire an event to all subscribed plugins
    void fireEvent(WaveEventType event);

    // Execute a registered command by id
    bool executeCommand(const std::string& id);

    // Read-only access for debug UI
    const std::vector<PluginInstance>& plugins() const { return m_plugins; }
    const std::vector<RegisteredCommand>& commands() const { return m_commands; }

    // Called by plugins through the C API
    void apiRegisterCommand(const char* id, const char* name,
                             void (*cb)(void*), void* ud, const std::string& pluginId);
    void apiSubscribe(WaveEventType event, void (*cb)(WaveEventType, void*), void* ud);

private:
    void discoverPlugins();
    bool loadPlugin(PluginInstance& pi);
    void initPlugin(PluginInstance& pi);
    void shutdownPlugin(PluginInstance& pi);
    void unloadPlugin(PluginInstance& pi);

    std::wstring pluginsDir() const;
    void buildHostAPI();

    App* m_app = nullptr;
    std::vector<PluginInstance> m_plugins;
    std::vector<EventSub> m_eventSubs;
    std::vector<RegisteredCommand> m_commands;
    WaveHostAPI m_hostAPI{};
    bool m_apiReady = false;
};

} // namespace wave
