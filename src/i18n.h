#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

enum class Language {
    English = 0,
    Japanese = 1,
};

struct Translations {
    const char* code;         // ISO language code
    const char* displayName;  // Native language name shown in dropdown

    const char* title;          // Overlay window title
    const char* dashboardName;  // SteamVR dashboard tab / tooltip name

    std::array<const char*, 3> builtinPresets;  // Preset names: {Standard, Vivid, Night}
    const char* emptySlotSuffix;                // Suffix for unassigned custom slots (e.g. " (Empty)")
    const char* save;                           // Save button label (default mode)
    const char* selectSlot;                     // Save button label (when waiting for P1-P3 selection)
    const char* reset;                          // Reset button label (restores standard preset)
    const char* resetConfirm;                   // Reset button label when confirming (e.g. "Confirm?")

    const char* panelBrightness;                // Hardware panel brightness slider label
    const char* adjustmentChannel;              // Adjustment channel selector section header
    std::array<const char*, 4> channelNames;    // Channel options: {All, Red, Green, Blue}
    const char* brightness;                     // Color gain / brightness slider label
    const char* saturation;                     // Saturation slider label
    const char* contrast;                       // Contrast slider label
    const char* gamma;                          // Gamma slider label

    const char* errorSaveSettings;  // Error prefix when saving SteamVR analogGain setting fails
    const char* applied;            // Success message when a setting is applied
    const char* errorSaveAdvanced;  // Error message when writing current.txt fails
    const char* errorSavePreset;    // Error message when writing presets.txt fails
    const char* slotEmpty;          // Error message when clicking an unassigned custom preset
    const char* presetApplied;      // Success message when a preset is loaded
    const char* promptSaveSlot;     // Prompt shown when entering save mode
    const char* saveCanceled;       // Notice shown when save mode is cancelled
    const char* savedToSlotPrefix;  // Prefix for slot save confirmation (e.g. "Saved to P")
    const char* savedToSlotSuffix;  // Suffix for slot save confirmation (e.g. " に保存しました")

    // Format a confirmation message when saving to a custom preset slot (e.g. P1, P2, P3).
    std::string formatSavedToSlot(int slotNumber) const;
};

/** Get the list of all available languages. */
const std::vector<Language>& availableLanguages();

/** Get translation table for the specified language. */
const Translations& getTranslations(Language lang);

/** Parse a language code or alias (case-insensitive, e.g. "en", "ja", "ja_JP", "japanese"). */
Language languageFromCode(const std::string& code);

/** Get canonical string code for a language ("en", "ja"). */
std::string languageCode(Language lang);

/** Load language preference from config directory (language.txt), defaulting to English. */
Language loadSavedLanguage(const std::filesystem::path& configDir);

/** Save language preference to config directory (language.txt). */
bool saveLanguage(const std::filesystem::path& configDir, Language lang);
