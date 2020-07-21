#pragma once

#include <unordered_map>
#include <string>

template <typename T>
T stout(const std::string& str, std::size_t *pos = nullptr, int base = 10) {
   static_assert(std::is_unsigned<T>());
   auto raw = std::stoull(str, pos, base);
   if (raw > std::numeric_limits<T>::max()) {
      throw std::out_of_range("unsigned integer out range");
   }
   return (T) raw;
}

bool stobool(const std::string& s);

template <typename Flag>
using Flags = std::unordered_map<Flag, bool>;

template <typename Flag>
using FlagSet = std::unordered_map<std::string, Flag>;

template <typename Flag>
void parse_flags(char *optarg, const FlagSet<Flag>& flagset, Flags<Flag>& flags);

#define MH_FLAG(flag) {#flag + 3, flag}

extern const std::unordered_map<std::string, uint32_t> mach_header_filetype_map;

template <typename... Args>
inline std::string fmtstr(const char *fmt, Args&&... args) {
   char *cstr;
   if (asprintf(fmt, args...) < 0) {
      throw std::runtime_error(strerror(errno));
   }
   std::string str = cstr;
   free(cstr);
   return str;
}
