#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <set>

namespace fontmaster {

/**
 * @brief Описание одного изображения глифа в CBDT.
 */
struct GlyphImage {
    std::vector<uint8_t> data;  ///< Сырые бинарные данные изображения (PNG, JPEG и т.д.)
    uint16_t width = 0;         ///< Ширина изображения (если доступно)
    uint16_t height = 0;        ///< Высота изображения (если доступно)
};

/**
 * @brief Описание страйка (strike) — набора цветных глифов для определённого размера.
 */
struct StrikeRecord {
    uint16_t ppem = 0;                 ///< Размер пикселей на EM
    uint16_t resolution = 72;          ///< DPI
    std::vector<uint16_t> glyphIDs;    ///< Список глифов в страйке
    std::unordered_map<uint16_t, GlyphImage> glyphImages; ///< Данные по каждому глифу
};

/**
 * @brief Класс для пересборки цветных таблиц CBDT/CBLC шрифта.
 * Используется после парсера для восстановления шрифта с модифицированными глифами.
 */
class CBDT_CBLC_Rebuilder {
public:
    /// Конструктор принимает исходные бинарные данные шрифта
    explicit CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData)
        : fontData(fontData) {}

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
    std::unordered_map<uint16_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;

    void rebuildCBLCTable(std::vector<uint8_t>& cblc);
    void rebuildCBDTTable(std::vector<uint8_t>& cbdt);
    void rebuildStrike(std::vector<uint8_t>& buf, const StrikeRecord& strike);
    void rebuildIndexSubtable1(std::vector<uint8_t>& buf, const StrikeRecord& strike,
                               uint16_t firstGlyph, uint16_t lastGlyph);

    std::vector<uint8_t> createUpdatedFont(const std::vector<uint8_t>& newCBLCTable,
                                           const std::vector<uint8_t>& newCBDTTable);
};

} // namespace fontmaster

