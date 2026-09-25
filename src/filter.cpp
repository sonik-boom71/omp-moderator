#include "filter.hpp"

#include <algorithm>

#include "cp1251.hpp"

namespace moderator {
namespace {

bool is_digit(unsigned char c) {
    return c >= '0' && c <= '9';
}

bool is_space(unsigned char c) {
    return c == ' ' || c == '\t';
}

bool is_host_char(unsigned char c) {
    return is_digit(c) || c == '-' || cp1251::is_letter(c);
}

unsigned char at(std::string_view s, std::size_t i) {
    return static_cast<unsigned char>(s[i]);
}

void skip_spaces(std::string_view s, std::size_t& i) {
    while (i < s.size() && is_space(at(s, i))) ++i;
}

bool read_octet(std::string_view s, std::size_t& i, std::string& out) {
    const std::size_t start = i;
    int value = 0;
    while (i < s.size() && i - start < 3 && is_digit(at(s, i))) value = value * 10 + (s[i++] - '0');
    if (i == start || (i < s.size() && is_digit(at(s, i))) || value > 255) return false;
    out.append(s.substr(start, i - start));
    return true;
}

// Finds every "a.b.c.d", also when spaced out as "a . b . c . d" to dodge filters.
std::vector<std::string> find_ips(std::string_view s) {
    std::vector<std::string> ips;
    for (std::size_t start = 0; start < s.size(); ++start) {
        if (!is_digit(at(s, start)) || (start > 0 && is_digit(at(s, start - 1)))) continue;
        std::size_t i = start;
        std::string ip;
        bool ok = read_octet(s, i, ip);
        for (int part = 0; ok && part < 3; ++part) {
            skip_spaces(s, i);
            ok = i < s.size() && s[i] == '.';
            if (!ok) break;
            ++i;
            skip_spaces(s, i);
            ip += '.';
            ok = read_octet(s, i, ip);
        }
        if (!ok) continue;
        ips.push_back(std::move(ip));
        start = i - 1;
    }
    return ips;
}

bool is_known_tld(const std::string& tld) {
    static const std::vector<std::string> kTlds{
        "ru",  "com",   "net",  "org",   "su",    "me",  "pro", "xyz", "online", "site", "club", "fun",
        "info", "by",   "kz",   "ua",    "io",    "gg",  "top", "store", "space", "link", "tk",  "cc",
        cp1251::from_utf8("рф"),
    };
    return std::find(kTlds.begin(), kTlds.end(), tld) != kTlds.end();
}

// Collects "label.tld" for every known TLD, also when spaced out as "site . ru".
std::vector<std::string> find_hosts(std::string_view s) {
    std::vector<std::string> hosts;
    for (std::size_t dot = 0; dot < s.size(); ++dot) {
        if (s[dot] != '.') continue;

        std::size_t label_end = dot;
        while (label_end > 0 && is_space(at(s, label_end - 1))) --label_end;
        std::size_t label_begin = label_end;
        while (label_begin > 0 && is_host_char(at(s, label_begin - 1))) --label_begin;
        if (label_begin == label_end) continue;

        std::size_t tld_begin = dot + 1;
        skip_spaces(s, tld_begin);
        std::size_t tld_end = tld_begin;
        while (tld_end < s.size() && cp1251::is_letter(at(s, tld_end))) ++tld_end;
        if (tld_end == tld_begin || (tld_end < s.size() && is_digit(at(s, tld_end)))) continue;

        const std::string tld = cp1251::to_lower(s.substr(tld_begin, tld_end - tld_begin));
        if (is_known_tld(tld)) hosts.push_back(cp1251::to_lower(s.substr(label_begin, label_end - label_begin)) + "." + tld);
    }
    return hosts;
}

bool is_allowed(const std::string& host, const Settings& settings) {
    return std::find(settings.allowed_hosts.begin(), settings.allowed_hosts.end(), host) !=
           settings.allowed_hosts.end();
}

bool has_advertising(std::string_view text, const Settings& settings) {
    const auto foreign = [&](const std::vector<std::string>& found) {
        return std::any_of(found.begin(), found.end(), [&](const std::string& host) { return !is_allowed(host, settings); });
    };
    return foreign(find_ips(text)) || foreign(find_hosts(text));
}

// Latin letters and digits that look like Cyrillic ones, a common way to dodge word filters.
unsigned char unmask(unsigned char c) {
    static const std::string kFrom = "acekmhoptxy036@" + cp1251::from_utf8("ё");
    static const std::string kTo = cp1251::from_utf8("асекмнортхуозбае");
    const auto pos = kFrom.find(static_cast<char>(c));
    return pos == std::string::npos ? c : static_cast<unsigned char>(kTo[pos]);
}

bool matches_word(const std::string& word, const std::vector<std::string>& bad_words) {
    return std::any_of(bad_words.begin(), bad_words.end(), [&](const std::string& bad) {
        if (bad.empty()) return false;
        if (bad[0] == '*') return bad.size() > 1 && word.find(std::string_view(bad).substr(1)) != std::string::npos;
        return word.compare(0, bad.size(), bad) == 0;
    });
}

bool has_bad_word(std::string_view text, const std::vector<std::string>& bad_words) {
    if (bad_words.empty()) return false;
    std::size_t i = 0;
    while (i < text.size()) {
        skip_spaces(text, i);
        // Punctuation inside a word is dropped, so "б.л.я" is checked as "бля".
        std::string plain;
        std::string unmasked;
        for (; i < text.size() && !is_space(at(text, i)); ++i) {
            const unsigned char c = cp1251::to_lower(at(text, i));
            if (cp1251::is_letter(c)) plain += static_cast<char>(c);
            const unsigned char u = unmask(c);
            if (cp1251::is_letter(u)) unmasked += static_cast<char>(u);
        }
        if (matches_word(plain, bad_words) || matches_word(unmasked, bad_words)) return true;
    }
    return false;
}

bool is_caps(std::string_view text, const Settings& settings) {
    int letters = 0;
    int upper = 0;
    for (char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (!cp1251::is_letter(byte)) continue;
        ++letters;
        if (cp1251::is_upper(byte)) ++upper;
    }
    return letters >= settings.caps_min_letters && upper * 100 >= letters * settings.caps_percent;
}

}  // namespace

Reason check_text(std::string_view text, const Settings& settings) {
    if (settings.anti_ads && has_advertising(text, settings)) return Reason::Advertising;
    if (has_bad_word(text, settings.bad_words)) return Reason::BadWord;
    if (settings.anti_caps && is_caps(text, settings)) return Reason::Caps;
    return Reason::None;
}

Reason Guard::check_message(int player, std::string_view text, Clock::time_point now) {
    PlayerState& state = players_[player];
    if (state.immune) return Reason::None;

    if (settings_.anti_flood) {
        while (!state.recent.empty() && now - state.recent.front() >= settings_.flood_interval) state.recent.pop_front();
        // Blocked attempts count too, so a spammer stays blocked until they pause.
        const bool flooding = static_cast<int>(state.recent.size()) >= settings_.flood_messages;
        state.recent.push_back(now);
        if (flooding) return Reason::Flood;
    }

    std::string normalized = cp1251::to_lower(text);
    if (settings_.repeat_interval.count() > 0 && normalized == state.last_text &&
        now - state.last_time < settings_.repeat_interval) {
        return Reason::Repeat;
    }

    const Reason reason = check_text(text, settings_);
    if (reason == Reason::None) {
        state.last_text = std::move(normalized);
        state.last_time = now;
    }
    return reason;
}

bool Guard::is_immune(int player) const {
    const auto it = players_.find(player);
    return it != players_.end() && it->second.immune;
}

}  // namespace moderator
