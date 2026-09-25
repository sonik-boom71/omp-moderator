#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// Everything here works on cp1251 text, as sent by Russian SA-MP clients.
namespace moderator {

// Values are shared with Pawn: keep in sync with pawn/moderator.inc.
enum class Reason {
    None = 0,
    Flood = 1,
    Repeat = 2,
    Advertising = 3,
    Caps = 4,
    BadWord = 5,
};

struct Settings {
    bool anti_ads = true;
    std::vector<std::string> allowed_hosts;  // lowercase, e.g. "mysite.ru" or "1.2.3.4"
    bool anti_caps = true;
    int caps_min_letters = 6;
    int caps_percent = 70;
    bool anti_flood = true;
    int flood_messages = 3;
    std::chrono::milliseconds flood_interval{2000};
    std::chrono::milliseconds repeat_interval{10000};  // zero disables the repeat check
    std::vector<std::string> bad_words;  // lowercase; a leading '*' matches anywhere in a word, not just the start
};

Reason check_text(std::string_view text, const Settings& settings);

using Clock = std::chrono::steady_clock;

class Guard {
public:
    void configure(Settings settings) { settings_ = std::move(settings); }
    const Settings& settings() const { return settings_; }

    Reason check_message(int player, std::string_view text, Clock::time_point now);
    void set_immune(int player, bool immune) { players_[player].immune = immune; }
    bool is_immune(int player) const;
    void forget(int player) { players_.erase(player); }
    void reset() { players_.clear(); }

private:
    struct PlayerState {
        bool immune = false;
        std::deque<Clock::time_point> recent;
        std::string last_text;
        Clock::time_point last_time{};
    };

    Settings settings_;
    std::unordered_map<int, PlayerState> players_;
};

}  // namespace moderator
