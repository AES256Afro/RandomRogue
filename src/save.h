// Persistent player profile — file on desktop, localStorage in the browser.
#pragma once
#include <string>

struct Profile {
    int deaths = 0;
    int daysTotal = 0;
    int bestDays = 0;
    int booksRead = 0;

    std::string toJson() const;
    static Profile fromJson(const std::string& text);
};

Profile LoadProfile();
void SaveProfile(const Profile& p);
