#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace aria::core {

enum class RomRevision {
    Unknown,
    USA_v1_0,
    EUR_v1_0,
    JPN_v1_0
};

struct RomValidationResult {
    bool isValid = false;
    RomRevision revision = RomRevision::Unknown;
    std::string sha1;
    std::string md5;
    size_t sizeBytes = 0;
    std::string title;
    std::string gameCode;
    std::string errorMessage;
};

class RomValidator {
public:
    // Known Hashes for Castlevania: Aria of Sorrow
    static constexpr const char* USA_SHA1 = "abd71fe01ebb201bcc133074db1dd8c5253776c7";
    static constexpr const char* USA_MD5  = "e7470df4d241f73060d14437011b90ce";
    static constexpr size_t EXPECTED_ROM_SIZE = 8 * 1024 * 1024; // 8 MB

    static RomValidationResult ValidateFile(const std::string& romPath);
    static RomValidationResult ValidateBuffer(const uint8_t* data, size_t size);

    static std::string ComputeSHA1(const uint8_t* data, size_t size);
    static std::string ComputeMD5(const uint8_t* data, size_t size);
};

} // namespace aria::core
