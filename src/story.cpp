#include "story.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

template <typename T>
int recentCount(const std::deque<T>& values, const T& wanted, int limit) {
    int seen = 0;
    int checked = 0;
    for (auto it = values.rbegin(); it != values.rend() && checked < limit;
         ++it, ++checked)
        if (*it == wanted) seen++;
    return seen;
}

template <typename T>
void pushBounded(std::deque<T>& values, const T& value, size_t limit) {
    values.push_back(value);
    while (values.size() > limit) values.pop_front();
}

bool isLocationTag(const std::string& tag) {
    static const std::set<std::string> locations = {
        "city", "tavern", "dungeon", "dungeon_finale", "cave", "forest",
        "road", "crash", "swamp", "mountains", "coast", "sea"
    };
    return locations.count(tag) > 0;
}

} // namespace

void StoryDirector::reset() {
    recentIds_.clear();
    recentTags_.clear();
    recentFamilies_.clear();
    recentApproaches_.clear();
    tagCounts_.clear();
}

std::vector<std::string> StoryDirector::tagsFor(const Event& event) {
    std::set<std::string> unique(event.tags.begin(), event.tags.end());
    for (const std::string& location : event.locations) unique.insert(location);

    for (const auto& slot : event.slots) {
        const std::string& q = slot.second;
        if (contains(q, "figure") || q == "rival" || q == "wronged_figure" ||
            q == "remembered_figure") {
            unique.insert("npc");
            unique.insert("relationship");
        }
        if (contains(q, "artifact")) unique.insert("artifact");
        if (contains(q, "chronicle")) unique.insert("history");
        if (contains(q, "beast")) unique.insert("beast");
        if (contains(q, "god")) unique.insert("divine");
        if (contains(q, "ghost") || contains(q, "stranger")) unique.insert("legacy");
        if (contains(q, "mystery")) unique.insert("mystery");
    }

    for (const std::string& when : event.when) {
        if (contains(when, "war_here")) unique.insert("war");
        if (contains(when, "plague_here")) unique.insert("plague");
        if (contains(when, "raining") || contains(when, "snowing") ||
            contains(when, "season")) unique.insert("weather");
        if (contains(when, "npc") || contains(when, "npc_rel"))
            unique.insert("relationship");
        if (contains(when, "clues") || contains(when, "mystery"))
            unique.insert("mystery");
    }

    auto inspectEffects = [&](const std::vector<Outcome>& outcomes) {
        for (const Outcome& outcome : outcomes) {
            for (const std::string& fx : outcome.effects) {
                if (fx.rfind("hp -", 0) == 0 || fx.rfind("die ", 0) == 0)
                    unique.insert("danger");
                if (fx.rfind("money ", 0) == 0 || fx.rfind("shop", 0) == 0)
                    unique.insert("economy");
                if (fx.rfind("rep ", 0) == 0 || fx.rfind("npc_", 0) == 0)
                    unique.insert("social");
                if (fx.rfind("contract", 0) == 0) unique.insert("quest");
                if (fx.rfind("schedule ", 0) == 0) unique.insert("consequence");
                if (fx.rfind("mystery_", 0) == 0) unique.insert("mystery");
                if (fx.rfind("region ", 0) == 0) unique.insert("regional");
            }
        }
    };
    for (const Choice& choice : event.choices) {
        inspectEffects(choice.outcomes);
        inspectEffects(choice.successOutcomes);
        inspectEffects(choice.failOutcomes);
    }

    std::string text = event.text;
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    if (contains(text, "laugh") || contains(text, "absurd") || contains(text, "committee") ||
        contains(text, "paperwork")) unique.insert("absurd");
    if (contains(text, "murder") || contains(text, "corpse") || contains(text, "funeral"))
        unique.insert("tragic");
    if (contains(text, "fight") || contains(text, "blade") || contains(text, "attack"))
        unique.insert("combat");

    return std::vector<std::string>(unique.begin(), unique.end());
}

std::string StoryDirector::familyFor(const Event& event) {
    if (!event.family.empty()) return event.family;
    for (const std::string& tag : tagsFor(event))
        if (!isLocationTag(tag)) return tag;
    return event.locations.empty() ? "unplaced" : event.locations.front();
}

int StoryDirector::score(const Event& event, const StoryContext& ctx) const {
    int result = 100;
    if (recentCount(recentIds_, event.id, 20) > 0) result = 8;

    const std::string family = familyFor(event);
    int familyHeat = recentCount(recentFamilies_, family, 8);
    if (familyHeat > 0) result = result * (familyHeat >= 2 ? 35 : 62) / 100;

    const std::vector<std::string> tags = tagsFor(event);
    int heat = 0;
    int novelty = 0;
    for (const std::string& tag : tags) {
        if (isLocationTag(tag)) continue;
        heat += recentCount(recentTags_, tag, 14);
        auto count = tagCounts_.find(tag);
        if (count == tagCounts_.end()) novelty += 16;
        else if (count->second <= 2) novelty += 7;

        if (tag == "relationship" && ctx.knownNpc) result += 28;
        if (tag == "quest" && ctx.quest) result += 24;
        if (tag == "artifact" && ctx.artifact) result += 18;
        if (tag == "war" && ctx.war) result += 35;
        if (tag == "plague" && ctx.plague) result += 35;
        if (tag == "weather" && ctx.weather != "clear") result += 24;
        if (tag == "mystery" && ctx.mystery) result += 40;
        if (tag == "companion" && ctx.companion) result += 20;
    }
    result += std::min(45, novelty);
    result -= std::min(55, heat * 7);

    if (!recentTags_.empty()) {
        const std::string& last = recentTags_.back();
        if (last == "danger" && std::find(tags.begin(), tags.end(), "absurd") != tags.end())
            result += 24;
        if (last == "tragic" && std::find(tags.begin(), tags.end(), "tragic") != tags.end())
            result -= 25;
    }

    bool freshApproach = false;
    for (const Choice& choice : event.choices)
        if (!choice.approach.empty() &&
            recentCount(recentApproaches_, choice.approach, 6) == 0)
            freshApproach = true;
    if (freshApproach) result += 12;

    if (result < 5) result = 5;
    if (result > 240) result = 240;
    return result;
}

std::vector<std::string> StoryDirector::explain(const Event& event,
                                                const StoryContext& ctx) const {
    std::vector<std::string> lines;
    const std::string family = familyFor(event);
    const std::vector<std::string> tags = tagsFor(event);
    lines.push_back("CARD " + event.id + "  SCORE " +
                    std::to_string(score(event, ctx)) + "%");
    lines.push_back("FAMILY " + family + "  RECENT " +
                    std::to_string(recentCount(recentFamilies_, family, 8)));
    std::string tagLine = "TAGS";
    for (const std::string& tag : tags) tagLine += " " + tag;
    lines.push_back(tagLine);

    std::string reasons = "CONTEXT";
    bool any = false;
    auto add = [&](const std::string& tag, bool active) {
        if (active && std::find(tags.begin(), tags.end(), tag) != tags.end()) {
            reasons += " +" + tag;
            any = true;
        }
    };
    add("relationship", ctx.knownNpc);
    add("quest", ctx.quest);
    add("artifact", ctx.artifact);
    add("war", ctx.war);
    add("plague", ctx.plague);
    add("weather", ctx.weather != "clear");
    add("mystery", ctx.mystery);
    add("companion", ctx.companion);
    if (!any) reasons += " neutral";
    lines.push_back(reasons);

    int heat = 0;
    for (const std::string& tag : tags)
        if (!isLocationTag(tag)) heat += recentCount(recentTags_, tag, 14);
    lines.push_back("COOLDOWN HEAT " + std::to_string(heat) +
                    "  EXACT RECENT " +
                    std::to_string(recentCount(recentIds_, event.id, 20)));
    return lines;
}

void StoryDirector::record(const Event& event, const Choice* choice, int day) {
    (void)day;
    pushBounded(recentIds_, event.id, 20);
    const std::string family = familyFor(event);
    pushBounded(recentFamilies_, family, 12);
    for (const std::string& tag : tagsFor(event)) {
        if (isLocationTag(tag)) continue;
        pushBounded(recentTags_, tag, 28);
        tagCounts_[tag]++;
    }
    if (choice && !choice->approach.empty())
        pushBounded(recentApproaches_, choice->approach, 10);
}

void StoryDirector::restore(const std::vector<std::string>& ids,
                            const std::vector<std::string>& tags,
                            const std::vector<std::string>& families,
                            const std::vector<std::string>& approaches,
                            const std::map<std::string, int>& counts) {
    reset();
    for (const std::string& value : ids) pushBounded(recentIds_, value, 20);
    for (const std::string& value : tags) pushBounded(recentTags_, value, 28);
    for (const std::string& value : families) pushBounded(recentFamilies_, value, 12);
    for (const std::string& value : approaches) pushBounded(recentApproaches_, value, 10);
    tagCounts_ = counts;
}
