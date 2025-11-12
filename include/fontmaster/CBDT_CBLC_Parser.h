#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>

namespace fontmaster {

/**
 * Описание изображения глифа (используется и в парсере, и в ребилдере).
 * Поля публичные для простоты доступа.
 */
struct GlyphImage {
    uint16_t glyphID = 0;
    uint16_t imageFormat = 0;   // формат изображения (см. спецификацию CBDT)
    uint32_t offset = 0;        // относительный offset внутри CBDT (у нас хранится как смещение от начала CBDT)
    uint32_t length = 0;        // длина извлечённых данных
    std::vector<uint8_t> data;  // реальные байты изображения (PNG/JPEG/TIFF/bitmap)
    uint16_t width = 0;
    uint16_t height = 0;
    int16_t bearingX = 0;
    int16_t bearingY = 0;
    uint16_t advance = 0;
};

/**
 * Описание страйка (strike) — набор растровых глифов для одного ppem/resolution.
 */
struct StrikeRecord {
    uint16_t ppem = 0;
    uint16_t resolution = 72;
    std::vector<uint16_t> glyphIDs;
    std::map<uint16_t, GlyphImage> glyphImages;
};

class CBDT_CBLC_Parser {
public:
    explicit CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData);

    /**
     * Выполнить разбор CBLC и CBDT таблиц. Возвращает true при успехе.
     */
    bool parse();

    /**
     * Доступ к разобранным страйкам: id -> StrikeRecord.
     * В парсере мы используем индекс-номер страйка (0..N-1).
     */
    const std::map<uint32_t, StrikeRecord>& getStrikes() const { return strikes; }

    /**
     * Список глифов, которые парсер считает "removed" (не имеют cmap-отображения).
     */
    const std::vector<uint16_t>& getRemovedGlyphs() const { return removedGlyphs; }

private:
    const std::vector<uint8_t>& fontData;
    std::map<uint32_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;

    // Основные шаги
    bool parseCBLCTable(uint32_t offset, uint32_t length);
    bool parseCBDTTable(uint32_t offset, uint32_t length);
    bool parseStrike(uint32_t offset, uint32_t strikeIndex);
    bool parseIndexSubtable(uint32_t offset, StrikeRecord& strike);

    // Форматы индексов
    bool parseIndexFormat1(uint32_t offset, StrikeRecord& strike,
                           uint16_t firstGlyph, uint16_t lastGlyph,
                           uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat2(uint32_t offset, StrikeRecord& strike,
                           uint16_t firstGlyph, uint16_t lastGlyph,
                           uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat5(uint32_t offset, StrikeRecord& strike,
                           uint16_t firstGlyph, uint16_t lastGlyph,
                           uint16_t imageFormat, uint32_t imageDataOffset);

    // Извлечение данных изображения
    bool extractGlyphImageData(uint32_t imageOffset, GlyphImage& image, uint32_t cbdtBase, uint32_t cbdtLength);
    bool extractBitmapData(const uint8_t* data, size_t available, GlyphImage& image);
    bool extractPNGData(const uint8_t* data, size_t available, GlyphImage& image);
    bool extractJPEGData(const uint8_t* data, size_t available, GlyphImage& image);
    bool extractTIFFData(const uint8_t* data, size_t available, GlyphImage& image);
    bool parseTIFFDirectory(const uint8_t* data, size_t available, uint32_t ifdOffset, GlyphImage& image, bool bigEndian);

    // Вспомогательные чтения (big endian)
    uint32_t readUInt32(const uint8_t* p) const;
    uint16_t readUInt16(const uint8_t* p) const;
    uint16_t readUInt16(const uint8_t* p, bool bigEndian) const;
    uint32_t readUInt32(const uint8_t* p, bool bigEndian) const;

    // Парсинг cmap (опционально использует utils::CMAPParser при наличии)
    bool parseCMAPTable(uint32_t offset, uint32_t length);
    uint32_t cbdtTableOffset = 0;
    uint32_t cbdtTableLength = 0;
};

} // namespace fontmaster
