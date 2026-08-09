#include <syslog.h>
#include <print>
#include <unistd.h>
#include <sys/stat.h>
#include <atomic>
#include <csignal>
#include <iostream>
#include <fcntl.h>
#include <sys/file.h>
#include <cstring>
#include <string>

std::atomic<bool> g_running { true };
constexpr const char* LOCK_FILE_PATH = "/var/run/Matt_daemon.lock";

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

bool ensure_one_instance()
{
	int fd = open(LOCK_FILE_PATH, O_RDWR | O_CREAT, 0640);
	if (fd < 0)
	{
		std::cerr << "Failed to open lock file: " << std::strerror(errno) << "\n";
		return false;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			std::cerr << "Error: Another instance of Matt_daemon is already running.\n";
		}
		else
		{
			std::cerr << "Failed to lock file: " << std::strerror(errno) << "\n";
		}
		close(fd);
		return false;
	}
	if (ftruncate(fd, 0) < 0)
	{
		std::cerr << "Failed to truncate file: " << std::strerror(errno) << "\n";
	}
	std::string pid_str = std::to_string(getpid()) + "\n";
	write(fd, pid_str.c_str(), pid_str.size());
	return true;	
}

int daemon(void)
{
	if (!(geteuid() == 0))
	{
		std::println("Matt_daemon requires root privileges.");
		openlog("Matt_daemon", LOG_PID, LOG_DAEMON);
		syslog(LOG_ERR, "Matt_daemon requires root privileges");
		closelog();
		exit(EXIT_FAILURE);
	}

	if (!ensure_one_instance())
	{
		openlog("Matt_daemon", LOG_PID, LOG_DAEMON);
		syslog(LOG_ERR, "An instance of Matt_daemon is already running.");
		closelog();
		exit(EXIT_FAILURE);
	}

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
	openlog("Matt_daemon", LOG_NOWAIT | LOG_PID, LOG_USER);
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
	struct sigaction sa_int{};
	struct sigaction sa_term{};

	sa_term.sa_flags = 0;
	sa_int.sa_flags = 0;

	sigemptyset(&sa_term.sa_mask);
	sigemptyset(&sa_int.sa_mask);

	sa_int.sa_handler = handle_sigint;
	sa_term.sa_handler = handle_sigterm;

	sigaction(SIGTERM, &sa_term, nullptr);
	sigaction(SIGINT, &sa_int, nullptr);
	daemon();	
	return (0);
}
