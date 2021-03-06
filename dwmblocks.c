#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <X11/Xlib.h>

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))
#define CMDLENGTH 50
#define MIN(a, b) ((a < b) ? a : b)
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

typedef struct
{
	char *icon;
	char *command;
	unsigned int interval;
	unsigned int signal;
} Block;

void sighandler(int num);
void buttonhandler(int sig, siginfo_t *si, void *ucontext);
void getcmds(int time);
#ifndef __OpenBSD__
void getsigcmds(int signal);
void setupsignals();
void sighandler(int signum);
#endif
int getstatus(char *str, char *last);
void statusloop();
void termhandler(int signum);
void pstdout();
#ifndef NO_X
void setroot();
static void (*writestatus) () = setroot;
static int setupX();
static Display *dpy;
static int screen;
static Window root;
#else
static void (*writestatus) () = pstdout;
#endif

#include "blocks.h"


static Display *dpy;
static int screen;
static Window root;
static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;

int gcd(int a, int b)
{
	int temp;
	while (b > 0)
	{
		temp = a % b;

		a = b;
		b = temp;
	}
	return a;
}

//opens process *cmd and stores output in *output
void getcmd(const Block *block, char *output)
{
	output[0] = block->signal + 1;

	strcpy(++output, block->icon);
	FILE *cmdf = popen(block->command, "r");
	if (!cmdf)
		return;
	int i = strlen(block->icon);
	fgets(output + i, CMDLENGTH - i - delimLen, cmdf);
	i = strlen(output);
	if (i == 0)
	{
		//return if block and command output are both empty
		pclose(cmdf);
		return;
	}
	if (delim[0] != '\0')
	{
		//only chop off newline if one is present at the end
		i = output[i - 1] == '\n' ? i - 1 : i;
		strncpy(output + i, delim, delimLen);
	}
	else
		output[i++] = '\0';
	pclose(cmdf);
}

void getcmds(int time)
{
	const Block *current;
	for (int i = 0; i < LENGTH(blocks); i++)
	{
		current = blocks + i;
		if ((current->interval != 0 && time % current->interval == 0) || time == -1)
		{
			getcmd(current, statusbar[i]);
		}
	}
}

#ifndef __OpenBSD__
void getsigcmds(int signal)
{
	const Block *current;
	for (int i = 0; i < LENGTH(blocks); i++)
	{
		current = blocks + i;
		if (current->signal == signal)
		{
			getcmd(current, statusbar[i]);
		}
	}
}

void setupsignals()
{
	struct sigaction sa;

	for (int i = SIGRTMIN; i <= SIGRTMAX; i++)
		signal(i, SIG_IGN);

	for (int i = 0; i < LENGTH(blocks); i++)
	{
		if (blocks[i].signal > 0)
		{
			signal(SIGRTMIN + blocks[i].signal, sighandler);
			sigaddset(&sa.sa_mask, SIGRTMIN + blocks[i].signal);
		}
	}
	sa.sa_sigaction = buttonhandler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa, NULL);
	struct sigaction sigchld_action = {
		.sa_handler = SIG_DFL,
		.sa_flags = SA_NOCLDWAIT};
	sigaction(SIGCHLD, &sigchld_action, NULL);
}
#endif

int getstatus(char *str, char *last)
{
	strcpy(last, str);
	str[0] = '\0';
	strcat(str, "<");

	for (unsigned int i = 0; i < LENGTH(blocks); i++)
	{
		strcat(str, statusbar[i]);
	}
	str[strlen(str) - strlen(delim)] = '\0';
	return strcmp(str, last); //0 if they are the same
}

#ifndef NO_X
void setroot()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only set root if text has changed.
		return;
	XStoreName(dpy, root, statusstr[0]);
	XFlush(dpy);
}

int setupX()
{
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "dwmblocks: Failed to open display\n");
		return 0;
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	return 1;
}
#endif

void pstdout()
{
	if (!getstatus(statusstr[0], statusstr[1])) //Only write out if text has changed.
		return;
	printf("%s\n", statusstr[0]);
	fflush(stdout);
}

void statusloop()
{
#ifndef __OpenBSD__
	setupsignals();
#endif
	// first figure out the default wait interval by finding the
	// greatest common denominator of the intervals
	unsigned int interval = -1;
	for (int i = 0; i < LENGTH(blocks); i++)
	{
		if (blocks[i].interval)
		{
			interval = gcd(blocks[i].interval, interval);
		}
	}
	unsigned int i = 0;
	int interrupted = 0;
	const struct timespec sleeptime = {interval, 0};
	struct timespec tosleep = sleeptime;
	getcmds(-1);
	while (statusContinue)
	{
		// sleep for tosleep (should be a sleeptime of interval seconds) and put what was left if interrupted back into tosleep
		interrupted = nanosleep(&tosleep, &tosleep);
		// if interrupted then just go sleep again for the remaining time
		if (interrupted == -1)
		{
			continue;
		}
		// if not interrupted then do the calling and writing
		getcmds(i);
		writestatus();
		// then increment since its actually been a second (plus the time it took the commands to run)
		i += interval;
		// set the time to sleep back to the sleeptime of 1s
		tosleep = sleeptime;
	}
}

#ifndef __OpenBSD__
void sighandler(int signum)
{
	getsigcmds(signum - SIGRTMIN);
	writestatus();
}

void buttonhandler(int sig, siginfo_t *si, void *ucontext)
{
	char button[2] = {'0' + si->si_value.sival_int & 0xff, '\0'};
	pid_t process_id = getpid();
	sig = si->si_value.sival_int >> 8;
	if (fork() == 0)
	{
		const Block *current;
		for (int i = 0; i < LENGTH(blocks); i++)
		{
			current = blocks + i;
			if (current->signal == sig)
				break;
		}
		char shcmd[1024];
		sprintf(shcmd, "%s && kill -%d %d", current->command, current->signal + 34, process_id);
		char *command[] = {"/bin/sh", "-c", shcmd, NULL};
		setenv("BLOCK_BUTTON", button, 1);
		setsid();
		execvp(command[0], command);
		exit(EXIT_SUCCESS);
	}
}

#endif

void termhandler(int signum)
{
	statusContinue = 0;
	exit(0);
}

int main(int argc, char** argv)
{
	for (int i = 0; i < argc; i++) {//Handle command line arguments
		if (!strcmp("-d",argv[i]))
			strncpy(delim, argv[++i], delimLen);
		else if (!strcmp("-p",argv[i]))
			writestatus = pstdout;
	}
#ifndef NO_X
	if (!setupX())
		return 1;
#endif
	delimLen = MIN(delimLen, strlen(delim));
	delim[delimLen++] = '\0';
	signal(SIGTERM, termhandler);
	signal(SIGINT, termhandler);
	statusloop();
#ifndef NO_X
	XCloseDisplay(dpy);
#endif
	return 0;
}