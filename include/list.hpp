#ifndef LIST_HPP
#define LIST_HPP
#include "pch.hpp"
#include "syn.hpp"

std::string list(int, const char**);
std::string list(int, char**);
std::string list(std::string const&);
std::string list(std::vector<token>&);

#endif
