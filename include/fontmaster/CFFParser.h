#ifndef CFFPARSER_H
#define CFFPARSER_H

#include <vector>
#include <cstdint>

namespace fontmaster {
namespace utils {

class CFFParser {
public:
    CFFParser(const std::vector<uint8_t>& data, uint32_t offset = 0);
    
    bool parse();
    
private:
    std::vector<uint8_t> fontData;
    uint32_t baseOffset;

    bool parseIndex(size_t& offset);
    uint32_t readOffset(const uint8_t* data, uint8_t offSize);
};

} // namespace utils
} // namespace fontmaster

#endif // CFFPARSER_H
