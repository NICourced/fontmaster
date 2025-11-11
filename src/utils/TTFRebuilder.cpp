#include "fontmaster/TTFRebuilder.h"
#include "fontmaster/TTFUtils.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <cstring>

namespace fontmaster {

TTFRebuilder::TTFRebuilder(const std::vector<uint8_t>& fontData) 
    : originalData(fontData) {
    parseOriginalStructure();
}

void TTFRebuilder::parseOriginalStructure() {
    auto tableRecords = utils::parseTTFTables(originalData);
    
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
        }
        
        tables[info.tag] = info;
        tableOrder.push_back(info);
    }
}

void TTFRebuilder::markTableModified(const std::string& tag) {
    if (tables.find(tag) != tables.end()) {
        tables[tag].modified = true;
    }
}

void TTFRebuilder::setTableData(const std::string& tag, const std::vector<uint8_t>& data) {
    if (tables.find(tag) != tables.end()) {
        tables[tag].data = data;
        tables[tag].newLength = data.size();
        tables[tag].modified = true;
    }
}

const std::vector<uint8_t>* TTFRebuilder::getTableData(const std::string& tag) const {
    auto it = tables.find(tag);
    if (it != tables.end()) {
        return &it->second.data;
    }
    return nullptr;
}

std::vector<uint8_t> TTFRebuilder::rebuild() {
    newData.clear();
    
    // 1. Пересобираем модифицированные таблицы
    for (auto& pair : tables) {
        if (pair.second.modified) {
            rebuildTable(pair.first);
        }
    }
    
    // 2. Рассчитываем новые смещения
    updateTableOffsets();
    
    // 3. Собираем новый заголовок и директорию таблиц
    rebuildTableDirectory();
    
    // 4. Копируем данные всех таблиц
    for (const auto& table : tableOrder) {
        size_t requiredSize = table.newOffset + table.newLength;
        if (newData.size() < requiredSize) {
            newData.resize(requiredSize);
        }
        
        const auto& tableInfo = tables[table.tag];
        std::copy(tableInfo.data.begin(), tableInfo.data.end(), 
                 newData.begin() + tableInfo.newOffset);
        
        // Выравнивание до 4 байт
        size_t alignedSize = (tableInfo.newLength + 3) & ~3;
        if (alignedSize > tableInfo.newLength) {
            std::fill(newData.begin() + tableInfo.newOffset + tableInfo.newLength,
                     newData.begin() + tableInfo.newOffset + alignedSize, 0);
        }
    }
    
    // 5. Обновляем checksum adjustment в head таблице
    updateHeadTableChecksumAdjustment();
    
    std::cout << "TTFRebuilder: Rebuilt font, original size: " << originalData.size() 
              << " bytes, new size: " << newData.size() << " bytes" << std::endl;
    
    return newData;
}

void TTFRebuilder::rebuildTableDirectory() {
    // Заголовок TTF
    utils::TTFHeader header;
    header.sfntVersion = 0x00010000; // TrueType
    header.numTables = tableOrder.size();
    
    // Вычисляем searchRange, entrySelector, rangeShift
    uint16_t maxPower2 = 1;
    uint16_t entrySelector = 0;
    while (maxPower2 * 2 <= header.numTables) {
        maxPower2 *= 2;
        entrySelector++;
    }
    header.searchRange = maxPower2 * 16;
    header.entrySelector = entrySelector;
    header.rangeShift = (header.numTables - maxPower2) * 16;
    
    // Записываем заголовок
    newData.resize(sizeof(utils::TTFHeader));
    std::memcpy(newData.data(), &header, sizeof(header));
    
    // Записываем записи таблиц
    size_t tableRecordsOffset = newData.size();
    newData.resize(tableRecordsOffset + header.numTables * sizeof(utils::TableRecord));
    
    for (size_t i = 0; i < tableOrder.size(); ++i) {
        const auto& table = tables[tableOrder[i].tag];
        utils::TableRecord record;
        std::memcpy(record.tag, table.tag.c_str(), 4);
        record.checksum = calculateTableChecksum(table.data);
        record.offset = table.newOffset;
        record.length = table.newLength;
        
        std::memcpy(newData.data() + tableRecordsOffset + i * sizeof(record), 
                   &record, sizeof(record));
    }
}

void TTFRebuilder::updateTableOffsets() {
    uint32_t currentOffset = sizeof(utils::TTFHeader) + 
                           tableOrder.size() * sizeof(utils::TableRecord);
    
    // Выравнивание до 4 байт
    currentOffset = (currentOffset + 3) & ~3;
    
    for (auto& tableName : tableOrder) {
        auto& table = tables[tableName.tag];
        table.newOffset = currentOffset;
        currentOffset += table.newLength;
        
        // Выравнивание каждой таблицы до 4 байт
        currentOffset = (currentOffset + 3) & ~3;
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

void TTFRebuilder::updateHeadTableChecksumAdjustment() {
    auto headTable = tables.find("head");
    if (headTable == tables.end()) return;
    
    // Вычисляем checksum всего шрифта
    uint32_t fontChecksum = 0;
    for (const auto& table : tableOrder) {
        fontChecksum += tables[table.tag].modified ? 
                       calculateTableChecksum(tables[table.tag].data) :
                       calculateTableChecksum(std::vector<uint8_t>(
                           originalData.begin() + table.originalOffset,
                           originalData.begin() + table.originalOffset + table.originalLength));
    }
    
    // checksumAdjustment = 0xB1B0AFBA - fontChecksum
    uint32_t checksumAdjustment = 0xB1B0AFBA - fontChecksum;
    
    // Обновляем поле в head таблице (смещение 8)
    if (headTable->second.data.size() >= 12) {
        std::memcpy(headTable->second.data.data() + 8, &checksumAdjustment, 4);
    }
}

void TTFRebuilder::rebuildTable(const std::string& tag) {
    // Базовая реализация - просто используем установленные данные
    // В производных классах можно переопределить для конкретных таблиц
    std::cout << "TTFRebuilder: Rebuilding table '" << tag << "'" << std::endl;
}

} // namespace fontmaster
