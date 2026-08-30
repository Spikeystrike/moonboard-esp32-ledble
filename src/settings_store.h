#pragma once

#include <string>

#include "runtime_settings.h"

class SettingsStore
{
public:
    bool load(RuntimeSettings &settings, std::string &error) const;
    bool save(const RuntimeSettings &settings, std::string &error) const;
};
