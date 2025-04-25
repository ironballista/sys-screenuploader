#pragma once

#include <filesystem>
#include <vector>
#include "config.hpp"

std::vector<std::filesystem::directory_entry> getAlbumItemsPastDate(int year, int month, int day);
std::vector<std::filesystem::path> getAlbumItemsPastTimestamp(std::filesystem::file_time_type since);
std::string getLastAlbumItem();
size_t filesize(const std::string &path);
std::string url_encode(const std::string &value);
