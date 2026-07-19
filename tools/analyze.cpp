#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

static std::string opening(const std::string& text) {
    std::string normalized;
    int words = 0;
    bool inWord = false;
    for (unsigned char c : text) {
        bool letter = std::isalnum(c) != 0;
        if (letter) {
            normalized += (char)std::tolower(c);
            inWord = true;
        } else if (inWord) {
            normalized += ' ';
            inWord = false;
            if (++words >= 7) break;
        }
    }
    return normalized;
}

static std::string structure(const json& event) {
    std::string out = std::to_string(event.value("choices", json::array()).size());
    for (auto& choice : event.value("choices", json::array())) {
        out += choice.contains("check") ? ":check" : ":plain";
        if (choice.contains("requires")) out += "+req";
        std::set<std::string> verbs;
        auto gather = [&](const json& outcomes) {
            if (!outcomes.is_array()) return;
            for (auto& outcome : outcomes)
                for (auto& effect : outcome.value("effects", json::array())) {
                    std::istringstream words(effect.get<std::string>());
                    std::string verb;
                    words >> verb;
                    verbs.insert(verb);
                }
        };
        gather(choice.value("outcomes", json::array()));
        gather(choice.value("success", json::array()));
        gather(choice.value("fail", json::array()));
        for (auto& verb : verbs) out += "+" + verb;
    }
    return out;
}

int main(int argc, char** argv) {
    std::string assets = argc > 1 ? argv[1] : "assets";
    json targets = json::parse(readFile(assets + "/data/scenario_targets.json"),
                               nullptr, false);
    json manifest = json::parse(readFile(assets + "/data/events/manifest.json"), nullptr, false);
    if (manifest.is_discarded() || !manifest.contains("files")) return 1;
    std::vector<json> events;
    for (auto& file : manifest["files"]) {
        json part = json::parse(readFile(assets + "/data/events/" + file.get<std::string>()),
                                nullptr, false);
        if (!part.is_array()) return 1;
        for (auto& event : part) events.push_back(event);
    }

    std::map<std::string, int> locations, tags, voiceCadence, approaches, families, fingerprints;
    std::map<std::string, int> primaryDecks, primaryThemes;
    std::map<std::string, std::vector<std::string>> openings, structures;
    int gated = 0, slotted = 0, delayed = 0, relationship = 0;
    int remastered = 0, fullyThemed = 0;
    for (auto& event : events) {
        std::string id = event.value("id", "");
        if (event.value("remaster", "") == "v17") remastered++;
        if (!event.value("theme", "").empty() &&
            !event.value("primary", "").empty()) fullyThemed++;
        std::string primary = event.value("primary", "");
        if (primary.empty()) {
            json eventLocations = event.value("locations", json::array());
            primary = eventLocations.empty() ? "special"
                                             : eventLocations.front().get<std::string>();
        }
        primaryDecks[primary]++;
        for (auto& location : event.value("locations", json::array()))
            locations[location.get<std::string>()]++;
        std::string primaryTheme = event.value("theme", "");
        for (auto& tag : event.value("tags", json::array())) {
            std::string tagName = tag.get<std::string>();
            if (tagName.rfind("voice_", 0) == 0) voiceCadence[tagName]++;
            else tags[tagName]++;
            if (primaryTheme.empty() && !targets.is_discarded() &&
                targets.contains("themes") && targets["themes"].contains(tagName))
                primaryTheme = tagName;
        }
        if (!primaryTheme.empty()) primaryThemes[primaryTheme]++;
        std::string family = event.value("family", "");
        if (!family.empty()) families[family]++;
        for (auto& value : event.value("fingerprints", json::array()))
            if (value.is_string()) fingerprints[value.get<std::string>()]++;
        if (event.contains("when")) gated++;
        if (event.contains("slots")) slotted++;
        openings[opening(event.value("text", ""))].push_back(id);
        structures[structure(event)].push_back(id);
        for (auto& choice : event.value("choices", json::array())) {
            std::string approach = choice.value("approach", "");
            if (!approach.empty()) approaches[approach]++;
            std::string dumped = choice.dump();
            if (dumped.find("schedule ") != std::string::npos) delayed++;
            if (dumped.find("npc_rel ") != std::string::npos) relationship++;
        }
    }

    std::printf("authoring: %d events, %d explicit families, %d semantic tags, %d approaches\n",
                (int)events.size(), (int)families.size(), (int)tags.size(),
                (int)approaches.size());
    std::printf("narrative remaster: %d legacy cards rewritten, %d/%d cards classified\n",
                remastered, fullyThemed, (int)events.size());
    std::printf("connections: %d gated, %d slotted, %d delayed choices, %d relationship choices\n",
                gated, slotted, delayed, relationship);
    int eightBeatFamilies = 0;
    for (const auto& pair : families) if (pair.second == 8) eightBeatFamilies++;
    std::printf("living politics: %d eight-beat families, %d structural fingerprints\n",
                eightBeatFamilies, (int)fingerprints.size());
    std::printf("location coverage:");
    for (auto& pair : locations) std::printf(" %s=%d", pair.first.c_str(), pair.second);
    std::printf("\nsemantic coverage:");
    for (auto& pair : tags) std::printf(" %s=%d", pair.first.c_str(), pair.second);
    std::printf("\n");
    std::printf("voice cadence:");
    for (auto& pair : voiceCadence)
        std::printf(" %s=%d", pair.first.c_str(), pair.second);
    std::printf("\n");

    if (!targets.is_discarded() && targets.is_object()) {
        int targetTotal = targets.value("total", 1000);
        std::printf("thousand plan: %d/%d authored, %d remaining\n",
                    (int)events.size(), targetTotal,
                    std::max(0, targetTotal - (int)events.size()));
        std::printf("primary deck gaps:");
        for (auto& [name, wanted] : targets["decks"].items()) {
            int have = primaryDecks[name];
            std::printf(" %s=%d", name.c_str(), std::max(0, wanted.get<int>() - have));
        }
        std::printf("\nprimary theme gaps:");
        for (auto& [name, wanted] : targets["themes"].items()) {
            int have = primaryThemes[name];
            std::printf(" %s=%d", name.c_str(), std::max(0, wanted.get<int>() - have));
        }
        std::printf("\n");
    }

    int repeatedOpenings = 0;
    for (auto& pair : openings)
        if (!pair.first.empty() && pair.second.size() > 1) repeatedOpenings++;
    size_t largestStructure = 0;
    std::string largestKey;
    for (auto& pair : structures)
        if (pair.second.size() > largestStructure) {
            largestStructure = pair.second.size();
            largestKey = pair.first;
        }
    std::printf("similarity watch: %d repeated seven-word openings; largest choice skeleton=%d events\n",
                repeatedOpenings, (int)largestStructure);
    if (!largestKey.empty()) std::printf("largest skeleton: %s\n", largestKey.c_str());
    return 0;
}
