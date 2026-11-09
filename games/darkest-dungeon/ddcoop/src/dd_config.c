#define WIN32_LEAN_AND_MEAN
#include "dd_config.h"

#include <windows.h>
#include <string.h>

int dd_config_load(const char *path, dd_config *cfg) {
    char role[16];
    memset(cfg, 0, sizeof *cfg);
    DWORD attr = GetFileAttributesA(path);
    int exists = attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);

    GetPrivateProfileStringA("net", "role", "off", role, sizeof role, path);
    cfg->role = _stricmp(role, "host") == 0 ? DD_ROLE_HOST : _stricmp(role, "guest") == 0 ? DD_ROLE_GUEST : DD_ROLE_OFF;
    cfg->port = (int)GetPrivateProfileIntA("net", "port", 27360, path);
    if (cfg->port < 1 || cfg->port > 65535) cfg->port = 27360;
    GetPrivateProfileStringA("net", "host", "", cfg->host, sizeof cfg->host, path);
    GetPrivateProfileStringA("net", "password", "", cfg->password, sizeof cfg->password, path);
    GetPrivateProfileStringA("coop", "coop_slots", "", cfg->coop_slots, sizeof cfg->coop_slots, path);
    cfg->verbose = (int)GetPrivateProfileIntA("log", "verbose", 0, path) != 0;
    return exists;
}

const char *dd_role_name(dd_role r) {
    switch (r) {
    case DD_ROLE_HOST: return "host";
    case DD_ROLE_GUEST: return "guest";
    default: return "off";
    }
}
