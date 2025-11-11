#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include <iostream>
#include <algorithm>

namespace fontmaster {

CBDT_CBLC_Parser::CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData) 
    : fontData(fontData) {}

bool CBDT_CBLC_Parser::parse() {
    auto tables = utils::parseTTFTables(fontData);
    
    // Парсинг CBLC таблицы
    auto cblcTable = std::find_if(tables.begin(), tables.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "CBLC";
        });
    
    if (cblcTable != tables.end()) {
        if (!parseCBLCTable(cblcTable->offset, cblcTable->length)) {
            return false;
        }
    }
    
    // Парсинг CBDT таблицы
    auto cbdtTable = std::find_if(tables.begin(), tables.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "CBDT";
        });
    
    if (cbdtTable != tables.end()) {
        if (!parseCBDTTable(cbdtTable->offset, cbdtTable->length)) {
            return false;
        }
    }
    
    // Парсинг cmap таблицы для определения удаленных глифов
    auto cmapTable = std::find_if(tables.begin(), tables.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "cmap";
        });
    
    if (cmapTable != tables.end()) {
        parseCMAPTable(cmapTable->offset, cmapTable->length);
    }
    
    std::cout << "CBDT/CBLC Parser: Found " << strikes.size() << " strikes, " 
              << removedGlyphs.size() << " removed glyphs" << std::endl;
    
    return true;
}

bool CBDT_CBLC_Parser::parseCBLCTable(uint32_t offset, uint32_t length) {
    if (offset + 8 > fontData.size()) {
        std::cerr << "CBLC: Table too small" << std::endl;
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    uint32_t version = readUInt32(data);
    uint16_t numStrikes = readUInt16(data + 4);
    
    std::cout << "CBLC: Version: " << std::hex << version << std::dec 
              << ", Strikes: " << numStrikes << std::endl;
    
    if (version != 0x00020000) {
        std::cerr << "CBLC: Unsupported version" << std::endl;
        return false;
    }
    
    // Парсинг информации о страйках
    uint32_t strikeOffsetsOffset = offset + 8;
    
    for (uint16_t i = 0; i < numStrikes; ++i) {
        if (strikeOffsetsOffset + 4 > fontData.size()) {
            break;
        }
        
        const uint8_t* offsetData = fontData.data() + strikeOffsetsOffset;
        uint32_t strikeOffset = readUInt32(offsetData);
        
        if (!parseStrike(offset + strikeOffset, i)) {
            return false;
        }
        strikeOffsetsOffset += 4;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseStrike(uint32_t offset, uint32_t strikeIndex) {
    if (offset + 48 > fontData.size()) {
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    StrikeRecord strike;
    strike.ppem = readUInt16(data);
    strike.resolution = readUInt16(data + 2);
    
    // Пропускаем colorRef
    uint16_t startGlyphIndex = readUInt16(data + 12);
    uint16_t endGlyphIndex = readUInt16(data + 14);
    uint16_t ppemX = readUInt16(data + 16);
    uint16_t ppemY = readUInt16(data + 18);
    uint8_t bitDepth = data[20];
    uint16_t flags = readUInt16(data + 21);
    
    uint32_t indexSubtableArrayOffset = readUInt32(data + 24);
    uint32_t numberOfIndexSubTables = readUInt32(data + 28);
    
    uint32_t indexSubtableOffset = offset + indexSubtableArrayOffset;
    
    for (uint32_t i = 0; i < numberOfIndexSubTables; ++i) {
        if (indexSubtableOffset + 8 > fontData.size()) {
            break;
        }
        
        if (!parseIndexSubtable(indexSubtableOffset, strike)) {
            return false;
        }
        indexSubtableOffset += 8;
    }
    
    strikes[strikeIndex] = strike;
    return true;
}

bool CBDT_CBLC_Parser::parseIndexSubtable(uint32_t offset, StrikeRecord& strike) {
    if (offset + 8 > fontData.size()) {
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    uint16_t firstGlyphIndex = readUInt16(data);
    uint16_t lastGlyphIndex = readUInt16(data + 2);
    uint32_t additionalOffsetToIndexSubtable = readUInt32(data + 4);
    
    uint32_t subtableOffset = offset + additionalOffsetToIndexSubtable;
    if (subtableOffset + 8 > fontData.size()) {
        return false;
    }
    
    const uint8_t* subtableData = fontData.data() + subtableOffset;
    uint16_t indexFormat = readUInt16(subtableData);
    uint16_t imageFormat = readUInt16(subtableData + 2);
    uint32_t imageDataOffset = readUInt32(subtableData + 4);
    
    switch (indexFormat) {
        case 1:
            return parseIndexFormat1(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        case 2:
            return parseIndexFormat2(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        case 5:
            return parseIndexFormat5(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        default:
            std::cout << "Unsupported index format: " << indexFormat << std::endl;
            return false;
    }
}

bool CBDT_CBLC_Parser::parseIndexFormat1(uint32_t offset, StrikeRecord& strike, 
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    const uint8_t* data = fontData.data() + offset + 8;
    
    uint8_t imageSize = data[0];
    int8_t bigMetrics = data[1];
    int8_t bearingX = static_cast<int8_t>(data[2]);
    int8_t bearingY = static_cast<int8_t>(data[3]);
    uint8_t advance = data[4];
    
    uint32_t glyphDataOffset = offset + 8 + 5;
    
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (glyphDataOffset + 4 > fontData.size()) {
            break;
        }
        
        const uint8_t* glyphOffsetData = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(glyphOffsetData);
        
        strike.glyphIDs.push_back(glyphID);
        
        GlyphImage glyphImage;
        glyphImage.glyphID = glyphID;
        glyphImage.imageFormat = imageFormat;
        glyphImage.offset = imageDataOffset + glyphImageOffset;
        glyphImage.bearingX = bearingX;
        glyphImage.bearingY = bearingY;
        glyphImage.advance = advance;
        
        if (bigMetrics) {
            glyphImage.width = imageSize;
            glyphImage.height = imageSize;
        } else {
            glyphImage.width = (imageSize + 7) / 8;
            glyphImage.height = imageSize;
        }
        
        strike.glyphImages[glyphID] = glyphImage;
        glyphDataOffset += 4;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseIndexFormat2(uint32_t offset, StrikeRecord& strike,
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    uint32_t glyphDataOffset = offset + 8;
    
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (glyphDataOffset + 6 > fontData.size()) {
            break;
        }
        
        const uint8_t* glyphDataPtr = fontData.data() + glyphDataOffset;
        
        uint32_t glyphImageOffset = readUInt32(glyphDataPtr);
        uint8_t imageSize = glyphDataPtr[4];
        int8_t bigMetrics = glyphDataPtr[5];
        
        GlyphImage glyphImage;
        glyphImage.glyphID = glyphID;
        glyphImage.imageFormat = imageFormat;
        glyphImage.offset = imageDataOffset + glyphImageOffset;
        
        if (bigMetrics) {
            if (glyphDataOffset + 16 > fontData.size()) {
                break;
            }
            glyphImage.width = readUInt16(glyphDataPtr + 6);
            glyphImage.height = readUInt16(glyphDataPtr + 8);
            glyphImage.bearingX = static_cast<int16_t>(readUInt16(glyphDataPtr + 10));
            glyphImage.bearingY = static_cast<int16_t>(readUInt16(glyphDataPtr + 12));
            glyphImage.advance = readUInt16(glyphDataPtr + 14);
            glyphDataOffset += 16;
        } else {
            if (glyphDataOffset + 9 > fontData.size()) {
                break;
            }
            glyphImage.width = imageSize;
            glyphImage.height = imageSize;
            glyphImage.bearingX = static_cast<int8_t>(glyphDataPtr[6]);
            glyphImage.bearingY = static_cast<int8_t>(glyphDataPtr[7]);
            glyphImage.advance = glyphDataPtr[8];
            glyphDataOffset += 9;
        }
        
        strike.glyphIDs.push_back(glyphID);
        strike.glyphImages[glyphID] = glyphImage;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseIndexFormat5(uint32_t offset, StrikeRecord& strike,
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    uint32_t glyphDataOffset = offset + 8;
    uint16_t numGlyphs = lastGlyph - firstGlyph + 1;
    
    std::vector<uint16_t> glyphIDs;
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 2 > fontData.size()) {
            break;
        }
        
        const uint8_t* glyphIDData = fontData.data() + glyphDataOffset;
        uint16_t glyphID = readUInt16(glyphIDData);
        glyphIDs.push_back(glyphID);
        glyphDataOffset += 2;
    }
    
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 4 > fontData.size()) {
            break;
        }
        
        const uint8_t* offsetData = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(offsetData);
        
        GlyphImage glyphImage;
        glyphImage.glyphID = glyphIDs[i];
        glyphImage.imageFormat = imageFormat;
        glyphImage.offset = imageDataOffset + glyphImageOffset;
        glyphImage.width = 0;
        glyphImage.height = 0;
        glyphImage.bearingX = 0;
        glyphImage.bearingY = 0;
        glyphImage.advance = 0;
        
        strike.glyphIDs.push_back(glyphIDs[i]);
        strike.glyphImages[glyphIDs[i]] = glyphImage;
        glyphDataOffset += 4;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseCBDTTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        std::cerr << "CBDT: Table too small" << std::endl;
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    uint32_t version = readUInt32(data);
    
    std::cout << "CBDT: Version: " << std::hex << version << std::dec << std::endl;
    
    for (auto& strikePair : strikes) {
        auto& strike = strikePair.second;
        
        for (auto& glyphPair : strike.glyphImages) {
            auto& glyphImage = glyphPair.second;
            
            if (glyphImage.offset >= offset + length) {
                continue;
            }
            
            if (!extractGlyphImageData(offset + glyphImage.offset, glyphImage)) {
                return false;
            }
        }
    }
    
    return true;
}

bool CBDT_CBLC_Parser::extractGlyphImageData(uint32_t imageOffset, GlyphImage& image) {
    if (imageOffset >= fontData.size()) {
        return false;
    }
    
    const uint8_t* imageData = fontData.data() + imageOffset;
    
    switch (image.imageFormat) {
        case 1: case 3: case 8:
            return extractBitmapData(imageData, image);
        case 2: case 4: case 9:
            return extractBitmapData(imageData, image);
        case 5: case 17: case 18:
            return extractPNGData(imageData, image);
        case 6:
            return extractJPEGData(imageData, image);
        case 7:
            return extractTIFFData(imageData, image);
        default:
            std::cout << "Unsupported image format: " << image.imageFormat << std::endl;
            return false;
    }
}

bool CBDT_CBLC_Parser::extractBitmapData(const uint8_t* data, GlyphImage& image) {
    uint32_t estimatedSize = calculateBitmapSize(image.width, image.height, image.imageFormat);
    uint32_t actualSize = std::min(estimatedSize, static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    
    image.data.assign(data, data + actualSize);
    image.length = actualSize;
    return true;
}

bool CBDT_CBLC_Parser::extractPNGData(const uint8_t* data, GlyphImage& image) {
    const uint8_t* current = data;
    uint32_t pngLength = 0;
    
    while (current + 8 <= fontData.data() + fontData.size()) {
        if (current[0] == 0x49 && current[1] == 0x45 && current[2] == 0x4E && current[3] == 0x44) {
            pngLength = (current - data) + 12;
            break;
        }
        current++;
    }
    
    if (pngLength == 0) {
        pngLength = std::min(static_cast<uint32_t>(1024 * 1024), 
                           static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    }
    
    image.data.assign(data, data + pngLength);
    image.length = pngLength;
    return true;
}

bool CBDT_CBLC_Parser::extractJPEGData(const uint8_t* data, GlyphImage& image) {
    const uint8_t* current = data;
    uint32_t jpegLength = 0;
    
    while (current + 2 <= fontData.data() + fontData.size()) {
        if (current[0] == 0xFF && current[1] == 0xD9) {
            jpegLength = (current - data) + 2;
            break;
        }
        current++;
    }
    
    if (jpegLength == 0) {
        jpegLength = std::min(static_cast<uint32_t>(1024 * 1024), 
                            static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    }
    
    image.data.assign(data, data + jpegLength);
    image.length = jpegLength;
    return true;
}

bool CBDT_CBLC_Parser::extractTIFFData(const uint8_t* data, GlyphImage& image) {
    if (data + 8 > fontData.data() + fontData.size()) {
        return false;
    }
    
    // Определяем порядок байт
    bool bigEndian = (data[0] == 0x4D && data[1] == 0x4D);
    
    // Проверяем TIFF signature
    if ((bigEndian && data[2] != 0x00 && data[3] != 0x2A) ||
        (!bigEndian && data[2] != 0x2A && data[3] != 0x00)) {
        std::cerr << "Invalid TIFF signature" << std::endl;
        return false;
    }
    
    // Читаем offset первого IFD
    uint32_t ifdOffset = readUInt32(data + 4, bigEndian);
    
    // Парсим TIFF директории
    return parseTIFFDirectory(data, ifdOffset, image, bigEndian);
}

bool CBDT_CBLC_Parser::parseTIFFDirectory(const uint8_t* data, uint32_t offset, GlyphImage& image, bool bigEndian) {
    if (data + offset + 2 > fontData.data() + fontData.size()) {
        return false;
    }
    
    const uint8_t* dirData = data + offset;
    uint16_t entryCount = readTIFFShort(dirData, bigEndian);
    
    if (data + offset + 2 + entryCount * 12 > fontData.data() + fontData.size()) {
        return false;
    }
    
    // Структура для хранения TIFF параметров
    struct TIFFInfo {
        uint32_t imageWidth = 0;
        uint32_t imageHeight = 0;
        uint32_t stripOffset = 0;
        uint32_t stripByteCount = 0;
        uint16_t bitsPerSample = 1;
        uint16_t compression = 1;
        uint16_t photometric = 0;
        uint16_t samplesPerPixel = 1;
        uint16_t resolutionUnit = 2; // По умолчанию - дюймы
        double xResolution = 72.0;   // По умолчанию 72 DPI
        double yResolution = 72.0;   // По умолчанию 72 DPI
        uint16_t orientation = 1;    // По умолчанию нормальная ориентация
        uint16_t predictor = 1;      // По умолчанию без предсказания
        uint32_t rowsPerStrip = 0;
    } tiffInfo;
    
    // Функция для чтения TIFF значения
    auto readTIFFValue = [&](uint32_t valueOffset, uint16_t dataType, uint32_t count) -> std::vector<uint32_t> {
        std::vector<uint32_t> values;
        
        const uint8_t* valueData = (count <= 4 && dataType != 5) ? 
            reinterpret_cast<const uint8_t*>(&valueOffset) : data + valueOffset;
        
        for (uint32_t i = 0; i < count && i < 16; ++i) { // Ограничиваем для безопасности
            switch (dataType) {
                case 1: // BYTE
                    if (valueData + 1 <= fontData.data() + fontData.size()) {
                        values.push_back(valueData[i]);
                    }
                    break;
                    
                case 3: // SHORT
                    if (valueData + (i + 1) * 2 <= fontData.data() + fontData.size()) {
                        values.push_back(readTIFFShort(valueData + i * 2, bigEndian));
                    }
                    break;
                    
                case 4: // LONG
                    if (valueData + (i + 1) * 4 <= fontData.data() + fontData.size()) {
                        values.push_back(readTIFFLong(valueData + i * 4, bigEndian));
                    }
                    break;
                    
                case 5: // RATIONAL
                    if (valueData + (i + 1) * 8 <= fontData.data() + fontData.size()) {
                        uint32_t numerator = readTIFFLong(valueData + i * 8, bigEndian);
                        uint32_t denominator = readTIFFLong(valueData + i * 8 + 4, bigEndian);
                        if (denominator != 0) {
                            values.push_back(numerator / denominator); // Упрощенное значение
                        } else {
                            values.push_back(0);
                        }
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        return values;
    };
    
    // Парсим все TIFF entries
    for (uint16_t i = 0; i < entryCount; ++i) {
        const uint8_t* entry = dirData + 2 + i * 12;
        uint16_t tag = readTIFFShort(entry, bigEndian);
        uint16_t type = readTIFFShort(entry + 2, bigEndian);
        uint32_t count = readTIFFLong(entry + 4, bigEndian);
        uint32_t valueOrOffset = readTIFFLong(entry + 8, bigEndian);
        
        auto values = readTIFFValue(valueOrOffset, type, count);
        
        if (values.empty()) continue;
        
        // Обрабатываем все важные теги
        switch (tag) {
            case TIFF_TAG_IMAGE_WIDTH:
                tiffInfo.imageWidth = values[0];
                break;
                
            case TIFF_TAG_IMAGE_LENGTH:
                tiffInfo.imageHeight = values[0];
                break;
                
            case TIFF_TAG_BITS_PER_SAMPLE:
                tiffInfo.bitsPerSample = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_COMPRESSION:
                tiffInfo.compression = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_PHOTOMETRIC:
                tiffInfo.photometric = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_STRIP_OFFSETS:
                tiffInfo.stripOffset = values[0];
                break;
                
            case TIFF_TAG_SAMPLES_PER_PIXEL:
                tiffInfo.samplesPerPixel = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_ROWS_PER_STRIP:
                tiffInfo.rowsPerStrip = values[0];
                break;
                
            case TIFF_TAG_STRIP_BYTE_COUNTS:
                tiffInfo.stripByteCount = values[0];
                break;
                
            case TIFF_TAG_ORIENTATION:
                tiffInfo.orientation = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_RESOLUTION_UNIT:
                tiffInfo.resolutionUnit = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_PREDICTOR:
                tiffInfo.predictor = static_cast<uint16_t>(values[0]);
                break;
                
            case TIFF_TAG_XRESOLUTION:
                if (type == 5 && count >= 1) {
                    const uint8_t* rationalData = data + valueOrOffset;
                    if (rationalData + 8 <= fontData.data() + fontData.size()) {
                        uint32_t numerator = readTIFFLong(rationalData, bigEndian);
                        uint32_t denominator = readTIFFLong(rationalData + 4, bigEndian);
                        if (denominator != 0) {
                            tiffInfo.xResolution = static_cast<double>(numerator) / denominator;
                        }
                    }
                }
                break;
                
            case TIFF_TAG_YRESOLUTION:
                if (type == 5 && count >= 1) {
                    const uint8_t* rationalData = data + valueOrOffset;
                    if (rationalData + 8 <= fontData.data() + fontData.size()) {
                        uint32_t numerator = readTIFFLong(rationalData, bigEndian);
                        uint32_t denominator = readTIFFLong(rationalData + 4, bigEndian);
                        if (denominator != 0) {
                            tiffInfo.yResolution = static_cast<double>(numerator) / denominator;
                        }
                    }
                }
                break;
        }
    }
    
    // Устанавливаем параметры изображения
    if (tiffInfo.imageWidth > 0) image.width = static_cast<uint16_t>(tiffInfo.imageWidth);
    if (tiffInfo.imageHeight > 0) image.height = static_cast<uint16_t>(tiffInfo.imageHeight);
    
    // Логируем информацию о TIFF
    std::cout << "TIFF Image: " << image.width << "x" << image.height 
              << ", " << tiffInfo.bitsPerSample << "bpp"
              << ", Compression: " << tiffInfo.compression
              << ", Resolution: " << tiffInfo.xResolution << "x" << tiffInfo.yResolution 
              << " " << (tiffInfo.resolutionUnit == 2 ? "DPI" : 
                         tiffInfo.resolutionUnit == 3 ? "DPCM" : "unknown") 
              << std::endl;
    
    // Рассчитываем размер данных
    uint32_t tiffLength = 0;
    if (tiffInfo.stripOffset > 0 && tiffInfo.stripByteCount > 0) {
        tiffLength = tiffInfo.stripOffset + tiffInfo.stripByteCount;
        
        // Добавляем буфер для заголовков и других данных
        tiffLength = std::min(tiffLength + 512, static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    } else {
        // Эвристика: ищем конец TIFF данных
        const uint8_t* current = data;
        uint32_t maxSearch = std::min(static_cast<uint32_t>(1024 * 1024), 
                                    static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
        
        // Ищем последовательность, которая может указывать на конец TIFF
        for (uint32_t pos = 0; pos < maxSearch - 8; ++pos) {
            if (current[pos] == 0xFF && current[pos + 1] == 0xD9) { // JPEG EOI
                tiffLength = pos + 2;
                break;
            }
            if (current[pos] == 0x49 && current[pos + 1] == 0x45 && 
                current[pos + 2] == 0x4E && current[pos + 3] == 0x44) { // PNG IEND
                tiffLength = pos + 12;
                break;
            }
        }
        
        if (tiffLength == 0) {
            tiffLength = maxSearch;
        }
    }
    
    // Сохраняем TIFF данные
    image.data.assign(data, data + tiffLength);
    image.length = tiffLength;
    
    // Корректируем метрики на основе разрешения, если нужно
    if (tiffInfo.xResolution != 72.0 || tiffInfo.yResolution != 72.0) {
        // В шрифтах обычно используется 72 DPI, корректируем если отличается
        double scaleX = tiffInfo.xResolution / 72.0;
        double scaleY = tiffInfo.yResolution / 72.0;
        
        if (image.bearingX == 0 && image.bearingY == 0) {
            image.bearingX = 0;
            image.bearingY = static_cast<int16_t>(image.height * scaleY);
            image.advance = static_cast<uint16_t>(image.width * scaleX + 1);
        }
    } else {
        // Стандартные метрики для 72 DPI
        if (image.bearingX == 0 && image.bearingY == 0) {
            image.bearingX = 0;
            image.bearingY = static_cast<int16_t>(image.height);
            image.advance = static_cast<uint16_t>(image.width + 1);
        }
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseCMAPTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        return false;
    }
    
    std::vector<uint8_t> cmapData(fontData.begin() + offset, 
                                 fontData.begin() + offset + length);
    
    utils::CMAPParser cmapParser(cmapData);
    if (cmapParser.parse()) {
        auto glyphToCharMap = cmapParser.getGlyphToCharMap();
        
        for (const auto& strikePair : strikes) {
            for (uint16_t glyphID : strikePair.second.glyphIDs) {
                if (glyphToCharMap.find(glyphID) == glyphToCharMap.end() || 
                    glyphToCharMap[glyphID].empty()) {
                    if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
                        removedGlyphs.push_back(glyphID);
                    }
                }
            }
        }
    }
    
    return true;
}

uint32_t CBDT_CBLC_Parser::calculateBitmapSize(uint16_t width, uint16_t height, uint16_t format) {
    switch (format) {
        case 1: case 3: case 8:
            return ((width + 7) / 8) * height;
        case 2: case 4: case 9:
            return ((width * height) + 7) / 8;
        default:
            return 1024;
    }
}

uint32_t CBDT_CBLC_Parser::readUInt32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint16_t CBDT_CBLC_Parser::readUInt16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

uint16_t CBDT_CBLC_Parser::readUInt16(const uint8_t* data, bool bigEndian) {
    if (bigEndian) {
        return (data[0] << 8) | data[1];
    } else {
        return (data[1] << 8) | data[0];
    }
}

uint32_t CBDT_CBLC_Parser::readUInt32(const uint8_t* data, bool bigEndian) {
    if (bigEndian) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    } else {
        return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
    }
}

uint16_t CBDT_CBLC_Parser::readTIFFShort(const uint8_t* data, bool bigEndian) {
    return readUInt16(data, bigEndian);
}

uint32_t CBDT_CBLC_Parser::readTIFFLong(const uint8_t* data, bool bigEndian) {
    return readUInt32(data, bigEndian);
}

} // namespace fontmaster
