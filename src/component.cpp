#include <sdk.hpp>
#include <Server/Components/Pawn/Impl/pawn_natives.hpp>
#include <Server/Components/Pawn/Impl/pawn_impl.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "cp1251.hpp"
#include "filter.hpp"

using moderator::Reason;

namespace {

struct WarningText {
    const char* key;
    const char* default_text;
};

constexpr WarningText kWarnings[] = {
    {"moderator.message_flood", "Не флудите: подождите пару секунд."},
    {"moderator.message_repeat", "Не повторяйте одно и то же сообщение."},
    {"moderator.message_ads", "Реклама в чате запрещена."},
    {"moderator.message_caps", "Не пишите капсом."},
    {"moderator.message_bad_word", "Следите за языком."},
};

std::size_t warning_index(Reason reason) {
    return static_cast<std::size_t>(reason) - 1;
}

const char* reason_name(Reason reason) {
    switch (reason) {
    case Reason::Flood: return "флуд";
    case Reason::Repeat: return "повтор";
    case Reason::Advertising: return "реклама";
    case Reason::Caps: return "капс";
    case Reason::BadWord: return "запрещённое слово";
    default: return "";
    }
}

std::string_view to_std(StringView view) {
    return {view.data(), view.size()};
}

bool read_bool(IConfig& config, StringView key) {
    const bool* value = config.getBool(key);
    return value && *value;
}

int read_int(IConfig& config, StringView key) {
    const int* value = config.getInt(key);
    return value ? *value : 0;
}

std::vector<std::string> read_strings(IConfig& config, StringView key) {
    std::vector<StringView> views(config.getStringsCount(key));
    config.getStrings(key, Span<StringView>(views.data(), views.size()));
    std::vector<std::string> result;
    for (const StringView view : views) result.emplace_back(view.data(), view.size());
    return result;
}

}  // namespace

class ModeratorComponent final : public IComponent,
                                 public PawnEventHandler,
                                 public PlayerTextEventHandler,
                                 public PlayerConnectEventHandler {
public:
    PROVIDE_UID(0x5A1C7E9D2B40F613);

    static ModeratorComponent* instance() { return instance_; }
    moderator::Guard& guard() { return guard_; }
    const moderator::Guard& guard() const { return guard_; }

    ModeratorComponent() { instance_ = this; }

    ~ModeratorComponent() {
        if (pawn_) pawn_->getEventDispatcher().removeEventHandler(this);
        if (core_) {
            core_->getPlayers().getPlayerTextDispatcher().removeEventHandler(this);
            core_->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
        }
        instance_ = nullptr;
    }

    StringView componentName() const override { return "Moderator"; }
    SemanticVersion componentVersion() const override { return SemanticVersion(0, 2, 0, 0); }

    void provideConfiguration(ILogger&, IEarlyConfig& config, bool defaults) override {
        const auto missing = [&](StringView key) { return defaults || config.getType(key) == ConfigOptionType_None; };
        const auto set_bool = [&](StringView key, bool value) { if (missing(key)) config.setBool(key, value); };
        const auto set_int = [&](StringView key, int value) { if (missing(key)) config.setInt(key, value); };
        const auto set_empty_list = [&](StringView key) { if (missing(key)) config.setStrings(key, Span<const StringView>()); };

        const moderator::Settings d;
        set_bool("moderator.anti_ads", d.anti_ads);
        set_empty_list("moderator.allowed_hosts");
        set_bool("moderator.anti_caps", d.anti_caps);
        set_int("moderator.caps_min_letters", d.caps_min_letters);
        set_int("moderator.caps_percent", d.caps_percent);
        set_bool("moderator.anti_flood", d.anti_flood);
        set_int("moderator.flood_messages", d.flood_messages);
        set_int("moderator.flood_interval_ms", static_cast<int>(d.flood_interval.count()));
        set_int("moderator.repeat_interval_ms", static_cast<int>(d.repeat_interval.count()));
        set_empty_list("moderator.bad_words");
        set_bool("moderator.log_blocked", true);
        for (const WarningText& warning : kWarnings) {
            if (missing(warning.key)) config.setString(warning.key, warning.default_text);
        }
    }

    void onLoad(ICore* core) override {
        core_ = core;
        load_settings(core->getConfig());
        core->getPlayers().getPlayerTextDispatcher().addEventHandler(this, EventPriority_FairlyHigh);
        core->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
        setAmxLookups(core);
    }

    void onInit(IComponentList* components) override {
        pawn_ = components->queryComponent<IPawnComponent>();
        if (!pawn_) {
            core_->logLn(LogLevel::Error, "[Moderator] Pawn component not found, natives are unavailable");
            return;
        }
        setAmxFunctions(pawn_->getAmxFunctions());
        setAmxLookups(components);
        pawn_->getEventDispatcher().addEventHandler(this);
    }

    void onFree(IComponent* component) override {
        if (component != pawn_) return;
        pawn_ = nullptr;
        setAmxFunctions();
        setAmxLookups();
    }

    void free() override { delete this; }
    void reset() override { guard_.reset(); }

    void onAmxLoad(IPawnScript& script) override { pawn_natives::AmxLoad(script.GetAMX()); }
    void onAmxUnload(IPawnScript&) override {}

    void onPlayerConnect(IPlayer& player) override { guard_.forget(player.getID()); }
    void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason) override { guard_.forget(player.getID()); }

    bool onPlayerText(IPlayer& player, StringView message) override {
        const Reason reason = guard_.check_message(player.getID(), to_std(message), moderator::Clock::now());
        if (reason == Reason::None) return true;

        if (log_blocked_) {
            core_->logLnU8(LogLevel::Message, "[Moderator] %s (%d), %s: %s",
                           cp1251::to_utf8(to_std(player.getName())).c_str(), player.getID(), reason_name(reason),
                           cp1251::to_utf8(to_std(message)).c_str());
        }
        const std::string& warning = warnings_[warning_index(reason)];
        if (notify_scripts(player.getID(), reason, message) && !warning.empty()) {
            player.sendClientMessage(Colour::FromRGBA(0xFF6347FF), StringView(warning.data(), warning.size()));
        }
        return false;
    }

private:
    void load_settings(IConfig& config) {
        moderator::Settings settings;
        settings.anti_ads = read_bool(config, "moderator.anti_ads");
        for (const auto& host : read_strings(config, "moderator.allowed_hosts")) {
            settings.allowed_hosts.push_back(cp1251::to_lower(cp1251::from_utf8(host)));
        }
        settings.anti_caps = read_bool(config, "moderator.anti_caps");
        settings.caps_min_letters = read_int(config, "moderator.caps_min_letters");
        settings.caps_percent = read_int(config, "moderator.caps_percent");
        settings.anti_flood = read_bool(config, "moderator.anti_flood");
        settings.flood_messages = read_int(config, "moderator.flood_messages");
        settings.flood_interval = std::chrono::milliseconds(read_int(config, "moderator.flood_interval_ms"));
        settings.repeat_interval = std::chrono::milliseconds(read_int(config, "moderator.repeat_interval_ms"));
        for (const auto& word : read_strings(config, "moderator.bad_words")) {
            settings.bad_words.push_back(cp1251::to_lower(cp1251::from_utf8(word)));
        }
        guard_.configure(std::move(settings));

        log_blocked_ = read_bool(config, "moderator.log_blocked");
        warnings_.clear();
        for (const WarningText& warning : kWarnings) {
            warnings_.push_back(cp1251::from_utf8(to_std(config.getString(warning.key))));
        }
    }

    // Returns false if any script asked to suppress the default warning.
    bool notify_scripts(int playerid, Reason reason, StringView text) {
        if (!pawn_) return true;
        bool show_warning = true;
        const auto call = [&](IPawnScript* script) {
            if (script && script->Call("OnModeratorBlock", DefaultReturnValue_True, playerid, static_cast<int>(reason),
                                       text) == 0) {
                show_warning = false;
            }
        };
        for (IPawnScript* script : pawn_->sideScripts()) call(script);
        call(pawn_->mainScript());
        return show_warning;
    }

    static inline ModeratorComponent* instance_ = nullptr;

    ICore* core_ = nullptr;
    IPawnComponent* pawn_ = nullptr;
    moderator::Guard guard_;
    std::vector<std::string> warnings_;
    bool log_blocked_ = true;
};

SCRIPT_API(Moderator_CheckText, int(std::string const& text)) {
    const auto* component = ModeratorComponent::instance();
    return component ? static_cast<int>(moderator::check_text(text, component->guard().settings())) : 0;
}

SCRIPT_API(Moderator_SetImmune, bool(IPlayer& player, bool immune)) {
    auto* component = ModeratorComponent::instance();
    if (!component) return false;
    component->guard().set_immune(player.getID(), immune);
    return true;
}

SCRIPT_API(Moderator_IsImmune, bool(IPlayer& player)) {
    const auto* component = ModeratorComponent::instance();
    return component && component->guard().is_immune(player.getID());
}

COMPONENT_ENTRY_POINT() {
    return new ModeratorComponent();
}
