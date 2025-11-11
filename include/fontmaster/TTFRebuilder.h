#ifndef TTFRebuilder_H
#define TTFRebuilder_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <functional>

namespace fontmaster {

class TTFRebuilder {
public:
    TTFRebuilder(const std::vector<uint8_t>& fontData);
    virtual ~TTFRebuilder() = default;

    // Основные методы
    void markTableModified(const std::string& tag);
    void setTableData(const std::string& tag, const std::vector<uint8_t>& data);
    const std::vector<uint8_t>* getTableData(const std::string& tag) const;
    std::vector<uint8_t> rebuild();

    // Методы для работы со специфичными таблицами
    void updateGlyfTable(const std::string& glyfTag = "glyf");
    void updateLocaTable(const std::string& locaTag = "loca");
    void updateHmtxTable(const std::string& hmtxTag = "hmtx");
    void updateHheaTable(const std::string& hheaTag = "hhea");
    void updateMaxpTable(const std::string& maxpTag = "maxp");

    // Утилиты
    uint32_t calculateChecksum(const std::vector<uint8_t>& data) const;
    void setTableRebuildHandler(const std::string& tag, 
                               std::function<void(const std::string&)> handler);

    // Методы для расчета метрик
    void calculateGlyphOffsets();
    void calculateHMetrics();

protected:
    virtual void rebuildTable(const std::string& tag);

private:
    struct TableInfo {
        std::string tag;
        uint32_t originalOffset;
        uint32_t originalLength;
        uint32_t newOffset;
        uint32_t newLength;
        bool modified;
        std::vector<uint8_t> data;
    };

    struct GlyphInfo {
        uint32_t offset;
        uint32_t length;
    };

    std::vector<uint8_t> originalData;
    std::vector<uint8_t> newData;
    std::unordered_map<std::string, TableInfo> tables;
    std::vector<std::string> tableOrder;
    std::unordered_map<std::string, std::function<void(const std::string&)>> rebuildHandlers;
    std::vector<GlyphInfo> glyphOffsets;
    uint16_t numGlyphs;
    uint16_t numHMetrics;

    void parseOriginalStructure();
    void updateTableOffsets();
    void rebuildTableDirectory();
    void updateHeadTableChecksumAdjustment();
    uint32_t calculateTableChecksum(const std::vector<uint8_t>& data) const;
    
    // Специфичные методы для таблиц
    void rebuildGlyfTable(const std::string& tag);
    void rebuildLocaTable(const std::string& tag);
    void rebuildHmtxTable(const std::string& tag);
    void rebuildHheaTable(const std::string& tag);
    void rebuildMaxpTable(const std::string& tag);
    void rebuildNameTable(const std::string& tag);
    void rebuildOS2Table(const std::string& tag);
    
    // Вспомогательные методы
    uint16_t getUInt16(const std::vector<uint8_t>& data, size_t offset) const;
    uint32_t getUInt32(const std::vector<uint8_t>& data, size_t offset) const;
    int16_t getInt16(const std::vector<uint8_t>& data, size_t offset) const;
    void setUInt16(std::vector<uint8_t>& data, size_t offset, uint16_t value) const;
    void setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value) const;
    void setInt16(std::vector<uint8_t>& data, size_t offset, int16_t value) const;

    // Методы для парсинга таблиц
    bool parseLocaTable();
    bool parseMaxpTable();
    bool parseHheaTable();
    void updateNumGlyphs(uint16_t newNumGlyphs);
    void updateNumberOfHMetrics(uint16_t newNumHMetrics);
};

} // namespace fontmaster

#endif
