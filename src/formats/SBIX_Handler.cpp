#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/POSTParser.h"
#include "fontmaster/MAXPParser.h"
#include <fstream>
#include <map>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cstring>

namespace fontmaster {

class SBIX_Font;

class SBIX_Handler : public FontFormatHandler {
public:
    bool canHandle(const std::string& filepath) override {
        try {
            std::ifstream file(filepath, std::ios::binary);
            if (!file) return false;
            
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            if (size < 1024) return false;
            
            std::vector<uint8_t> header(1024);
            file.read(reinterpret_cast<char*>(header.data()), header.size());
            
            auto tables = utils::parseTTFTables(header);
            return utils::hasTable(tables, "sbix");
        } catch (...) {
            return false;
        }
    }
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) override;
    
    FontFormat getFormat() const override { return FontFormat::SBIX; }
};

#pragma pack(push, 1)
struct SBIXHeader {
    uint16_t version;
    uint16_t flags;
    uint32_t numStrikes;
};

struct StrikeOffset {
    uint32_t offset;
    uint32_t length;
};

struct StrikeHeader {
    uint16_t ppem;
    uint16_t resolution;
    uint32_t glyphDataOffset;
};

struct GlyphDataOffset {
    uint32_t offset;
};

struct GlyphDataHeader {
    int16_t originOffsetX;
    int16_t originOffsetY;
    char graphicType[4];
    uint32_t dataLength;
};
#pragma pack(pop)

// Вспомогательный класс TTFWriter
class TTFWriter {
private:
    std::vector<uint8_t> data;
    size_t pos;

public:
    TTFWriter() : pos(0) {}
    
    void writeUInt8(uint8_t value) {
        if (pos >= data.size()) data.resize(pos + 1);
        data[pos++] = value;
    }
    
    void writeInt16(int16_t value) {
        writeUInt16(static_cast<uint16_t>(value));
    }
    
    void writeUInt16(uint16_t value) {
        if (pos + 2 > data.size()) data.resize(pos + 2);
        data[pos] = (value >> 8) & 0xFF;
        data[pos + 1] = value & 0xFF;
        pos += 2;
    }
    
    void writeUInt32(uint32_t value) {
        if (pos + 4 > data.size()) data.resize(pos + 4);
        data[pos] = (value >> 24) & 0xFF;
        data[pos + 1] = (value >> 16) & 0xFF;
        data[pos + 2] = (value >> 8) & 0xFF;
        data[pos + 3] = value & 0xFF;
        pos += 4;
    }
    
    void writeBytes(const std::vector<uint8_t>& bytes) {
        if (pos + bytes.size() > data.size()) data.resize(pos + bytes.size());
        std::copy(bytes.begin(), bytes.end(), data.begin() + pos);
        pos += bytes.size();
    }
    
    void seek(size_t newPos) {
        if (newPos > data.size()) data.resize(newPos);
        pos = newPos;
    }
    
    size_t getPosition() const { return pos; }
    const std::vector<uint8_t>& getData() const { return data; }
};

class SBIX_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, std::vector<uint8_t>> glyphImages;
    std::map<uint32_t, std::string> unicodeToGlyphName;
    std::map<std::string, uint32_t> glyphNameToUnicode;
    std::vector<std::string> removedGlyphs;
    
    // SBIX специфичные данные
    std::vector<StrikeHeader> strikes;
    const utils::TableRecord* sbixTableRecord;
    uint16_t numGlyphs;
    
public:
    SBIX_Font(const std::string& path) : filepath(path), sbixTableRecord(nullptr), numGlyphs(0) {
        loadFontData();
        parseFont();
    }
    
    virtual ~SBIX_Font() = default;
    
    bool load() override {
        return !fontData.empty();
    }
    
    const std::vector<uint8_t>& getFontData() const override {
        return fontData;
    }
    
    void setFontData(const std::vector<uint8_t>& data) override {
        fontData = data;
    }
    
private:
    void loadFontData() {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw FontLoadException(filepath, "Cannot open file");
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        fontData.resize(size);
        if (!file.read(reinterpret_cast<char*>(fontData.data()), size)) {
            throw FontLoadException(filepath, "Cannot read file data");
        }
    }
    
    void parseFont() {
        auto tables = utils::parseTTFTables(fontData);
        if (!utils::hasTable(tables, "sbix")) {
            throw FontFormatException("SBIX", "sbix table not found");
        }
        
        sbixTableRecord = utils::findTable(tables, "sbix");
        if (!sbixTableRecord) {
            throw FontFormatException("SBIX", "sbix table record not found");
        }
        
        // Получаем количество глифов из maxp таблицы
        const utils::TableRecord* maxpTable = utils::findTable(tables, "maxp");
        if (maxpTable) {
            utils::MAXPParser maxpParser(fontData, maxpTable->offset);
            if (maxpParser.parse()) {
                numGlyphs = maxpParser.getNumGlyphs();
                std::cout << "MAXP: Found " << numGlyphs << " glyphs" << std::endl;
            }
        }
        
        if (numGlyphs == 0) {
            throw FontFormatException("SBIX", "Cannot determine number of glyphs");
        }
        
        parseSBIXTable();
        buildGlyphMappings();
    }
    
    void parseSBIXTable() {
        uint32_t sbixOffset = sbixTableRecord->offset;
        utils::TTFReader reader(fontData);
        reader.seek(sbixOffset);
        
        // Чтение заголовка SBIX
        SBIXHeader header;
        header.version = reader.readUInt16();
        header.flags = reader.readUInt16();
        header.numStrikes = reader.readUInt32();
        
        std::cout << "SBIX Table: version=" << header.version 
                  << ", flags=" << header.flags 
                  << ", strikes=" << header.numStrikes << std::endl;
        
        // Чтение смещений до strikes
        std::vector<StrikeOffset> strikeOffsets;
        for (uint32_t i = 0; i < header.numStrikes; ++i) {
            StrikeOffset so;
            so.offset = reader.readUInt32();
            so.length = reader.readUInt32();
            strikeOffsets.push_back(so);
        }
        
        // Парсинг каждого strike
        for (uint32_t i = 0; i < header.numStrikes; ++i) {
            parseStrike(sbixOffset + strikeOffsets[i].offset, i);
        }
    }
    
    void parseStrike(uint32_t strikeOffset, uint32_t strikeIndex) {
        utils::TTFReader reader(fontData);
        reader.seek(strikeOffset);
        
        StrikeHeader strike;
        strike.ppem = reader.readUInt16();
        strike.resolution = reader.readUInt16();
        strike.glyphDataOffset = reader.readUInt32();
        
        strikes.push_back(strike);
        
        std::cout << "Strike " << strikeIndex << ": ppem=" << strike.ppem 
                  << ", resolution=" << strike.resolution << std::endl;
        
        // Чтение смещений глифов
        std::vector<GlyphDataOffset> glyphOffsets;
        for (uint16_t i = 0; i < numGlyphs; ++i) {
            GlyphDataOffset go;
            go.offset = reader.readUInt32();
            glyphOffsets.push_back(go);
        }
        
        // Парсинг данных глифов
        for (uint16_t glyphIndex = 0; glyphIndex < numGlyphs; ++glyphIndex) {
            if (glyphOffsets[glyphIndex].offset == 0) {
                continue; // Нет данных для этого глифа
            }
            
            uint32_t glyphDataOffset = strikeOffset + glyphOffsets[glyphIndex].offset;
            parseGlyphData(glyphDataOffset, glyphIndex, strikeIndex);
        }
    }
    
    void parseGlyphData(uint32_t glyphDataOffset, uint16_t glyphIndex, uint32_t strikeIndex) {
        utils::TTFReader reader(fontData);
        reader.seek(glyphDataOffset);
        
        GlyphDataHeader glyphHeader;
        glyphHeader.originOffsetX = reader.readInt16();
        glyphHeader.originOffsetY = reader.readInt16();
        
        // Чтение graphicType
        auto graphicTypeBytes = reader.readBytes(4);
        std::memcpy(glyphHeader.graphicType, graphicTypeBytes.data(), 4);
        
        glyphHeader.dataLength = reader.readUInt32();
        
        // Чтение данных изображения
        auto imageData = reader.readBytes(glyphHeader.dataLength);
        
        // Получаем имя глифа
        std::string glyphName = getGlyphName(glyphIndex);
        if (!glyphName.empty()) {
            glyphImages[glyphName] = imageData;
            
            // Добавляем информацию о формате
            std::string format(glyphHeader.graphicType, 4);
            if (strikeIndex == 0) { // Выводим только для первого strike
                std::cout << "Glyph " << glyphName << " [" << glyphIndex << "]: " 
                          << format << " (" << imageData.size() << " bytes)" << std::endl;
            }
        }
    }
    
    std::string getGlyphName(uint16_t glyphIndex) {
        // Пытаемся получить имя из post таблицы
        auto tables = utils::parseTTFTables(fontData);
        const utils::TableRecord* postTable = utils::findTable(tables, "post");
        
        if (postTable) {
            try {
                utils::POSTParser postParser(fontData, postTable->offset, numGlyphs);
                if (postParser.parse()) {
                    auto glyphNames = postParser.getGlyphNames();
                    auto it = glyphNames.find(glyphIndex);
                    if (it != glyphNames.end()) {
                        return it->second;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing POST table: " << e.what() << std::endl;
            }
        }
        
        // Если нет имени в post, генерируем по умолчанию
        return "glyph" + std::to_string(glyphIndex);
    }
    
    void buildGlyphMappings() {
        // Используем cmap таблицу для построения mapping Unicode -> Glyph Name
        auto tables = utils::parseTTFTables(fontData);
        const utils::TableRecord* cmapTable = utils::findTable(tables, "cmap");
        
        if (cmapTable) {
            try {
                utils::CMAPParser cmapParser(fontData);
                cmapParser.parse();
                
                // Строим маппинг Unicode -> Glyph Name
                for (uint16_t glyphIndex = 0; glyphIndex < numGlyphs; ++glyphIndex) {
                    std::string glyphName = getGlyphName(glyphIndex);
                    auto charCodes = cmapParser.getCharCodes(glyphIndex);
                    
                    for (uint32_t charCode : charCodes) {
                        unicodeToGlyphName[charCode] = glyphName;
                        glyphNameToUnicode[glyphName] = charCode;
                    }
                }
                
                std::cout << "Built glyph mappings for " << unicodeToGlyphName.size() 
                          << " Unicode characters" << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "Error parsing CMAP table: " << e.what() << std::endl;
            }
        }
    }

    // ==================== МЕТОДЫ ПЕРЕСБОРКИ SBIX ТАБЛИЦЫ ====================
    
    void rebuildSBIXTable(std::vector<uint8_t>& outputData) {
        auto tables = utils::parseTTFTables(outputData);
        const utils::TableRecord* sbixTableRec = utils::findTable(tables, "sbix");
        if (!sbixTableRec) {
            throw FontSaveException(filepath, "SBIX table not found during rebuild");
        }

        uint32_t sbixOffset = sbixTableRec->offset;
        utils::TTFReader reader(outputData);
        reader.seek(sbixOffset);

        // Читаем оригинальный заголовок SBIX
        SBIXHeader header;
        header.version = reader.readUInt16();
        header.flags = reader.readUInt16();
        header.numStrikes = reader.readUInt32();

        // Читаем оригинальные смещения strikes
        std::vector<StrikeOffset> originalStrikeOffsets;
        for (uint32_t i = 0; i < header.numStrikes; ++i) {
            StrikeOffset so;
            so.offset = reader.readUInt32();
            so.length = reader.readUInt32();
            originalStrikeOffsets.push_back(so);
        }

        // Собираем новые данные для SBIX таблицы
        TTFWriter writer;

        // Записываем заголовок
        writer.writeUInt16(header.version);
        writer.writeUInt16(header.flags);
        writer.writeUInt32(header.numStrikes);

        // Резервируем место для strike offsets (будем заполнять позже)
        size_t strikeOffsetsPos = writer.getPosition();
        for (uint32_t i = 0; i < header.numStrikes; ++i) {
            writer.writeUInt32(0); // offset - заполним позже
            writer.writeUInt32(0); // length - заполним позже
        }

        // Обрабатываем каждый strike
        std::vector<StrikeOffset> newStrikeOffsets;
        for (uint32_t strikeIndex = 0; strikeIndex < header.numStrikes; ++strikeIndex) {
            uint32_t strikeStartPos = writer.getPosition();
            
            // Перестраиваем strike
            uint32_t originalStrikeOffset = sbixOffset + originalStrikeOffsets[strikeIndex].offset;
            rebuildStrike(outputData, originalStrikeOffset, strikeIndex, writer);
            
            uint32_t strikeLength = writer.getPosition() - strikeStartPos;
            
            StrikeOffset newSO;
            newSO.offset = strikeStartPos;
            newSO.length = strikeLength;
            newStrikeOffsets.push_back(newSO);
        }

        // Обновляем смещения strikes в заголовке
        size_t currentPos = writer.getPosition();
        writer.seek(strikeOffsetsPos);
        for (const auto& strikeOffset : newStrikeOffsets) {
            writer.writeUInt32(strikeOffset.offset);
            writer.writeUInt32(strikeOffset.length);
        }
        writer.seek(currentPos);

        // Заменяем SBIX таблицу в выходных данных
        replaceTable(outputData, "sbix", writer.getData());
    }

    void rebuildStrike(std::vector<uint8_t>& outputData, uint32_t strikeOffset, 
                       uint32_t strikeIndex, TTFWriter& writer) {
        utils::TTFReader reader(outputData);
        reader.seek(strikeOffset);

        // Читаем заголовок strike
        StrikeHeader strikeHeader;
        strikeHeader.ppem = reader.readUInt16();
        strikeHeader.resolution = reader.readUInt16();
        strikeHeader.glyphDataOffset = reader.readUInt32();

        // Записываем заголовок strike
        writer.writeUInt16(strikeHeader.ppem);
        writer.writeUInt16(strikeHeader.resolution);
        
        // Резервируем место для glyphDataOffset (заполним позже)
        size_t glyphDataOffsetPos = writer.getPosition();
        writer.writeUInt32(0);

        // Получаем количество глифов
        auto tables = utils::parseTTFTables(outputData);
        const utils::TableRecord* maxpTable = utils::findTable(tables, "maxp");
        if (!maxpTable) return;
        
        uint16_t numGlyphsInStrike = numGlyphs;

        // Резервируем место для смещений глифов
        size_t glyphOffsetsPos = writer.getPosition();
        for (uint16_t i = 0; i < numGlyphsInStrike; ++i) {
            writer.writeUInt32(0);
        }

        // Обрабатываем данные глифов
        std::vector<uint32_t> newGlyphOffsets;
        uint32_t glyphDataStart = writer.getPosition();
        
        for (uint16_t glyphIndex = 0; glyphIndex < numGlyphsInStrike; ++glyphIndex) {
            std::string glyphName = getGlyphName(glyphIndex);
            
            // Проверяем, не удален ли глиф
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphName) != removedGlyphs.end()) {
                newGlyphOffsets.push_back(0); // Глиф удален
                continue;
            }

            uint32_t glyphStartPos = writer.getPosition();
            newGlyphOffsets.push_back(glyphStartPos - glyphDataStart);

            auto it = glyphImages.find(glyphName);
            if (it != glyphImages.end() && !it->second.empty()) {
                // Используем модифицированное изображение
                writeGlyphDataWithNewImage(writer, it->second, "png ");
            } else {
                // Используем оригинальное изображение
                uint32_t originalGlyphOffset = strikeOffset + strikeHeader.glyphDataOffset;
                reader.seek(originalGlyphOffset + glyphIndex * 4);
                uint32_t originalDataOffset = reader.readUInt32();
                
                if (originalDataOffset != 0) {
                    copyOriginalGlyphData(outputData, strikeOffset + originalDataOffset, writer);
                } else {
                    // Нет оригинальных данных - записываем пустые данные
                    writeEmptyGlyphData(writer);
                }
            }
        }

        // Обновляем glyphDataOffset
        size_t currentPos = writer.getPosition();
        writer.seek(glyphDataOffsetPos);
        writer.writeUInt32(glyphDataStart);
        writer.seek(currentPos);

        // Обновляем смещения глифов
        writer.seek(glyphOffsetsPos);
        for (uint32_t offset : newGlyphOffsets) {
            writer.writeUInt32(offset);
        }
        writer.seek(currentPos);
    }

    void writeGlyphDataWithNewImage(TTFWriter& writer, 
                                   const std::vector<uint8_t>& imageData,
                                   const std::string& graphicType) {
        // Записываем заголовок глиф-данных
        writer.writeInt16(0); // originOffsetX
        writer.writeInt16(0); // originOffsetY
        
        // Записываем graphicType
        for (size_t i = 0; i < 4; ++i) {
            if (i < graphicType.length()) {
                writer.writeUInt8(static_cast<uint8_t>(graphicType[i]));
            } else {
                writer.writeUInt8(' ');
            }
        }
        
        // Записываем длину данных
        writer.writeUInt32(static_cast<uint32_t>(imageData.size()));
        
        // Записываем данные изображения
        writer.writeBytes(imageData);
    }

    void writeEmptyGlyphData(TTFWriter& writer) {
        // Записываем пустые данные глифа
        writer.writeInt16(0); // originOffsetX
        writer.writeInt16(0); // originOffsetY
        writer.writeUInt8(' '); writer.writeUInt8(' '); 
        writer.writeUInt8(' '); writer.writeUInt8(' '); // graphicType
        writer.writeUInt32(0); // dataLength
        // Нет данных изображения
    }

    void copyOriginalGlyphData(std::vector<uint8_t>& outputData, 
                              uint32_t glyphDataOffset, 
                              TTFWriter& writer) {
        utils::TTFReader reader(outputData);
        reader.seek(glyphDataOffset);
        
        // Копируем заголовок
        int16_t originOffsetX = reader.readInt16();
        int16_t originOffsetY = reader.readInt16();
        auto graphicType = reader.readBytes(4);
        uint32_t dataLength = reader.readUInt32();
        
        // Записываем заголовок
        writer.writeInt16(originOffsetX);
        writer.writeInt16(originOffsetY);
        writer.writeBytes(graphicType);
        writer.writeUInt32(dataLength);
        
        // Копируем данные
        auto imageData = reader.readBytes(dataLength);
        writer.writeBytes(imageData);
    }

    void replaceTable(std::vector<uint8_t>& outputData, 
                      const std::string& tableTag, 
                      const std::vector<uint8_t>& newTableData) {
        auto tables = utils::parseTTFTables(outputData);
        const utils::TableRecord* tableRecord = utils::findTable(tables, tableTag);
        if (!tableRecord) return;

        uint32_t tableOffset = tableRecord->offset;
        uint32_t originalLength = tableRecord->length;
        uint32_t newLength = static_cast<uint32_t>(newTableData.size());

        if (newLength <= originalLength) {
            // Просто перезаписываем на том же месте
            std::copy(newTableData.begin(), newTableData.end(), 
                      outputData.begin() + tableOffset);
            
            // Заполняем остаток нулями если нужно
            if (newLength < originalLength) {
                std::fill(outputData.begin() + tableOffset + newLength,
                         outputData.begin() + tableOffset + originalLength, 0);
            }
        } else {
            // Таблица увеличилась - используем простую стратегию: добавляем в конец
            simpleTableReplacement(outputData, tableTag, newTableData);
        }
    }

    void simpleTableReplacement(std::vector<uint8_t>& outputData,
                               const std::string& tableTag,
                               const std::vector<uint8_t>& newTableData) {
        // Простая стратегия: добавляем новую таблицу в конец файла
        // и обновляем offset в directory
        
        auto tables = utils::parseTTFTables(outputData);
        uint32_t newTableOffset = static_cast<uint32_t>(outputData.size());
        
        // Добавляем новую таблицу в конец
        outputData.insert(outputData.end(), newTableData.begin(), newTableData.end());
        
        // Выравнивание по 4 байта
        while (outputData.size() % 4 != 0) {
            outputData.push_back(0);
        }
        
        // Обновляем offset в таблице directory
        uint32_t directoryStart = sizeof(utils::TTFHeader);
        for (size_t i = 0; i < tables.size(); ++i) {
            std::string currentTag(tables[i].tag, 4);
            if (currentTag == tableTag) {
                uint32_t directoryEntryOffset = directoryStart + i * sizeof(utils::TableRecord) + 8; // +8 для пропуска tag и checksum
                
                // Записываем новое смещение
                TTFWriter writer;
                writer.writeUInt32(newTableOffset);
                std::vector<uint8_t> offsetData = writer.getData();
                
                std::copy(offsetData.begin(), offsetData.end(),
                          outputData.begin() + directoryEntryOffset);
                break;
            }
        }
    }

    void rebuildFontStructure(std::vector<uint8_t>& outputData) {
        // Обновляем checksumAdjustment в head таблице
        updateHeadTableChecksum(outputData);
    }

    void updateHeadTableChecksum(std::vector<uint8_t>& outputData) {
        auto tables = utils::parseTTFTables(outputData);
        const utils::TableRecord* headTable = utils::findTable(tables, "head");
        if (!headTable) return;

        // Устанавливаем checksumAdjustment в 0 для расчета
        uint32_t checksumAdjustmentOffset = headTable->offset + 8;
        TTFWriter writer;
        writer.writeUInt32(0);
        std::vector<uint8_t> zeroChecksum = writer.getData();
        
        std::copy(zeroChecksum.begin(), zeroChecksum.end(),
                  outputData.begin() + checksumAdjustmentOffset);

        // Рассчитываем новую контрольную сумму
        uint32_t newChecksum = calculateFontChecksum(outputData);
        
        // Записываем новую контрольную сумму
        writer.seek(0);
        writer.writeUInt32(newChecksum);
        std::vector<uint8_t> newChecksumData = writer.getData();
        
        std::copy(newChecksumData.begin(), newChecksumData.end(),
                  outputData.begin() + checksumAdjustmentOffset);
    }

    uint32_t calculateFontChecksum(std::vector<uint8_t>& fontData) {
        auto tables = utils::parseTTFTables(fontData);
        
        // Суммируем контрольные суммы всех таблиц
        uint32_t totalChecksum = 0;
        for (const auto& table : tables) {
            uint32_t tableChecksum = calculateTableChecksum(
                fontData, table.offset, table.length);
            totalChecksum += tableChecksum;
        }
        
        // Magic number from OpenType spec
        uint32_t magicNumber = 0xB1B0AFBA;
        return magicNumber - totalChecksum;
    }

    uint32_t calculateTableChecksum(const std::vector<uint8_t>& data, 
                                   uint32_t offset, uint32_t length) {
        uint32_t sum = 0;
        uint32_t nLongs = (length + 3) / 4;
        
        for (uint32_t i = 0; i < nLongs; ++i) {
            uint32_t value = 0;
            for (int j = 0; j < 4; ++j) {
                uint32_t pos = offset + i * 4 + j;
                if (pos < offset + length) {
                    value = (value << 8) | data[pos];
                }
            }
            sum += value;
        }
        
        return sum;
    }
    
public:
    FontFormat getFormat() const override { return FontFormat::SBIX; }
    
    bool removeGlyph(const std::string& glyphName) override {
        auto it = glyphImages.find(glyphName);
        if (it == glyphImages.end()) {
            return false;
        }
        
        glyphImages.erase(it);
        removedGlyphs.push_back(glyphName);
        return true;
    }
    
    bool removeGlyph(uint32_t unicode) override {
        std::string name = findGlyphName(unicode);
        if (name.empty()) return false;
        return removeGlyph(name);
    }
    
    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override {
        auto it = glyphImages.find(glyphName);
        if (it == glyphImages.end()) {
            return false;
        }
        
        it->second = newImage;
        return true;
    }
    
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        for (const auto& [name, imageData] : glyphImages) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), name) != removedGlyphs.end()) {
                continue;
            }
            
            GlyphInfo info;
            info.name = name;
            
            // Определяем формат на основе данных
            if (imageData.size() >= 8) {
                if (imageData[0] == 0x89 && imageData[1] == 0x50 && imageData[2] == 0x4E && imageData[3] == 0x47) {
                    info.format = "png";
                } else if (imageData[0] == 0xFF && imageData[1] == 0xD8 && imageData[2] == 0xFF) {
                    info.format = "jpg";
                } else {
                    info.format = "unknown";
                }
            } else {
                info.format = "unknown";
            }
            
            info.image_data = imageData;
            info.data_size = imageData.size();
            
            // Находим Unicode код
            auto it = glyphNameToUnicode.find(name);
            if (it != glyphNameToUnicode.end()) {
                info.unicode = it->second;
            }
            
            result.push_back(info);
        }
        return result;
    }
    
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override {
        auto it = glyphImages.find(glyphName);
        if (it != glyphImages.end()) {
            GlyphInfo info;
            info.name = glyphName;
            info.image_data = it->second;
            info.data_size = it->second.size();
            
            // Определяем формат
            if (it->second.size() >= 8) {
                if (it->second[0] == 0x89 && it->second[1] == 0x50 && 
                    it->second[2] == 0x4E && it->second[3] == 0x47) {
                    info.format = "png";
                } else if (it->second[0] == 0xFF && it->second[1] == 0xD8 && it->second[2] == 0xFF) {
                    info.format = "jpg";
                } else {
                    info.format = "unknown";
                }
            } else {
                info.format = "unknown";
            }
            
            // Находим Unicode код
            auto unicodeIt = glyphNameToUnicode.find(glyphName);
            if (unicodeIt != glyphNameToUnicode.end()) {
                info.unicode = unicodeIt->second;
            }
            
            return info;
        }
        throw GlyphNotFoundException(glyphName);
    }
    
    std::string findGlyphName(uint32_t unicode) const override {
        auto it = unicodeToGlyphName.find(unicode);
        return it != unicodeToGlyphName.end() ? it->second : "";
    }
    
    bool save(const std::string& outputPath) override {
        try {
            std::ofstream file(outputPath, std::ios::binary);
            if (!file) {
                throw FontSaveException(outputPath, "Cannot create output file");
            }

            // Создаем копию исходных данных для модификации
            std::vector<uint8_t> outputData = fontData;
        
            // Перестраиваем SBIX таблицу с учетом изменений
            rebuildSBIXTable(outputData);
        
            // Обновляем структуру шрифта
            rebuildFontStructure(outputData);
        
            file.write(reinterpret_cast<const char*>(outputData.data()), outputData.size());
            return file.good();
        } catch (const std::exception& e) {
            throw FontSaveException(outputPath, std::string("Save failed: ") + e.what());
        }
    }
};

std::unique_ptr<Font> SBIX_Handler::loadFont(const std::string& filepath) {
    return std::make_unique<SBIX_Font>(filepath);
}

static bool registerSBIXHandler() {
    return true;
}

static bool sbixHandlerRegistered = registerSBIXHandler();

} // namespace fontmaster
