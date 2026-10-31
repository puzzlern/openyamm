#pragma once

#include "game/app/NewGameOptions.h"
#include "game/content/RulesetDefinition.h"
#include "game/data/GameDataRepository.h"
#include "game/party/Party.h"
#include "game/ui/MenuDesignScreen.h"
#include "game/ui/UiLayoutManager.h"

#include <SDL3/SDL.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
class GameAudioSystem;
class MawRules;
struct GameRules;

class NewGameScreen : public MenuDesignScreen
{
public:
    enum class StatId
    {
        Might = 0,
        Intellect,
        Personality,
        Endurance,
        Accuracy,
        Speed,
        Luck,
        Count,
    };

    struct CreationCandidate
    {
        uint32_t characterDataId = 0;
        uint32_t classId = 0;
        uint32_t raceId = 0;
        std::string defaultName;
        std::string className;
        std::string raceName;
        std::vector<uint32_t> availableClassIds;
        bool hasCustomDefaultStats = false;
        std::array<int, static_cast<size_t>(StatId::Count)> defaultStats = {};
        std::array<std::string, 2> defaultOptionalSkills = {};
    };

    using ContinueAction = std::function<void(const std::vector<Character> &, const NewGameOptions &)>;
    // Loads the common data under the chosen ruleset's tables before party creation reads them.
    using ActivateRulesetAction = std::function<bool(const std::string &rulesetId, std::string &error)>;
    using BackAction = std::function<void()>;

    NewGameScreen(
        const Engine::AssetFileSystem &assetFileSystem,
        GameAudioSystem *pGameAudioSystem,
        const GameDataRepository &gameData,
        bool debugGodLichRoster,
        bool includeGodLichCandidate,
        bool allowIncompleteCharacterCreation,
        ContinueAction continueAction,
        ActivateRulesetAction activateRulesetAction,
        BackAction backAction);
    ~NewGameScreen() override;

    AppMode mode() const override;
    void prepareForFirstFrame();
    void onEnter() override;
    void onExit() override;
    void handleSdlEvent(const SDL_Event &event) override;
    bool textInputActive() const override;

  private:
    friend struct NewGameScreenTestAccess;

    enum class FlowStage
    {
        ContinentSelection,
        CharacterCreation
    };

    // The portrait grid open over the screen: picking a portrait, or a voice shown by the portrait it came from.
    enum class PortraitPicker
    {
        Closed,
        Portrait,
        Voice
    };

    struct SelectedContinent
    {
        uint32_t id = 1;
        std::string key = "jadame";
        std::string name = "Jadam";
    };

    struct CreationState
    {
        size_t selectedCandidateIndex = 0;
        uint32_t selectedClassId = 0;
        int selectedVoiceId = 0;
        std::string name;
        std::array<int, static_cast<size_t>(StatId::Count)> baseStats = {};
        std::array<int, static_cast<size_t>(StatId::Count)> currentStats = {};
        std::vector<std::string> defaultSkills;
        std::vector<std::string> optionalSkills;
        std::vector<std::string> selectedOptionalSkills;
        bool nameEditing = false;
        std::string nameEditBuffer;
        std::string statusMessage;
    };

    void drawScreen(float deltaSeconds) override;
    void drawContinentSelection(float deltaSeconds);
    void drawRulesetSwitch();
    void drawRulesPanel();
    // One rule setting: name and control on a line, description below; returns the y after it.
    float drawRulesSetting(const std::string &id, const std::string &name, const std::string &description,
                           bool select, const std::string &value, bool enabled, bool last, float y, bool &activated);
    // Text of a Rules panel element at content position y (logical units), word-wrapped; returns its height.
    float drawRulesText(const std::string &id, const std::string &text, float y, uint32_t color = 0);
    Rect rulesRect(const std::string &id, float y) const;
    // A row of tabs inside a brass outline; returns the clicked tab.
    std::optional<size_t> drawSegmentedSwitch(const std::string &id, const Rect &frame,
                                              const std::vector<std::string> &labels, size_t selected, bool enabled);
    const RulesetDefinition &chosenRulesetDefinition() const;
    void loadRulesets();
    void selectContinent(const std::string &continentKey);
    void initializeCharacterCreationForSelectedContinent();
    void ensurePartyStates();
    void saveActivePartyState();
    void switchActivePartySlot(size_t slotIndex);
    void addPartySlot();
    void removePartySlot();
    void resetStateForCandidate(size_t candidateIndex);
    void rebuildCandidates();
    void refreshSkillChoices(bool applyCandidateDefaults);
    size_t candidateCount() const;
    const CreationCandidate &candidateAt(size_t candidateIndex) const;
    void beginNameEditing();
    void endNameEditing(bool commitEdit);
    void deleteNameEditCharacter();
    void resetNameBackspaceRepeat();
    void updateNameBackspaceRepeat(float deltaSeconds);
    void ensurePcNamesLoaded();
    std::string generateDefaultNameForState(const CreationState &state);
    bool partyNameAlreadyUsed(const std::string &name) const;
    size_t pcNameColumnForState(const CreationState &state) const;
    bool tryIncreaseStat(StatId statId);
    bool tryDecreaseStat(StatId statId);
    bool tryToggleOptionalSkill(const std::string &skillName);
    int currentBonusPool() const;
    int bonusPoolForState(const CreationState &state) const;
    Character buildCharacter() const;
    Character buildCharacterFromState(const CreationState &state) const;
    std::vector<Character> buildPartyCharacters() const;
    std::string selectedClassName() const;
    std::string classNameForState(const CreationState &state) const;
    std::vector<int> availableVoiceIdsForSelectedCandidate() const;
    std::vector<std::string> wrapTextToWidth(const std::string &fontName, const std::string &text, float maxWidth, float scale);
    const CharacterDollEntry *selectedCharacterEntry() const;
    const CharacterDollEntry *characterEntryForState(const CreationState &state) const;
    const CreationCandidate &selectedCandidate() const;
    const CreationCandidate &candidateForState(const CreationState &state) const;
    bool isGodLichState(const CreationState &state) const;
    bool isGodLichSelected() const;
    std::array<int, static_cast<size_t>(StatId::Count)> statsForRace(const std::string &raceName) const;
    void cycleCandidate(int direction);
    void selectCandidate(size_t candidateIndex);
    void openPortraitPicker(PortraitPicker picker);
    // A grid of portraits, opened by clicking the portrait or the voice; a click picks one.
    void drawPortraitPicker();
    void cycleClass(int direction);
    void resetCurrentState(bool applyCandidateDefaults = false);
    void confirmCreation();
    void cancelCreation();
    Character buildVoicePreviewCharacter() const;
    void showCreationCompletionError();
    // The rules the creation popups read (a ruleset's skill texts, race card and skill matrix): a throwaway copy of the
    // chosen ruleset's rules, never given to the party the game starts with; Standard's for Standard.
    const GameRules &creationRules();
    // The class/skill matrix of a character's class line with its race: the best cap of each skill, best first; a skill
    // that needs a promotion for it carries a "*".
    std::vector<std::pair<std::string, uint32_t>> creationSkillMatrixLines(const Character &character) const;
    std::optional<TexturePixelsBgra> buildCreationPreviewDollPixels(
        const CharacterDollEntry &entry,
        const CharacterDollTypeEntry *pDollType);
    void playVoicePreview();

    GameAudioSystem *m_pGameAudioSystem = nullptr;
    const GameDataRepository *m_pGameData = nullptr;
    bool m_debugGodLichRoster = false;
    bool m_includeGodLichCandidate = false;
    bool m_allowIncompleteCharacterCreation = false;
    ContinueAction m_continueAction;
    ActivateRulesetAction m_activateRulesetAction;
    BackAction m_backAction;
    FlowStage m_stage = FlowStage::ContinentSelection;
    SelectedContinent m_selectedContinent = {};
    RulesetSelection m_ruleset;
    std::unique_ptr<MawRules> m_pCreationRules;
    RulesetSelection m_creationRulesSelection;
    // Standard first, then the installed ruleset packages.
    std::vector<RulesetDefinition> m_rulesets;
    // The options last chosen for each ruleset in this run, so switching rulesets back and forth keeps them.
    std::unordered_map<std::string, RulesetOptions> m_rulesetOptions;
    bool m_rulesPanelOpen = false;
    RulesetOptions m_rulesDraft;
    // The difficulty and progression chosen before a challenge mode forced others, restored when it no longer does.
    std::optional<RulesetDifficulty> m_unforcedDifficulty;
    std::optional<RulesetProgression> m_unforcedProgression;
    float m_rulesScroll = 0;
    float m_rulesContentHeight = 0;
    std::vector<CreationCandidate> m_candidates;
    std::vector<CreationState> m_partyStates;
    std::array<std::vector<std::string>, 8> m_pcNameColumns;
    CreationState m_state = {};
    size_t m_partySize = 1;
    size_t m_activePartySlot = 0;
    std::mt19937 m_nameRng;
    bool m_pcNamesLoaded = false;
    bool m_nameBackspaceHeld = false;
    float m_nameBackspaceRepeatTimer = 0.0f;
    std::string m_creationPreviewDollCacheKey;
    std::optional<TexturePixelsBgra> m_creationPreviewDollPixels;
    bool m_characterCreationInitialized = false;
    PortraitPicker m_portraitPicker = PortraitPicker::Closed;
    bool m_portraitPickerNew = false;
    // The next line a click on the voice plays; back to the first when another voice is previewed.
    uint32_t m_voiceLineIndex = 0;
    bool m_escapePressed = false;
    bool m_returnPressed = false;
};
}
