#ifndef SIG_HPP
#define SIG_HPP
#include "pch.hpp"
#include "global.hpp"

#define SIGEXIT 0

const extern std::map<std::string, int> txt2sig;
const extern std::map<int, std::string> sig2txt;
const extern std::set<int> dflsigs;
extern int selfpipe_wrt, selfpipe_rd;

int tcsetpgrp2(pid_t);
sighandler_t signal2(int, sighandler_t);
int selfpipe_trick();
void reset_sigs();
extern "C" void sighandler(int);
int get_sig(std::string);
extern struct pollfd selfpipe_wait;

#endif
