#define FMT " [" << index << "] (" << pid << ") "
#ifndef WAIT_ANY
	#define WAIT_ANY -1
#endif

typedef struct {
	pid_t    pid;
	int      argc=0, state=FG;
	char   **argv;
} Job;

std::map<Jid, Job> jt;

extern inline void
jobs()
{
	int i;
	for (auto const& [k,v] : jt) {
		std::cout << " [" << std::setw(5) << k     << "] ";
		std::cout << " (" << std::setw(5) << v.pid << ") ";

		// Status
		if (v.state == BG)
			std::cout << "Background ";
		if (v.state == FG)
			std::cout << "Foregorund ";
		if (v.state == ST)
			std::cout << "Stopped    ";

		for (i = 0; i < v.argc; ++i)
			std::cout << v.argv[i] << ' ';
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
void
deljob(Jid index)
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

/** Converts a C-string to uppercase.
 * 
 * @param {char*}s
 * @return void
 */
void
upper(char *s)
{
	while (*s) {
		*s = toupper(*s);
		++s;
	}
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
	int cs;
	bool ok;

	while ((pid = waitpid(WAIT_ANY, &cs, WNOHANG|WUNTRACED)) > 0) {
		Jid index = -1;
		for (auto &it : jt) {
			if (it.second.pid == pid) {
				index = it.first;
				break;
			}
		}

		if (index != -1) {
			// Process signaled
			if (WIFSIGNALED(cs)) {
				if (TERMINAL) {
					std::cerr << FMT << strsignal(WTERMSIG(cs));
					std::cerr << '\n';
				}
				deljob(index);
			}
    
			// Process stopped
			if (WIFSTOPPED(cs)) {
				if (TERMINAL) {
					char *name = strdup(strsignal(WSTOPSIG(cs)));
					upper(name);
					std::cerr << FMT << "Stopped (" << name << ")\n";
					free(name);
				}
				jt[index].state = ST;
			}
    
			// Process exited
			if (WIFEXITED(cs)) {
				if (TERMINAL) {
					std::cerr << FMT << "Done ";
					std::cerr << '(' << WEXITSTATUS(cs) << ")\n";
				}
				deljob(index);
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
