#ifndef XOFFSET_SIGNATURE_TYPE_NAMES_HPP
#define XOFFSET_SIGNATURE_TYPE_NAMES_HPP

#include <string>
#include <vector>

namespace xoffset::signature {

inline std::vector<std::string> exported_type_names() {
    return {"Player", "Item", "GameData"};
}

} // namespace xoffset::signature

#endif // XOFFSET_SIGNATURE_TYPE_NAMES_HPP
