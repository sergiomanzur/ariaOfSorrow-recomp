#include <iostream>
#include <cassert>
#include "../src/core/rom_validator.hpp"

void TestEmptyRom() {
    auto res = aria::core::RomValidator::ValidateBuffer(nullptr, 0);
    assert(!res.isValid);
    assert(!res.errorMessage.empty());
    std::cout << "[PASS] TestEmptyRom\n";
}

void TestCorruptedRom() {
    std::vector<uint8_t> fakeData(1024, 0xFF);
    auto res = aria::core::RomValidator::ValidateBuffer(fakeData.data(), fakeData.size());
    assert(!res.isValid);
    assert(res.revision == aria::core::RomRevision::Unknown);
    std::cout << "[PASS] TestCorruptedRom\n";
}

void TestRealRomFile() {
    const std::string romPath = "Castlevania - Aria of Sorrow (USA).gba";
    auto res = aria::core::RomValidator::ValidateFile(romPath);
    if (res.isValid) {
        assert(res.revision == aria::core::RomRevision::USA_v1_0);
        assert(res.sha1 == aria::core::RomValidator::USA_SHA1);
        assert(res.md5 == aria::core::RomValidator::USA_MD5);
        assert(res.sizeBytes == aria::core::RomValidator::EXPECTED_ROM_SIZE);
        std::cout << "[PASS] TestRealRomFile: USA ROM successfully verified (" << res.title << ", code: " << res.gameCode << ")\n";
    } else {
        std::cout << "[WARN] TestRealRomFile: ROM file not present in test cwd, error: " << res.errorMessage << "\n";
    }
}

int main() {
    std::cout << "--- Running RomValidator Unit Tests ---\n";
    TestEmptyRom();
    TestCorruptedRom();
    TestRealRomFile();
    std::cout << "--- All RomValidator Tests Passed! ---\n";
    return 0;
}
