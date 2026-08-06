#pragma once
// ============================================================================
// applepie_mgr.h — Standardized Plugin Interface for Endfield
// ============================================================================
// Each plugin includes a copy of this header and implements the exported
// functions. The Applepie Manager discovers them via GetProcAddress at runtime.
//
// Exported function naming convention: AP_<FunctionName>
// All functions use extern "C" to avoid name mangling.
// ============================================================================

#include <cstdint>

#define APPLEPIE_PLUGIN_API_VERSION 1

#ifdef APPLEPIE_PLUGIN_IMPL
  // Plugin side: export the functions
  #define APPLEPIE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
  // Manager side: import / declare function pointer types
  #define APPLEPIE_PLUGIN_EXPORT extern "C" __declspec(dllimport)
#endif

// ============================================================================
// Data Structures
// ============================================================================

struct AP_PluginInfo {
    int         apiVersion;         // Must be APPLEPIE_PLUGIN_API_VERSION
    const char* id;                 // Unique identifier: "anti_afk", "eiem", etc.
    const char* displayName;        // Human-readable: "Anti-AFK 防踢出"
    const char* description;        // Brief description
    const char* configFile;         // Config filename (relative to plugin\), or nullptr
    bool        supportsHotDisable; // true if runtime pause/resume is supported
};

struct AP_HotkeyInfo {
    const char* name;               // Display name: "终结技1"
    const char* configKey;          // Config file key: "ult_1_key" (nullptr = not configurable)
    int         currentVK;          // Current virtual key code
};

// ============================================================================
// Function Pointer Types (used by the Applepie Manager)
// ============================================================================

// Required: Returns pointer to static AP_PluginInfo (must remain valid for DLL lifetime)
typedef AP_PluginInfo*  (*pfn_AP_GetPluginInfo)();

// Optional: Enable (resume) plugin functionality at runtime. Returns true on success.
typedef bool            (*pfn_AP_PluginEnable)();

// Optional: Disable (pause) plugin functionality at runtime. Hooks passthrough.
typedef bool            (*pfn_AP_PluginDisable)();

// Optional: Hot-reload config file and apply changes. Returns true on success.
typedef bool            (*pfn_AP_ReloadConfig)();

// Optional: Fill outArray with hotkey info, up to maxCount entries. Returns actual count.
typedef int             (*pfn_AP_GetHotkeys)(AP_HotkeyInfo* outArray, int maxCount);

// Optional: Set plugin UI language. langCode is "zh", "en", etc.
typedef void            (*pfn_AP_SetLanguage)(const char* langCode);
