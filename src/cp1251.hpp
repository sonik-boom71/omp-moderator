#pragma once

#include <string>
#include <string_view>

// Russian SA-MP clients send chat in cp1251, while config.json is UTF-8.
namespace cp1251 {

std::string from_utf8(std::string_view utf8);
std::string to_utf8(std::string_view text);

unsigned char to_lower(unsigned char c);
std::string to_lower(std::string_view text);
bool is_letter(unsigned char c);
bool is_upper(unsigned char c);

}  // namespace cp1251
