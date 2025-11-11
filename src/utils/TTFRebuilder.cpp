#include "fontmaster/TTFRebuilder.h"
#include "fontmaster/TTFUtils.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <functional>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace fontmaster {

// Константы для таблиц
const uint32_t TTF_MAGIC = 0x00010000;
const uint32_t HEAD_MAGIC = 0x5F0F3CF5;
const size_t HEAD_TABLE_SIZE = 54;
const size_t MAXP_TABLE_SIZE = 32;
const size_t HHEA_TABLE_SIZE = 36;
const size_t OS2_TABLE_SIZE = 96;

// Максимальный индекс для стандартных имен глифов
const uint16_t MAX_STANDARD_NAME_INDEX = 32767;

TTFRebuilder::TTFRebuilder(const std::vector<uint8_t>& fontData) 
    : originalData(fontData), numGlyphs(0), numHMetrics(0), locaShortFormat(false) {
    
    // Регистрируем обработчики для таблиц, требующих специальной логики
    rebuildHandlers["glyf"] = [this](const std::string& tag) { rebuildGlyfTable(tag); };
    rebuildHandlers["loca"] = [this](const std::string& tag) { rebuildLocaTable(tag); };
    rebuildHandlers["hmtx"] = [this](const std::string& tag) { rebuildHmtxTable(tag); };
    rebuildHandlers["hhea"] = [this](const std::string& tag) { rebuildHheaTable(tag); };
    rebuildHandlers["maxp"] = [this](const std::string& tag) { rebuildMaxpTable(tag); };
    rebuildHandlers["name"] = [this](const std::string& tag) { rebuildNameTable(tag); };
    rebuildHandlers["OS/2"] = [this](const std::string& tag) { rebuildOS2Table(tag); };
    rebuildHandlers["head"] = [this](const std::string& tag) { rebuildHeadTable(tag); };
    rebuildHandlers["post"] = [this](const std::string& tag) { rebuildPostTable(tag); };
    
    try {
        parseOriginalStructure();
        
        // Парсим необходимые таблицы для инициализации
        if (!parseHeadTable()) {
            throw std::runtime_error("Failed to parse head table");
        }
        if (!parseMaxpTable()) {
            throw std::runtime_error("Failed to parse maxp table");
        }
        if (!parseHheaTable()) {
            throw std::runtime_error("Failed to parse hhea table");
        }
        if (!parseLocaTable()) {
            throw std::runtime_error("Failed to parse loca table");
        }
        
        parseGlyfTable();
        
    } catch (const std::exception& e) {
        std::cerr << "TTFRebuilder initialization error: " << e.what() << std::endl;
        throw;
    }
}

void TTFRebuilder::parseOriginalStructure() {
    if (originalData.size() < sizeof(utils::TTFHeader)) {
        throw std::runtime_error("Font data too small for TTF header");
    }
    
    auto tableRecords = utils::parseTTFTables(originalData);
    
    if (tableRecords.empty()) {
        throw std::runtime_error("No tables found in font data");
    }
    
    for (const auto& record : tableRecords) {
        TableInfo info;
        info.tag = std::string(record.tag, 4);
        info.originalOffset = record.offset;
        info.originalLength = record.length;
        info.newOffset = 0;
        info.newLength = record.length;
        info.modified = false;
        
        // Копируем оригинальные данные таблицы
        if (record.offset + record.length <= originalData.size()) {
            info.data.assign(originalData.begin() + record.offset, 
                           originalData.begin() + record.offset + record.length);
        } else {
            throw std::runtime_error("Table " + info.tag + " extends beyond font data");
        }
        
        tables[info.tag] = info;
        tableOrder.push_back(info.tag);
    }
    
    std::cout << "TTFRebuilder: Parsed " << tableOrder.size() << " tables" << std::endl;
}

void TTFRebuilder::markTableModified(const std::string& tag) {
    auto it = tables.find(tag);
    if (it != tables.end()) {
        it->second.modified = true;
        std::cout << "TTFRebuilder: Marked table '" << tag << "' as modified" << std::endl;
    } else {
        throw std::runtime_error("Table '" + tag + "' not found");
    }
}

void TTFRebuilder::setTableData(const std::string& tag, const std::vector<uint8_t>& data) {
    auto it = tables.find(tag);
    if (it != tables.end()) {
        it->second.data = data;
        it->second.newLength = data.size();
        it->second.modified = true;
        std::cout << "TTFRebuilder: Set table '" << tag << "' data, size: " 
                  << data.size() << " bytes" << std::endl;
    } else {
        throw std::runtime_error("Table '" + tag + "' not found");
    }
}

const std::vector<uint8_t>* TTFRebuilder::getTableData(const std::string& tag) const {
    auto it = tables.find(tag);
    if (it != tables.end()) {
        return &it->second.data;
    }
    return nullptr;
}

bool TTFRebuilder::hasTable(const std::string& tag) const {
    return tables.find(tag) != tables.end();
}

void TTFRebuilder::setTableRebuildHandler(const std::string& tag, 
                                         std::function<void(const std::string&)> handler) {
    rebuildHandlers[tag] = handler;
    std::cout << "TTFRebuilder: Set custom rebuild handler for table '" << tag << "'" << std::endl;
}

std::vector<uint8_t> TTFRebuilder::rebuild() {
    newData.clear();
    
    try {
        // 1. Пересобираем модифицированные таблицы
        for (auto& pair : tables) {
            if (pair.second.modified) {
                rebuildTable(pair.first);
            }
        }
        
        // 2. Синхронизируем связанные таблицы
        updateGlyfTable();
        updateLocaTable();
        updateHmtxTable();
        updateHheaTable();
        updateMaxpTable();
        
        // 3. Пересчитываем глобальные метрики шрифта
        recalculateFontMetrics();
        
        // 4. Рассчитываем новые смещения
        updateTableOffsets();
        
        // 5. Собираем новый заголовок и директорию таблиц
        rebuildTableDirectory();
        
        // 6. Копируем данные всех таблиц
        for (const auto& tag : tableOrder) {
            const auto& tableInfo = tables[tag];
            size_t requiredSize = tableInfo.newOffset + tableInfo.newLength;
            if (newData.size() < requiredSize) {
                newData.resize(requiredSize);
            }
            
            std::copy(tableInfo.data.begin(), tableInfo.data.end(), 
                     newData.begin() + tableInfo.newOffset);
            
            // Выравнивание до 4 байт
            size_t alignedSize = (tableInfo.newLength + 3) & ~3;
            if (alignedSize > tableInfo.newLength) {
                std::fill(newData.begin() + tableInfo.newOffset + tableInfo.newLength,
                         newData.begin() + tableInfo.newOffset + alignedSize, 0);
            }
        }
        
        // 7. Обновляем checksum adjustment в head таблице
        updateHeadTableChecksumAdjustment();
        
        std::cout << "TTFRebuilder: Successfully rebuilt font, original size: " 
                  << originalData.size() << " bytes, new size: " << newData.size() 
                  << " bytes" << std::endl;
        
        return newData;
        
    } catch (const std::exception& e) {
        std::cerr << "TTFRebuilder::rebuild error: " << e.what() << std::endl;
        throw;
    }
}

void TTFRebuilder::rebuildTable(const std::string& tag) {
    auto it = tables.find(tag);
    if (it == tables.end()) {
        throw std::runtime_error("Table '" + tag + "' not found during rebuild");
    }
    
    TableInfo& tableInfo = it->second;
    
    // Проверяем есть ли специальный обработчик для этой таблицы
    auto handlerIt = rebuildHandlers.find(tag);
    if (handlerIt != rebuildHandlers.end()) {
        handlerIt->second(tag);
    } else {
        // Базовая обработка для таблиц без специальной логики
        if (!tableInfo.modified) {
            // Если таблица не модифицирована, используем оригинальные данные
            if (tableInfo.originalOffset + tableInfo.originalLength <= originalData.size()) {
                tableInfo.data.assign(originalData.begin() + tableInfo.originalOffset,
                                    originalData.begin() + tableInfo.originalOffset + tableInfo.originalLength);
            }
        }
        tableInfo.newLength = tableInfo.data.size();
    }
    
    std::cout << "TTFRebuilder: Rebuilt table '" << tag << "', size: " 
              << tableInfo.newLength << " bytes" << std::endl;
}

void TTFRebuilder::rebuildGlyfTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    
    validateGlyphData();
    calculateGlyphMetrics();
    
    std::cout << "TTFRebuilder: Processed glyf table with " 
              << glyphOffsets.size() << " glyphs, size: " 
              << tableInfo.data.size() << " bytes" << std::endl;
}

void TTFRebuilder::rebuildLocaTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    
    auto headIt = tables.find("head");
    if (headIt == tables.end()) {
        throw std::runtime_error("head table not found for loca table rebuild");
    }
    
    validateTableData("head", HEAD_TABLE_SIZE);
    const auto& headData = headIt->second.data;
    
    int16_t indexToLocFormat = getInt16(headData, 50);
    locaShortFormat = (indexToLocFormat == 0);
    
    calculateGlyphOffsets();
    
    std::vector<uint8_t> newLocaData;
    
    if (locaShortFormat) {
        newLocaData.resize((numGlyphs + 1) * 2);
        for (size_t i = 0; i <= numGlyphs; ++i) {
            uint32_t offset = (i < glyphOffsets.size()) ? glyphOffsets[i].offset / 2 : 0;
            if (offset > 0xFFFF) {
                locaShortFormat = false;
                setInt16(headIt->second.data, 50, 1);
                std::cout << "TTFRebuilder: Switching loca to long format due to offset overflow" << std::endl;
                rebuildLocaTable(tag);
                return;
            }
            setUInt16(newLocaData, i * 2, static_cast<uint16_t>(offset));
        }
    } else {
        newLocaData.resize((numGlyphs + 1) * 4);
        for (size_t i = 0; i <= numGlyphs; ++i) {
            uint32_t offset = (i < glyphOffsets.size()) ? glyphOffsets[i].offset : 0;
            setUInt32(newLocaData, i * 4, offset);
        }
    }
    
    tableInfo.data = newLocaData;
    tableInfo.newLength = newLocaData.size();
    
    std::cout << "TTFRebuilder: Rebuilt loca table with " << numGlyphs 
              << " glyphs, format: " << (locaShortFormat ? "short" : "long") 
              << ", size: " << tableInfo.newLength << " bytes" << std::endl;
}

void TTFRebuilder::rebuildHmtxTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    
    calculateHMetrics();
    
    std::vector<uint8_t> newHmtxData;
    newHmtxData.resize(numHMetrics * 4 + (numGlyphs - numHMetrics) * 2);
    
    for (uint16_t i = 0; i < numHMetrics; ++i) {
        if (i < glyphOffsets.size()) {
            setUInt16(newHmtxData, i * 4, glyphOffsets[i].advanceWidth);
            setInt16(newHmtxData, i * 4 + 2, glyphOffsets[i].leftSideBearing);
        } else {
            setUInt16(newHmtxData, i * 4, 500);
            setInt16(newHmtxData, i * 4 + 2, 0);
        }
    }
    
    // УДАЛЕНО: uint16_t lastAdvanceWidth = (numHMetrics > 0 && numHMetrics - 1 < glyphOffsets.size()) ...
    
    for (uint16_t i = numHMetrics; i < numGlyphs; ++i) {
        int16_t lsb = (i < glyphOffsets.size()) ? glyphOffsets[i].leftSideBearing : 0;
        setInt16(newHmtxData, numHMetrics * 4 + (i - numHMetrics) * 2, lsb);
    }
    
    tableInfo.data = newHmtxData;
    tableInfo.newLength = newHmtxData.size();
    
    std::cout << "TTFRebuilder: Rebuilt hmtx table with " << numGlyphs 
              << " glyphs, " << numHMetrics << " h-metrics, size: " 
              << tableInfo.newLength << " bytes" << std::endl;
}

void TTFRebuilder::rebuildHheaTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    validateTableData(tag, HHEA_TABLE_SIZE);
    
    setUInt16(tableInfo.data, 34, numHMetrics);
    updateHheaMetrics();
    
    std::cout << "TTFRebuilder: Updated hhea table, numberOfHMetrics: " 
              << numHMetrics << std::endl;
}

void TTFRebuilder::rebuildMaxpTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    validateTableData(tag, 6);
    
    setUInt16(tableInfo.data, 4, numGlyphs);
    updateMaxpTableValues();
    
    std::cout << "TTFRebuilder: Updated maxp table, numGlyphs: " 
              << numGlyphs << std::endl;
}

void TTFRebuilder::rebuildNameTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    
    if (tableInfo.modified) {
        updateNameTableChecksum();
    }
}

void TTFRebuilder::rebuildOS2Table(const std::string& tag) {
    auto& tableInfo = tables[tag];
    
    if (tableInfo.modified || tables["glyf"].modified || tables["hmtx"].modified) {
        validateTableData(tag, OS2_TABLE_SIZE);
        updateOS2Metrics();
        std::cout << "TTFRebuilder: Updated OS/2 table metrics" << std::endl;
    }
}

void TTFRebuilder::rebuildHeadTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    validateTableData(tag, HEAD_TABLE_SIZE);
    
    uint64_t currentTimestamp = (uint64_t)time(nullptr) + 2082844800;
    setUInt32(tableInfo.data, 4, (currentTimestamp >> 32) & 0xFFFFFFFF);
    setUInt32(tableInfo.data, 8, currentTimestamp & 0xFFFFFFFF);
    
    setUInt32(tableInfo.data, 12, HEAD_MAGIC);
    setInt16(tableInfo.data, 50, locaShortFormat ? 0 : 1);
    
    std::cout << "TTFRebuilder: Updated head table" << std::endl;
}

void TTFRebuilder::rebuildPostTable(const std::string& tag) {
    auto& tableInfo = tables[tag];
    
    if (tableInfo.modified) {
        updatePostTableFormat2();
    }
}

// УДАЛЕНО: Реализации без параметров - используем версии с параметрами по умолчанию
// void TTFRebuilder::updateGlyfTable() {
// void TTFRebuilder::updateLocaTable() {
// void TTFRebuilder::updateHmtxTable() {
// void TTFRebuilder::updateHheaTable() {
// void TTFRebuilder::updateMaxpTable() {

void TTFRebuilder::updateGlyfTable(const std::string& glyfTag) {
    if (tables.find(glyfTag) == tables.end() || !tables[glyfTag].modified) return;
    
    markTableModified("loca");
    markTableModified("maxp");
    markTableModified("hmtx");
    markTableModified("OS/2");
    
    calculateGlyphOffsets();
    calculateGlyphMetrics();
}

void TTFRebuilder::updateLocaTable(const std::string& locaTag) {
    if (tables.find(locaTag) == tables.end()) return;
    
    if (tables[locaTag].modified) {
        rebuildLocaTable(locaTag);
    }
}

void TTFRebuilder::updateHmtxTable(const std::string& hmtxTag) {
    if (tables.find(hmtxTag) == tables.end()) return;
    
    if (tables[hmtxTag].modified) {
        rebuildHmtxTable(hmtxTag);
        markTableModified("hhea");
    }
}

void TTFRebuilder::updateHheaTable(const std::string& hheaTag) {
    if (tables.find(hheaTag) == tables.end()) return;
    
    if (tables[hheaTag].modified) {
        rebuildHheaTable(hheaTag);
    }
}

void TTFRebuilder::updateMaxpTable(const std::string& maxpTag) {
    if (tables.find(maxpTag) == tables.end()) return;
    
    if (tables[maxpTag].modified) {
        rebuildMaxpTable(maxpTag);
    }
}

uint16_t TTFRebuilder::getUInt16(const std::vector<uint8_t>& data, size_t offset) const {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Read UInt16 beyond data boundary");
    }
    return (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
}

uint32_t TTFRebuilder::getUInt32(const std::vector<uint8_t>& data, size_t offset) const {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Read UInt32 beyond data boundary");
    }
    return (static_cast<uint32_t>(data[offset]) << 24) | 
           (static_cast<uint32_t>(data[offset + 1]) << 16) | 
           (static_cast<uint32_t>(data[offset + 2]) << 8) | 
           data[offset + 3];
}

int16_t TTFRebuilder::getInt16(const std::vector<uint8_t>& data, size_t offset) const {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Read Int16 beyond data boundary");
    }
    return (static_cast<int16_t>(data[offset]) << 8) | data[offset + 1];
}

void TTFRebuilder::setUInt16(std::vector<uint8_t>& data, size_t offset, uint16_t value) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Write UInt16 beyond data boundary");
    }
    data[offset] = (value >> 8) & 0xFF;
    data[offset + 1] = value & 0xFF;
}

void TTFRebuilder::setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Write UInt32 beyond data boundary");
    }
    data[offset] = (value >> 24) & 0xFF;
    data[offset + 1] = (value >> 16) & 0xFF;
    data[offset + 2] = (value >> 8) & 0xFF;
    data[offset + 3] = value & 0xFF;
}

void TTFRebuilder::setInt16(std::vector<uint8_t>& data, size_t offset, int16_t value) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Write Int16 beyond data boundary");
    }
    data[offset] = (value >> 8) & 0xFF;
    data[offset + 1] = value & 0xFF;
}

void TTFRebuilder::calculateGlyphOffsets() {
    glyphOffsets.clear();
    
    auto glyfIt = tables.find("glyf");
    if (glyfIt == tables.end() || glyfIt->second.data.empty()) {
        for (uint16_t i = 0; i <= numGlyphs; ++i) {
            glyphOffsets.push_back({0, 0, 0, 0, true});
        }
        return;
    }
    
    const auto& glyfData = glyfIt->second.data;
    uint32_t currentOffset = 0;
    
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        GlyphInfo glyph;
        glyph.offset = currentOffset;
        glyph.isEmpty = false;
        
        if (currentOffset >= glyfData.size()) {
            glyph.length = 0;
            glyph.isEmpty = true;
        } else {
            int16_t numberOfContours = getInt16(glyfData, currentOffset);
            
            if (numberOfContours >= 0) {
                glyph.length = parseSimpleGlyphLength(glyfData, currentOffset);
            } else {
                glyph.length = parseCompositeGlyphLength(glyfData, currentOffset);
            }
            
            if (currentOffset + glyph.length > glyfData.size()) {
                glyph.length = glyfData.size() - currentOffset;
                std::cerr << "TTFRebuilder: Glyph " << i << " length exceeds table bounds" << std::endl;
            }
        }
        
        glyphOffsets.push_back(glyph);
        currentOffset += glyph.length;
        
        if (currentOffset % 2 != 0 && currentOffset < glyfData.size()) {
            currentOffset++;
        }
    }
    
    glyphOffsets.push_back({currentOffset, 0, 0, 0, true});
    
    std::cout << "TTFRebuilder: Calculated " << glyphOffsets.size() 
              << " glyph offsets, total glyf size: " << currentOffset << " bytes" << std::endl;
}

uint32_t TTFRebuilder::parseSimpleGlyphLength(const std::vector<uint8_t>& data, uint32_t offset) const {
    if (offset + 12 > data.size()) return 0;
    
    int16_t numberOfContours = getInt16(data, offset);
    if (numberOfContours < 0) return 0;
    
    uint32_t length = 10 + 2 * numberOfContours;
    
    uint16_t instructionLength = getUInt16(data, offset + length);
    length += 2 + instructionLength;
    
    uint32_t currentPos = length;
    for (int16_t contour = 0; contour < numberOfContours; ++contour) {
        uint16_t pointsInContour = getUInt16(data, offset + 10 + contour * 2);
        
        for (uint16_t point = 0; point < pointsInContour; ++point) {
            if (currentPos >= data.size()) break;
            
            uint8_t flag = data[offset + currentPos];
            currentPos++;
            
            if (flag & 0x02) {
                currentPos += 1;
            } else if (!(flag & 0x10)) {
                currentPos += 2;
            }
            
            if (flag & 0x04) {
                currentPos += 1;
            } else if (!(flag & 0x20)) {
                currentPos += 2;
            }
        }
    }
    
    return currentPos;
}

uint32_t TTFRebuilder::parseCompositeGlyphLength(const std::vector<uint8_t>& data, uint32_t offset) const {
    if (offset + 10 > data.size()) return 0;
    
    uint32_t currentPos = 10;
    
    bool moreComponents = true;
    while (moreComponents && currentPos < data.size()) {
        uint16_t flags = getUInt16(data, offset + currentPos);
        currentPos += 2;
        
        // УДАЛЕНО: uint16_t glyphIndex = getUInt16(data, offset + currentPos);
        getUInt16(data, offset + currentPos); // Просто читаем, но не используем
        currentPos += 2;
        
        if (flags & 0x0001) {
            currentPos += 4;
        } else {
            currentPos += 2;
        }
        
        if (flags & 0x0008) {
            currentPos += 2;
        } else if (flags & 0x0040) {
            currentPos += 4;
        } else if (flags & 0x0080) {
            currentPos += 8;
        }
        
        moreComponents = (flags & 0x0020);
    }
    
    return currentPos;
}

void TTFRebuilder::calculateGlyphMetrics() {
    auto glyfIt = tables.find("glyf");
    if (glyfIt == tables.end()) return;
    
    const auto& glyfData = glyfIt->second.data;
    
    for (size_t i = 0; i < glyphOffsets.size() && i < numGlyphs; ++i) {
        auto& glyph = glyphOffsets[i];
        
        if (glyph.isEmpty || glyph.offset >= glyfData.size()) {
            auto hmtxIt = tables.find("hmtx");
            if (hmtxIt != tables.end() && i < numHMetrics) {
                const auto& hmtxData = hmtxIt->second.data;
                if (hmtxData.size() >= (i * 4 + 4)) {
                    glyph.advanceWidth = getUInt16(hmtxData, i * 4);
                    glyph.leftSideBearing = getInt16(hmtxData, i * 4 + 2);
                } else {
                    glyph.advanceWidth = 500;
                    glyph.leftSideBearing = 0;
                }
            } else {
                glyph.advanceWidth = 500;
                glyph.leftSideBearing = 0;
            }
            continue;
        }
        
        int16_t numberOfContours = getInt16(glyfData, glyph.offset);
        
        if (numberOfContours == 0) {
            glyph.advanceWidth = 500;
            glyph.leftSideBearing = 0;
        } else {
            int16_t xMin = getInt16(glyfData, glyph.offset + 2);
            // УДАЛЕНО: int16_t yMin = getInt16(glyfData, glyph.offset + 4);
            getInt16(glyfData, glyph.offset + 4); // Просто читаем yMin
            int16_t xMax = getInt16(glyfData, glyph.offset + 6);
            // УДАЛЕНО: int16_t yMax = getInt16(glyfData, glyph.offset + 8);
            getInt16(glyfData, glyph.offset + 8); // Просто читаем yMax
            
            int16_t glyphWidth = xMax - xMin;
            glyph.advanceWidth = std::max(static_cast<uint16_t>(glyphWidth + 50), static_cast<uint16_t>(500));
            glyph.leftSideBearing = xMin;
        }
    }
    
    std::cout << "TTFRebuilder: Calculated metrics for " << glyphOffsets.size() << " glyphs" << std::endl;
}

void TTFRebuilder::calculateHMetrics() {
    std::map<uint16_t, uint32_t> advanceWidthCounts;
    std::set<uint16_t> uniqueAdvanceWidths;
    
    for (const auto& glyph : glyphOffsets) {
        if (!glyph.isEmpty) {
            advanceWidthCounts[glyph.advanceWidth]++;
            uniqueAdvanceWidths.insert(glyph.advanceWidth);
        }
    }
    
    uint16_t optimalHMetrics = numGlyphs;
    uint32_t threshold = numGlyphs / 10;
    
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (i >= glyphOffsets.size()) break;
        
        uint16_t currentWidth = glyphOffsets[i].advanceWidth;
        uint32_t count = advanceWidthCounts[currentWidth];
        
        if (count < threshold && i > numGlyphs / 3) {
            optimalHMetrics = i + 1;
            break;
        }
    }
    
    numHMetrics = std::min(optimalHMetrics, numGlyphs);
    // ИСПРАВЛЕНО: убрана лишняя проверка, так как numHMetrics всегда <= numGlyphs <= 0xFFFF
    // if (numHMetrics > 0xFFFF) {
    //     numHMetrics = 0xFFFF;
    // }
    
    std::cout << "TTFRebuilder: Optimized h-metrics: " << numHMetrics 
              << " (unique advance widths: " << uniqueAdvanceWidths.size() << ")" << std::endl;
}

void TTFRebuilder::setNumGlyphs(uint16_t newNumGlyphs) {
    if (newNumGlyphs != numGlyphs) {
        numGlyphs = newNumGlyphs;
        markTableModified("maxp");
        markTableModified("loca");
        markTableModified("hmtx");
        std::cout << "TTFRebuilder: Set numGlyphs to " << numGlyphs << std::endl;
    }
}

void TTFRebuilder::setNumberOfHMetrics(uint16_t newNumHMetrics) {
    if (newNumHMetrics != numHMetrics) {
        numHMetrics = newNumHMetrics;
        markTableModified("hhea");
        markTableModified("hmtx");
        std::cout << "TTFRebuilder: Set numberOfHMetrics to " << numHMetrics << std::endl;
    }
}

bool TTFRebuilder::parseLocaTable() {
    auto locaIt = tables.find("loca");
    if (locaIt == tables.end()) return false;
    return true;
}

bool TTFRebuilder::parseMaxpTable() {
    auto maxpIt = tables.find("maxp");
    if (maxpIt == tables.end()) return false;
    
    const auto& maxpData = maxpIt->second.data;
    if (maxpData.size() < 6) return false;
    
    numGlyphs = getUInt16(maxpData, 4);
    return true;
}

bool TTFRebuilder::parseHheaTable() {
    auto hheaIt = tables.find("hhea");
    if (hheaIt == tables.end()) return false;
    
    const auto& hheaData = hheaIt->second.data;
    if (hheaData.size() < 36) return false;
    
    numHMetrics = getUInt16(hheaData, 34);
    return true;
}

bool TTFRebuilder::parseHeadTable() {
    auto headIt = tables.find("head");
    if (headIt == tables.end()) return false;
    
    const auto& headData = headIt->second.data;
    if (headData.size() < HEAD_TABLE_SIZE) return false;
    
    uint32_t magic = getUInt32(headData, 12);
    if (magic != HEAD_MAGIC) {
        std::cerr << "TTFRebuilder: Warning - head table magic number incorrect" << std::endl;
    }
    
    int16_t indexToLocFormat = getInt16(headData, 50);
    locaShortFormat = (indexToLocFormat == 0);
    
    return true;
}

void TTFRebuilder::parseGlyfTable() {
    auto glyfIt = tables.find("glyf");
    if (glyfIt == tables.end()) return;
    calculateGlyphOffsets();
}

void TTFRebuilder::recalculateFontMetrics() {
    updateHheaMetrics();
    updateOS2Metrics();
}

void TTFRebuilder::updateHheaMetrics() {
    auto hheaIt = tables.find("hhea");
    if (hheaIt == tables.end()) return;
    
    int16_t ascender = -10000;
    int16_t descender = 10000;
    int16_t lineGap = 200;
    uint16_t maxAdvance = 0;
    int16_t minLeftSideBearing = 0;
    int16_t maxRightSideBearing = 0;
    
    for (const auto& glyph : glyphOffsets) {
        if (glyph.isEmpty) continue;
        
        auto os2It = tables.find("OS/2");
        if (os2It != tables.end() && os2It->second.data.size() >= 10) {
            ascender = std::max(ascender, getInt16(os2It->second.data, 68));
            descender = std::min(descender, getInt16(os2It->second.data, 70));
        }
        
        maxAdvance = std::max(maxAdvance, glyph.advanceWidth);
        
        int16_t rightSideBearing = glyph.advanceWidth - glyph.leftSideBearing;
        minLeftSideBearing = std::min(minLeftSideBearing, glyph.leftSideBearing);
        maxRightSideBearing = std::max(maxRightSideBearing, rightSideBearing);
    }
    
    setInt16(hheaIt->second.data, 4, ascender);
    setInt16(hheaIt->second.data, 6, descender);
    setInt16(hheaIt->second.data, 8, lineGap);
    setUInt16(hheaIt->second.data, 10, maxAdvance);
    setInt16(hheaIt->second.data, 12, minLeftSideBearing);
    setInt16(hheaIt->second.data, 14, maxRightSideBearing);
}

void TTFRebuilder::updateOS2Metrics() {
    auto os2It = tables.find("OS/2");
    if (os2It == tables.end()) return;
    
    uint32_t totalWidth = 0;
    uint32_t count = 0;
    for (const auto& glyph : glyphOffsets) {
        if (!glyph.isEmpty) {
            totalWidth += glyph.advanceWidth;
            count++;
        }
    }
    uint16_t avgWidth = count > 0 ? totalWidth / count : 500;
    setInt16(os2It->second.data, 2, static_cast<int16_t>(avgWidth));
    
    auto hheaIt = tables.find("hhea");
    if (hheaIt != tables.end() && hheaIt->second.data.size() >= 16) {
        int16_t ascender = getInt16(hheaIt->second.data, 4);
        int16_t descender = getInt16(hheaIt->second.data, 6);
        
        setInt16(os2It->second.data, 68, ascender);
        setInt16(os2It->second.data, 70, descender);
        setInt16(os2It->second.data, 72, getInt16(hheaIt->second.data, 8));
    }
}

void TTFRebuilder::updateMaxpTableValues() {
    auto maxpIt = tables.find("maxp");
    if (maxpIt == tables.end()) return;
    
    uint16_t maxPoints = 0;
    uint16_t maxContours = 0;
    uint16_t maxCompositePoints = 0;
    uint16_t maxCompositeContours = 0;
    uint16_t maxSizeOfInstructions = 0;
    uint16_t maxComponentElements = 0;
    uint16_t maxComponentDepth = 0;
    
    auto glyfIt = tables.find("glyf");
    if (glyfIt != tables.end()) {
        const auto& glyfData = glyfIt->second.data;
        
        for (const auto& glyph : glyphOffsets) {
            if (glyph.isEmpty || glyph.offset >= glyfData.size()) continue;
            
            int16_t numberOfContours = getInt16(glyfData, glyph.offset);
            
            if (numberOfContours >= 0) {
                maxContours = std::max(maxContours, static_cast<uint16_t>(numberOfContours));
                
                uint16_t points = calculateSimpleGlyphPoints(glyfData, glyph.offset);
                maxPoints = std::max(maxPoints, points);
                
                uint16_t instructionLength = getUInt16(glyfData, glyph.offset + 10 + 2 * numberOfContours);
                maxSizeOfInstructions = std::max(maxSizeOfInstructions, instructionLength);
                
            } else {
                auto compositeStats = calculateCompositeGlyphStats(glyfData, glyph.offset);
                maxCompositePoints = std::max(maxCompositePoints, compositeStats.maxPoints);
                maxCompositeContours = std::max(maxCompositeContours, compositeStats.maxContours);
                maxComponentElements = std::max(maxComponentElements, compositeStats.maxComponents);
                maxComponentDepth = std::max(maxComponentDepth, compositeStats.maxDepth);
            }
        }
    }
    
    if (maxpIt->second.data.size() >= 32) {
        setUInt16(maxpIt->second.data, 6, maxPoints);
        setUInt16(maxpIt->second.data, 8, maxContours);
        setUInt16(maxpIt->second.data, 10, maxCompositePoints);
        setUInt16(maxpIt->second.data, 12, maxCompositeContours);
        setUInt16(maxpIt->second.data, 14, maxSizeOfInstructions);
        setUInt16(maxpIt->second.data, 16, maxComponentElements);
        setUInt16(maxpIt->second.data, 18, maxComponentDepth);
    }
}

uint16_t TTFRebuilder::calculateSimpleGlyphPoints(const std::vector<uint8_t>& data, uint32_t offset) const {
    if (offset + 12 > data.size()) return 0;
    
    int16_t numberOfContours = getInt16(data, offset);
    if (numberOfContours <= 0) return 0;
    
    uint16_t totalPoints = 0;
    for (int16_t i = 0; i < numberOfContours; ++i) {
        if (offset + 10 + (i + 1) * 2 > data.size()) break;
        uint16_t pointsInContour = getUInt16(data, offset + 10 + i * 2);
        totalPoints += pointsInContour;
    }
    
    return totalPoints;
}

TTFRebuilder::CompositeGlyphStats TTFRebuilder::calculateCompositeGlyphStats(const std::vector<uint8_t>& data, uint32_t offset) const {
    CompositeGlyphStats stats = {0, 0, 0, 0};
    
    if (offset + 10 > data.size()) return stats;
    
    uint32_t currentPos = 10;
    uint16_t componentCount = 0;
    
    bool moreComponents = true;
    while (moreComponents && currentPos < data.size()) {
        componentCount++;
        stats.maxComponents = componentCount;
        
        uint16_t flags = getUInt16(data, offset + currentPos);
        currentPos += 2;
        
        uint16_t glyphIndex = getUInt16(data, offset + currentPos);
        currentPos += 2;
        
        if (glyphIndex < glyphOffsets.size() && !glyphOffsets[glyphIndex].isEmpty) {
            auto componentStats = analyzeGlyphForComposite(data, glyphOffsets[glyphIndex].offset);
            stats.maxPoints = std::max(stats.maxPoints, componentStats.maxPoints);
            stats.maxContours = std::max(stats.maxContours, componentStats.maxContours);
            stats.maxDepth = std::max(stats.maxDepth, static_cast<uint16_t>(componentStats.maxDepth + 1));
        }
        
        if (flags & 0x0001) {
            currentPos += 4;
        } else {
            currentPos += 2;
        }
        
        if (flags & 0x0008) {
            currentPos += 2;
        } else if (flags & 0x0040) {
            currentPos += 4;
        } else if (flags & 0x0080) {
            currentPos += 8;
        }
        
        moreComponents = (flags & 0x0020);
    }
    
    return stats;
}

TTFRebuilder::CompositeGlyphStats TTFRebuilder::analyzeGlyphForComposite(const std::vector<uint8_t>& data, uint32_t offset) const {
    CompositeGlyphStats stats = {0, 0, 0, 0};
    
    if (offset >= data.size()) return stats;
    
    int16_t numberOfContours = getInt16(data, offset);
    
    if (numberOfContours >= 0) {
        stats.maxContours = std::max(stats.maxContours, static_cast<uint16_t>(numberOfContours));
        stats.maxPoints = calculateSimpleGlyphPoints(data, offset);
    } else {
        stats = calculateCompositeGlyphStats(data, offset);
        stats.maxDepth++;
    }
    
    return stats;
}

void TTFRebuilder::updateNameTableChecksum() {
    auto nameIt = tables.find("name");
    if (nameIt == tables.end()) return;
    
    auto& nameData = nameIt->second.data;
    if (nameData.size() < 6) {
        throw std::runtime_error("name table too small");
    }
    
    uint16_t format = getUInt16(nameData, 0);
    uint16_t count = getUInt16(nameData, 2);
    uint16_t stringOffset = getUInt16(nameData, 4);
    
    size_t expectedSize = 6 + count * 12;
    if (nameData.size() < expectedSize) {
        throw std::runtime_error("name table structure corrupted");
    }
    
    std::vector<uint8_t> newNameData;
    std::vector<uint8_t> stringStorage;
    std::vector<NameRecord> nameRecords;
    
    for (uint16_t i = 0; i < count; ++i) {
        size_t recordOffset = 6 + i * 12;
        NameRecord record;
        record.platformID = getUInt16(nameData, recordOffset);
        record.encodingID = getUInt16(nameData, recordOffset + 2);
        record.languageID = getUInt16(nameData, recordOffset + 4);
        record.nameID = getUInt16(nameData, recordOffset + 6);
        record.length = getUInt16(nameData, recordOffset + 8);
        record.offset = getUInt16(nameData, recordOffset + 10);
        
        // ИСПРАВЛЕНО: приведение типов для сравнения
        if (static_cast<size_t>(stringOffset + record.offset + record.length) > nameData.size()) {
            std::cerr << "TTFRebuilder: Fixing invalid string offset in name record " << i << std::endl;
            record.offset = static_cast<uint16_t>(stringStorage.size());
        }
        
        size_t stringStart = stringOffset + record.offset;
        if (stringStart + record.length <= nameData.size()) {
            std::vector<uint8_t> stringData(
                nameData.begin() + stringStart,
                nameData.begin() + stringStart + record.length
            );
            
            if (record.platformID == 1 && record.encodingID == 0) {
                fixMacRomanEncoding(stringData);
            } else if (record.platformID == 3 && record.encodingID == 1) {
                fixUnicodeEncoding(stringData);
            }
            
            record.offset = static_cast<uint16_t>(stringStorage.size());
            record.length = static_cast<uint16_t>(stringData.size());
            
            stringStorage.insert(stringStorage.end(), stringData.begin(), stringData.end());
        } else {
            record.offset = static_cast<uint16_t>(stringStorage.size());
            record.length = 0;
        }
        
        nameRecords.push_back(record);
    }
    
    if (stringStorage.size() % 2 != 0) {
        stringStorage.push_back(0);
    }
    
    newNameData.resize(6 + nameRecords.size() * 12 + stringStorage.size());
    
    setUInt16(newNameData, 0, format);
    setUInt16(newNameData, 2, static_cast<uint16_t>(nameRecords.size()));
    setUInt16(newNameData, 4, 6 + static_cast<uint16_t>(nameRecords.size() * 12));
    
    for (size_t i = 0; i < nameRecords.size(); ++i) {
        size_t recordOffset = 6 + i * 12;
        const auto& record = nameRecords[i];
        
        setUInt16(newNameData, recordOffset, record.platformID);
        setUInt16(newNameData, recordOffset + 2, record.encodingID);
        setUInt16(newNameData, recordOffset + 4, record.languageID);
        setUInt16(newNameData, recordOffset + 6, record.nameID);
        setUInt16(newNameData, recordOffset + 8, record.length);
        setUInt16(newNameData, recordOffset + 10, record.offset);
    }
    
    std::copy(stringStorage.begin(), stringStorage.end(), 
              newNameData.begin() + 6 + nameRecords.size() * 12);
    
    nameIt->second.data = newNameData;
    nameIt->second.newLength = newNameData.size();
    
    std::cout << "TTFRebuilder: Rebuilt name table with " << nameRecords.size() 
              << " records, string storage: " << stringStorage.size() << " bytes" << std::endl;
}

void TTFRebuilder::updatePostTableFormat2() {
    auto postIt = tables.find("post");
    if (postIt == tables.end()) return;
    
    auto& postData = postIt->second.data;
    if (postData.size() < 32) {
        throw std::runtime_error("post table too small");
    }
    
    uint32_t format = getUInt32(postData, 0);
    if (format != 0x00020000) {
        return;
    }
    
    uint32_t italicAngle = getUInt32(postData, 4);
    int16_t underlinePosition = getInt16(postData, 8);
    int16_t underlineThickness = getInt16(postData, 10);
    uint32_t isFixedPitch = getUInt32(postData, 12);
    uint32_t minMemType42 = getUInt32(postData, 16);
    uint32_t maxMemType42 = getUInt32(postData, 20);
    uint32_t minMemType1 = getUInt32(postData, 24);
    uint32_t maxMemType1 = getUInt32(postData, 28);
    
    uint16_t numberOfGlyphs = getUInt16(postData, 32);
    
    if (numberOfGlyphs != numGlyphs) {
        std::cout << "TTFRebuilder: Updating post table glyph count from " 
                  << numberOfGlyphs << " to " << numGlyphs << std::endl;
    }
    
    std::vector<uint8_t> newPostData;
    // ИСПРАВЛЕНО: приведение типов для сравнения
    size_t newSize = 34 + numGlyphs * 2;
    newPostData.resize(newSize);
    
    setUInt32(newPostData, 0, format);
    setUInt32(newPostData, 4, italicAngle);
    setInt16(newPostData, 8, underlinePosition);
    setInt16(newPostData, 10, underlineThickness);
    setUInt32(newPostData, 12, isFixedPitch);
    setUInt32(newPostData, 16, minMemType42);
    setUInt32(newPostData, 20, maxMemType42);
    setUInt32(newPostData, 24, minMemType1);
    setUInt32(newPostData, 28, maxMemType1);
    setUInt16(newPostData, 32, numGlyphs);
    
    // ИСПРАВЛЕНО: приведение типов для сравнения
    if (postData.size() >= static_cast<size_t>(34 + numberOfGlyphs * 2)) {
        for (uint16_t i = 0; i < numGlyphs; ++i) {
            if (i < numberOfGlyphs) {
                uint16_t nameIndex = getUInt16(postData, 34 + i * 2);
                setUInt16(newPostData, 34 + i * 2, nameIndex);
            } else {
                setUInt16(newPostData, 34 + i * 2, calculateStandardGlyphNameIndex(i));
            }
        }
    } else {
        for (uint16_t i = 0; i < numGlyphs; ++i) {
            setUInt16(newPostData, 34 + i * 2, calculateStandardGlyphNameIndex(i));
        }
    }
    
    postIt->second.data = newPostData;
    postIt->second.newLength = newPostData.size();
    
    std::cout << "TTFRebuilder: Rebuilt post table format 2.0 with " 
              << numGlyphs << " glyph name indexes" << std::endl;
}

void TTFRebuilder::fixMacRomanEncoding(std::vector<uint8_t>& stringData) {
    for (auto& byte : stringData) {
        if (byte >= 0x80 && byte <= 0x9F) {
            byte = '?';
        }
    }
    
    if (stringData.empty() || stringData.back() != 0) {
        stringData.push_back(0);
    }
}

void TTFRebuilder::fixUnicodeEncoding(std::vector<uint8_t>& stringData) {
    if (stringData.size() % 2 != 0) {
        stringData.push_back(0);
    }
    
    if (stringData.size() >= 2) {
        uint16_t bom = (static_cast<uint16_t>(stringData[0]) << 8) | stringData[1];
        if (bom != 0xFEFF && bom != 0xFFFE) {
            std::vector<uint8_t> newStringData = {0xFE, 0xFF};
            newStringData.insert(newStringData.end(), stringData.begin(), stringData.end());
            stringData = newStringData;
        }
    }
    
    for (size_t i = 0; i + 1 < stringData.size(); i += 2) {
        uint16_t codePoint = (static_cast<uint16_t>(stringData[i]) << 8) | stringData[i + 1];
        
        if ((codePoint >= 0xD800 && codePoint <= 0xDFFF) ||
            (codePoint >= 0xFDD0 && codePoint <= 0xFDEF) ||
            codePoint == 0xFFFE || codePoint == 0xFFFF) {
            stringData[i] = 0xFF;
            stringData[i + 1] = 0xFD;
        }
    }
}

uint16_t TTFRebuilder::calculateStandardGlyphNameIndex(uint16_t glyphIndex) {
    if (glyphIndex < 258) {
        return glyphIndex;
    }
    
    uint16_t unicodeValue = getUnicodeFromCmap(glyphIndex);
    if (unicodeValue != 0xFFFF) {
        return generateUnicodeGlyphNameIndex(unicodeValue);
    }
    
    if (isCompositeGlyph(glyphIndex)) {
        return generateCompositeGlyphName(glyphIndex);
    }
    
    return generateDefaultGlyphName(glyphIndex);
}

uint16_t TTFRebuilder::getUnicodeFromCmap(uint16_t glyphIndex) {
    auto cmapIt = tables.find("cmap");
    if (cmapIt == tables.end()) return 0xFFFF;
    
    const auto& cmapData = cmapIt->second.data;
    if (cmapData.size() < 4) return 0xFFFF;
    
    // УДАЛЕНО: uint16_t version = getUInt16(cmapData, 0);
    getUInt16(cmapData, 0); // Просто читаем version
    uint16_t numTables = getUInt16(cmapData, 2);
    
    for (uint16_t i = 0; i < numTables; ++i) {
        size_t tableOffset = 4 + i * 8;
        if (tableOffset + 8 > cmapData.size()) break;
        
        uint16_t platformID = getUInt16(cmapData, tableOffset);
        uint16_t encodingID = getUInt16(cmapData, tableOffset + 2);
        uint32_t subtableOffset = getUInt32(cmapData, tableOffset + 4);
        
        if ((platformID == 0) ||
            (platformID == 3 && encodingID == 1) ||
            (platformID == 3 && encodingID == 10)) {
            uint32_t offset = subtableOffset;
            if (offset >= cmapData.size()) continue;
            
            uint16_t format = getUInt16(cmapData, offset);
            
            if (format == 4) {
                return findGlyphInFormat4Subtable(cmapData, offset, glyphIndex);
            } else if (format == 12) {
                return findGlyphInFormat12Subtable(cmapData, offset, glyphIndex);
            }
        }
    }
    
    return 0xFFFF;
}

uint16_t TTFRebuilder::findGlyphInFormat4Subtable(const std::vector<uint8_t>& cmapData, uint32_t offset, uint16_t glyphIndex) {
    if (offset + 14 > cmapData.size()) return 0xFFFF;
    
    // УДАЛЕНО: uint16_t length = getUInt16(cmapData, offset + 2);
    getUInt16(cmapData, offset + 2); // Просто читаем length
    // УДАЛЕНО: uint16_t language = getUInt16(cmapData, offset + 4);
    getUInt16(cmapData, offset + 4); // Просто читаем language
    uint16_t segCountX2 = getUInt16(cmapData, offset + 6);
    uint16_t segCount = segCountX2 / 2;
    
    uint32_t endCountOffset = offset + 14;
    uint32_t startCountOffset = endCountOffset + segCountX2 + 2;
    uint32_t idDeltaOffset = startCountOffset + segCountX2;
    uint32_t idRangeOffsetOffset = idDeltaOffset + segCountX2;
    // УДАЛЕНО: uint32_t glyphIdArrayOffset = idRangeOffsetOffset + segCountX2;
    
    for (uint16_t i = 0; i < segCount; ++i) {
        uint16_t endCount = getUInt16(cmapData, endCountOffset + i * 2);
        uint16_t startCount = getUInt16(cmapData, startCountOffset + i * 2);
        int16_t idDelta = getInt16(cmapData, idDeltaOffset + i * 2);
        uint16_t idRangeOffset = getUInt16(cmapData, idRangeOffsetOffset + i * 2);
        
        if (glyphIndex >= startCount && glyphIndex <= endCount) {
            if (idRangeOffset == 0) {
                return (glyphIndex + idDelta) & 0xFFFF;
            } else {
                uint32_t glyphOffset = idRangeOffsetOffset + i * 2 + idRangeOffset + 
                                     (glyphIndex - startCount) * 2;
                if (glyphOffset + 2 <= cmapData.size()) {
                    uint16_t glyphId = getUInt16(cmapData, glyphOffset);
                    if (glyphId != 0) {
                        return (glyphId + idDelta) & 0xFFFF;
                    }
                }
            }
        }
    }
    
    return 0xFFFF;
}

uint16_t TTFRebuilder::findGlyphInFormat12Subtable(const std::vector<uint8_t>& cmapData, uint32_t offset, uint16_t glyphIndex) {
    if (offset + 16 > cmapData.size()) return 0xFFFF;
    
    uint16_t format = getUInt16(cmapData, offset);
    if (format != 12) return 0xFFFF;
    
    // УДАЛЕНО: uint32_t length = getUInt32(cmapData, offset + 4);
    getUInt32(cmapData, offset + 4); // Просто читаем length
    // УДАЛЕНО: uint32_t language = getUInt32(cmapData, offset + 8);
    getUInt32(cmapData, offset + 8); // Просто читаем language
    uint32_t numGroups = getUInt32(cmapData, offset + 12);
    
    uint32_t groupsOffset = offset + 16;
    
    for (uint32_t i = 0; i < numGroups; ++i) {
        uint32_t groupOffset = groupsOffset + i * 12;
        if (groupOffset + 12 > cmapData.size()) break;
        
        uint32_t startCharCode = getUInt32(cmapData, groupOffset);
        uint32_t endCharCode = getUInt32(cmapData, groupOffset + 4);
        uint32_t startGlyphID = getUInt32(cmapData, groupOffset + 8);
        
        if (glyphIndex >= startGlyphID && glyphIndex <= startGlyphID + (endCharCode - startCharCode)) {
            uint32_t charCode = startCharCode + (glyphIndex - startGlyphID);
            return static_cast<uint16_t>(charCode & 0xFFFF);
        }
    }
    
    return 0xFFFF;
}

bool TTFRebuilder::isCompositeGlyph(uint16_t glyphIndex) {
    auto glyfIt = tables.find("glyf");
    if (glyfIt == tables.end()) return false;
    
    if (glyphIndex >= glyphOffsets.size()) return false;
    
    const auto& glyph = glyphOffsets[glyphIndex];
    if (glyph.isEmpty || glyph.offset >= glyfIt->second.data.size()) return false;
    
    int16_t numberOfContours = getInt16(glyfIt->second.data, glyph.offset);
    return numberOfContours < 0;
}

uint16_t TTFRebuilder::generateUnicodeGlyphNameIndex(uint16_t unicodeValue) {
    // Стандартные имена для часто используемых Unicode символов
    static const std::map<uint16_t, uint16_t> unicodeToNameIndex = {
        {0x0020, 258}, {0x00A0, 258}, {0x2000, 258}, {0x2001, 259}, {0x2002, 259},
        {0x2003, 259}, {0x2004, 260}, {0x2005, 260}, {0x2006, 260}, {0x2007, 261},
        {0x2008, 261}, {0x2009, 262}, {0x200A, 262}, {0x2010, 263}, {0x2011, 263},
        {0x2012, 264}, {0x2013, 264}, {0x2014, 265}, {0x2015, 265}, {0x2017, 266},
        {0x2018, 267}, {0x2019, 267}, {0x201A, 268}, {0x201B, 268}, {0x201C, 269},
        {0x201D, 269}, {0x201E, 270}, {0x201F, 270}, {0x2020, 271}, {0x2021, 272},
        {0x2022, 273}, {0x2023, 273}, {0x2024, 274}, {0x2025, 275}, {0x2026, 276},
        {0x2030, 277}, {0x2032, 278}, {0x2033, 279}, {0x2039, 280}, {0x203A, 281},
        {0x203C, 282}, {0x2044, 283}, {0x2070, 284}, {0x2074, 285}, {0x2075, 286},
        {0x2076, 287}, {0x2077, 288}, {0x2078, 289}, {0x2079, 290}, {0x207A, 291},
        {0x207B, 292}, {0x207C, 293}, {0x207D, 294}, {0x207E, 295}, {0x2080, 296},
        {0x2081, 297}, {0x2082, 298}, {0x2083, 299}, {0x2084, 300}, {0x2085, 301},
        {0x2086, 302}, {0x2087, 303}, {0x2088, 304}, {0x2089, 305}, {0x20A0, 306},
        {0x20A1, 307}, {0x20A2, 308}, {0x20A3, 309}, {0x20A4, 310}, {0x20A5, 311},
        {0x20A6, 312}, {0x20A7, 313}, {0x20A8, 314}, {0x20A9, 315}, {0x20AA, 316},
        {0x20AB, 317}, {0x20AC, 318}, {0x20AD, 319}, {0x20AE, 320}, {0x20AF, 321},
        {0x20B0, 322}, {0x20B1, 323}, {0x2100, 324}, {0x2101, 325}, {0x2102, 326},
        {0x2103, 327}, {0x2105, 328}, {0x2106, 329}, {0x2107, 330}, {0x2108, 331},
        {0x2109, 332}, {0x210A, 333}, {0x210B, 334}, {0x210C, 335}, {0x210D, 336},
        {0x210E, 337}, {0x210F, 338}, {0x2110, 339}, {0x2111, 340}, {0x2112, 341},
        {0x2113, 342}, {0x2115, 343}, {0x2116, 344}, {0x2117, 345}, {0x2118, 346},
        {0x2119, 347}, {0x211A, 348}, {0x211B, 349}, {0x211C, 350}, {0x211D, 351},
        {0x2120, 352}, {0x2122, 353}, {0x2124, 354}, {0x2126, 355}, {0x2128, 356},
        {0x212A, 357}, {0x212B, 358}, {0x212C, 359}, {0x212D, 360}, {0x212F, 361},
        {0x2130, 362}, {0x2131, 363}, {0x2133, 364}, {0x2134, 365}, {0x2135, 366},
        {0x2136, 367}, {0x2137, 368}, {0x2138, 369}, {0x2153, 370}, {0x2154, 371},
        {0x2155, 372}, {0x2156, 373}, {0x2157, 374}, {0x2158, 375}, {0x2159, 376},
        {0x215A, 377}, {0x215B, 378}, {0x215C, 379}, {0x215D, 380}, {0x215E, 381},
        {0x2190, 382}, {0x2191, 383}, {0x2192, 384}, {0x2193, 385}, {0x2194, 386},
        {0x2195, 387}, {0x21A8, 388}, {0x2200, 389}, {0x2202, 390}, {0x2203, 391},
        {0x2205, 392}, {0x2206, 393}, {0x2207, 394}, {0x2208, 395}, {0x2209, 396},
        {0x220B, 397}, {0x220F, 398}, {0x2211, 399}, {0x2212, 400}, {0x2217, 401},
        {0x221A, 402}, {0x221D, 403}, {0x221E, 404}, {0x221F, 405}, {0x2220, 406},
        {0x2227, 407}, {0x2228, 408}, {0x2229, 409}, {0x222A, 410}, {0x222B, 411},
        {0x2234, 412}, {0x223C, 413}, {0x2245, 414}, {0x2248, 415}, {0x2260, 416},
        {0x2261, 417}, {0x2264, 418}, {0x2265, 419}, {0x2282, 420}, {0x2283, 421},
        {0x2284, 422}, {0x2285, 423}, {0x2286, 424}, {0x2287, 425}, {0x2295, 426},
        {0x2297, 427}, {0x22A5, 428}, {0x22C5, 429}, {0x2302, 430}, {0x2310, 431},
        {0x2320, 432}, {0x2321, 433}, {0x2329, 434}, {0x232A, 435}, {0x2500, 436},
        {0x2502, 437}, {0x250C, 438}, {0x2510, 439}, {0x2514, 440}, {0x2518, 441},
        {0x251C, 442}, {0x2524, 443}, {0x252C, 444}, {0x2534, 445}, {0x253C, 446},
        {0x2550, 447}, {0x2551, 448}, {0x2552, 449}, {0x2553, 450}, {0x2554, 451},
        {0x2555, 452}, {0x2556, 453}, {0x2557, 454}, {0x2558, 455}, {0x2559, 456},
        {0x255A, 457}, {0x255B, 458}, {0x255C, 459}, {0x255D, 460}, {0x255E, 461},
        {0x255F, 462}, {0x2560, 463}, {0x2561, 464}, {0x2562, 465}, {0x2563, 466},
        {0x2564, 467}, {0x2565, 468}, {0x2566, 469}, {0x2567, 470}, {0x2568, 471},
        {0x2569, 472}, {0x256A, 473}, {0x256B, 474}, {0x256C, 475}, {0x2580, 476},
        {0x2584, 477}, {0x2588, 478}, {0x258C, 479}, {0x2590, 480}, {0x2591, 481},
        {0x2592, 482}, {0x2593, 483}, {0x25A0, 484}, {0x25A1, 485}, {0x25AA, 486},
        {0x25AB, 487}, {0x25AC, 488}, {0x25B2, 489}, {0x25BA, 490}, {0x25BC, 491},
        {0x25C4, 492}, {0x25CA, 493}, {0x25CB, 494}, {0x25CF, 495}, {0x25D8, 496},
        {0x25D9, 497}, {0x25E6, 498}, {0x263A, 499}, {0x263B, 500}, {0x263C, 501},
        {0x2640, 502}, {0x2642, 503}, {0x2660, 504}, {0x2663, 505}, {0x2665, 506},
        {0x2666, 507}, {0x266A, 508}, {0x266B, 509}, {0xF6BE, 510}, {0xF6BF, 511},
        {0xF6C0, 512}, {0xF6C1, 513}, {0xF6C2, 514}, {0xF6C3, 515}, {0xF6C4, 516},
        {0xF6C5, 517}, {0xF6C6, 518}, {0xF6C7, 519}, {0xF6C8, 520}, {0xF6C9, 521},
        {0xF6CA, 522}, {0xF6CB, 523}, {0xF6CC, 524}, {0xF6CD, 525}, {0xF6CE, 526},
        {0xF6CF, 527}, {0xF6D0, 528}, {0xF6D1, 529}, {0xF6D2, 530}, {0xF6D3, 531}
    };
    
    auto it = unicodeToNameIndex.find(unicodeValue);
    if (it != unicodeToNameIndex.end()) {
        return it->second;
    }
    
    // Для остальных Unicode символов генерируем имя по шаблону uniXXXX
    // Начинаем с 532 чтобы избежать конфликтов со стандартными именами
    static uint16_t nextUnicodeNameIndex = 532;
    
    // Проверяем не превысили ли максимальный индекс
    if (nextUnicodeNameIndex > MAX_STANDARD_NAME_INDEX) {
        std::cerr << "TTFRebuilder: Warning - exceeded maximum standard name index for Unicode glyph" << std::endl;
        return 0; // Возвращаем .notdef
    }
    
    return nextUnicodeNameIndex++;
}

uint16_t TTFRebuilder::generateCompositeGlyphName(uint16_t /*glyphIndex*/) {
    // Для составных глифов используем имена по шаблону compXXXX
    static uint16_t nextCompositeNameIndex = 1000;
    
    // Проверяем не превысили ли максимальный индекс
    if (nextCompositeNameIndex > MAX_STANDARD_NAME_INDEX) {
        std::cerr << "TTFRebuilder: Warning - exceeded maximum standard name index for composite glyph" << std::endl;
        return 0; // Возвращаем .notdef
    }
    
    return nextCompositeNameIndex++;
}

uint16_t TTFRebuilder::generateDefaultGlyphName(uint16_t /*glyphIndex*/) {
    // Для остальных глифов используем имена по шаблону gXXXX
    static uint16_t nextDefaultNameIndex = 2000;
    
    // Проверяем не превысили ли максимальный индекс
    if (nextDefaultNameIndex > MAX_STANDARD_NAME_INDEX) {
        std::cerr << "TTFRebuilder: Warning - exceeded maximum standard name index for default glyph" << std::endl;
        return 0; // Возвращаем .notdef
    }
    
    return nextDefaultNameIndex++;
}

void TTFRebuilder::validateTableData(const std::string& tag, size_t minSize) const {
    auto it = tables.find(tag);
    if (it == tables.end()) {
        throw std::runtime_error("Table '" + tag + "' not found");
    }
    if (it->second.data.size() < minSize) {
        throw std::runtime_error("Table '" + tag + "' too small, expected at least " + 
                               std::to_string(minSize) + " bytes, got " + 
                               std::to_string(it->second.data.size()));
    }
}

void TTFRebuilder::validateGlyphData() const {
    auto glyfIt = tables.find("glyf");
    if (glyfIt == tables.end()) return;
    
    const auto& glyfData = glyfIt->second.data;
    
    for (const auto& glyph : glyphOffsets) {
        if (glyph.isEmpty) continue;
        
        if (glyph.offset >= glyfData.size()) {
            throw std::runtime_error("Glyph offset beyond glyf table bounds");
        }
        
        if (glyph.offset + glyph.length > glyfData.size()) {
            throw std::runtime_error("Glyph extends beyond glyf table bounds");
        }
    }
}

uint32_t TTFRebuilder::calculateChecksum(const std::vector<uint8_t>& data) const {
    return calculateTableChecksum(data);
}

void TTFRebuilder::updateTableOffsets() {
    uint32_t currentOffset = sizeof(utils::TTFHeader) + 
                           tableOrder.size() * sizeof(utils::TableRecord);
    
    currentOffset = (currentOffset + 3) & ~3;
    
    for (auto& tableName : tableOrder) {
        auto& table = tables[tableName];
        table.newOffset = currentOffset;
        currentOffset += table.newLength;
        
        currentOffset = (currentOffset + 3) & ~3;
    }
}

void TTFRebuilder::rebuildTableDirectory() {
    utils::TTFHeader header;
    header.sfntVersion = TTF_MAGIC;
    header.numTables = static_cast<uint16_t>(tableOrder.size());
    
    uint16_t maxPower2 = 1;
    uint16_t entrySelector = 0;
    while (maxPower2 * 2 <= header.numTables) {
        maxPower2 *= 2;
        entrySelector++;
    }
    header.searchRange = maxPower2 * 16;
    header.entrySelector = entrySelector;
    header.rangeShift = (header.numTables - maxPower2) * 16;
    
    newData.resize(sizeof(utils::TTFHeader));
    std::memcpy(newData.data(), &header, sizeof(header));
    
    size_t tableRecordsOffset = newData.size();
    newData.resize(tableRecordsOffset + header.numTables * sizeof(utils::TableRecord));
    
    for (size_t i = 0; i < tableOrder.size(); ++i) {
        const auto& table = tables[tableOrder[i]];
        utils::TableRecord record;
        std::memcpy(record.tag, table.tag.c_str(), 4);
        record.checksum = calculateTableChecksum(table.data);
        record.offset = table.newOffset;
        record.length = table.newLength;
        
        std::memcpy(newData.data() + tableRecordsOffset + i * sizeof(record), 
                   &record, sizeof(record));
    }
}

void TTFRebuilder::updateHeadTableChecksumAdjustment() {
    auto headTable = tables.find("head");
    if (headTable == tables.end()) return;
    
    uint32_t fontChecksum = 0;
    
    uint32_t headerChecksum = calculateTableChecksum(
        std::vector<uint8_t>(newData.begin(), newData.begin() + sizeof(utils::TTFHeader) + 
                            tableOrder.size() * sizeof(utils::TableRecord))
    );
    fontChecksum += headerChecksum;
    
    for (const auto& table : tableOrder) {
        fontChecksum += calculateTableChecksum(tables[table].data);
    }
    
    uint32_t checksumAdjustment = 0xB1B0AFBA - fontChecksum;
    
    if (headTable->second.data.size() >= 12) {
        setUInt32(headTable->second.data, 8, checksumAdjustment);
    }
}

uint32_t TTFRebuilder::calculateTableChecksum(const std::vector<uint8_t>& data) const {
    uint32_t sum = 0;
    size_t nLongs = (data.size() + 3) / 4;
    
    for (size_t i = 0; i < nLongs; ++i) {
        uint32_t value = 0;
        size_t bytesToCopy = std::min(size_t(4), data.size() - i * 4);
        std::memcpy(&value, data.data() + i * 4, bytesToCopy);
        sum += value;
    }
    
    return sum;
}

} // namespace fontmaster
