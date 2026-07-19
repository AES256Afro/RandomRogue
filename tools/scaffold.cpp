#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char** argv) {
    const std::string id = argc > 1 ? argv[1] : "r15_new_event";
    const std::string location = argc > 2 ? argv[2] : "road";
    const std::string family = argc > 3 ? argv[3] : "standalone";
    json event = {
        {"id", id},
        {"locations", json::array({location})},
        {"weight", 10},
        {"family", family},
        {"tags", json::array({"solidarity", "politics"})},
        {"text", "Describe a material problem, who benefits from it, and who pays."},
        {"choices", json::array({
            {{"text", "Act together."},
             {"approach", "solidarity"},
             {"outcomes", json::array({
                 {{"weight", 1}, {"text", "People discover that power has a plural pronoun."},
                  {"effects", json::array({"collective 1", "region solidarity +1"})}}
             })}},
            {{"text", "Try an individual solution."},
             {"approach", "pragmatic"},
             {"outcomes", json::array({
                 {{"weight", 1}, {"text", "The immediate problem moves somewhere less visible."},
                  {"effects", json::array({"money +4", "region unrest +1"})}}
             })}},
            {{"text", "Ask who owns the problem."},
             {"approach", "investigate"},
             {"check", {{"stat", "INT"}, {"dc", 12}}},
             {"outcomes", json::array({
                 {{"weight", 1}, {"text", "The paperwork develops a culprit."},
                  {"effects", json::array({"rumor 3 Somebody is profiting from this."})}}
             })}}
        })}
    };
    std::cout << event.dump(2) << "\n";
    return 0;
}
