#include <chrono>
#include <cstdio>
#include <string>

#include "cp1251.hpp"
#include "filter.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using chatguard::Reason;
using namespace std::chrono_literals;

int g_failures = 0;

void report(const char* file, int line, const char* expr) {
    std::printf("FAIL %s:%d: %s\n", file, line, expr);
    ++g_failures;
}

#define CHECK(cond)                                     \
    do {                                                \
        if (!(cond)) report(__FILE__, __LINE__, #cond); \
    } while (0)

// Test phrases are written in UTF-8 and converted to what a Russian client would send.
std::string t(const char* utf8) {
    return cp1251::from_utf8(utf8);
}

Reason check(const char* utf8, const chatguard::Settings& settings = {}) {
    return chatguard::check_text(t(utf8), settings);
}

void test_encoding() {
    for (int b = 0; b < 256; ++b) {
        const std::string byte(1, static_cast<char>(b));
        CHECK(cp1251::from_utf8(cp1251::to_utf8(byte)) == byte);
    }
    CHECK(cp1251::to_utf8(t("Привет, Ёж!")) == "Привет, Ёж!");
    CHECK(cp1251::from_utf8("a\xD0") == "a?");
    CHECK(cp1251::from_utf8("\xD0\x41") == "?A");
    CHECK(cp1251::from_utf8("\xF0\x9F\x98\x80") == "?");
    CHECK(cp1251::to_lower(t("ПРИВЕТ Ёж ABC")) == t("привет ёж abc"));

#ifdef _WIN32
    for (int b = 0; b < 256; ++b) {
        const char byte = static_cast<char>(b);
        wchar_t wide = 0;
        MultiByteToWideChar(1251, 0, &byte, 1, &wide, 1);
        char utf8[8];
        const int size = WideCharToMultiByte(CP_UTF8, 0, &wide, 1, utf8, sizeof utf8, nullptr, nullptr);
        if (cp1251::to_utf8(std::string(1, byte)) != std::string(utf8, static_cast<std::size_t>(size))) {
            std::printf("FAIL: byte 0x%02X decodes differently from Windows cp1251\n", b);
            ++g_failures;
        }
    }
#endif
}

void test_advertising() {
    CHECK(check("всем привет, как дела?") == Reason::None);
    CHECK(check("заходите 185.169.134.67:7777") == Reason::Advertising);
    CHECK(check("заходите 185 . 169 . 134 . 67") == Reason::Advertising);
    CHECK(check("звони 8.800.555.35.35") == Reason::None);
    CHECK(check("версия 1.2, цена 3.50") == Reason::None);
    CHECK(check("256.1.1.1") == Reason::None);

    CHECK(check("лучший сервер samp-rp.ru") == Reason::Advertising);
    CHECK(check("лучший сервер samp-rp . RU") == Reason::Advertising);
    CHECK(check("подпишись t.me/channel") == Reason::Advertising);
    CHECK(check("заходи на сервер.рф") == Reason::Advertising);
    CHECK(check("Mr.Smith, привет... как ты?") == Reason::None);

    chatguard::Settings own;
    own.allowed_hosts = {"mysite.ru", "1.2.3.4"};
    CHECK(check("наш форум mysite.ru", own) == Reason::None);
    CHECK(check("наш форум forum.mysite.ru", own) == Reason::None);
    CHECK(check("наш IP 1.2.3.4:7777", own) == Reason::None);
    CHECK(check("наш IP 1.2.3.4, а лучше 5.6.7.8", own) == Reason::Advertising);
    CHECK(check("наш форум mysite.ru, а не other.ru", own) == Reason::Advertising);

    chatguard::Settings off;
    off.anti_ads = false;
    CHECK(check("заходите 185.169.134.67", off) == Reason::None);
}

void test_caps() {
    CHECK(check("ВСЕМ ПРИВЕТ") == Reason::Caps);
    CHECK(check("ПРИВЕТ") == Reason::Caps);
    CHECK(check("ОК") == Reason::None);
    CHECK(check("Привет Всем") == Reason::None);
    CHECK(check("HELLO world") == Reason::None);

    chatguard::Settings off;
    off.anti_caps = false;
    CHECK(check("ВСЕМ ПРИВЕТ", off) == Reason::None);
}

void test_bad_words() {
    chatguard::Settings s;
    s.bad_words = {t("дурак"), "*" + t("тупиц")};
    CHECK(check("ты ДуРаК", s) == Reason::BadWord);
    CHECK(check("ты д.у.р.а.к", s) == Reason::BadWord);
    CHECK(check("ты дyр@к", s) == Reason::BadWord);
    CHECK(check("ну ты и сверхтупица", s) == Reason::BadWord);
    CHECK(check("какой дурачок", s) == Reason::None);
    CHECK(check("мы идём гулять", s) == Reason::None);
}

void test_flood_and_repeat() {
    chatguard::Guard guard;
    const auto t0 = chatguard::Clock::now();

    CHECK(guard.check_message(1, t("первое"), t0) == Reason::None);
    CHECK(guard.check_message(1, t("второе"), t0 + 100ms) == Reason::None);
    CHECK(guard.check_message(1, t("третье"), t0 + 200ms) == Reason::None);
    CHECK(guard.check_message(1, t("четвёртое"), t0 + 300ms) == Reason::Flood);
    CHECK(guard.check_message(2, t("другой игрок"), t0 + 300ms) == Reason::None);
    CHECK(guard.check_message(1, t("после паузы"), t0 + 2400ms) == Reason::None);

    CHECK(guard.check_message(3, t("привет всем"), t0) == Reason::None);
    CHECK(guard.check_message(3, t("Привет всем"), t0 + 5s) == Reason::Repeat);
    CHECK(guard.check_message(3, t("привет всем"), t0 + 20s) == Reason::None);

    CHECK(guard.check_message(4, t("обычное сообщение"), t0) == Reason::None);
    CHECK(guard.check_message(4, t("заходи samp-rp.ru"), t0 + 5s) == Reason::Advertising);
    CHECK(guard.check_message(4, t("заходи samp-rp.ru"), t0 + 10s) == Reason::Advertising);
}

void test_immunity() {
    chatguard::Guard guard;
    const auto now = chatguard::Clock::now();

    guard.set_immune(5, true);
    CHECK(guard.is_immune(5));
    CHECK(guard.check_message(5, t("заходи samp-rp.ru"), now) == Reason::None);
    CHECK(!guard.is_immune(6));

    guard.forget(5);
    CHECK(!guard.is_immune(5));
    CHECK(guard.check_message(5, t("заходи samp-rp.ru"), now) == Reason::Advertising);

    guard.set_immune(7, true);
    guard.reset();
    CHECK(!guard.is_immune(7));
}

}  // namespace

int main() {
    test_encoding();
    test_advertising();
    test_caps();
    test_bad_words();
    test_flood_and_repeat();
    test_immunity();

    if (g_failures == 0) {
        std::printf("All tests passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
