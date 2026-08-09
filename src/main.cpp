#include <syslog.h>
#include <print>
#include <unistd.h>
#include <sys/stat.h>
#include <atomic>
#include <csignal>

std::atomic<bool> g_running { true };

void do_heartbeat(void)
{
	// this wouldn't work obviously :)
	std::println("Matt_daemon is working as expected.");
}

void handle_sigterm(int signum) {
	(void) signum;
	syslog(LOG_NOTICE, "Received a SIGTERM, exit right now...");
	exit(EXIT_SUCCESS);
}

void handle_sigint(int signum) {
	(void) signum;
	syslog(LOG_NOTICE, "Received a SIGINT, exiting right now...");
	exit(EXIT_SUCCESS);
}

int daemon(void)
{
	pid_t pid, sid;
	
	pid = fork();
	if (pid > 0)
	{
		/* parent process */
		exit(EXIT_SUCCESS);
	}
	else if (pid < 0)
	{
		/* something went wrong */
		exit(EXIT_FAILURE);
	}

	umask(0);
	/* child process */
	openlog("Matt_daemon.log", LOG_NOWAIT | LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "Successfully started Matt_daemon");
	
	sid = setsid();
	if (sid < 0)
	{
		syslog(LOG_ERR, "Could not generate session ID for child process");
		exit(EXIT_FAILURE);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	const int SLEEP_INT = 5;
	while (g_running.load(std::memory_order_relaxed))
	{
		do_heartbeat();
		sleep(SLEEP_INT);
	}

	syslog(LOG_NOTICE, "For some reason, we quit the main loop. Matt_daemon is stopping right now.");
	closelog();
	exit(EXIT_SUCCESS);
	return 0;
}

int main(void)
{
	struct sigaction sa{};
	sa.sa_handler = handle_sigterm;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGINT, &sa, nullptr);
	daemon();	
	return (0);
}
