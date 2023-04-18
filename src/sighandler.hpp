#define FMT " [" << index << "] (" << pid << ") "
#ifndef WAIT_ANY
	#define WAIT_ANY -1
#endif

typedef struct {
	pid_t    pid;
	int      argc, state;
	char   **argv;
} Job;

std::map<Jid, Job> jt;

extern inline void
jobs()
{
	int i;
	for (auto const& v : jt) {
		std::cout << " [" << std::setw(5) << v.first      << "] ";
		std::cout << " (" << std::setw(5) << v.second.pid << ") ";

		// Status
		if (v.second.state == BG)
			std::cout << "Background ";
		if (v.second.state == FG)
			std::cout << "Foregorund ";
		if (v.second.state == ST)
			std::cout << "Stopped    ";

		for (i = 0; i < v.second.argc; ++i)
			std::cout << v.second.argv[i] << ' ';
		std::cout << std::endl;
	}
}

/** Adds a new job to the job table.
 * 
 * @param {pid_t}pid,{int}state;argc,{char**}argv
 * @return void
 */
Jid
addjob(pid_t pid, int state, int argc, char *argv[])
{
	int index, i, size;
	if (jt.empty())
		index = 1;
	else
		index = jt.rbegin()->first+1;
	
	jt[index].pid   = pid;
	jt[index].state = state;
	jt[index].argc  = argc;

	jt[index].argv  = new char*[argc+1];
	for (i = 0; i < argc; ++i) {
		jt[index].argv[i] = new char[
			strlen(argv[i])+1
		];
		strcpy(jt[index].argv[i], argv[i]);
	}
	return index;
}

/** Deletes a job from the job table.
 *
 * @param {Job*}job
 * @return void
 */
sub deljob(Jid index)
{
	std::map<Jid, Job>::iterator it;
	int i;

	it = jt.find(index);
	if (it != jt.end()) {
		// Avoid memory leak
		for (i = 0; i < jt[index].argc; ++i)
			delete [] jt[index].argv[i];
		delete [] jt[index].argv;

		jt.erase(it);
	}
}

/** Get current FG pid.
 * 
 * @param  none
 * @return void
 */
static pid_t
getfg()
{
	for (auto &it : jt)
		if (it.second.state == FG)
			return it.second.pid;
	return 0;
}

/** Wait until PID stops being fg process.
 * 
 * @param {pid_t}pid
 * @return void
 */
extern inline void
waitproc(pid_t pid)
{
	sigset_t mask;
	sigemptyset(&mask);
	for ever {
		if (getfg() != pid)
			return;
		sigsuspend(&mask);
	}
}

/** Converts a C-string to uppercase.
 * 
 * @param {char*}s
 * @return void
 */
//void upper(char *s) { while (*s) *s = toupper(*s++); }

extern std::string
bg_fg(int argc, char *argv[])
{
	Job *j;
	Jid index = -1;
	if (argc != 2 && (!isdigit(*argv[1]) && *argv[1] != '%'))
		syntax_error("<pid>|<%jid>");
	if (*argv[1] == '%') {
		int num = atoi(&argv[1][1]);
		if (jt.find(num) != jt.end()) {
			j = &jt[num];
			index = num;
		}
	} else {
		for (auto& it : jt) {
			if (it.second.pid == atoi(argv[1])) {
				j = &it.second;
				index = it.first;
				break;
			}
		}
	}
	if (index == -1)
		syntax_error("Invalid job");
	
	kill(-j->pid, SIGCONT);
	if (!strcmp(argv[0], "bg")) {
		j->state = BG;
		if (TERMINAL) {
			pid_t pid = j->pid;
			std::cerr << FMT;
		}
	} else {
		j->state = FG;
		for ever {
			if (j->pid != getfg()) {
				if (TERMINAL)
					std::cerr << j->pid << " no longer fg proc\n";
				break;
			} else {
				sleep(1);
			}
		}
	}
	return "0";
}

/** Reaps zombie processes.
 *
 * @param {int}signum
 * @return void
 */
void
sigchld_handler(int signum)
{
	pid_t pid;
	int cs, mode;

	while ((pid = waitpid(WAIT_ANY, &cs, WNOHANG|WUNTRACED)) > 0) {
		Jid index = -1;
		for (auto &it : jt) {
			if (it.second.pid == pid) {
				index = it.first;
				mode  = it.second.state;
				break;
			}
		}
		if (index != -1) {
			// set foreground PGID to shell's PID
			if (pid == getfg())
				if (tcgetpgrp(STDIN_FILENO != zrcpid))
					tcsetpgrp(STDIN_FILENO, zrcpid);
			if (WIFSIGNALED(cs)) {
				if (TERMINAL)
					std::cerr << FMT << strsignal(WTERMSIG(cs)) << '\n';
				deljob(index);
			}
			if (WIFSTOPPED(cs)) {
				if (TERMINAL)
					std::cerr << FMT << strsignal(WSTOPSIG(cs)) << '\n';
				jt[index].state = ST;
			}
			if (WIFEXITED(cs)) {
				if (TERMINAL && mode == BG)
					std::cerr << FMT << strsignal(WEXITSTATUS(cs)) << '\n';
				deljob(index);
				ret_val = itoa(WEXITSTATUS(cs));
			}
			setvar("!", itoa(pid));
		}
	}
}

/** Handles SIGINT.
 * 
 * @param {int}signum
 * @return void
 */
inline void
sigint_handler(int signum)
{
	pid_t pid;

	pid = getfg();
	if (pid)
		kill(-pid, signum);
	return;
}

/** Handles SIGTSTP.
 *
 * @param {int}signum
 * @return void
 */
void
sigtstp_handler(int signum)
{
	sigint_handler(SIGTSTP);
	return;
}

/** Handles SIGQUIT.
 *
 * @param {int}signum
 * @return void
 */
void
sigquit_handler(int signum)
{
	exit(EXIT_FAILURE);
}

/** signal(2) alternative.
 *
 * @param {int}signum,{void (*)(int)}h
 * @return void
 */
//typedef void Handle(int);
Handle*
signal2(int signum, Handle *h)
{
	struct sigaction sa, oa;

	sa.sa_handler = h;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(signum, &sa, &oa);
	return oa.sa_handler;
}
