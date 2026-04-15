#include "stringutils.h"
#include <algorithm>
#include <cctype>

std::string trimString(const std::string& s) {
    auto begin = std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); });
    auto end   = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base();
    return (begin < end) ? std::string(begin, end) : std::string{};
}
