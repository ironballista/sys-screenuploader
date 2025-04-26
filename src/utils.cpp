#include <vector>
#include <cstring>
#include <string>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <ranges>

#include "utils.hpp"

namespace fs = std::filesystem;

constexpr std::string getAlbumPath() {
    return "img:/";
}

constexpr bool isDigitsOnly(const std::string &str) {
    return str.find_first_not_of("0123456789") == std::string::npos;
}

std::vector<fs::path> getAlbumItemsPastTimestamp(fs::file_time_type since) {
    // Iterate over all directory entries under the `img` path
    // Filter out those which do not satisfy basic criteria
    // Filter out those whose last write time is older than `since`
    std::vector<std::string> years, months, days;
    auto files = std::vector<fs::path>{};

    std::string albumPath = getAlbumPath();
    if (!fs::is_directory(albumPath)) return {};

    for (auto &entry : fs::directory_iterator(albumPath))
        if (entry.is_directory() && isDigitsOnly(entry.path().filename()) && entry.path().filename().string().length() == 4)
            years.push_back(entry.path());
    if (years.empty()) return {};
    sort(years.begin(), years.end());

    for (auto &entry : fs::directory_iterator(years.back()))
        if (entry.is_directory() && isDigitsOnly(entry.path().filename()) && entry.path().filename().string().length() == 2)
            months.push_back(entry.path());
    if (months.empty()) return {};
    sort(months.begin(), months.end());

    for (auto &entry : fs::directory_iterator(months.back()))
        if (entry.is_directory() && isDigitsOnly(entry.path().filename()) && entry.path().filename().string().length() == 2)
            days.push_back(entry.path());
    if (days.empty()) return {};
    sort(days.begin(), days.end());

    auto ec = std::error_code{};

    for (auto &entry : fs::directory_iterator(days.back()))
        if (entry.is_regular_file() && entry.last_write_time(ec) > since && !ec)
            files.push_back(entry.path());

    return files;
}

std::vector<fs::directory_entry> getAlbumItemsPastDate(int year, int month, int day) {
    const std::string sYear = std::to_string(year);
    const std::string sMonth = std::to_string(month);
    const std::string sDay = std::to_string(day);

    auto valid_year = [](const fs::directory_entry &path) {
        return path.is_directory() && isDigitsOnly(path.path().filename()) &&
            path.path().filename().string().length() == 4;
    };

    auto year_past = [&](const fs::directory_entry &path) {
        return path.path().filename().string() >= sYear;
    };

    auto valid_month = [](const fs::directory_entry &path) {
        return path.is_directory() && isDigitsOnly(path.path().filename()) &&
            path.path().filename().string().length() == 2;
    };

    auto month_past = [&](const fs::directory_entry &path) {
        return path.path().filename().string() >= sMonth;
    };

    auto valid_day = [](const fs::directory_entry &path) {
        return path.is_directory() && isDigitsOnly(path.path().filename()) &&
            path.path().filename().string().length() == 2;
    };

    auto day_past = [&](const fs::directory_entry &path) {
        return path.path().filename().string() >= sDay;
    };

    auto explore_dir = [](const fs::directory_entry &path) {
        return fs::directory_iterator(path);
    };

    auto files = std::views::all(fs::directory_iterator(getAlbumPath())) |
        std::views::filter(valid_year) | std::views::filter(year_past) |
        std::views::transform(explore_dir) | std::views::join |
        std::views::filter(valid_month) | std::views::filter(month_past) |
        std::views::transform(explore_dir) | std::views::join |
        std::views::filter(valid_day) | std::views::filter(day_past) |
        std::views::transform(explore_dir) | std::views::join;

    std::vector<fs::directory_entry> paths {};
    
    for (auto file : files) {
      paths.push_back(file);
    }

    return paths;
}

std::string getLastAlbumItem() {
    std::vector<std::string> years, months, days, files;
    std::string albumPath = getAlbumPath();
    if (!fs::is_directory(albumPath)) return "<No album directory: " + albumPath + ">";

    for (auto &entry : fs::directory_iterator(albumPath))
        if (entry.is_directory() && isDigitsOnly(entry.path().filename()) && entry.path().filename().string().length() == 4)
            years.push_back(entry.path());
    if (years.empty()) return "<No years in " + albumPath + ">";
    sort(years.begin(), years.end());

    for (auto &entry : fs::directory_iterator(years.back()))
        if (entry.is_directory() && isDigitsOnly(entry.path().filename()) && entry.path().filename().string().length() == 2)
            months.push_back(entry.path());
    if (months.empty()) return "<No months in " + years.back() + ">";
    sort(months.begin(), months.end());

    for (auto &entry : fs::directory_iterator(months.back()))
        if (entry.is_directory() && isDigitsOnly(entry.path().filename()) && entry.path().filename().string().length() == 2)
            days.push_back(entry.path());
    if (days.empty()) return "<No days in " + months.back() + ">";
    sort(days.begin(), days.end());

    for (auto &entry : fs::directory_iterator(days.back()))
        if (entry.is_regular_file())
            files.push_back(entry.path());
    if (files.empty()) return "<No screenshots in " + days.back() + ">";
    sort(files.begin(), files.end());

    return files.back();
}

size_t filesize(const std::string &path) {
    std::streampos begin, end;
    std::ifstream f(path, std::ios::binary);
    begin = f.tellg();
    f.seekg(0, std::ios::end);
    end = f.tellg();
    f.close();
    return end - begin;
}

std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}
