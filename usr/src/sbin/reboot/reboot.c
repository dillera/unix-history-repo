/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980, 1986 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)reboot.c	5.13 (Berkeley) %G%";
#endif /* not lint */

#include <sys/reboot.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void err __P((const char *fmt, ...));
void usage __P((void));

int dohalt;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int i;
	struct passwd *pw;
	int ch, howto, lflag, nflag, qflag, sverrno;
	char *p, *user;

	if (!strcmp((p = rindex(*argv, '/')) ? p + 1 : *argv, "halt")) {
		dohalt = 1;
		howto = RB_HALT;
	} else
		howto = 0;
	lflag = nflag = qflag = 0;
	while ((ch = getopt(argc, argv, "lnq")) != EOF)
		switch(ch) {
		case 'l':		/* Undocumented; used by shutdown. */
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			howto |= RB_NOSYNC;
			break;
		case 'q':
			qflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (geteuid())
		err("%s", strerror(EPERM));

	if (qflag) {
		reboot(howto);
		err("%s", strerror(errno));
	}

	/* Log the reboot. */
	if (!lflag)  {
		if ((user = getlogin()) == NULL)
			user = (pw = getpwuid(getuid())) ?
			    pw->pw_name : "???";
		if (dohalt) {
			openlog("halt", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "halted by %s", user);
		} else {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "rebooted by %s", user);
		}
	}
	logwtmp("~", "shutdown", "");

	/*
	 * Do a sync early on, so disks start transfers while we're off
	 * killing processes.  Don't worry about writes done before the
	 * processes die, the reboot system call syncs the disks.
	 */
	if (!nflag)
		sync();

	/* Just stop init -- if we fail, we'll restart it. */
	if (kill(1, SIGTSTP) == -1)
		err("SIGTSTP init: %s", strerror(errno));

	/* Ignore the SIGHUP we get when our parent shell dies. */
	(void)signal(SIGHUP, SIG_IGN);

	/* Send a SIGTERM first, a chance to save the buffers. */
	if (kill(-1, SIGTERM) == -1)
		err("SIGTERM processes: %s", strerror(errno));

	/*
	 * After the processes receive the signal, start the rest of the
	 * buffers on their way.  Wait 5 seconds between the SIGTERM and
	 * the SIGKILL to give everybody a chance.
	 */
	sleep(2);
	if (!nflag)
		sync();
	sleep(3);

	for (i = 1;; ++i) {
		if (kill(-1, SIGKILL) == -1) {
			if (errno == ESRCH)
				break;
			goto restart;
		}
		if (i > 5) {
			(void)fprintf(stderr,
			    "WARNING: some process(es) wouldn't die\n");
			break;
		}
		(void)sleep(2 * i);
	}

	reboot(howto);
	/* FALLTHROUGH */

restart:
	sverrno = errno;
	err("%s%s", kill(1, SIGHUP) == -1 ? "(can't restart init): " : "",
	    strerror(sverrno));
	/* NOTREACHED */
}

void
usage()
{
	(void)fprintf(stderr, "usage: %s [-nq]\n", dohalt ? "halt" : "reboot");
	exit(1);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "%s: ", dohalt ? "halt" : "reboot");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
