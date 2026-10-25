#include "i18n.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {

Translations createEnglish() {
    Translations t{};

    // Language Metadata
    t.code = "en";
    t.displayName = "English";

    // Dashboard & Window Header
    t.title = "Display Color Adjustment";
    t.dashboardName = "Display";

    // Preset Buttons
    t.builtinPresets = {"Standard", "Vivid", "Night"};
    t.emptySlotSuffix = " (Empty)";
    t.save = "Save";
    t.selectSlot = "Select Slot";
    t.reset = "Reset";
    t.resetConfirm = "Confirm?";

    // Adjustment Controls & Sliders
    t.panelBrightness = "Panel Brightness";
    t.adjustmentChannel = "Adjustment Channel";
    t.channelNames = {"All", "Red", "Green", "Blue"};
    t.brightness = "Brightness";
    t.saturation = "Saturation";
    t.contrast = "Contrast";
    t.gamma = "Gamma";

    // Status & Feedback Messages
    t.errorSaveSettings = "Failed to save settings: ";
    t.applied = "Applied";
    t.errorSaveAdvanced = "Failed to save advanced settings";
    t.errorSavePreset = "Failed to save preset";
    t.slotEmpty = "This slot is empty";
    t.presetApplied = "Preset applied";
    t.promptSaveSlot = "Select slot P1-P3 to save";
    t.saveCanceled = "Save canceled";
    t.savedToSlotPrefix = "Saved to P";
    t.savedToSlotSuffix = "";

    return t;
}

Translations createJapanese() {
    Translations t{};

    // Language Metadata
    t.code = "ja";
    t.displayName = "日本語";

    // Dashboard & Window Header
    t.title = "ディスプレイ色調整";
    t.dashboardName = "ディスプレイ";

    // Preset Buttons
    t.builtinPresets = {"標準", "鮮やか", "夜間"};
    t.emptySlotSuffix = "（空）";
    t.save = "保存";
    t.selectSlot = "保存先を選択";
    t.reset = "リセット";
    t.resetConfirm = "確認？";

    // Adjustment Controls & Sliders
    t.panelBrightness = "パネル輝度";
    t.adjustmentChannel = "調整チャンネル";
    t.channelNames = {"全体", "赤", "緑", "青"};
    t.brightness = "明るさ";
    t.saturation = "彩度";
    t.contrast = "コントラスト";
    t.gamma = "ガンマ";

    // Status & Feedback Messages
    t.errorSaveSettings = "設定の保存に失敗: ";
    t.applied = "適用しました";
    t.errorSaveAdvanced = "詳細設定を保存できません";
    t.errorSavePreset = "プリセットを保存できません";
    t.slotEmpty = "このスロットは未保存です";
    t.presetApplied = "プリセットを適用しました";
    t.promptSaveSlot = "保存先 P1〜P3 を選んでください";
    t.saveCanceled = "保存をキャンセルしました";
    t.savedToSlotPrefix = "P";
    t.savedToSlotSuffix = " に保存しました";

    return t;
}

const Translations kTranslationsEn = createEnglish();
const Translations kTranslationsJa = createJapanese();

const std::vector<Language> kLanguages = {
    Language::English,
    Language::Japanese,
};

}  // namespace

std::string Translations::formatSavedToSlot(int slotNumber) const {
    return std::string(savedToSlotPrefix) + std::to_string(slotNumber) + savedToSlotSuffix;
}

const std::vector<Language>& availableLanguages() {
    return kLanguages;
}

const Translations& getTranslations(Language lang) {
    switch (lang) {
        case Language::Japanese:
            return kTranslationsJa;
        case Language::English:
        default:
            return kTranslationsEn;
    }
}

Language languageFromCode(const std::string& code) {
    std::string s = code;
    // Trim leading and trailing whitespace
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);

    // Convert to lowercase
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (s == "ja" || s.rfind("ja_", 0) == 0 || s == "japanese" || s == "jp") {
        return Language::Japanese;
    }
    return Language::English;
}

std::string languageCode(Language lang) {
    switch (lang) {
        case Language::Japanese:
            return "ja";
        case Language::English:
        default:
            return "en";
    }
}

Language loadSavedLanguage(const std::filesystem::path& configDir) {
    const auto path = configDir / "language.txt";
    std::ifstream in(path);
    std::string code;
    if (in >> code && !code.empty()) {
        return languageFromCode(code);
    }
    return Language::English;
}

bool saveLanguage(const std::filesystem::path& configDir, Language lang) {
    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);

    const auto path = configDir / "language.txt";
    const auto temp = configDir / "language.txt.tmp";

    std::ofstream out(temp, std::ios::trunc);
    if (!out) return false;
    out << languageCode(lang) << '\n';
    out.close();

    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}
