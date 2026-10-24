#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class Party;
class ItemTable;
class MergedPotionSettingTable;
class MergedReagentSettingTable;

struct GameplayScrollMenuEntry
{
    size_t memberIndex = 0;
    uint8_t gridX = 0;
    uint8_t gridY = 0;
    uint32_t objectDescriptionId = 0;
    uint32_t quantity = 1;
    uint16_t potionPower = 0;
    std::string label;
};

struct GameplayItemMenuLayout
{
    size_t maxVisibleRows = 0;
    float rowHeight = 0.0f;
    float width = 0.0f;
    float iconBoxWidth = 0.0f;
    float iconBoxHeight = 0.0f;
};

inline GameplayItemMenuLayout gameplayItemMenuLayout(bool potions)
{
    return potions ? GameplayItemMenuLayout{6, 40.0f, 165.0f, 26.0f, 34.0f}
                   : GameplayItemMenuLayout{8, 24.0f, 150.0f, 18.0f, 18.0f};
}

std::vector<GameplayScrollMenuEntry> collectGameplayScrollMenuEntries(const Party &party, const ItemTable &itemTable);
std::vector<GameplayScrollMenuEntry> collectGameplayPotionMenuEntries(
    const Party &party,
    const ItemTable &itemTable,
    const MergedPotionSettingTable *pPotionSettingTable,
    const MergedReagentSettingTable *pReagentSettingTable);
} // namespace OpenYAMM::Game
