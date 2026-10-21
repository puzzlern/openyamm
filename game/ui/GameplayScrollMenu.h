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

std::vector<GameplayScrollMenuEntry> collectGameplayScrollMenuEntries(const Party &party, const ItemTable &itemTable);
std::vector<GameplayScrollMenuEntry> collectGameplayPotionMenuEntries(
    const Party &party,
    const ItemTable &itemTable,
    const MergedPotionSettingTable *pPotionSettingTable,
    const MergedReagentSettingTable *pReagentSettingTable);
} // namespace OpenYAMM::Game
