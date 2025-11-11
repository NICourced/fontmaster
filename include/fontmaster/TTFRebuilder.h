#ifndef TTFRebuilder_H
#define TTFRebuilder_H

#include <vector>
#include <string>
#include <map>
#include <functional>
#include <cstdint>

namespace fontmaster {

namespace utils {
    struct TTFHeader;
    struct TableRecord;
    std::vector<TableRecord> parseTTFTables(const std::vector<uint8_t>& data);
}

class TTFRebuilder {
public:
    struct GlyphInfo {
        uint32_t offset;
        uint32_t length;
        uint16_t advanceWidth;
        int16_t leftSideBearing;
        bool isEmpty;
    };

    struct NameRecord {
        uint16_t platformID;
        uint16_t encodingID;
        uint16_t languageID;
        uint16_t nameID;
        uint16_t length;
        uint16_t offset;
    };

    struct CompositeGlyphStats {
        uint16_t maxPoints;
        uint16_t maxContours;
        uint16_t maxComponents;
        uint16_t maxDepth;
    };

    struct TableInfo {
        std::string tag;
        uint32_t originalOffset;
        uint32_t originalLength;
        uint32_t newOffset;
        uint32_t newLength;
        bool modified;
        std::vector<uint8_t> data;
    };

    TTFRebuilder(const std::vector<uint8_t>& fontData);
    
    void markTableModified(const std::string& tag);
    void setTableData(const std::string& tag, const std::vector<uint8_t>& data);
    const std::vector<uint8_t>* getTableData(const std::string& tag) const;
    bool hasTable(const std::string& tag) const;
    void setTableRebuildHandler(const std::string& tag, std::function<void(const std::string&)> handler);
    
    std::vector<uint8_t> rebuild();
    
    void setNumGlyphs(uint16_t newNumGlyphs);
    void setNumberOfHMetrics(uint16_t newNumHMetrics);

    // Основные методы обновления таблиц
    void updateGlyfTable(const std::string& glyfTag = "glyf");
    void updateLocaTable(const std::string& locaTag = "loca");
    void updateHmtxTable(const std::string& hmtxTag = "hmtx");
    void updateHheaTable(const std::string& hheaTag = "hhea");
    void updateMaxpTable(const std::string& maxpTag = "maxp");

private:
    std::vector<uint8_t> originalData;
    std::vector<uint8_t> newData;
    std::map<std::string, TableInfo> tables;
    std::vector<std::string> tableOrder;
    std::map<std::string, std::function<void(const std::string&)>> rebuildHandlers;
    
    std::vector<GlyphInfo> glyphOffsets;
    uint16_t numGlyphs;
    uint16_t numHMetrics;
    bool locaShortFormat;

    // Вспомогательные методы для работы с данными
    uint16_t getUInt16(const std::vector<uint8_t>& data, size_t offset) const;
    uint32_t getUInt32(const std::vector<uint8_t>& data, size_t offset) const;
    int16_t getInt16(const std::vector<uint8_t>& data, size_t offset) const;
    void setUInt16(std::vector<uint8_t>& data, size_t offset, uint16_t value);
    void setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value);
    void setInt16(std::vector<uint8_t>& data, size_t offset, int16_t value);
    
    // Методы парсинга оригинальной структуры
    void parseOriginalStructure();
    bool parseHeadTable();
    bool parseMaxpTable();
    bool parseHheaTable();
    bool parseLocaTable();
    void parseGlyfTable();
    
    // Методы пересборки таблиц
    void rebuildTable(const std::string& tag);
    void rebuildGlyfTable(const std::string& tag);
    void rebuildLocaTable(const std::string& tag);
    void rebuildHmtxTable(const std::string& tag);
    void rebuildHheaTable(const std::string& tag);
    void rebuildMaxpTable(const std::string& tag);
    void rebuildNameTable(const std::string& tag);
    void rebuildOS2Table(const std::string& tag);
    void rebuildHeadTable(const std::string& tag);
    void rebuildPostTable(const std::string& tag);
    
    // Методы расчета и обновления
    void calculateGlyphOffsets();
    uint32_t parseSimpleGlyphLength(const std::vector<uint8_t>& data, uint32_t offset) const;
    uint32_t parseCompositeGlyphLength(const std::vector<uint8_t>& data, uint32_t offset) const;
    void calculateGlyphMetrics();
    void calculateHMetrics();
    void recalculateFontMetrics();
    void updateHheaMetrics();
    void updateOS2Metrics();
    void updateMaxpTableValues();
    
    // Методы для работы с точками и контурами
    uint16_t calculateSimpleGlyphPoints(const std::vector<uint8_t>& data, uint32_t offset) const;
    CompositeGlyphStats calculateCompositeGlyphStats(const std::vector<uint8_t>& data, uint32_t offset) const;
    CompositeGlyphStats analyzeGlyphForComposite(const std::vector<uint8_t>& data, uint32_t offset) const;
    
    // Методы для работы с таблицей имен
    void updateNameTableChecksum();
    void fixMacRomanEncoding(std::vector<uint8_t>& stringData);
    void fixUnicodeEncoding(std::vector<uint8_t>& stringData);
    
    // Методы для работы с таблицей post
    void updatePostTableFormat2();
    uint16_t calculateStandardGlyphNameIndex(uint16_t glyphIndex);
    uint16_t getUnicodeFromCmap(uint16_t glyphIndex);
    uint16_t findGlyphInFormat4Subtable(const std::vector<uint8_t>& cmapData, uint32_t offset, uint16_t glyphIndex);
    uint16_t findGlyphInFormat12Subtable(const std::vector<uint8_t>& cmapData, uint32_t offset, uint16_t glyphIndex);
    bool isCompositeGlyph(uint16_t glyphIndex);
    uint16_t generateUnicodeGlyphNameIndex(uint16_t unicodeValue);
    uint16_t generateCompositeGlyphName(uint16_t glyphIndex);
    uint16_t generateDefaultGlyphName(uint16_t glyphIndex);
    
    // Методы валидации
    void validateTableData(const std::string& tag, size_t minSize) const;
    void validateGlyphData() const;
    
    // Методы для сборки финального файла
    void updateTableOffsets();
    void rebuildTableDirectory();
    void updateHeadTableChecksumAdjustment();
    uint32_t calculateTableChecksum(const std::vector<uint8_t>& data) const;
    uint32_t calculateChecksum(const std::vector<uint8_t>& data) const;
};

} // namespace fontmaster

#endif // TTFRebuilder_H
