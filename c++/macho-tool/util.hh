#pragma once

#include <unordered_map>
#include <string>

bool stobool(const std::string& s);

template <typename Flag>
using Flags = std::unordered_map<Flag, bool>;

template <typename Flag>
using FlagSet = std::unordered_map<std::string, Flag>;

template <typename Flag>
void parse_flags(char *optarg, const FlagSet<Flag>& flagset, Flags<Flag>& flags);

#define MH_FLAG(flag) {#flag + 3, flag}

extern const std::unordered_map<std::string, uint32_t> mach_header_filetype_map;
