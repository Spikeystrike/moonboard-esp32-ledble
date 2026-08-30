#pragma once

#include <stdint.h>

#include <string>

class OtaAuthStore
{
public:
    OtaAuthStore();

    bool load(std::string &error);
    bool configured() const;
    bool setPassword(const std::string &password, std::string &error);
    bool verifyPassword(const std::string &password) const;

private:
    bool configured_;
    uint8_t salt_[16];
    uint8_t hash_[32];
};
