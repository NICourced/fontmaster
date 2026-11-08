#pragma once
#include "fontmaster/CBDT_CBLC_Types.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <set>

namespace fontmaster {

/**
 * @brief Класс для пересборки цветных таблиц CBDT/CBLC шрифта.
 * Используется после парсера для восстановления шрифта с модифицированными глифами.
 */
class CBDT_CBLC_Rebuilder {
public:
    /// Конструктор принимает исходные бинарные данные шрифта
    explicit CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData,
                       const std::map<uint16_t, StrikeRecord>& strikes,
                       const std::vector<uint16_t>& removedGlyphs)
        : fontData(fontData), strikes(strikes), removedGlyphs(removedGlyphs) {}
    /**
     * @brief Добавить страйк (набор глифов)
     */
    void addStrike(uint16_t id, const StrikeRecord& strike) {
        strikes[id] = strike;
    }

    /**
     * @brief Удалить глиф из всех страйков.
     */
    void removeGlyph(uint16_t glyphId) {
        removedGlyphs.push_back(glyphId);
    }

    /**
     * @brief Собрать обновлённый шрифт с новыми таблицами CBDT и CBLC.
     * @return Бинарные данные шрифта (TTF)
     */
    std::vector<uint8_t> rebuild();

private:
    const std::vector<uint8_t>& fontData;
    std::map<uint16_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;

};

} // namespace fontmaster
