#pragma once
#include <switch.h>
#include <string>
#include "config.hpp"

bool sendFileToServer(const std::string &path, size_t size);
