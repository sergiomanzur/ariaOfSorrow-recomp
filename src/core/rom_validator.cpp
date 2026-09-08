#include "rom_validator.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <array>

namespace aria::core {

namespace {

// Standard SHA-1 implementation
class SHA1 {
public:
    SHA1() { reset(); }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            buffer[buffer_idx++] = data[i];
            if (buffer_idx == 64) {
                process_block(buffer);
                bit_count += 512;
                buffer_idx = 0;
            }
        }
    }

    std::string finalize() {
        bit_count += buffer_idx * 8;
        buffer[buffer_idx++] = 0x80;
        if (buffer_idx > 56) {
            while (buffer_idx < 64) buffer[buffer_idx++] = 0;
            process_block(buffer);
            buffer_idx = 0;
        }
        while (buffer_idx < 56) buffer[buffer_idx++] = 0;
        for (int i = 7; i >= 0; --i) {
            buffer[buffer_idx++] = static_cast<uint8_t>((bit_count >> (i * 8)) & 0xFF);
        }
        process_block(buffer);

        std::ostringstream ss;
        for (uint32_t val : state) {
            ss << std::hex << std::setfill('0') << std::setw(8) << val;
        }
        return ss.str();
    }

private:
    void reset() {
        state[0] = 0x67452301;
        state[1] = 0xEFCDAB89;
        state[2] = 0x98BADCFE;
        state[3] = 0x10325476;
        state[4] = 0xC3D2E1F0;
        bit_count = 0;
        buffer_idx = 0;
    }

    static uint32_t rol(uint32_t value, size_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void process_block(const uint8_t* block) {
        uint32_t w[80];
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (block[i * 4 + 0] << 24) |
                   (block[i * 4 + 1] << 16) |
                   (block[i * 4 + 2] << 8)  |
                   (block[i * 4 + 3]);
        }
        for (size_t i = 16; i < 80; ++i) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];

        for (size_t i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

    uint32_t state[5];
    uint64_t bit_count = 0;
    uint8_t buffer[64];
    size_t buffer_idx = 0;
};

// Standard MD5 implementation
class MD5 {
public:
    MD5() { reset(); }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            buffer[buffer_idx++] = data[i];
            if (buffer_idx == 64) {
                process_block(buffer);
                bit_count += 512;
                buffer_idx = 0;
            }
        }
    }

    std::string finalize() {
        bit_count += buffer_idx * 8;
        buffer[buffer_idx++] = 0x80;
        if (buffer_idx > 56) {
            while (buffer_idx < 64) buffer[buffer_idx++] = 0;
            process_block(buffer);
            buffer_idx = 0;
        }
        while (buffer_idx < 56) buffer[buffer_idx++] = 0;
        for (int i = 0; i < 8; ++i) {
            buffer[buffer_idx++] = static_cast<uint8_t>((bit_count >> (i * 8)) & 0xFF);
        }
        process_block(buffer);

        std::ostringstream ss;
        for (uint32_t val : state) {
            for (int b = 0; b < 4; ++b) {
                ss << std::hex << std::setfill('0') << std::setw(2)
                   << ((val >> (b * 8)) & 0xFF);
            }
        }
        return ss.str();
    }

private:
    void reset() {
        state[0] = 0x67452301;
        state[1] = 0xEFCDAB89;
        state[2] = 0x98BADCFE;
        state[3] = 0x10325476;
        bit_count = 0;
        buffer_idx = 0;
    }

    static uint32_t rol(uint32_t value, size_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void process_block(const uint8_t* block) {
        static const uint32_t K[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
        };

        static const uint32_t S[64] = {
            7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
            5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
            4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
            6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
        };

        uint32_t m[16];
        for (size_t i = 0; i < 16; ++i) {
            m[i] = (block[i * 4 + 0]) |
                   (block[i * 4 + 1] << 8) |
                   (block[i * 4 + 2] << 16) |
                   (block[i * 4 + 3] << 24);
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];

        for (size_t i = 0; i < 64; ++i) {
            uint32_t f, g;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }
            uint32_t temp = d;
            d = c;
            c = b;
            b = b + rol(a + f + K[i] + m[g], S[i]);
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

    uint32_t state[4];
    uint64_t bit_count = 0;
    uint8_t buffer[64];
    size_t buffer_idx = 0;
};

} // anonymous namespace

std::string RomValidator::ComputeSHA1(const uint8_t* data, size_t size) {
    SHA1 sha1;
    sha1.update(data, size);
    return sha1.finalize();
}

std::string RomValidator::ComputeMD5(const uint8_t* data, size_t size) {
    MD5 md5;
    md5.update(data, size);
    return md5.finalize();
}

RomValidationResult RomValidator::ValidateBuffer(const uint8_t* data, size_t size) {
    RomValidationResult result;
    result.sizeBytes = size;

    if (data == nullptr || size == 0) {
        result.isValid = false;
        result.errorMessage = "Empty or null ROM buffer provided.";
        return result;
    }

    // Extract GBA Header Title (0x0A0 - 0x0AB: 12 bytes) and Game Code (0x0AC - 0x0AF: 4 bytes)
    if (size >= 0x0B0) {
        char titleBuf[13] = {0};
        char gameCodeBuf[5] = {0};
        std::memcpy(titleBuf, data + 0x0A0, 12);
        std::memcpy(gameCodeBuf, data + 0x0AC, 4);
        result.title = std::string(titleBuf);
        result.gameCode = std::string(gameCodeBuf);
    }

    result.sha1 = ComputeSHA1(data, size);
    result.md5 = ComputeMD5(data, size);

    // Verify USA v1.0 Release
    if (result.sha1 == USA_SHA1 && result.md5 == USA_MD5) {
        result.isValid = true;
        result.revision = RomRevision::USA_v1_0;
        return result;
    }

    result.isValid = false;
    result.revision = RomRevision::Unknown;
    std::ostringstream ss;
    ss << "Unrecognized ROM checksum. SHA-1: " << result.sha1
       << ", MD5: " << result.md5
       << ". Expected USA release (SHA-1: " << USA_SHA1 << ")";
    result.errorMessage = ss.str();
    return result;
}

RomValidationResult RomValidator::ValidateFile(const std::string& romPath) {
    RomValidationResult result;

    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.isValid = false;
        result.errorMessage = "Failed to open ROM file: " + romPath;
        return result;
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        result.isValid = false;
        result.errorMessage = "ROM file is empty: " + romPath;
        return result;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        result.isValid = false;
        result.errorMessage = "Failed to read data from ROM file: " + romPath;
        return result;
    }

    return ValidateBuffer(buffer.data(), buffer.size());
}

} // namespace aria::core
