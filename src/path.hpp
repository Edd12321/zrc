#ifndef PATH_HPP
#define PATH_HPP
#include <string>
#include <unordered_map>

extern std::unordered_map<std::string, std::string> hctable;

std::string basename(std::string const&);
std::unordered_map<std::string, std::string> pathwalk();

#endif
