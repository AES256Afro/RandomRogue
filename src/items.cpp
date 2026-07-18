#include "items.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void ItemDb::loadItemsJsonText(const char* jsonText) {
    if (!jsonText) return;
    json j = json::parse(jsonText, nullptr, false);
    if (j.is_discarded() || !j.is_array()) return;
    for (auto& e : j) {
        ItemTemplate t;
        t.id = e.value("id", "");
        t.name = e.value("name", "");
        t.type = e.value("type", "misc");
        t.value = e.value("value", 0);
        t.passive = e.value("passive", "");
        if (e.contains("use") && e["use"].is_array())
            for (auto& u : e["use"])
                if (u.is_string()) t.useEffects.push_back(u.get<std::string>());
        if (!t.id.empty()) {
            templates_[t.id] = t;
            order_.push_back(t.id);
        }
    }
}

void ItemDb::generateFamilies() {
    struct Mat { const char* name; int tier; };       // value & bonus scale
    struct Kind { const char* name; const char* stat; };
    static const Mat kMats[4] = {{"Rusted", 0}, {"Iron", 1}, {"Steel", 2},
                                 {"Sky-Metal", 3}};
    static const Kind kWeapons[4] = {{"Sword", "str"}, {"Axe", "str"},
                                     {"Spear", "dex"}, {"Maul", "str"}};
    static const char* kArmors[3] = {"Jerkin", "Hauberk", "Cuirass"};

    for (auto& m : kMats) {
        for (auto& k : kWeapons) {
            ItemTemplate t;
            t.id = std::string("gen_") + m.name + "_" + k.name;
            for (char& c : t.id) c = (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
            t.name = std::string(m.name) + " " + k.name;
            t.type = "weapon";
            t.value = 3 + m.tier * 6;
            int bonus = 1 + (m.tier + 1) / 2; // 1,2,2,3
            t.passive = std::string("check ") + k.stat + " +" + std::to_string(bonus);
            templates_[t.id] = t;
            order_.push_back(t.id);
        }
        for (int a = 0; a < 3; a++) {
            ItemTemplate t;
            t.id = std::string("gen_") + m.name + "_" + kArmors[a];
            for (char& c : t.id) c = (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
            t.name = std::string(m.name) + " " + kArmors[a];
            t.type = "armor";
            t.value = 4 + m.tier * 6 + a * 2;
            t.passive = "armor " + std::to_string(1 + (m.tier + a) / 3);
            templates_[t.id] = t;
            order_.push_back(t.id);
        }
    }
}

void ItemDb::loadQuirksJsonText(const char* jsonText) {
    if (!jsonText) return;
    json j = json::parse(jsonText, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return;
    auto load = [&](const char* key, std::vector<std::string>& out) {
        if (!j.contains(key) || !j[key].is_array()) return;
        for (auto& v : j[key])
            if (v.is_string()) out.push_back(v.get<std::string>());
    };
    load("texts", quirkTexts_);
    load("good", goodPassives_);
    load("bad", badPassives_);
    if (j.contains("paired") && j["paired"].is_array())
        for (auto& p : j["paired"])
            if (p.is_object())
                quirkPairs_.emplace_back(p.value("text", ""), p.value("passive", ""));
}

ItemInstance ItemDb::make(const std::string& id, Rng& rng) const {
    ItemInstance item;
    auto it = templates_.find(id);
    if (it == templates_.end()) {
        item.templateId = id;
        item.name = id;
        return item;
    }
    const ItemTemplate& t = it->second;
    item.templateId = t.id;
    item.name = t.name;
    item.type = t.type;
    item.value = t.value;
    item.useEffects = t.useEffects;
    item.passive = t.passive;
    return item;
}

ItemInstance ItemDb::loot(Rng& rng, const std::string& tier) const {
    std::vector<std::string> pool;
    for (auto& id : order_) {
        const ItemTemplate& t = templates_.at(id);
        if (t.type == "book") continue; // books come from bookshelves, not grab-bags
        bool fine = t.value > 8;
        if ((tier == "fine") == fine) pool.push_back(id);
    }
    if (pool.empty()) pool = order_;
    return make(rng.pick(pool), rng);
}
