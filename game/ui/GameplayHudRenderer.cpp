#include "game/ui/GameplayHudRenderer.h"

#include "game/app/KeyboardBindings.h"
#include "game/party/PartySpellSystem.h"
#include "game/tables/SpellTable.h"
#include "game/ui/SpellbookUiLayout.h"

#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/TurnBasedCombatRuntime.h"
#include "game/ui/GameplayScrollMenu.h"
#include "game/ui/GameplayUiSkin.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
bool isOverlayHudState(GameplayHudScreenState hudScreenState)
{
    return hudScreenState == GameplayHudScreenState::Dialogue
        || hudScreenState == GameplayHudScreenState::Character
        || hudScreenState == GameplayHudScreenState::Chest
        || hudScreenState == GameplayHudScreenState::Spellbook
        || hudScreenState == GameplayHudScreenState::Rest
        || hudScreenState == GameplayHudScreenState::Menu
        || hudScreenState == GameplayHudScreenState::Controls
        || hudScreenState == GameplayHudScreenState::Keyboard
        || hudScreenState == GameplayHudScreenState::VideoOptions
        || hudScreenState == GameplayHudScreenState::SaveGame
        || hudScreenState == GameplayHudScreenState::LoadGame
        || hudScreenState == GameplayHudScreenState::Journal
        || hudScreenState == GameplayHudScreenState::QuickReference;
}

std::optional<GameplayScreenRuntime::ResolvedHudLayoutElement> resolveLayout(
    GameplayScreenRuntime &context,
    const std::string &layoutId,
    float fallbackWidth,
    float fallbackHeight,
    int screenWidth,
    int screenHeight)
{
    return context.resolveHudLayoutElement(layoutId, screenWidth, screenHeight, fallbackWidth, fallbackHeight);
}

std::optional<GameplayHudTextureHandle> loadContextActionIcon(
    GameplayScreenRuntime &context,
    const GameplayContextAction &action)
{
    if (action.kind == GameplayContextActionKind::DropHeldItem
        && context.heldInventoryItem().active
        && context.itemTable() != nullptr)
    {
        const ItemDefinition *pItemDefinition =
            context.itemTable()->get(context.heldInventoryItem().item.objectDescriptionId);

        if (pItemDefinition != nullptr && !pItemDefinition->iconName.empty())
        {
            const std::optional<GameplayHudTextureHandle> itemIcon =
                context.gameplayUiRuntime().ensureItemIconTextureLoaded(pItemDefinition->iconName);

            if (itemIcon)
            {
                return itemIcon;
            }
        }
    }

    const UiLayoutManager::LayoutElement *pIcon = context.findHudLayoutElement(action.iconId);
    return pIcon != nullptr && !pIcon->primaryAsset.empty()
        ? context.gameplayUiRuntime().ensureHudTextureLoaded(pIcon->primaryAsset) : std::nullopt;
}

std::string fitContextActionLabel(
    GameplayScreenRuntime &context,
    const std::string &fontName,
    const std::string &text,
    float maxWidth,
    float textScale)
{
    if (text.empty() || context.measureHudTextWidth(fontName, text) * textScale <= maxWidth)
    {
        return text;
    }

    std::string fitted = text;

    while (fitted.size() > 4)
    {
        fitted.pop_back();
        const std::string candidate = fitted + "...";

        if (context.measureHudTextWidth(fontName, candidate) * textScale <= maxWidth)
        {
            return candidate;
        }
    }

    return "...";
}

std::string trimContextActionLabelPart(const std::string &text)
{
    const size_t begin = text.find_first_not_of(' ');

    if (begin == std::string::npos)
    {
        return "";
    }

    const size_t end = text.find_last_not_of(' ');
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> fitContextActionLabelLines(
    GameplayScreenRuntime &context,
    const std::string &fontName,
    const std::string &text,
    float maxWidth,
    float textScale)
{
    if (text.empty())
    {
        return {};
    }

    if (context.measureHudTextWidth(fontName, text) * textScale <= maxWidth)
    {
        return {text};
    }

    size_t bestSplitIndex = std::string::npos;
    float bestScore = std::numeric_limits<float>::max();

    for (size_t index = 1; index + 1 < text.size(); ++index)
    {
        if (text[index] != ' ')
        {
            continue;
        }

        const std::string firstLine = trimContextActionLabelPart(text.substr(0, index));
        const std::string secondLine = trimContextActionLabelPart(text.substr(index + 1));

        if (firstLine.empty() || secondLine.empty())
        {
            continue;
        }

        const float firstWidth = context.measureHudTextWidth(fontName, firstLine) * textScale;
        const float secondWidth = context.measureHudTextWidth(fontName, secondLine) * textScale;

        if (firstWidth <= maxWidth && secondWidth <= maxWidth)
        {
            const float score = std::max(firstWidth, secondWidth) + std::abs(firstWidth - secondWidth) * 0.25f;

            if (score < bestScore)
            {
                bestScore = score;
                bestSplitIndex = index;
            }
        }
    }

    if (bestSplitIndex != std::string::npos)
    {
        return {
            trimContextActionLabelPart(text.substr(0, bestSplitIndex)),
            trimContextActionLabelPart(text.substr(bestSplitIndex + 1))
        };
    }

    std::vector<std::string> lines;
    std::string currentLine;
    size_t wordBegin = 0;

    while (wordBegin < text.size())
    {
        while (wordBegin < text.size() && text[wordBegin] == ' ')
        {
            ++wordBegin;
        }

        if (wordBegin >= text.size())
        {
            break;
        }

        size_t wordEnd = text.find(' ', wordBegin);

        if (wordEnd == std::string::npos)
        {
            wordEnd = text.size();
        }

        const std::string word = text.substr(wordBegin, wordEnd - wordBegin);
        const std::string candidate = currentLine.empty() ? word : currentLine + " " + word;

        if (!currentLine.empty() && context.measureHudTextWidth(fontName, candidate) * textScale > maxWidth)
        {
            lines.push_back(currentLine);
            currentLine = word;

            if (lines.size() == 1)
            {
                break;
            }
        }
        else
        {
            currentLine = candidate;
        }

        wordBegin = wordEnd + 1;
    }

    if (lines.empty())
    {
        lines.push_back(fitContextActionLabel(context, fontName, text, maxWidth, textScale));
    }
    else
    {
        const size_t remainingBegin = wordBegin < text.size() ? wordBegin : text.size();
        const std::string remaining = trimContextActionLabelPart(text.substr(remainingBegin));
        const std::string secondLine = trimContextActionLabelPart(
            currentLine + (remaining.empty() ? std::string() : " " + remaining));
        lines.push_back(fitContextActionLabel(context, fontName, secondLine, maxWidth, textScale));
    }

    return lines;
}

void renderCenteredContextActionLabelLines(
    GameplayScreenRuntime &context,
    const UiLayoutManager::LayoutElement &layout,
    const GameplayResolvedHudLayoutElement &rect,
    const std::vector<std::string> &lines)
{
    if (lines.empty())
    {
        return;
    }

    const float fontScale = std::max(0.5f, layout.textScale * rect.scale);
    const float fontHeight = static_cast<float>(std::max(1, context.hudFontHeight(layout.fontName)));
    const float lineHeight = fontHeight * fontScale;
    const float lineGap = lines.size() > 1 ? std::max(1.0f, 1.0f * rect.scale) : 0.0f;
    const float totalHeight = lineHeight * static_cast<float>(lines.size())
        + lineGap * static_cast<float>(lines.size() - 1);
    float textY = std::round(rect.y + (rect.height - totalHeight) * 0.5f);

    for (const std::string &line : lines)
    {
        const float lineWidth = context.measureHudTextWidth(layout.fontName, line) * fontScale;
        const float textX = std::round(rect.x + (rect.width - lineWidth) * 0.5f);
        context.renderHudTextLine(layout.fontName, layout.textColorAbgr, line, textX, textY, fontScale);
        textY += lineHeight + lineGap;
    }
}

void renderContextAction(GameplayScreenRuntime &context, int width, int height)
{
    const GameplayContextActionState &state = context.contextActionStateReadOnly();

    if (!state.visible || state.primaryIndex >= state.actions.size())
    {
        return;
    }

    const GameplayContextAction &action = state.actions[state.primaryIndex];

    if (!context.settingsSnapshot().contextActionPopup
        && action.kind != GameplayContextActionKind::DropHeldItem)
    {
        return;
    }

    const UiLayoutManager::LayoutElement *pButtonLayout =
        context.findHudLayoutElement("OutdoorMobileContextActionButton");

    if (pButtonLayout == nullptr)
    {
        return;
    }

    const std::optional<GameplayResolvedHudLayoutElement> buttonRect =
        resolveLayout(
            context,
            "OutdoorMobileContextActionButton",
            pButtonLayout->width > 0.0f ? pButtonLayout->width : 260.0f,
            pButtonLayout->height > 0.0f ? pButtonLayout->height : 56.0f,
            width,
            height);

    if (!buttonRect)
    {
        return;
    }

    const GameplayHudPointerTarget &pressedTarget = context.interactionState().gameplayHudPressedTarget;
    const bool pressed =
        context.interactionState().gameplayHudClickLatch
        && pressedTarget.type == GameplayHudPointerTargetType::ContextActionButton
        && pressedTarget.index == state.primaryIndex;
    GameplayUiSkin::renderPanel(context, *buttonRect, false, 0.32f);

    const float iconSize = std::min(42.0f * buttonRect->scale, buttonRect->height - 10.0f * buttonRect->scale);
    const float iconX = buttonRect->x + 7.0f * buttonRect->scale;
    const float iconY = buttonRect->y + (buttonRect->height - iconSize) * 0.5f;
    const std::optional<GameplayHudTextureHandle> icon = loadContextActionIcon(context, action);

    if (icon)
    {
        context.submitHudTexturedQuad(*icon, iconX, iconY + (pressed ? buttonRect->scale : 0), iconSize, iconSize);
    }

    UiLayoutManager::LayoutElement labelLayout = {};
    labelLayout.fontName = "Fondamento";
    labelLayout.textColorAbgr = pressed ? GameplayUiSkin::Gold : GameplayUiSkin::Ivory;
    labelLayout.textAlignX = UiLayoutManager::TextAlignX::Center;
    labelLayout.textAlignY = UiLayoutManager::TextAlignY::Middle;
    labelLayout.textScale = 0.4166667f;
    labelLayout.textPadX = 0.0f;
    labelLayout.textPadY = 0.0f;

    GameplayResolvedHudLayoutElement labelRect = {};
    labelRect.x = iconX + iconSize + 8.0f * buttonRect->scale;
    labelRect.y = buttonRect->y;
    labelRect.width = std::max(1.0f, buttonRect->x + buttonRect->width - labelRect.x - 8.0f * buttonRect->scale);
    labelRect.height = buttonRect->height;
    labelRect.scale = buttonRect->scale;

    if (!action.label.empty())
    {
        const std::vector<std::string> labelLines =
            fitContextActionLabelLines(
                context,
                labelLayout.fontName,
                action.label,
                std::max(1.0f, labelRect.width - 6.0f * labelRect.scale),
                labelLayout.textScale * labelRect.scale);
        renderCenteredContextActionLabelLines(context, labelLayout, labelRect, labelLines);
    }
}

bool pointerOverRect(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &rect)
{
    const GameplayInputFrame *pInput = context.currentGameplayInputFrame();
    return pInput != nullptr
        && pInput->pointerX >= rect.x && pInput->pointerX < rect.x + rect.width
        && pInput->pointerY >= rect.y && pInput->pointerY < rect.y + rect.height;
}

uint32_t turnButtonTextColor(uint32_t baseColor, bool hovered, bool selected)
{
    return hovered ? 0xffd4efffu : selected ? GameplayUiSkin::Gold : baseColor;
}

// Shrinks a label's scale just enough for the text to fit the available width (in layout units).
float fittedTextScale(
    GameplayScreenRuntime &context, const std::string &fontName, const std::string &text, float baseScale,
    float availableWidth)
{
    const float measured = context.measureHudTextWidth(fontName, text);
    return measured * baseScale > availableWidth && measured > 0.0f ? availableWidth / measured : baseScale;
}

// "(F)", or empty while the action is unbound.
std::string hotkeyLabel(GameplayScreenRuntime &context, KeyboardAction action)
{
    const InputBinding binding = context.settingsSnapshot().keyboard.binding(action);
    return binding.kind == InputBindingKind::None ? std::string{} : "(" + inputBindingDisplayName(binding) + ")";
}

struct TurnButtonContent
{
    std::optional<GameplayHudTextureHandle> icon;
    std::string title;
    std::string detail;
    // Small count in the icon's corner, e.g. wand charges.
    std::string badge;
    uint32_t badgeColorAbgr = 0;
    // Marks a button that opens a menu upwards.
    bool menuArrow = false;
    // A page drawn behind the icon, e.g. the spellbook page a spell's art belongs on, with the texture rectangle that
    // lines up with the icon's drawn bounds.
    std::optional<GameplayHudTextureHandle> backing;
    float backingU0 = 0.0f;
    float backingV0 = 0.0f;
    float backingU1 = 1.0f;
    float backingV1 = 1.0f;
};

// The backing texture cut to an oval around the icon, fading out over its outer rings. Each ring is a stack of thin
// horizontal strips whose texture coordinates continue the icon's, so the icon sits on its own patch of the page.
void renderIconBacking(
    GameplayScreenRuntime &context,
    const TurnButtonContent &content,
    const GameplayResolvedHudLayoutElement &iconRect,
    float radiusX,
    float radiusY)
{
    if (!content.backing || iconRect.width <= 0.0f || iconRect.height <= 0.0f)
    {
        return;
    }
    const float centerX = iconRect.x + iconRect.width * 0.5f;
    const float centerY = iconRect.y + iconRect.height * 0.5f;
    const auto u = [&](float x)
    {
        return std::clamp(content.backingU0 + (x - iconRect.x) / iconRect.width * (content.backingU1 - content.backingU0),
            0.0f, 1.0f);
    };
    const auto v = [&](float y)
    {
        return std::clamp(content.backingV0 + (y - iconRect.y) / iconRect.height * (content.backingV1 - content.backingV0),
            0.0f, 1.0f);
    };
    // Outermost first; each smaller, more opaque oval covers the middle of the last. The alphas stack to an even ramp
    // (1/8, 2/8 ... of the page) from the rim to the solid core.
    const struct
    {
        float fraction;
        uint32_t alpha;
    } layers[] = {{1.0f, 0x20u}, {0.94f, 0x24u}, {0.88f, 0x2bu}, {0.82f, 0x33u}, {0.76f, 0x40u}, {0.70f, 0x55u},
        {0.64f, 0x80u}, {0.58f, 0xffu}};
    std::vector<GameplayHudBatchQuad> quads;
    for (const auto &layer : layers)
    {
        const float layerRadiusY = radiusY * layer.fraction;
        const int rows = std::max(4, static_cast<int>(std::ceil(2.0f * layerRadiusY / 1.5f)));
        const float rowHeight = 2.0f * layerRadiusY / static_cast<float>(rows);
        for (int row = 0; row < rows; ++row)
        {
            const float t = ((static_cast<float>(row) + 0.5f) * rowHeight - layerRadiusY) / layerRadiusY;
            const float halfWidth = radiusX * layer.fraction * std::sqrt(std::max(0.0f, 1.0f - t * t));
            GameplayHudBatchQuad quad;
            quad.textureHandle = content.backing->textureHandle;
            quad.x = centerX - halfWidth;
            quad.y = centerY - layerRadiusY + static_cast<float>(row) * rowHeight;
            quad.width = 2.0f * halfWidth;
            quad.height = rowHeight;
            quad.u0 = u(quad.x);
            quad.u1 = u(quad.x + quad.width);
            quad.v0 = v(quad.y);
            quad.v1 = v(quad.y + quad.height);
            quad.tintAbgr = (layer.alpha << 24) | 0x00ffffffu;
            quads.push_back(quad);
        }
    }
    context.submitHudQuadBatch(quads);
}

// The turn indicator, Pass, wand, scroll and potion buttons share one look: the turn plate art, an icon on the left,
// and the indicator's two text rows (ObsidianTurnPhase above ObsidianTurnDetail) on the right. The plate art has no
// hover or pressed variants, so state shows in the content: brighter or gold text, and a one-pixel press offset.
void renderTurnButton(
    GameplayScreenRuntime &context,
    const GameplayResolvedHudLayoutElement &rect,
    const TurnButtonContent &content,
    bool hovered,
    bool pressed,
    bool selected)
{
    GameplayUiSkin::renderTexture(context, "obsidian_hud_turn_normal", rect);
    const float scale = rect.scale;
    const float pressOffset = pressed ? scale : 0.0f;

    const float iconX = rect.x + 4.0f * scale;
    const float iconY = rect.y + 4.0f * scale + pressOffset;
    const float iconWidth = 38.0f * scale;
    const float iconHeight = rect.height - 8.0f * scale;
    const std::optional<GameplayHudTextureHandle> &icon = content.icon;
    if (icon && icon->width > 0 && icon->height > 0)
    {
        const float fit = std::min(iconWidth / icon->width, iconHeight / icon->height);
        const GameplayResolvedHudLayoutElement drawRect = {iconX + (iconWidth - icon->width * fit) * 0.5f,
            iconY + (iconHeight - icon->height * fit) * 0.5f, icon->width * fit, icon->height * fit, scale};
        // The oval stays inside the plate: the icon box's centre is 23 units from its left edge.
        renderIconBacking(context, content, drawRect, 22.5f * scale, rect.height * 0.5f - scale);
        context.submitHudTexturedQuad(*icon, drawRect.x, drawRect.y, drawRect.width, drawRect.height);
    }

    const UiLayoutManager::LayoutElement *pTitleTemplate = context.findHudLayoutElement("ObsidianTurnPhase");
    const UiLayoutManager::LayoutElement *pDetailTemplate = context.findHudLayoutElement("ObsidianTurnDetail");
    if (!content.badge.empty() && pDetailTemplate != nullptr)
    {
        UiLayoutManager::LayoutElement badge = *pDetailTemplate;
        badge.textAlignX = UiLayoutManager::TextAlignX::Right;
        badge.textColorAbgr = content.badgeColorAbgr != 0 ? content.badgeColorAbgr : pDetailTemplate->textColorAbgr;
        context.renderLayoutLabel(
            badge, {iconX, iconY + iconHeight - 9.6f * scale, iconWidth + 2.0f * scale, 9.6f * scale, scale},
            content.badge, false);
    }

    const float arrowSize = 9.0f * scale;
    if (content.menuArrow)
    {
        GameplayUiSkin::renderTexture(context, "obsidian_action_up",
            {rect.x + rect.width - arrowSize - 4.0f * scale, rect.y + 3.0f * scale + pressOffset, arrowSize,
                arrowSize, scale});
    }

    const float textX = rect.x + 46.0f * scale;
    const float textWidth = rect.x + rect.width - (content.menuArrow ? arrowSize + 6.0f * scale : 5.0f * scale) - textX;
    const auto drawRow = [&](const UiLayoutManager::LayoutElement *pTemplate, float y, float rowHeight,
                             const std::string &text)
    {
        if (pTemplate == nullptr || text.empty())
        {
            return;
        }
        UiLayoutManager::LayoutElement label = *pTemplate;
        label.textAlignX = UiLayoutManager::TextAlignX::Left;
        label.textPadX = 0.0f;
        label.textColorAbgr = turnButtonTextColor(pTemplate->textColorAbgr, hovered, selected);
        label.textScale = fittedTextScale(context, label.fontName, text, pTemplate->textScale, textWidth / scale);
        context.renderLayoutLabel(label, {textX, y + pressOffset, textWidth, rowHeight * scale, scale}, text, false);
    };
    if (content.detail.empty())
    {
        drawRow(pTitleTemplate, rect.y + (rect.height - 12.8f * scale) * 0.5f, 12.8f, content.title);
        return;
    }
    drawRow(pTitleTemplate, rect.y + 7.466667f * scale, 12.8f, content.title);
    drawRow(pDetailTemplate, rect.y + 21.333333f * scale, 9.6f, content.detail);
}

std::optional<GameplayHudTextureHandle> itemIcon(GameplayScreenRuntime &context, const ItemDefinition *pItem)
{
    if (pItem == nullptr || pItem->iconName.empty())
    {
        return std::nullopt;
    }
    return context.gameplayUiRuntime().ensureItemIconTextureLoaded(pItem->iconName);
}

// Fits an item icon inside a box, keeping its aspect ratio, centred.
void renderFittedItemIcon(
    GameplayScreenRuntime &context,
    const std::string &iconName,
    float boxX,
    float boxY,
    float boxWidth,
    float boxHeight)
{
    if (iconName.empty())
    {
        return;
    }
    const std::optional<GameplayHudTextureHandle> icon =
        context.gameplayUiRuntime().ensureItemIconTextureLoaded(iconName);
    if (!icon || icon->width <= 0 || icon->height <= 0)
    {
        return;
    }
    const float fit = std::min(boxWidth / icon->width, boxHeight / icon->height);
    const float iconWidth = icon->width * fit;
    const float iconHeight = icon->height * fit;
    context.submitHudTexturedQuad(
        *icon, boxX + (boxWidth - iconWidth) * 0.5f, boxY + (boxHeight - iconHeight) * 0.5f, iconWidth, iconHeight);
}

// The weapon the active character's next attack would use, as the attack itself picks it: a wand or melee weapon
// in hand, or the bow when no hostile is within melee reach. Nothing for bare fists.
const ItemDefinition *attackWeaponPreview(GameplayScreenRuntime &context)
{
    const Party *pParty = context.partyReadOnly();
    const ItemTable *pItemTable = context.itemTable();
    if (pParty == nullptr || pItemTable == nullptr)
    {
        return nullptr;
    }
    const size_t memberIndex = pParty->activeMemberIndex();
    const auto equipped = [&](EquipmentSlot slot) -> const ItemDefinition *
    {
        const uint32_t itemId = pParty->equippedItemId(memberIndex, slot);
        return itemId != 0 ? pItemTable->get(itemId) : nullptr;
    };
    const ItemDefinition *pMainHand = equipped(EquipmentSlot::MainHand);
    const ItemDefinition *pBow = equipped(EquipmentSlot::Bow);
    if (pMainHand != nullptr && pMainHand->equipStat == "WeaponW")
    {
        return pMainHand;
    }
    if (pBow != nullptr && !context.interactionState().turnHostileInMeleeRange)
    {
        return pBow;
    }
    if (pMainHand != nullptr)
    {
        return pMainHand;
    }
    const ItemDefinition *pOffHand = equipped(EquipmentSlot::OffHand);
    return pOffHand != nullptr && pOffHand->equipStat.rfind("Weapon", 0) == 0 ? pOffHand : nullptr;
}

// The bound attack spell's coloured spellbook art (as when highlighted in the book) on its patch of the school's page,
// while the active character can pay its mana: the attack casts it then instead of swinging. Leaves content alone
// and returns false otherwise.
bool applyAttackSpellPreview(GameplayScreenRuntime &context, int width, int height, TurnButtonContent &content)
{
    const Party *pParty = context.partyReadOnly();
    const SpellTable *pSpellTable = context.spellTable();
    const Character *pCharacter = pParty != nullptr ? pParty->activeMember() : nullptr;
    if (pCharacter == nullptr || pSpellTable == nullptr || pCharacter->attackSpellName.empty()
        || !context.interactionState().turnAttackSpellHasTarget)
    {
        return false;
    }
    const SpellEntry *pSpell = pSpellTable->findByName(pCharacter->attackSpellName);
    if (pSpell == nullptr)
    {
        return false;
    }
    const uint32_t spellId = static_cast<uint32_t>(pSpell->id);
    const std::optional<uint32_t> manaCost = PartySpellSystem::manaCost(*pCharacter, *pSpellTable, spellId);
    if (manaCost && !pParty->canSpendSpellPoints(pParty->activeMemberIndex(), static_cast<int>(*manaCost)))
    {
        return false;
    }
    const SpellbookSchoolUiDefinition *pSchool = findSpellbookSchoolUiDefinitionForSpellId(spellId);
    if (pSchool == nullptr)
    {
        return false;
    }
    const std::string spellLayoutId = spellbookSpellLayoutId(pSchool->school, spellId - pSchool->firstSpellId + 1);
    const UiLayoutManager::LayoutElement *pLayout = context.findHudLayoutElement(spellLayoutId);
    if (pLayout == nullptr)
    {
        return false;
    }
    // Same choice as the spellbook's selected spell.
    const std::string &asset = !pLayout->pressedAsset.empty() ? pLayout->pressedAsset
        : !pLayout->hoverAsset.empty() ? pLayout->hoverAsset : pLayout->primaryAsset;
    content.icon = context.gameplayUiRuntime().ensureHudTextureLoaded(asset);
    if (!content.icon)
    {
        return false;
    }

    // Where the spell sits on its page, as fractions of the page, from the book's own layout.
    const UiLayoutManager::LayoutElement *pPageLayout = context.findHudLayoutElement(pSchool->pPageLayoutId);
    const std::optional<GameplayResolvedHudLayoutElement> page =
        context.resolveHudLayoutElement(pSchool->pPageLayoutId, width, height, 0, 0);
    const std::optional<GameplayResolvedHudLayoutElement> spell =
        context.resolveHudLayoutElement(spellLayoutId, width, height, 0, 0);
    if (pPageLayout == nullptr || pPageLayout->width <= 0.0f || pPageLayout->height <= 0.0f)
    {
        return true;
    }
    content.backing = context.gameplayUiRuntime().ensureHudTextureLoaded(pPageLayout->primaryAsset);
    if (page && spell && page->width > 0.0f && page->height > 0.0f)
    {
        content.backingU0 = (spell->x - page->x) / page->width;
        content.backingV0 = (spell->y - page->y) / page->height;
        content.backingU1 = (spell->x + spell->width - page->x) / page->width;
        content.backingV1 = (spell->y + spell->height - page->y) / page->height;
    }
    else
    {
        // Spells are centre-anchored on their page (spellbook.yml).
        const float centerX = pPageLayout->width * 0.5f + pLayout->offsetX;
        const float centerY = pPageLayout->height * 0.5f + pLayout->offsetY;
        content.backingU0 = (centerX - pLayout->width * 0.5f) / pPageLayout->width;
        content.backingV0 = (centerY - pLayout->height * 0.5f) / pPageLayout->height;
        content.backingU1 = (centerX + pLayout->width * 0.5f) / pPageLayout->width;
        content.backingV1 = (centerY + pLayout->height * 0.5f) / pPageLayout->height;
    }
    return true;
}

// The turn indicator itself: the current stage, with the attack hotkey or the movement points left.
void renderTurnIndicator(GameplayScreenRuntime &context, int width, int height)
{
    if (context.currentHudScreenState() != GameplayHudScreenState::Gameplay
        || !context.turnBasedCombatRuntime().active())
    {
        return;
    }
    const std::optional<GameplayResolvedHudLayoutElement> rect =
        context.resolveHudLayoutElement("ObsidianTurnPlate", width, height, 0, 0);
    if (!rect)
    {
        return;
    }
    const TurnBasedCombatRuntime &turn = context.turnBasedCombatRuntime();
    TurnButtonContent content;
    if (turn.stage() == TurnBasedCombatStage::Movement)
    {
        content.icon = context.gameplayUiRuntime().ensureItemIconTextureLoaded("6boots1"); // MM6 Old Leather Boots
    }
    else if (!applyAttackSpellPreview(context, width, height, content))
    {
        const ItemDefinition *pWeapon = attackWeaponPreview(context);
        // Bare hands show plain Leather Gloves.
        content.icon = pWeapon != nullptr ? itemIcon(context, pWeapon)
                                          : context.gameplayUiRuntime().ensureItemIconTextureLoaded("item127");
    }
    if (turn.stage() == TurnBasedCombatStage::Movement)
    {
        content.title = "Move";
        content.detail = std::to_string(turn.movementActionPoints()) + " AP";
    }
    else if (turn.stage() == TurnBasedCombatStage::Attack)
    {
        content.title = "Attack";
        content.detail = hotkeyLabel(context, KeyboardAction::Attack);
    }
    else
    {
        content.title = "Waiting";
    }
    const bool pressed = context.interactionState().gameplayHudClickLatch
        && context.interactionState().gameplayHudPressedTarget.type == GameplayHudPointerTargetType::AttackButton;
    renderTurnButton(context, *rect, content, pointerOverRect(context, *rect), pressed, false);
}

void renderPassTurnButton(GameplayScreenRuntime &context, int width, int height)
{
    const std::optional<GameplayResolvedHudLayoutElement> buttonRect =
        context.resolvePassTurnButton(width, height);

    if (!buttonRect)
    {
        return;
    }

    const bool hovered = pointerOverRect(context, *buttonRect);
    const bool pressed =
        context.interactionState().gameplayHudClickLatch
        && context.interactionState().gameplayHudPressedTarget.type == GameplayHudPointerTargetType::PassTurnButton;
    TurnButtonContent content;
    content.icon = context.gameplayUiRuntime().ensureItemIconTextureLoaded("6lshield8"); // Lord Kilburn's Shield
    content.title =
        context.turnBasedCombatRuntime().stage() == TurnBasedCombatStage::Movement ? "End Move" : "Pass";
    content.detail = hotkeyLabel(context, KeyboardAction::Pass);
    renderTurnButton(context, *buttonRect, content, hovered, pressed, false);
}

void renderUseWandButton(GameplayScreenRuntime &context, int width, int height)
{
    const auto rect = context.resolveUseWandButton(width, height);
    const Party *pParty = context.partyReadOnly();
    if (!rect || pParty == nullptr || context.itemTable() == nullptr)
    {
        return;
    }
    const bool hovered = pointerOverRect(context, *rect);
    const bool pressed = context.interactionState().gameplayHudClickLatch
        && context.interactionState().gameplayHudPressedTarget.type == GameplayHudPointerTargetType::UseWandButton;

    // Same slot choice as the wand attack: the dedicated wand slot first, else a wand held in the main hand.
    const size_t memberIndex = pParty->activeMemberIndex();
    const EquipmentSlot slot =
        pParty->equippedItemId(memberIndex, EquipmentSlot::Wand) != 0 ? EquipmentSlot::Wand : EquipmentSlot::MainHand;
    const EquippedItemRuntimeState *pRuntime = pParty->equippedItemRuntime(memberIndex, slot);

    TurnButtonContent content;
    content.icon = itemIcon(context, context.itemTable()->get(pParty->equippedItemId(memberIndex, slot)));
    content.title = "Wand";
    content.detail = hotkeyLabel(context, KeyboardAction::UseWand);
    if (pRuntime != nullptr && pRuntime->identified)
    {
        content.badge = std::to_string(pRuntime->currentCharges);
        content.badgeColorAbgr = pRuntime->currentCharges == 0 ? GameplayUiSkin::Penalty : 0;
    }
    renderTurnButton(context, *rect, content, hovered, pressed, false);
}

void renderScrollMenu(GameplayScreenRuntime &context, int width, int height)
{
    if ((!context.interactionState().scrollMenuOpen && !context.interactionState().potionsMenuOpen)
        || context.currentHudScreenState() != GameplayHudScreenState::Gameplay
        || context.itemTable() == nullptr)
    {
        return;
    }

    const Party *pParty = context.partyReadOnly();
    const UiLayoutManager::LayoutElement *pFontTemplate = context.findHudLayoutElement("ObsidianTurnPhase");
    if (pParty == nullptr || pFontTemplate == nullptr)
    {
        return;
    }

    const bool potionsMenu = context.interactionState().potionsMenuOpen;
    const GameplayItemMenuLayout layout = gameplayItemMenuLayout(potionsMenu);
    const std::vector<GameplayScrollMenuEntry> entries = potionsMenu
        ? collectGameplayPotionMenuEntries(
            *pParty, *context.itemTable(), context.mergedPotionSettingTable(), context.mergedReagentSettingTable())
        : collectGameplayScrollMenuEntries(*pParty, *context.itemTable());
    const std::optional<GameplayResolvedHudLayoutElement> panelRect =
        context.resolveItemMenuPanel(width, height, potionsMenu, entries.size());
    if (!panelRect)
    {
        return;
    }
    const GameplayResolvedHudLayoutElement &panel = *panelRect;
    const float rowHeight = layout.rowHeight;
    const size_t visibleRows = std::max<size_t>(1, std::min(layout.maxVisibleRows, entries.size()));
    const size_t maxScroll = entries.size() > visibleRows ? entries.size() - visibleRows : 0;
    context.interactionState().scrollMenuScrollOffset =
        std::min(context.interactionState().scrollMenuScrollOffset, maxScroll);
    const float scale = panel.scale;
    // Same plain plate as the buttons, no frame.
    GameplayUiSkin::renderTexture(context, "obsidian_hud_turn_normal", panel);
    const std::optional<GameplayHudPaletteColor> hoverFill =
        context.gameplayUiRuntime().ensureHudPaletteColor(0x30c7dce5u); // Translucent ivory.

    // Same font as the turn buttons (ObsidianTurnPhase).
    UiLayoutManager::LayoutElement label = *pFontTemplate;
    label.textColorAbgr = GameplayUiSkin::Ivory;
    label.textAlignX = UiLayoutManager::TextAlignX::Left;
    label.textAlignY = UiLayoutManager::TextAlignY::Middle;
    label.textPadY = 0.0f;
    const float baseTextScale = pFontTemplate->textScale;
    label.textScale = baseTextScale;
    label.textPadX = layout.iconBoxWidth + 9.0f;
    const GameplayInputFrame *pInput = context.currentGameplayInputFrame();
    for (size_t row = 0; row < visibleRows; ++row)
    {
        const GameplayResolvedHudLayoutElement rowRect = {
            panel.x + 2 * scale,
            panel.y + static_cast<float>(row) * rowHeight * scale + scale,
            panel.width - 4 * scale,
            (rowHeight - 1) * scale,
            scale};
        const size_t entryIndex = context.interactionState().scrollMenuScrollOffset + row;
        const bool hasEntry = entryIndex < entries.size();
        const float pointerX = pInput != nullptr ? pInput->pointerX : -1.0f;
        const float pointerY = pInput != nullptr ? pInput->pointerY : -1.0f;
        const bool hovered = hasEntry && pointerX >= rowRect.x && pointerX < rowRect.x + rowRect.width
            && pointerY >= rowRect.y && pointerY < rowRect.y + rowRect.height;
        if (hovered && hoverFill)
        {
            context.submitHudQuadBatch(
                {gameplayHudPaletteQuad(*hoverFill, rowRect.x, rowRect.y, rowRect.width, rowRect.height)});
        }

        if (hasEntry)
        {
            const ItemDefinition *pDefinition = context.itemTable()->get(entries[entryIndex].objectDescriptionId);
            if (pDefinition != nullptr)
            {
                renderFittedItemIcon(
                    context,
                    pDefinition->iconName,
                    rowRect.x + 4.0f * scale,
                    rowRect.y + (rowRect.height - layout.iconBoxHeight * scale) * 0.5f,
                    layout.iconBoxWidth * scale,
                    layout.iconBoxHeight * scale);
            }
        }

        const std::string text = hasEntry
            ? entries[entryIndex].label
                + (entries[entryIndex].quantity > 1
                        ? " (x" + std::to_string(entries[entryIndex].quantity) + ")"
                        : std::string{})
            : (potionsMenu ? "No potions" : "No scrolls");
        label.textScale = fittedTextScale(
            context, label.fontName, text, baseTextScale, rowRect.width / scale - label.textPadX - 6.0f);
        context.renderLayoutLabel(label, rowRect, text);
    }
}

// The item the button shows: kept while the party still carries it, otherwise a new random pick.
uint32_t pickItemMenuPreview(const std::vector<GameplayScrollMenuEntry> &entries, uint32_t current)
{
    if (entries.empty())
    {
        return 0;
    }
    for (const GameplayScrollMenuEntry &entry : entries)
    {
        if (entry.objectDescriptionId == current)
        {
            return current;
        }
    }
    static std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<size_t> pick(0, entries.size() - 1);
    return entries[pick(generator)].objectDescriptionId;
}

void renderItemMenuButton(
    GameplayScreenRuntime &context,
    int width,
    int height,
    const char *layoutId,
    const char *labelText,
    KeyboardAction hotkeyAction)
{
    const Party *pParty = context.partyReadOnly();
    if (context.currentHudScreenState() != GameplayHudScreenState::Gameplay
        || pParty == nullptr
        || context.itemTable() == nullptr)
    {
        return;
    }

    const std::optional<GameplayResolvedHudLayoutElement> rect =
        context.resolveHudLayoutElement(layoutId, width, height, 0, 0);
    if (!rect)
    {
        return;
    }

    const GameplayInputFrame *pInput = context.currentGameplayInputFrame();
    const bool hovered = pointerOverRect(context, *rect);
    const bool pressed = hovered && pInput->leftMouseButton.held;
    const bool potions = layoutId != std::string("OutdoorButtonScrolls");
    const bool selected = potions
        ? context.interactionState().potionsMenuOpen
        : context.interactionState().scrollMenuOpen;

    const std::vector<GameplayScrollMenuEntry> entries = potions
        ? collectGameplayPotionMenuEntries(
            *pParty, *context.itemTable(), context.mergedPotionSettingTable(), context.mergedReagentSettingTable())
        : collectGameplayScrollMenuEntries(*pParty, *context.itemTable());
    uint32_t &previewItemId = potions
        ? context.interactionState().potionsButtonPreviewItemId
        : context.interactionState().scrollsButtonPreviewItemId;
    previewItemId = pickItemMenuPreview(entries, previewItemId);

    TurnButtonContent content;
    content.icon = itemIcon(context, previewItemId != 0 ? context.itemTable()->get(previewItemId) : nullptr);
    content.title = labelText;
    content.detail = hotkeyLabel(context, hotkeyAction);
    content.menuArrow = true;
    renderTurnButton(context, *rect, content, hovered, pressed, selected);
}

} // namespace

void GameplayHudRenderer::renderGameplayHud(GameplayScreenRuntime &context, int width, int height)
{
    const Party *pParty = context.partyReadOnly();
    if (pParty == nullptr || !context.hasHudRenderResources() || width <= 0 || height <= 0)
    {
        return;
    }
    context.prepareHudView(width, height);
    const GameplayHudScreenState state = context.currentHudScreenState();
    const bool fullscreen = isOverlayHudState(state)
        && !activeEventDialogPreservesGameplayHud(context.activeEventDialog());
    std::string status;
    if (context.houseBankState().inputActive())
    {
        status = std::string(context.houseBankState().inputMode
            == GameplayUiController::HouseBankInputMode::Deposit ? "Deposit: " : "Withdraw: ")
            + context.houseBankState().inputText + "_";
    }
    else if (context.statusBarEventRemainingSeconds() > 0 && !context.statusBarEventText().empty())
    {
        status = context.statusBarEventText();
    }
    else
    {
        status = context.statusBarHoverText();
    }
    if (status.empty() && state == GameplayHudScreenState::Dialogue)
    {
        status = context.interactionState().dialogueStatusHint;
    }
    if (!status.empty() && (state == GameplayHudScreenState::Gameplay || state == GameplayHudScreenState::Character
        || state == GameplayHudScreenState::Dialogue || state == GameplayHudScreenState::Chest
        || state == GameplayHudScreenState::Spellbook))
    {
        const std::string id = fullscreen ? "OutdoorStatusBar" : "OutdoorGameplayStatusBar";
        const UiLayoutManager::LayoutElement *pLayout = context.findHudLayoutElement(id);
        std::optional<GameplayResolvedHudLayoutElement> rect = context.resolveHudLayoutElement(id, width, height, 0, 0);
        if (pLayout != nullptr && rect)
        {
            if (fullscreen)
            {
                UiLayoutManager::LayoutElement label = *pLayout;
                const float textWidth = context.measureHudTextWidth(label.fontName, status);
                const float availableWidth = rect->width / rect->scale - 2 * std::abs(label.textPadX);
                if (textWidth > availableWidth)
                {
                    label.textScale = std::max(0.75f, availableWidth / textWidth);
                }
                const IGameplayWorldRuntime *pWorld = context.worldRuntime();
                const EventRuntimeState *pEvents = pWorld != nullptr ? pWorld->eventRuntimeState() : nullptr;
                const bool editing = context.houseBankState().inputActive()
                    || (pEvents != nullptr && pEvents->pendingInputPrompt.has_value());
                // Keep the entered suffix and caret visible when the input exceeds the fixed rail.
                while (editing && !status.empty()
                    && context.measureHudTextWidth(label.fontName, status) * label.textScale > availableWidth)
                {
                    size_t next = 1;
                    while (next < status.size() && (static_cast<unsigned char>(status[next]) & 0xc0) == 0x80)
                    {
                        ++next;
                    }
                    status.erase(0, next);
                }
                context.renderLayoutLabel(label, *rect, status);
            }
            else
            {
                const std::optional<GameplayHudFontHandle> font = context.findHudFont(pLayout->fontName);
                if (font)
                {
                    const float bottom = rect->y + rect->height;
                    const float scale = rect->scale * pLayout->textScale;
                    const float maxWidth = 330.666667f * rect->scale;
                    std::vector<std::string> lines = context.wrapHudTextToWidth(*font, status,
                        (maxWidth - 16 * rect->scale) / scale);
                    const size_t maximumLines = 8;
                    if (lines.size() > maximumLines)
                    {
                        lines.resize(maximumLines);
                        lines.back() += "...";
                    }
                    float textWidth = 0;
                    for (const std::string &line : lines)
                    {
                        textWidth = std::max(textWidth, context.measureHudTextWidth(font->fontName, line) * scale);
                    }
                    const float lineHeight = font->fontHeight * scale;
                    rect->width = std::min(maxWidth, textWidth + 16 * rect->scale);
                    rect->x = (width - rect->width) * 0.5f;
                    rect->height = std::min(78.933333f * rect->scale,
                        lineHeight * lines.size() + 12 * rect->scale);
                    rect->y = bottom - rect->height;
                    const std::optional<GameplayResolvedHudLayoutElement> dock =
                        context.resolveHudLayoutElement("ObsidianTurnDock", width, height, 0, 0);
                    if (context.turnBasedCombatRuntime().active() && dock
                        && dock->x + dock->width > rect->x && dock->x < rect->x + rect->width
                        && dock->y < rect->y + rect->height && dock->y + dock->height > rect->y)
                    {
                        rect->y = dock->y - rect->height - 8 * rect->scale;
                    }
                    GameplayUiSkin::renderTexture(context, "obsidian_hud_status_plate", *rect);
                    for (size_t i = 0; i < lines.size(); ++i)
                    {
                        context.renderLayoutLabel(*pLayout, {rect->x + 8 * rect->scale,
                            rect->y + 6 * rect->scale + i * lineHeight,
                            rect->width - 16 * rect->scale, lineHeight, rect->scale}, lines[i]);
                    }
                }
            }
        }
    }
    if (!fullscreen)
    {
        for (bool gold : {true, false})
        {
            const std::string id = gold ? "OutdoorGoldLabel" : "OutdoorFoodLabel";
            const UiLayoutManager::LayoutElement *pLayout = context.findHudLayoutElement(id);
            const std::optional<GameplayResolvedHudLayoutElement> rect =
                context.resolveHudLayoutElement(id, width, height, 0, 0);
            if (pLayout != nullptr && rect)
            {
                context.renderLayoutLabel(*pLayout, *rect, std::to_string(gold ? pParty->gold() : pParty->food()));
            }
        }
    }
    if (context.settingsSnapshot().contextActionPopup && state == GameplayHudScreenState::Gameplay)
    {
        renderContextAction(context, width, height);
    }
    renderTurnIndicator(context, width, height);
    renderPassTurnButton(context, width, height);
    renderUseWandButton(context, width, height);
    renderItemMenuButton(context, width, height, "OutdoorButtonScrolls", "Scrolls", KeyboardAction::Scrolls);
    renderItemMenuButton(context, width, height, "OutdoorButtonPotions", "Potions", KeyboardAction::Potions);
    renderScrollMenu(context, width, height);
}
} // namespace OpenYAMM::Game
