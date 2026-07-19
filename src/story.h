#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "events.h"

// The Story Director does not invent outcomes or secretly change a roll. It
// only decides which eligible card is most interesting right now. Its state is
// saved with the run, so closing the game cannot erase narrative cooldowns.
struct StoryContext {
    int day = 1;
    std::string location;
    std::string weather;
    bool companion = false;
    bool artifact = false;
    bool quest = false;
    bool knownNpc = false;
    bool war = false;
    bool plague = false;
    bool mystery = false;
};

class StoryDirector {
public:
    void reset();

    // Returns a percentage multiplier. 100 is the authored weight, 25 is a
    // strong cooldown, and values above 100 reward relevance or novelty.
    int score(const Event& event, const StoryContext& ctx) const;
    std::vector<std::string> explain(const Event& event,
                                     const StoryContext& ctx) const;
    void record(const Event& event, const Choice* choice, int day);

    static std::vector<std::string> tagsFor(const Event& event);
    static std::string familyFor(const Event& event);

    const std::deque<std::string>& recentIds() const { return recentIds_; }
    const std::deque<std::string>& recentTags() const { return recentTags_; }
    const std::deque<std::string>& recentFamilies() const { return recentFamilies_; }
    const std::deque<std::string>& recentApproaches() const { return recentApproaches_; }
    const std::map<std::string, int>& tagCounts() const { return tagCounts_; }

    void restore(const std::vector<std::string>& ids,
                 const std::vector<std::string>& tags,
                 const std::vector<std::string>& families,
                 const std::vector<std::string>& approaches,
                 const std::map<std::string, int>& counts);

private:
    std::deque<std::string> recentIds_;
    std::deque<std::string> recentTags_;
    std::deque<std::string> recentFamilies_;
    std::deque<std::string> recentApproaches_;
    std::map<std::string, int> tagCounts_;
};
