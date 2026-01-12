#include "pch.hpp"
#include "global.hpp"
#include "command.hpp"
#include "custom_cmd.hpp"
#include "sig.hpp"
#include "vars.hpp"

const std::map<std::string, int> txt2sig = {
	{ "sighup"  , SIGHUP   }, { "sigint"   , SIGINT    }, { "sigquit", SIGQUIT },
	{ "sigill"  , SIGILL   }, { "sigtrap"  , SIGTRAP   }, { "sigabrt", SIGABRT },
	{ "sigbus"  , SIGBUS   }, { "sigfpe"   , SIGFPE    }, { "sigkill", SIGKILL },
	{ "sigusr1" , SIGUSR1  }, { "sigsegv"  , SIGSEGV   }, { "sigusr2", SIGUSR2 },
	{ "sigpipe" , SIGPIPE  }, { "sigalrm"  , SIGALRM   }, { "sigterm", SIGTERM },
	/* #16: unused         */ { "sigchld"  , SIGCHLD   }, { "sigcont", SIGCONT },
	{ "sigstop" , SIGSTOP  }, { "sigtstp"  , SIGTSTP   }, { "sigttin", SIGTTIN },
	{ "sigttou" , SIGTTOU  }, { "sigurg"   , SIGURG    }, { "sigxcpu", SIGXCPU },
	{ "sigxfsz" , SIGXFSZ  }, { "sigvtalrm", SIGVTALRM }, { "sigprof", SIGPROF },
	{ "sigwinch", SIGWINCH },
#ifdef SIGPOLL
	{ "sigio"   , SIGPOLL  }, { "sigpoll"  , SIGPOLL   },
	                          // (both SIGIO and SIGPOLL are the same)
#endif
	{ "sigsys"  , SIGSYS   },


#ifdef SIGPWR
	{ "sigpwr"  , SIGPWR   },
#endif
#ifdef SIGRTMIN
	{ "sigrtmin", SIGRTMIN },
#endif
#ifdef SIGRTMAX
	{ "sigrtmax", SIGRTMAX },
#endif
	{ "sigexit" , SIGEXIT  }
	//Special zrc "pseudo-signal"
};
const std::map<int, std::string> sig2txt = [] {
	std::map<int, std::string> ret;
	for (auto const& it : txt2sig)
		ret.emplace(it.second, it.first);
	return ret;
}();
const std::set<int> dflsigs = {SIGINT, SIGQUIT, SIGTSTP, SIGTTOU, SIGTTIN};


/** Tcsetpgrp replacement **/
int tcsetpgrp2(pid_t pgid) {
	sigset_t mask, old;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTTOU);
	sigaddset(&mask, SIGTTIN);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_BLOCK, &mask, &old);
	int ret_val = tcsetpgrp(tty_fd, pgid);
	sigprocmask(SIG_SETMASK, &old, NULL);
	return ret_val;
}

/** Signal replacement **/
sighandler_t signal2(int sig, sighandler_t f) {
	struct sigaction sa, old;
	sa.sa_handler = f;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(sig, &sa, &old) == -1)
		return SIG_ERR;
	return old.sa_handler;
}

/** Self-pipe trick for signal safe trapping **/
decltype(sigtraps) sigtraps;
int selfpipe_rd, selfpipe_wrt;
struct pollfd selfpipe_wait;

void selfpipe_trick() {
	static struct pollfd selfpipe_wait;
	selfpipe_wait.fd = selfpipe_rd;
	selfpipe_wait.events = POLLIN;
	
	int sig, ret;
	while ((ret = poll(&selfpipe_wait, 1, 0)) > 0 && (selfpipe_wait.revents & POLLIN)) {
		while (read(selfpipe_rd, &sig, sizeof sig) == sizeof sig) {
			if (sigtraps.find(sig) == sigtraps.end()) {
				if (sig == SIGHUP) {
					jtable.sighupper();
					exit(129);
				}
				continue;
			}
			if (sig == SIGCHLD && interactive_sesh && !jtable.empty())
				jtable.reaper();
			auto& trap = sigtraps.at(sig);
			if (trap.active)
				trap();
		}
	}
}

void reset_sigs() {
	for (auto const& it : txt2sig) {
		int sig = it.second;
		if (sig == SIGEXIT)
			killed_sigexit = true;
		else signal(sig, SIG_DFL);
	}
}

extern "C" void sighandler(int sig) {
	int wrt;
	do
		wrt = write(selfpipe_wrt, &sig, sizeof sig);
	while (wrt < 0 && errno != EINTR);
}

int get_sig(std::string str) {
	auto num = stonum(str);
	if (!isnan(num) && sig2txt.find(num) != sig2txt.end())
		return num;
	for (auto& c : str)
		c = tolower(c);
	if (str.substr(0, 3) != "sig")
		str = "sig" + str;
	if (txt2sig.find(str) != txt2sig.end())
		return txt2sig.at(str);
	return -1;
}
