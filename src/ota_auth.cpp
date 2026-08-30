#include "ota_auth.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <cstring>

namespace
{
constexpr char NAMESPACE_NAME[] = "ota_auth";
constexpr uint8_t AUTH_SCHEMA_VERSION = 1;
constexpr size_t SALT_SIZE = 16;
constexpr size_t HASH_SIZE = 32;
constexpr size_t MIN_PASSWORD_LENGTH = 8;
constexpr size_t MAX_PASSWORD_LENGTH = 64;
constexpr unsigned int PASSWORD_HASH_ITERATIONS = 10000;

bool calculateHash(
    const uint8_t *salt,
    const std::string &password,
    uint8_t *hash)
{
    mbedtls_md_context_t context;
    mbedtls_md_init(&context);
    const mbedtls_md_info_t *sha256 =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    const bool success =
        sha256 != nullptr &&
        mbedtls_md_setup(&context, sha256, 1) == 0 &&
        mbedtls_pkcs5_pbkdf2_hmac(
            &context,
            reinterpret_cast<const unsigned char *>(password.data()),
            password.size(),
            salt,
            SALT_SIZE,
            PASSWORD_HASH_ITERATIONS,
            HASH_SIZE,
            hash) == 0;
    mbedtls_md_free(&context);
    return success;
}

bool passwordLengthValid(const std::string &password)
{
    return
        password.size() >= MIN_PASSWORD_LENGTH &&
        password.size() <= MAX_PASSWORD_LENGTH;
}
} // namespace

OtaAuthStore::OtaAuthStore()
    : configured_(false),
      salt_{},
      hash_{}
{
}

bool OtaAuthStore::load(std::string &error)
{
    configured_ = false;
    std::memset(salt_, 0, sizeof(salt_));
    std::memset(hash_, 0, sizeof(hash_));

    Preferences preferences;
    if (!preferences.begin(NAMESPACE_NAME, true))
    {
        // A missing read-only namespace is the expected state before the
        // first OTA-page visit and password setup.
        error.clear();
        return true;
    }

    const uint8_t schema = preferences.getUChar("schema", 0);
    if (schema == 0)
    {
        preferences.end();
        error.clear();
        return true;
    }

    if (
        schema != AUTH_SCHEMA_VERSION ||
        preferences.getBytesLength("salt") != sizeof(salt_) ||
        preferences.getBytesLength("hash") != sizeof(hash_))
    {
        preferences.end();
        error = "Stored OTA password data is incomplete";
        return false;
    }

    const bool loaded =
        preferences.getBytes("salt", salt_, sizeof(salt_)) == sizeof(salt_) &&
        preferences.getBytes("hash", hash_, sizeof(hash_)) == sizeof(hash_);
    preferences.end();
    if (!loaded)
    {
        error = "Could not read the stored OTA password data";
        return false;
    }

    configured_ = true;
    error.clear();
    return true;
}

bool OtaAuthStore::configured() const
{
    return configured_;
}

bool OtaAuthStore::setPassword(
    const std::string &password,
    std::string &error)
{
    if (!passwordLengthValid(password))
    {
        error = "Das Passwort muss 8 bis 64 Zeichen lang sein";
        return false;
    }

    uint8_t candidateSalt[SALT_SIZE];
    uint8_t candidateHash[HASH_SIZE];
    esp_fill_random(candidateSalt, sizeof(candidateSalt));
    if (!calculateHash(candidateSalt, password, candidateHash))
    {
        error = "Das Passwort konnte nicht verarbeitet werden";
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(NAMESPACE_NAME, false))
    {
        error = "Der OTA-Passwortspeicher konnte nicht geöffnet werden";
        return false;
    }

    // Invalidate the record before replacing it. A power loss during the
    // write never leaves a partially valid password record behind.
    bool success = preferences.putUChar("schema", 0) == sizeof(uint8_t);
    success &=
        preferences.putBytes("salt", candidateSalt, sizeof(candidateSalt)) ==
        sizeof(candidateSalt);
    success &=
        preferences.putBytes("hash", candidateHash, sizeof(candidateHash)) ==
        sizeof(candidateHash);
    if (success)
    {
        success =
            preferences.putUChar("schema", AUTH_SCHEMA_VERSION) ==
            sizeof(uint8_t);
    }
    preferences.end();

    if (!success)
    {
        error = "Das OTA-Passwort konnte nicht dauerhaft gespeichert werden";
        return false;
    }

    std::memcpy(salt_, candidateSalt, sizeof(salt_));
    std::memcpy(hash_, candidateHash, sizeof(hash_));
    configured_ = true;
    error.clear();
    return true;
}

bool OtaAuthStore::verifyPassword(const std::string &password) const
{
    if (!configured_ || !passwordLengthValid(password))
        return false;

    uint8_t candidateHash[HASH_SIZE];
    if (!calculateHash(salt_, password, candidateHash))
        return false;

    // Compare every byte to avoid revealing a matching prefix through timing.
    uint8_t difference = 0;
    for (size_t index = 0; index < sizeof(hash_); ++index)
        difference |= candidateHash[index] ^ hash_[index];
    return difference == 0;
}
