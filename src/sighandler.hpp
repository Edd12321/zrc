#ifndef WAIT_ANY
	#define WAIT_ANY -1
#endif

struct Job {
	pid_t    pid;
	int      argc, state;
	char   **argv;
};

std::map<Jid, Job> jt;

void
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
	int index, i;
	if (jt.empty())
		index = 1;
	else
		index = jt.rbegin()->first+1;
	
	jt[index].pid   = pid;
	jt[index].state = state;
	jt[index].argc  = argc;

	jt[index].argv  = new char*[argc+1];
	for (i = 0; i < argc; ++i)
		jt[index].argv[i] = strdup(argv[i]);
	return index;
}

/** Deletes a job from the job table.
 *
 * @param {Job*}job
 * @return void
 */
void
deljob(Jid index)
{
	std::map<Jid, Job>::iterator it;
	int i;

	it = jt.find(index);
	if (it != jt.end()) {
		// Avoid memory leak
		for (i = 0; i < jt[index].argc; ++i)
			free(jt[index].argv[i]);
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
inline void
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

/** Async signal-safe print for signal handler
 * 
 * @param {int}index,{int}pid,{const char*}msg
 * @return none
 */
const auto dig_max = (int)log10(INT_MAX)+1;
void
async_message(int index, int pid, const char *msg)
{
	static std::function<void(char*, int)> itoa2 = [] (char *num, int x) {
		long k = int(log10(x));
		num[k+1] = '\0';
		while (x) {
			num[k--] = x%10+'0';
			x /= 10;
		}
	};
	char i[dig_max], p[dig_max];
	itoa2(i, index);
	itoa2(p, pid);
	write(JOB_MSG, " [", 2);
	write(JOB_MSG, i, strlen(i));
	write(JOB_MSG, "] (", 3);
	write(JOB_MSG, p, strlen(p));
	write(JOB_MSG, ") ", 2);
	write(JOB_MSG, msg, strlen(msg));
	write(JOB_MSG, "\n", 1);
}

/** Converts a C-string to uppercase.
 * 
 * @param {char*}s
 * @return void
 */
//void upper(char *s) { while (*s) *s = toupper(*s++); }

std::string
bg_fg(int argc, char *argv[])
{
	Job *j;
	Jid index = -1;
	if (argc != 2 || (!isdigit(*argv[1]) && *argv[1] != '%'))
		syntax_error("<pid>|<%jid>");
	if (*argv[1] == '%') {
		int num = (ull)expr(argv[1]+1);
		if (jt.find(num) != jt.end()) {
			j = &jt[num];
			index = num;
		}
	} else {
		for (auto& it : jt) {
			if (it.second.pid == (ull)expr(argv[1])) {
				j = &it.second;
				index = it.first;
				break;
			}
		}
	}
	if (index == -1)
		syntax_error("Invalid job");
	
	if (!strcmp(argv[0], "bg")) {
		kill(-j->pid, SIGCONT);
		j->state = BG;
		if (TERMINAL) {
			pid_t pid = j->pid;
			async_message(index, pid, "");
		}
	} else {
		j->state = FG;
		tcsetpgrp(STDIN_FILENO, j->pid);
		kill(-j->pid, SIGCONT);
		for ever {
			if (j->pid != getfg()) {
				if (TERMINAL)
					async_message(index, j->pid, "no longer fg proc");
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
				if (tcgetpgrp(STDIN_FILENO) != zrcpid)
					tcsetpgrp(STDIN_FILENO, zrcpid);
			////
			if (WIFSIGNALED(cs)) {
				if (TERMINAL)
					async_message(index, pid, strsignal(WTERMSIG(cs))); 
				deljob(index);
			}
			////
			if (WIFSTOPPED(cs)) {
				if (TERMINAL)
					async_message(index, pid, strsignal(WSTOPSIG(cs)));
				jt[index].state = ST;
			}
			////
			if (WIFEXITED(cs)) {
				if (TERMINAL && mode == BG)
					async_message(index, pid, "Done");
				deljob(index);
				ret_val = itoa(WEXITSTATUS(cs));
			}
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
