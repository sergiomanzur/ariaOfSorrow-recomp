#include "graphics/hd_ui_system.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace aria::graphics {

void HdUiSystem::OnWriteString(uint16_t tileX, uint16_t tileY, uint8_t tileInfo, const char* str, uint64_t currentFrame) {
    if (!str || str[0] == '\0') return;

    std::string text(str);
    // Filter out whitespace-only strings
    bool hasNonSpace = false;
    for (char c : text) {
        if (c != ' ' && c != '\t') {
            hasNonSpace = true;
            break;
        }
    }
    if (!hasNonSpace) return;

    // Check if we already have a string at this tile row/col
    for (auto& item : m_activeTexts) {
        if (item.tileX == tileX && item.tileY == tileY) {
            item.text = text;
            item.tileInfo = tileInfo;
            item.lastSeenFrame = currentFrame;
            item.isDialogue = (tileY >= 12 && text.length() > 10);
            return;
        }
    }

    CapturedText newText;
    newText.tileX = tileX;
    newText.tileY = tileY;
    newText.tileInfo = tileInfo;
    newText.text = text;
    newText.lastSeenFrame = currentFrame;
    newText.isDialogue = (tileY >= 12 && text.length() > 10);

    m_activeTexts.push_back(newText);
}

void HdUiSystem::RenderOverlay(ImDrawList* drawList,
                              float vpX, float vpY, float vpW, float vpH,
                              int baseW, int baseH,
                              uint64_t currentFrame) {
    if (!drawList || m_activeTexts.empty() || baseW <= 0 || baseH <= 0) {
        return;
    }

    float scaleX = vpW / static_cast<float>(baseW);
    float scaleY = vpH / static_cast<float>(baseH);

    // Expire strings older than 45 frames (~0.75s)
    m_activeTexts.erase(
        std::remove_if(m_activeTexts.begin(), m_activeTexts.end(),
                       [currentFrame](const CapturedText& item) {
                           return (currentFrame > item.lastSeenFrame + 45);
                       }),
        m_activeTexts.end());

    // Check if any dialogue text is active in the bottom half of the screen
    bool dialogueActive = false;
    float dialogueTopY = vpY + vpH;
    float dialogueBottomY = vpY;

    for (const auto& item : m_activeTexts) {
        if (item.isDialogue) {
            dialogueActive = true;
            float lineY = vpY + (item.tileY * 8.0f) * scaleY;
            dialogueTopY = std::min(dialogueTopY, lineY - 6.0f);
            dialogueBottomY = std::max(dialogueBottomY, lineY + 16.0f * scaleY);
        }
    }

    // Render HD sleek dialogue frame container if dialogue is active
    if (dialogueActive && dialogueBottomY > dialogueTopY) {
        float boxX1 = vpX + 16.0f * scaleX;
        float boxX2 = vpX + (baseW - 16.0f) * scaleX;
        ImVec2 boxMin(boxX1, dialogueTopY);
        ImVec2 boxMax(boxX2, dialogueBottomY + 8.0f);

        // Dark translucent background
        drawList->AddRectFilled(boxMin, boxMax, IM_COL32(14, 18, 30, 230), 6.0f);
        // Subtle gold/silver accent border
        drawList->AddRect(boxMin, boxMax, IM_COL32(195, 175, 115, 180), 6.0f, 0, 1.5f);
    }

    // Render vectorized high-resolution typography
    for (const auto& item : m_activeTexts) {
        float posX = vpX + (item.tileX * 8.0f) * scaleX;
        float posY = vpY + (item.tileY * 8.0f) * scaleY;

        ImU32 textColor = IM_COL32(245, 245, 252, 255);
        if (item.tileInfo & 0x01) {
            textColor = IM_COL32(255, 220, 120, 255); // Gold / highlight
        } else if (item.tileInfo & 0x02) {
            textColor = IM_COL32(140, 210, 255, 255); // Cyan / info
        }

        // Crisp shadow
        drawList->AddText(ImVec2(posX + 1.0f, posY + 1.0f), IM_COL32(0, 0, 0, 220), item.text.c_str());
        // Main vectorized text
        drawList->AddText(ImVec2(posX, posY), textColor, item.text.c_str());
    }
}

void HdUiSystem::Clear() {
    m_activeTexts.clear();
}

} // namespace aria::graphics
