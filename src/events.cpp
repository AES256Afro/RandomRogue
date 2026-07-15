#include "events.h"
#include <nlohmann/json.hpp>
#include <cstdio>

using json = nlohmann::json;

bool Requirement::met(const Character& c) const {
    if (!stat.empty() && c.stats[statFromName(stat)] < gte) return false;
    if (moneyGte > 0 && c.money < moneyGte) return false;
    if (!item.empty() && !c.hasItem(item)) return false;
    return true;
}

std::string Requirement::label() const {
    if (!stat.empty()) {
        std::string s = stat;
        for (char& ch : s) ch = (char)((ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch);
        return "[" + s + " " + std::to_string(gte) + "] ";
    }
    if (moneyGte > 0) return "[" + std::to_string(moneyGte) + "g] ";
    if (!item.empty()) return "[needs " + item + "] ";
    return "";
}

namespace {

std::vector<Outcome> parseOutcomes(const json& arr) {
    std::vector<Outcome> out;
    if (!arr.is_array()) return out;
    for (auto& o : arr) {
        Outcome oc;
        oc.weight = o.value("weight", 1);
        oc.text = o.value("text", "");
        if (o.contains("effects") && o["effects"].is_array())
            for (auto& e : o["effects"])
                if (e.is_string()) oc.effects.push_back(e.get<std::string>());
        out.push_back(std::move(oc));
    }
    return out;
}

const Outcome* pickWeighted(const std::vector<Outcome>& outcomes, Rng& rng) {
    if (outcomes.empty()) return nullptr;
    int total = 0;
    for (auto& o : outcomes) total += o.weight;
    int roll = rng.range(1, total > 0 ? total : 1);
    for (auto& o : outcomes) {
        roll -= o.weight;
        if (roll <= 0) return &o;
    }
    return &outcomes.back();
}

} // namespace

void EventDeck::loadJsonText(const char* jsonText) {
    if (!jsonText) return;
    json j = json::parse(jsonText, nullptr, false);
    if (j.is_discarded() || !j.is_array()) return;
    for (auto& e : j) {
        Event ev;
        ev.id = e.value("id", "");
        ev.weight = e.value("weight", 10);
        ev.text = e.value("text", "");
        if (e.contains("locations") && e["locations"].is_array())
            for (auto& l : e["locations"])
                if (l.is_string()) ev.locations.push_back(l.get<std::string>());
        if (e.contains("slots") && e["slots"].is_object())
            for (auto& [name, q] : e["slots"].items())
                if (q.is_string()) ev.slots.emplace_back(name, q.get<std::string>());
        if (e.contains("choices") && e["choices"].is_array()) {
            for (auto& c : e["choices"]) {
                Choice ch;
                ch.text = c.value("text", "");
                if (c.contains("requires") && c["requires"].is_object()) {
                    const json& r = c["requires"];
                    ch.requires_.stat = r.value("stat", "");
                    ch.requires_.gte = r.value("gte", 0);
                    ch.requires_.moneyGte = r.value("money", 0);
                    ch.requires_.item = r.value("item", "");
                }
                if (c.contains("check") && c["check"].is_object()) {
                    ch.check.stat = c["check"].value("stat", "");
                    ch.check.dc = c["check"].value("dc", 10);
                    ch.successOutcomes = parseOutcomes(c.value("success", json::array()));
                    ch.failOutcomes = parseOutcomes(c.value("fail", json::array()));
                } else {
                    ch.outcomes = parseOutcomes(c.value("outcomes", json::array()));
                }
                ev.choices.push_back(std::move(ch));
            }
        }
        if (!ev.id.empty() && !ev.choices.empty())
            events_.push_back(std::move(ev));
    }
}

const Event* EventDeck::draw(Rng& rng, const std::string& location) {
    for (int pass = 0; pass < 2; pass++) {
        std::vector<const Event*> pool;
        int total = 0;
        for (auto& e : events_) {
            if (used_.count(e.id)) continue;
            for (auto& l : e.locations) {
                if (l == location) {
                    pool.push_back(&e);
                    total += e.weight;
                    break;
                }
            }
        }
        if (pool.empty()) {
            // Exhausted this location's pool: recycle only its events.
            std::set<std::string> keep;
            for (auto& id : used_) {
                bool isHere = false;
                for (auto& e : events_)
                    if (e.id == id)
                        for (auto& l : e.locations)
                            if (l == location) isHere = true;
                if (!isHere) keep.insert(id);
            }
            used_ = keep;
            continue;
        }
        int roll = rng.range(1, total > 0 ? total : 1);
        for (auto* e : pool) {
            roll -= e->weight;
            if (roll <= 0) {
                used_.insert(e->id);
                return e;
            }
        }
    }
    return nullptr;
}

ResolvedOutcome EventDeck::resolve(const Choice& choice, const Character& c, Rng& rng) {
    ResolvedOutcome r;
    const Outcome* picked = nullptr;
    if (!choice.check.stat.empty()) {
        int stat = statFromName(choice.check.stat);
        int mod = Character::mod(c.stats[stat]) + c.checkBonus(stat);
        int die = rng.d(20);
        int totalRoll = die + mod;
        bool success = totalRoll >= choice.check.dc;
        char buf[96];
        std::string statUp = choice.check.stat;
        for (char& ch : statUp) ch = (char)((ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch);
        std::snprintf(buf, sizeof(buf), "%s check: d20(%d)%+d = %d vs DC %d -- %s",
                      statUp.c_str(), die, mod, totalRoll, choice.check.dc,
                      success ? "success!" : "failure");
        r.rollText = buf;
        picked = pickWeighted(success ? choice.successOutcomes : choice.failOutcomes, rng);
    } else {
        picked = pickWeighted(choice.outcomes, rng);
    }
    if (picked) {
        r.text = picked->text;
        r.effects = picked->effects;
    }
    return r;
}
