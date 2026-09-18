#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace aria::graphics {

struct CapturedText {
    uint16_t tileX = 0;
    uint16_t tileY = 0;
    uint8_t tileInfo = 0;
    std::string text;
    uint64_t lastSeenFrame = 0;
    bool isDialogue = false;
};

class HdUiSystem {
public:
    static HdUiSystem& Get() {
        static HdUiSystem instance;
        return instance;
    }

    void OnWriteString(uint16_t tileX, uint16_t tileY, uint8_t tileInfo, const char* str, uint64_t currentFrame);

    void RenderOverlay(ImDrawList* drawList,
                      float vpX, float vpY, float vpW, float vpH,
                      int baseW = 240, int baseH = 160,
                      uint64_t currentFrame = 0);

    void Clear();

    size_t GetActiveTextCount() const { return m_activeTexts.size(); }

private:
    HdUiSystem() = default;

    std::vector<CapturedText> m_activeTexts;
};

} // namespace aria::graphics
