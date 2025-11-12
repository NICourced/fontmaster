#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h" // если есть
#include <iostream>
#include <algorithm>
#include <cstring>

namespace fontmaster {

CBDT_CBLC_Parser::CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData)
    : fontData(fontData) {}

bool CBDT_CBLC_Parser::parse() {
    using namespace utils;
    try {
        auto tables = parseTTFTables(fontData);

        // Найти CBLC и CBDT записи
        const TableRecord* cblcRec = findTable(tables, "CBLC");
        const TableRecord* cbdtRec = findTable(tables, "CBDT");
        if (!cblcRec || !cbdtRec) {
            std::cerr << "CBDT/CBLC Parser: missing CBLC or CBDT table\n";
            return false;
        }

        // Сохраним оффсеты для последующей работы
        uint32_t cblcOffset = cblcRec->offset;
        uint32_t cblcLength = cblcRec->length;
        cbdtTableOffset = cbdtRec->offset;
        cbdtTableLength = cbdtRec->length;

        if (!parseCBLCTable(cblcOffset, cblcLength)) {
            std::cerr << "CBDT/CBLC Parser: parseCBLCTable failed\n";
            return false;
        }

        if (!parseCBDTTable(cbdtTableOffset, cbdtTableLength)) {
            std::cerr << "CBDT/CBLC Parser: parseCBDTTable failed\n";
            return false;
        }

        // Попытка разобрать cmap для определения удалённых глифов (если таблица cmap есть)
        const TableRecord* cmapRec = findTable(tables, "cmap");
        if (cmapRec) {
            parseCMAPTable(cmapRec->offset, cmapRec->length);
        }

        std::cout << "CBDT/CBLC Parser: parsed strikes=" << strikes.size()
                  << ", removedGlyphs=" << removedGlyphs.size() << std::endl;
        return true;

    } catch (const std::exception& ex) {
        std::cerr << "CBDT/CBLC Parser exception: " << ex.what() << std::endl;
        return false;
    }
}

/* ---------- CBLC parsing ---------- */

bool CBDT_CBLC_Parser::parseCBLCTable(uint32_t offset, uint32_t length) {
    if (offset + 8 > fontData.size()) {
        std::cerr << "CBLC: table out of bounds\n";
        return false;
    }

    const uint8_t* base = fontData.data() + offset;
    uint32_t version = readUInt32(base);
    uint32_t numStrikes = readUInt32(base + 4);

    // Смещения массива strikeOffsets начинаются в base+8
    uint32_t strikeArrayPos = offset + 8;
    for (uint32_t i = 0; i < numStrikes; ++i) {
        if (strikeArrayPos + i * 4 + 4 > fontData.size()) break;
        const uint8_t* p = fontData.data() + strikeArrayPos + i * 4;
        uint32_t strikeOffset = readUInt32(p);
        if (offset + strikeOffset >= fontData.size()) continue;
        if (!parseStrike(offset + strikeOffset, i)) {
            std::cerr << "CBLC: failed parseStrike index=" << i << std::endl;
            // не прерываем весь разбор — пропускаем ошибочные страйки
            continue;
        }
    }
    return true;
}

bool CBDT_CBLC_Parser::parseStrike(uint32_t offset, uint32_t strikeIndex) {
    if (offset + 32 > fontData.size()) return false;
    const uint8_t* data = fontData.data() + offset;

    StrikeRecord strike;
    strike.ppem = readUInt16(data + 0);
    strike.resolution = readUInt16(data + 2);

    // startGlyphIndex / endGlyphIndex расположены чаще всего позже — попробуем безопасно прочитать
    uint16_t startGlyphIndex = readUInt16(data + 12);
    uint16_t endGlyphIndex = readUInt16(data + 14);

    // numberOfIndexSubTables и offset к ним
    uint32_t indexSubtableArrayOffset = readUInt32(data + 24);
    uint32_t numberOfIndexSubTables = readUInt32(data + 28);

    // Обходим indexSubtableArray
    uint32_t indexSubtableArrayPos = offset + indexSubtableArrayOffset;
    for (uint32_t i = 0; i < numberOfIndexSubTables; ++i) {
        if (indexSubtableArrayPos + i * 8 + 8 > fontData.size()) break;
        const uint8_t* entry = fontData.data() + indexSubtableArrayPos + i * 8;
        uint16_t firstGlyph = readUInt16(entry + 0);
        uint16_t lastGlyph  = readUInt16(entry + 2);
        uint32_t additionalOffset = readUInt32(entry + 4);
        uint32_t subtableOffset = offset + indexSubtableArrayOffset + additionalOffset;
        if (subtableOffset >= fontData.size()) continue;

        // Для всех glyphID в этом диапазоне мы добавим их в список glyphIDs и попытаемся распарсить субтаблицу
        for (uint16_t gid = firstGlyph; gid <= lastGlyph; ++gid) {
            strike.glyphIDs.push_back(gid);
        }

        if (!parseIndexSubtable(subtableOffset, strike)) {
            std::cerr << "CBLC: parseIndexSubtable failed at offset " << subtableOffset << std::endl;
            // продолжаем, возможно в других сабтаблицах есть данные
        }
    }

    strikes[strikeIndex] = std::move(strike);
    return true;
}

bool CBDT_CBLC_Parser::parseIndexSubtable(uint32_t offset, StrikeRecord& strike) {
    if (offset + 8 > fontData.size()) return false;
    const uint8_t* p = fontData.data() + offset;

    uint16_t indexFormat = readUInt16(p);
    uint16_t imageFormat = readUInt16(p + 2);
    uint32_t imageDataOffset = readUInt32(p + 4);

    switch (indexFormat) {
        case 1: return parseIndexFormat1(offset, strike,
                                         /*firstGlyph*/ 0, /*lastGlyph*/ 0,
                                         imageFormat, imageDataOffset);
        case 2: return parseIndexFormat2(offset, strike,
                                         /*firstGlyph*/ 0, /*lastGlyph*/ 0,
                                         imageFormat, imageDataOffset);
        case 5: return parseIndexFormat5(offset, strike,
                                         /*firstGlyph*/ 0, /*lastGlyph*/ 0,
                                         imageFormat, imageDataOffset);
        default:
            // Неподдерживаемые форматы - возвращаем true чтобы не ломать общий разбор
            std::cout << "CBLC: Unsupported indexFormat " << indexFormat << std::endl;
            return true;
    }
}

/* indexFormat 1:
   at subtable offset:
   uint16 indexFormat
   uint16 imageFormat
   uint32 imageDataOffset
   uint8 imageSize
   int8 bigMetrics
   int8 bearingX
   int8 bearingY
   uint8 advance
   uint32 glyphOffsets[] (for first..last)
*/
bool CBDT_CBLC_Parser::parseIndexFormat1(uint32_t offset, StrikeRecord& strike,
                                         uint16_t /*firstGlyph*/, uint16_t /*lastGlyph*/,
                                         uint16_t imageFormat, uint32_t imageDataOffset) {
    if (offset + 8 + 5 > fontData.size()) return false;
    const uint8_t* p = fontData.data() + offset + 8;

    uint8_t imageSize = p[0];
    int8_t bigMetrics = static_cast<int8_t>(p[1]);
    int8_t bearingX = static_cast<int8_t>(p[2]);
    int8_t bearingY = static_cast<int8_t>(p[3]);
    uint8_t advance = p[4];

    size_t glyphDataOffset = offset + 8 + 5;
    // glyphOffsets array length = number of glyphs in range (we don't have explicit first/last here,
    // но они были добавлены ранее при чтении indexSubtableArray; поэтому безопасно пробегаем strike.glyphIDs)
    for (size_t i = 0; i < strike.glyphIDs.size(); ++i) {
        uint16_t gid = strike.glyphIDs[i];
        if (glyphDataOffset + 4 > fontData.size()) break;
        uint32_t glyphImageOffset = readUInt32(fontData.data() + glyphDataOffset);

        GlyphImage img;
        img.glyphID = gid;
        img.imageFormat = imageFormat;
        img.offset = imageDataOffset + glyphImageOffset;
        img.bearingX = bearingX;
        img.bearingY = bearingY;
        img.advance = advance;

        if (bigMetrics) {
            img.width = imageSize;
            img.height = imageSize;
        } else {
            img.width = static_cast<uint16_t>((imageSize + 7) / 8);
            img.height = imageSize;
        }

        // сохраняем; данные будем извлекать позже при разборе CBDT
        strike.glyphImages[gid] = std::move(img);
        glyphDataOffset += 4;
    }
    return true;
}

/* indexFormat 2:
   contains per-glyph records (offset, size, maybe bigMetrics fields)
*/
bool CBDT_CBLC_Parser::parseIndexFormat2(uint32_t offset, StrikeRecord& strike,
                                         uint16_t /*firstGlyph*/, uint16_t /*lastGlyph*/,
                                         uint16_t imageFormat, uint32_t imageDataOffset) {
    uint32_t glyphDataOffset = offset + 8;
    size_t i = 0;
    while (i < strike.glyphIDs.size()) {
        if (glyphDataOffset + 6 > fontData.size()) break;
        const uint8_t* p = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(p);
        uint8_t imageSize = p[4];
        int8_t bigMetrics = static_cast<int8_t>(p[5]);

        GlyphImage img;
        img.glyphID = strike.glyphIDs[i];
        img.imageFormat = imageFormat;
        img.offset = imageDataOffset + glyphImageOffset;

        if (bigMetrics) {
            if (glyphDataOffset + 16 > fontData.size()) break;
            img.width = readUInt16(fontData.data() + glyphDataOffset + 6);
            img.height = readUInt16(fontData.data() + glyphDataOffset + 8);
            img.bearingX = static_cast<int16_t>(readUInt16(fontData.data() + glyphDataOffset + 10));
            img.bearingY = static_cast<int16_t>(readUInt16(fontData.data() + glyphDataOffset + 12));
            img.advance = readUInt16(fontData.data() + glyphDataOffset + 14);
            glyphDataOffset += 16;
        } else {
            if (glyphDataOffset + 9 > fontData.size()) break;
            img.width = imageSize;
            img.height = imageSize;
            img.bearingX = static_cast<int8_t>(fontData[glyphDataOffset + 6]);
            img.bearingY = static_cast<int8_t>(fontData[glyphDataOffset + 7]);
            img.advance = fontData[glyphDataOffset + 8];
            glyphDataOffset += 9;
        }

        strike.glyphImages[img.glyphID] = std::move(img);
        ++i;
    }
    return true;
}

/* indexFormat 5:
   first a list of glyphIDs, then offsets for each glyph
*/
bool CBDT_CBLC_Parser::parseIndexFormat5(uint32_t offset, StrikeRecord& strike,
                                         uint16_t /*firstGlyph*/, uint16_t /*lastGlyph*/,
                                         uint16_t imageFormat, uint32_t imageDataOffset) {
    uint32_t glyphDataOffset = offset + 8;
    // Читаем numGlyphs равный количеству glyphIDs в диапазоне
    size_t numGlyphs = strike.glyphIDs.size();
    std::vector<uint16_t> glyphIDs;
    glyphIDs.reserve(numGlyphs);
    for (size_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 2 > fontData.size()) break;
        uint16_t gid = readUInt16(fontData.data() + glyphDataOffset);
        glyphIDs.push_back(gid);
        glyphDataOffset += 2;
    }
    // Затем идут offsets
    for (size_t i = 0; i < glyphIDs.size(); ++i) {
        if (glyphDataOffset + 4 > fontData.size()) break;
        uint32_t glyphImageOffset = readUInt32(fontData.data() + glyphDataOffset);
        GlyphImage img;
        img.glyphID = glyphIDs[i];
        img.imageFormat = imageFormat;
        img.offset = imageDataOffset + glyphImageOffset;
        // остальное неизвестно — будет заполнено при extract
        strike.glyphImages[img.glyphID] = std::move(img);
        glyphDataOffset += 4;
    }
    return true;
}

/* ---------- CBDT parsing: извлечение данных изображений ---------- */

bool CBDT_CBLC_Parser::parseCBDTTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        std::cerr << "CBDT: table too small\n";
        return false;
    }
    const uint8_t* base = fontData.data() + offset;
    uint32_t version = readUInt32(base);
    // Проходим по всем страйкам и их glyphImages и извлекаем данные
    for (auto& strikePair : strikes) {
        StrikeRecord& strike = strikePair.second;
        for (auto& gpair : strike.glyphImages) {
            GlyphImage& gi = gpair.second;
            // gi.offset — относительно начала CBDT (по спецификации imageDataOffset + glyphOffset)
            if (!extractGlyphImageData(gi.offset, gi, offset, length)) {
                // Если извлечение не удалось — ставим пустые данные, но не фатальная ошибка
                std::cerr << "CBDT: failed to extract image for glyph " << gi.glyphID << std::endl;
                continue;
            }
        }
    }
    return true;
}

bool CBDT_CBLC_Parser::extractGlyphImageData(uint32_t imageOffset, GlyphImage& image, uint32_t cbdtBase, uint32_t cbdtLength) {
    // imageOffset — смещение от начала CBDT таблицы
    if (cbdtBase + imageOffset >= fontData.size()) return false;
    const uint8_t* data = fontData.data() + cbdtBase + imageOffset;
    size_t available = static_cast<size_t>(std::min<uint32_t>(fontData.size() - (cbdtBase + imageOffset), cbdtLength));

    // Попробуем угадать формат по image.imageFormat
    switch (image.imageFormat) {
        case 1: case 2: case 3: case 4: case 8: case 9:
            return extractBitmapData(data, available, image);
        case 5: case 17: case 18:
            return extractPNGData(data, available, image);
        case 6:
            return extractJPEGData(data, available, image);
        case 7:
            return extractTIFFData(data, available, image);
        default:
            // Эвристики: если начинается с PNG сигнатуры — PNG, если 0xFF 0xD8 — JPEG, иначе bitmap
            if (available >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
                return extractPNGData(data, available, image);
            }
            if (available >= 2 && data[0] == 0xFF && data[1] == 0xD8) {
                return extractJPEGData(data, available, image);
            }
            return extractBitmapData(data, available, image);
    }
}

bool CBDT_CBLC_Parser::extractBitmapData(const uint8_t* data, size_t available, GlyphImage& image) {
    // Для простоты берем весь доступный кусок (эвристика). В rebuilder-е мы будем просто склеивать данные.
    size_t copyLen = available;
    if (copyLen == 0) return false;
    image.data.assign(data, data + copyLen);
    image.length = static_cast<uint32_t>(copyLen);
    return true;
}

bool CBDT_CBLC_Parser::extractPNGData(const uint8_t* data, size_t available, GlyphImage& image) {
    // Найдём IEND chunk. Проверяем наличие сигнатуры PNG в начале
    if (available < 8) return false;
    // PNG сигнатура 8 байт
    if (!(data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
          data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A)) {
        // не PNG
        return false;
    }
    // Ищем "IEND" + 4 байта CRC после
    const uint8_t* cur = data;
    const uint8_t* end = data + available;
    while (cur + 8 <= end) {
        if (cur[1] == 'I' && cur[2] == 'E' && cur[3] == 'N' && cur[4] == 'D') {
            // смещение до "IEND" найдено (предполагаем, что перед ним находится длина/chunk)
            size_t iendPos = cur - data;
            size_t total = iendPos + 8; // "IEND" chunk 4 bytes tag + 4 bytes CRC (+ preceding length not considered)
            if (total > available) total = available;
            image.data.assign(data, data + total);
            image.length = static_cast<uint32_t>(image.data.size());
            return true;
        }
        ++cur;
    }
    // fallback: просто возьмём максимум до доступного
    image.data.assign(data, data + available);
    image.length = static_cast<uint32_t>(available);
    return true;
}

bool CBDT_CBLC_Parser::extractJPEGData(const uint8_t* data, size_t available, GlyphImage& image) {
    // ищем EOI 0xFF 0xD9
    const uint8_t* cur = data;
    const uint8_t* end = data + available;
    while (cur + 2 <= end) {
        if (cur[0] == 0xFF && cur[1] == 0xD9) {
            size_t len = (cur + 2) - data;
            image.data.assign(data, data + len);
            image.length = static_cast<uint32_t>(len);
            return true;
        }
        ++cur;
    }
    image.data.assign(data, data + available);
    image.length = static_cast<uint32_t>(available);
    return true;
}

bool CBDT_CBLC_Parser::extractTIFFData(const uint8_t* data, size_t available, GlyphImage& image) {
    if (available < 8) return false;
    bool bigEndian = (data[0] == 'M' && data[1] == 'M');
    // Проверка сигнатуры TIFF (II -> 0x49 0x49, MM -> 0x4D 0x4D)
    if (!((!bigEndian && data[0] == 0x49 && data[1] == 0x49) || (bigEndian && data[0] == 0x4D && data[1] == 0x4D))) {
        return false;
    }
    uint32_t ifdOffset = readUInt32(data + 4, bigEndian);
    if (!parseTIFFDirectory(data, available, ifdOffset, image, bigEndian)) {
        // fallback: берем весь доступный кусок
        image.data.assign(data, data + available);
        image.length = static_cast<uint32_t>(available);
        return true;
    }
    // если parseTIFFDirectory не сохранил данные — сохраняем весь блок
    if (image.data.empty()) {
        image.data.assign(data, data + available);
        image.length = static_cast<uint32_t>(available);
    }
    return true;
}

bool CBDT_CBLC_Parser::parseTIFFDirectory(const uint8_t* data, size_t available, uint32_t ifdOffset, GlyphImage& image, bool bigEndian) {
    if (ifdOffset + 2 > available) return false;
    const uint8_t* dir = data + ifdOffset;
    uint16_t entryCount = readUInt16(dir, bigEndian);
    if (ifdOffset + 2 + entryCount * 12 > available) return false;

    // Минимально извлечём некоторые теги: width/height/strip offsets/byte counts
    struct TIFFInfo {
        uint32_t imageWidth = 0, imageHeight = 0;
        uint32_t stripOffset = 0, stripByteCount = 0;
        uint16_t bitsPerSample = 1;
        uint16_t compression = 1;
        double xRes = 72.0, yRes = 72.0;
    } info;

    for (uint16_t i = 0; i < entryCount; ++i) {
        const uint8_t* entry = dir + 2 + i * 12;
        uint16_t tag = readUInt16(entry, bigEndian);
        uint16_t type = readUInt16(entry + 2, bigEndian);
        uint32_t count = readUInt32(entry + 4, bigEndian);
        uint32_t valueOffset = readUInt32(entry + 8, bigEndian);

        // Простая поддержка нужных тегов
        switch (tag) {
            case 256: // ImageWidth
                if (type == 3) info.imageWidth = valueOffset;
                break;
            case 257: // ImageLength
                if (type == 3) info.imageHeight = valueOffset;
                break;
            case 273: // StripOffsets
                info.stripOffset = valueOffset;
                break;
            case 279: // StripByteCounts
                info.stripByteCount = valueOffset;
                break;
            case 282: // XResolution (RATIONAL)
                // пропускаем для простоты
                break;
            case 283: // YResolution
                break;
            default:
                break;
        }
    }

    // Если есть stripOffset/byteCount — попытаемся вырезать кусок
    if (info.stripOffset && info.stripByteCount && info.stripOffset + info.stripByteCount <= available) {
        image.data.assign(data + info.stripOffset, data + info.stripOffset + info.stripByteCount);
        image.length = info.stripByteCount;
        if (info.imageWidth) image.width = static_cast<uint16_t>(info.imageWidth);
        if (info.imageHeight) image.height = static_cast<uint16_t>(info.imageHeight);
        return true;
    }

    return false;
}

/* ---------- CMAP parsing (опционально) ---------- */

bool CBDT_CBLC_Parser::parseCMAPTable(uint32_t offset, uint32_t length) {
    // Пробуем использовать utils::CMAPParser, если он есть
    try {
        std::vector<uint8_t> cmapData(fontData.begin() + offset, fontData.begin() + offset + length);
        utils::CMAPParser cmapParser(cmapData);
        if (!cmapParser.parse()) return false;

        auto glyphToCharMap = cmapParser.getGlyphToCharMap();
        // Для каждого страйка: если glyphID не содержит отображения в cmap -> считаем удалённым
        for (const auto& s : strikes) {
            for (uint16_t gid : s.second.glyphIDs) {
                if (glyphToCharMap.find(gid) == glyphToCharMap.end() ||
                    glyphToCharMap.at(gid).empty()) {
                    if (std::find(removedGlyphs.begin(), removedGlyphs.end(), gid) == removedGlyphs.end())
                        removedGlyphs.push_back(gid);
                }
            }
        }
        return true;
    } catch (...) {
        // если нет CMAPParser или он упал — просто возвращаем false, это не фатально
        return false;
    }
}

/* ---------- helpers ---------- */

uint32_t CBDT_CBLC_Parser::readUInt32(const uint8_t* p) const {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint16_t CBDT_CBLC_Parser::readUInt16(const uint8_t* p) const {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

uint16_t CBDT_CBLC_Parser::readUInt16(const uint8_t* p, bool bigEndian) const {
    if (bigEndian) return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
    return (uint16_t(p[1]) << 8) | uint16_t(p[0]);
}

uint32_t CBDT_CBLC_Parser::readUInt32(const uint8_t* p, bool bigEndian) const {
    if (bigEndian) return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    return (uint32_t(p[3]) << 24) | (uint32_t(p[2]) << 16) | (uint32_t(p[1]) << 8) | uint32_t(p[0]);
}

} // namespace fontmaster
