#include "cp1251.hpp"

#include <array>
#include <cstdint>

namespace cp1251 {
namespace {

// Code points of bytes 0x80..0xBF; 0xC0..0xFF map linearly onto U+0410..U+044F.
constexpr std::array<char16_t, 64> kHigh{
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
};

constexpr unsigned char kUpperYo = 0xA8;
constexpr unsigned char kLowerYo = 0xB8;

char32_t decode(unsigned char byte) {
    if (byte < 0x80) return byte;
    if (byte >= 0xC0) return 0x0410 + (byte - 0xC0);
    return kHigh[byte - 0x80];
}

char encode(char32_t code) {
    if (code < 0x80) return static_cast<char>(code);
    if (code >= 0x0410 && code <= 0x044F) return static_cast<char>(0xC0 + (code - 0x0410));
    for (std::size_t i = 0; i < kHigh.size(); ++i) {
        if (kHigh[i] == code) return static_cast<char>(0x80 + i);
    }
    return '?';
}

void append_utf8(std::string& out, char32_t code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

}  // namespace

std::string from_utf8(std::string_view utf8) {
    std::string out;
    std::size_t i = 0;
    while (i < utf8.size()) {
        const auto lead = static_cast<unsigned char>(utf8[i]);
        const std::size_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
        bool valid = (lead < 0x80 || extra > 0) && utf8.size() - i > extra;
        char32_t code = lead & (0x7F >> (extra ? extra + 1 : 0));
        for (std::size_t k = 1; valid && k <= extra; ++k) {
            const auto next = static_cast<unsigned char>(utf8[i + k]);
            valid = (next & 0xC0) == 0x80;
            code = (code << 6) | (next & 0x3F);
        }
        out += valid ? encode(code) : '?';
        i += valid ? extra + 1 : 1;
    }
    return out;
}

std::string to_utf8(std::string_view text) {
    std::string out;
    for (char c : text) append_utf8(out, decode(static_cast<unsigned char>(c)));
    return out;
}

unsigned char to_lower(unsigned char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDF)) return static_cast<unsigned char>(c + 0x20);
    if (c == kUpperYo) return kLowerYo;
    return c;
}

std::string to_lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(to_lower(static_cast<unsigned char>(c)));
    return out;
}

bool is_letter(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0xC0 || c == kUpperYo || c == kLowerYo;
}

bool is_upper(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDF) || c == kUpperYo;
}

}  // namespace cp1251
