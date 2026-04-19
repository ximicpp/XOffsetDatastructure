#ifndef XOFFSET_SIGNATURE_TYPE_NAMES_HPP
#define XOFFSET_SIGNATURE_TYPE_NAMES_HPP

#include "signature_type_list.hpp"

#include <string>
#include <vector>

namespace xoffset::signature {

inline std::vector<std::string> exported_type_names() {
    std::vector<std::string> names;
    #define XOFFSET_PUSH_NAME(Type, Name) \
        names.emplace_back(Name);
    XOFFSET_SIGNATURE_TYPE_LIST(XOFFSET_PUSH_NAME)
    #undef XOFFSET_PUSH_NAME
    return names;
}

} // namespace xoffset::signature

#endif // XOFFSET_SIGNATURE_TYPE_NAMES_HPP
