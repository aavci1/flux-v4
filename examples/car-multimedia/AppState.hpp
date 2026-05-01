#pragma once

#include "Common.hpp"

namespace car {

enum class Screen { Home, Map, Music, Phone, Climate, Vehicle, Settings };
enum class TireStatus { Ok, Warn };
enum class FlowMode { Face, Feet, Window };

struct NavInfo {
    std::string destination = "Maximilianstraße 12";
    std::string address = "München, Bayern · 80539";
    std::string eta = "17:58";
    std::string distance = "14.2 km";
    std::string via = "A8 · B2R";
};

struct MusicTrack {
    std::string title = "Strobe";
    std::string artist = "deadmau5";
    std::string album = "For Lack of a Better Name";
    std::string source = "Spotify / Lossless";
    float progress = 0.36f;
    bool playing = true;
};

struct QueueItem {
    std::string title;
    std::string artist;
    std::string duration;
};

struct VehicleStats {
    int battery = 72;
    int range = 384;
    float trip = 24.6f;
    float efficiency = 17.4f;
};

struct TireSet {
    struct Tire {
        float psi;
        TireStatus status;
    };
    Tire fl{2.4f, TireStatus::Ok};
    Tire fr{2.4f, TireStatus::Ok};
    Tire rl{2.2f, TireStatus::Warn};
    Tire rr{2.5f, TireStatus::Ok};
};

struct Recent {
    std::string initials;
    std::string name;
    std::string when;
    bool outgoing;
    bool missed;
};

struct AssistFlags {
    bool cruise = true;
    bool lane = true;
    bool park = true;
    bool night = false;
};

struct ClimateState {
    float tempL = 21.5f;
    float tempR = 22.0f;
    int fan = 4;
    bool ac = true;
    bool auto_ = true;
    bool heated = false;
    bool defrost = false;
    bool recirc = false;
    std::vector<FlowMode> flow{FlowMode::Face, FlowMode::Feet};

    bool operator==(ClimateState const &) const = default;
};

struct VehicleControls {
    std::string mode = "Comfort";
};

struct Profile {
    std::string name = "Alex";
    std::string initials = "AM";
};

struct State {
    Screen active = Screen::Home;
    NavInfo nav;
    MusicTrack music;
    std::vector<QueueItem> queue = makeQueue();
    VehicleStats vehicleStats;
    TireSet tires;
    std::vector<Recent> recents = makeRecents();
    AssistFlags assist;
    ClimateState climate;
    VehicleControls vehicleControls;
    Profile profile;
    std::string clock = "17:42";
    int outsideTemp = 9;
    std::string locationText = "München · 9 °C · Clear";
    bool driving = true;

    static std::vector<QueueItem> makeQueue() {
        return {
            {"Strobe", "deadmau5", "10:32"},
            {"Ghosts 'n' Stuff", "deadmau5, Rob Swire", "4:17"},
            {"Mira", "Bonobo", "5:12"},
            {"Linger", "Tycho", "5:48"},
            {"Cirrus", "Bonobo", "5:43"},
            {"A Walk", "Tycho", "5:22"},
            {"Innerbloom", "RUFUS DU SOL", "9:38"},
        };
    }

    static std::vector<Recent> makeRecents() {
        return {
            {"SH", "Sophie Hartmann", "Today, 17:14", false, false},
            {"MR", "Markus Reiter", "Today, 14:02", true, false},
            {"DK", "Dr. Klein", "Today, 09:48", false, true},
            {"AS", "Anna Schmidt", "Yesterday", true, false},
            {"LB", "Lukas Berger", "Yesterday", false, false},
            {"JM", "Julia Maier", "Tuesday", true, false},
        };
    }
};

inline std::optional<Screen> screenFromName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (name == "home") return Screen::Home;
    if (name == "map" || name == "navigation") return Screen::Map;
    if (name == "music" || name == "media") return Screen::Music;
    if (name == "phone") return Screen::Phone;
    if (name == "climate") return Screen::Climate;
    if (name == "vehicle" || name == "car") return Screen::Vehicle;
    if (name == "settings") return Screen::Settings;
    return std::nullopt;
}

inline State makeInitialState() {
    State state;
    if (auto const* screen = std::getenv("CAR_MULTIMEDIA_SCREEN")) {
        if (auto parsed = screenFromName(screen)) {
            state.active = *parsed;
        }
    }
    return state;
}

} // namespace car
