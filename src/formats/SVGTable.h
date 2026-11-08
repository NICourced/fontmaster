#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
namespace fontmaster::detail {
class SVGTable {
public:
    using Document = std::shared_ptr<const std::vector<uint8_t>>;
    std::map<uint16_t, Document> glyphs;
    SVGTable() = default;
    explicit SVGTable(const std::vector<uint8_t>& table, uint16_t glyphCount);
    std::vector<uint8_t> build() const;
    bool replace(uint16_t gid, const std::vector<uint8_t>& image, uint16_t upem);
};
} // namespace fontmaster::detail
