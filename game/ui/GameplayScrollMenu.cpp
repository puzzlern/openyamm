#include "game/ui/GameplayScrollMenu.h"

#include "game/items/ItemRuntime.h"
#include "game/items/InventoryItemUseRuntime.h"
#include "game/party/Party.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"

#include <unordered_map>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
std::vector<GameplayScrollMenuEntry> collectMenuEntries(
    const Party &party,
    const ItemTable &itemTable,
    const bool potionsOnly,
    const MergedPotionSettingTable *pPotionSettingTable,
    const MergedReagentSettingTable *pReagentSettingTable)
{
    std::vector<GameplayScrollMenuEntry> entries;
    std::unordered_map<uint64_t, size_t> entryIndexes;
    const std::vector<Character> &members = party.members();
    for (size_t memberIndex = 0; memberIndex < members.size(); ++memberIndex)
    {
        const Character &member = members[memberIndex];
        for (const InventoryItem &item : member.inventory)
        {
            const ItemDefinition *pDefinition = itemTable.get(item.objectDescriptionId);
            if (pDefinition == nullptr)
            {
                continue;
            }
            const MergedPotionSettingEntry *pPotionSetting = potionsOnly && pPotionSettingTable != nullptr
                ? pPotionSettingTable->getByItemId(item.objectDescriptionId) : nullptr;
            const bool isPotion = potionsOnly
                && pPotionSetting != nullptr
                && pPotionSetting->drinkable
                && InventoryItemUseRuntime::classifyItemUse(
                    item, itemTable, pPotionSettingTable, pReagentSettingTable)
                    == InventoryItemUseAction::ConsumePotion;
            if ((potionsOnly && !isPotion) || (!potionsOnly && pDefinition->equipStat != "Sscroll"))
            {
                continue;
            }

            const uint16_t potionPower = potionsOnly ? std::max<uint16_t>(1, item.standardEnchantPower) : 0;
            const uint64_t key = (static_cast<uint64_t>(item.objectDescriptionId) << 17)
                | (static_cast<uint64_t>(item.identified) << 16)
                | potionPower;
            const auto existing = entryIndexes.find(key);
            if (existing != entryIndexes.end())
            {
                entries[existing->second].quantity += item.quantity;
                continue;
            }

            const std::string itemName = item.identified
                ? ItemRuntime::displayName(item, *pDefinition)
                : "<unidentified>";
            GameplayScrollMenuEntry entry = {};
            entry.memberIndex = memberIndex;
            entry.gridX = item.gridX;
            entry.gridY = item.gridY;
            entry.objectDescriptionId = item.objectDescriptionId;
            entry.quantity = item.quantity;
            entry.label = itemName;
            entry.potionPower = potionPower;
            if (potionsOnly)
            {
                entry.label += " (Power " + std::to_string(potionPower) + ")";
            }
            entryIndexes.emplace(key, entries.size());
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}
} // namespace

std::vector<GameplayScrollMenuEntry> collectGameplayScrollMenuEntries(const Party &party, const ItemTable &itemTable)
{
    return collectMenuEntries(party, itemTable, false, nullptr, nullptr);
}

std::vector<GameplayScrollMenuEntry> collectGameplayPotionMenuEntries(
    const Party &party,
    const ItemTable &itemTable,
    const MergedPotionSettingTable *pPotionSettingTable,
    const MergedReagentSettingTable *pReagentSettingTable)
{
    return collectMenuEntries(party, itemTable, true, pPotionSettingTable, pReagentSettingTable);
}
} // namespace OpenYAMM::Game
