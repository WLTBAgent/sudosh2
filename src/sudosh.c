/*

sudosh - sudo shell that supports input and output logging to syslog

Copyright 2004 and $Date: 2010/07/09 20:05:53 $
                Douglas Richard Hanks Jr.

Licensed under the Open Software License version 2.0

This program is free software; you can redistribute it
and/or modify it under the terms of the Open Software
License, version 2.0 by Lauwrence E. Rosen.

This program is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the Open Software License for details.

*/

#include "getopt.h"
#include "struct.h"
#include "super.h"
#ifdef __linux__
#include <sys/ioctl.h>
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>
#else
/* Fallback definitions for systems without <linux/fs.h> (e.g., musl libc) */
#ifndef FS_IOC_GETFLAGS
#define FS_IOC_GETFLAGS _IOR('f', 1, long)
#endif
#ifndef FS_IOC_SETFLAGS
#define FS_IOC_SETFLAGS _IOW('f', 2, long)
#endif
#ifndef FS_APPEND_FL
#define FS_APPEND_FL 0x00000020
#endif
#endif
#endif

#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#define DO_WRITE(a, b, c) do_write(a, b, c, __FILE__, __LINE__)

static struct termios termorig;
static struct winsize winorig;

struct pst {
  char *master;
  char *slave;
  int mfd;
  int sfd;
} pspair;

struct s_file {
  int fd;
  int bytes;
  struct stat stat;
  struct stat cstat;
  struct stat tstat;
  char name[BUFSIZ];
  char str[BUFSIZ];
};

struct s_file script;
#ifdef RECORDINPUT
struct s_file input;
#endif

struct s_file timing;

struct s_env {
  char str[BUFSIZ];
  char *ptr;
};

struct s_user {
  char to[BUFSIZ];
  char from[BUFSIZ];
  char *vshell;
  struct s_env home;
  struct s_env to_home;
  struct s_env term;
  struct s_env path;
  struct s_env mail;
  struct s_env shell;
  struct s_env logname;
  struct passwd *pw;
};

struct s_user user;
struct s_option sudosh_option;

static char *progname;
char start_msg[BUFSIZ];

int loginshell = 0;
uid_t orig_euid;

static void bye(int);
static void newwinsize(int);
static void prepchild(struct pst *);
static void rawmode(int);
static int findms(struct pst *);
void mysyslog(int, const char *, ...);
char *rand2str(size_t len);
int do_write(int, void *, size_t, char *, unsigned int);
void set_file_flag(struct s_file *file, int flag);
void unset_file_flag(struct s_file *file, int flag);
void set_perms_and_close_file(struct s_file *file);
void set_perms_and_open_file(struct s_file *file);
int written = 0;

extern void parse(option *, const char *);

extern int unlockpt(int);
extern int grantpt(int);
extern char *ptsname(int);

extern char *optarg;
extern int optind;

int main(int argc, char *argv[], char *environ[]) {
  int n = 1;
  int valid = -1;
  char iobuf[BUFSIZ];
  char sysconfdir[BUFSIZ];
  char c_str[BUFSIZ];
  char c_command[BUFSIZ];
  char *p = NULL;
  char *rand = rand2str(16);
  time_t now = time((time_t *)NULL);
  struct stat s;
  struct sigaction saterm;
  struct sigaction sawinch;
  struct sigaction sachild;
  struct timeval tv;
  double oldtime, newtime;
  struct stat ttybuf;
  int c;
  char argtest[BUFSIZ];

  user.vshell = NULL;
  user.shell.ptr = NULL;
  user.home.ptr = NULL;
  user.term.ptr = NULL;

  progname = argv[0];

  if ((p = (char *)strrchr(progname, '/')) != NULL)
    progname = p + 1;

  if (*progname == '-')
    loginshell = 1;

  /* Who are you? */
  user.pw = getpwuid((uid_t)getuid());

  if (user.pw == NULL) {
    fprintf(stderr, "I do not know who you are.  Stopping.\n");
    perror("getpwuid");
    exit(EXIT_FAILURE);
  }

  snprintf(user.to, BUFSIZ, "%s", user.pw->pw_name);

  user.term.ptr = getenv("TERM");

  if (user.term.ptr == NULL)
    user.term.ptr = "dumb";

  if (strlen(user.term.ptr) < 1)
    user.term.ptr = "dumb";

  snprintf(sysconfdir, BUFSIZ - 1, "%s/sudosh.conf", SYSCONFDIR);
  parse(&sudosh_option, sysconfdir);

  while ((c = getopt(argc, argv, "c:hivV")) != EOF) {
    switch (c) {
    case 'c':
      //              fprintf(stderr,"optarg is [%s]\n",optarg);
      snprintf(user.from, BUFSIZ, "%s", user.pw->pw_name);
      snprintf(c_str, BUFSIZ, "%s", optarg);
      snprintf(c_command, BUFSIZ, "%s", optarg);
      p = strchr(c_str, ' ');
      if (p) {
        p[0] = 0;
        //              fprintf(stderr,"args=%s\n",c_args);
      }

      if (c_str[0] != 0) {
        // Test for methods of escape
        if (strchr(sudosh_option.argallow, '*') == NULL &&
            (strchr(c_command, ';') != NULL || strchr(c_command, '&') != NULL ||
             strchr(c_command, '|') != NULL || strchr(c_command, '<') != NULL ||
             strchr(c_command, '>') != NULL ||
             strchr(c_command, '`') != NULL)) {
          fprintf(stderr,
                  "\"%s\" isn't allowed to be executed with process or "
                  "redirect controls.\n",
                  c_command);
          exit(EXIT_FAILURE);
        }

        //              fprintf(stderr,"Testing c\n");
        // Make sure that c_str is in argallow

        sprintf(argtest, "$%.100s$", c_str);
        //              fprintf(stderr,"Testing for %s\n",argtest);

        if (strstr(sudosh_option.argallow, argtest) != NULL ||
            strchr(sudosh_option.argallow, '*') != NULL) {
          FILE *f;
          snprintf(script.name, (size_t)BUFSIZ - 1,
                   "%.1024s/%.128s%c%.128s%cinteractive%c%i%c%.128s",
                   sudosh_option.logdir, user.from, sudosh_option.fdl, user.to,
                   sudosh_option.fdl, sudosh_option.fdl, (int)now,
                   sudosh_option.fdl, rand);

          f = fopen(script.name, "w");

          if (f == (FILE *)0) {
            fprintf(stderr, "%.100s: %.100s (%i)\n", script.name,
                    strerror(errno), errno);
            exit(EXIT_FAILURE);
          }

          fprintf(f, "%.256s\n", c_str);
          fclose(f);

          execl("/bin/sh", "sh", "-c", c_command, (char *)0);
          exit(EXIT_SUCCESS);
          break;
        } else {
          fprintf(stderr, "\"%s\" isn't allowed to be executed.\n", c_str);
          exit(EXIT_FAILURE);
          break;
        }
      }
      break;
    case 'h':
    case '?':
      fprintf(stdout,
              "Usage: sudosh\n"
              "sudo shell that supports input and output logging to syslog\n"
              "\n"
              "-h, --help	display this help and exit\n"
              "-i, --init	initialize logdir (mkdir and chmod) (ignored "
              "for compatibility)\n"
              "-v, --version	output version information and exit\n"
              "\n"
              "Report bugs to <%s>\n",
              PACKAGE_BUGREPORT);
      exit(EXIT_SUCCESS);
      break;
    case 'i':
      fprintf(stdout,
              "Ignoring initialize option, this is done automatically\n");
      exit(EXIT_SUCCESS);
      break;
    case 'v':
    case 'V':
      fprintf(stdout, "%s version %s\n", PACKAGE_NAME, VERSION);
      exit(EXIT_SUCCESS);
      break;
    default:
      fputs("Try `sudosh -h' for more information.\n", stderr);
      exit(EXIT_FAILURE);
      break;
    }
  }

  if (ttyname(0) != NULL) {
    if (stat(ttyname(0), &ttybuf) == 0) {
      if ((getpwuid(ttybuf.st_uid)->pw_name) == NULL) {
        fprintf(stderr, "I have no idea who you are.\n");
        exit(EXIT_FAILURE);
      }
      snprintf(user.from, BUFSIZ, "%s", getpwuid(ttybuf.st_uid)->pw_name);
    } else {
      fprintf(stderr, "Couldn't stat %s\n", ttyname(0));
      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "%s: couldn't get your controlling terminal.\n", progname);
    exit(EXIT_FAILURE);
  }
  user.pw = getpwuid((uid_t)getuid());

  snprintf(user.home.str, BUFSIZ - 1, "HOME=%s", user.pw->pw_dir);
  snprintf(user.to_home.str, BUFSIZ, "%s", user.pw->pw_dir);
  snprintf(user.term.str, BUFSIZ - 1, "TERM=%s", user.term.ptr);

#ifdef HAVE_GETUSERSHELL
  if ((user.shell.ptr = getenv("SHELL")) == NULL)
    user.shell.ptr = user.pw->pw_shell;

  /* check against /etc/shells to make sure it's a real shell */
  setusershell();
  while ((user.vshell = (char *)getusershell()) != (char *)0) {
    if (strcmp(user.shell.ptr, user.vshell) == 0)
      valid = 1;
  }
  endusershell();

  if (valid != 1) {
    if (user.shell.ptr == NULL) {
      fprintf(stderr, "Could not determine a valid shell.\n");
      if (sudosh_option.priority != -1)
        mysyslog(sudosh_option.priority, "Could not determine a valid shell");
      exit(EXIT_FAILURE);
    } else {
      fprintf(stderr, "%s is not in /etc/shells\n", user.shell.ptr);
      mysyslog(sudosh_option.priority, "%s,%s: %s is not in /etc/shells",
               user.from, ttyname(0), user.shell.ptr);
      exit(EXIT_FAILURE);
    }
  }

  if (stat((const char *)user.shell.ptr, &s) == -1) {
    fprintf(stderr, "Shell %s doesn't exist.\n", user.shell.ptr);
    if (sudosh_option.priority != -1)
      mysyslog(sudosh_option.priority, "%s,%s: shell %s doesn't exist.",
               user.from, ttyname(0), user.shell.ptr);
    exit(EXIT_FAILURE);
  }
#else
  user.shell.ptr = user.pw->pw_shell;
#endif /* HAVE_GETUSERSHELL */

  if (loginshell)
    user.shell.ptr = sudosh_option.defshell;

  snprintf(script.name, (size_t)BUFSIZ - 1,
           "%.1024s/%.128s%c%.128s%cscript%c%i%c%.128s", sudosh_option.logdir,
           user.from, sudosh_option.fdl, user.to, sudosh_option.fdl,
           sudosh_option.fdl, (int)now, sudosh_option.fdl, rand);
  snprintf(timing.name, (size_t)BUFSIZ - 1,
           "%.1024s/%.128s%c%.128s%ctime%c%i%c%.128s", sudosh_option.logdir,
           user.from, sudosh_option.fdl, user.to, sudosh_option.fdl,
           sudosh_option.fdl, (int)now, sudosh_option.fdl, rand);
#ifdef RECORDINPUT
  snprintf(input.name, (size_t)BUFSIZ - 1,
           "%.1024s/%.128s%c%.128s%cinput%c%i%c%.128s", sudosh_option.logdir,
           user.from, sudosh_option.fdl, user.to, sudosh_option.fdl,
           sudosh_option.fdl, (int)now, sudosh_option.fdl, rand);
#endif
  snprintf(start_msg, BUFSIZ - 1,
           "starting session for %.128s as %.128s, tty %.128s, shell %.128s",
           user.from, user.to, ttyname(0), user.shell.ptr);

  set_perms_and_open_file(&script);
  set_perms_and_open_file(&timing);
#ifdef RECORDINPUT
  set_perms_and_open_file(&input);
#endif

  if (sudosh_option.priority != -1)
    mysyslog(sudosh_option.priority, start_msg);
  rawmode(0);

  if (findms(&pspair) < 0) {
    perror("open pty failed");
    bye(EXIT_FAILURE);
  }

  switch (fork()) {
  case 0:
    close(pspair.mfd);
    prepchild(&pspair);
  case -1:
    perror("fork failed");
    bye(EXIT_FAILURE);
  default:
    close(pspair.sfd);
  }

  orig_euid = geteuid();

  if (seteuid(getuid()) != 0) {
    perror("setuid failed");
    bye(EXIT_FAILURE);
  }

  memset(&sawinch, 0, sizeof sawinch);
  sawinch.sa_handler = newwinsize;
  sawinch.sa_flags = SA_RESTART;
  sigaction(SIGWINCH, &sawinch, (struct sigaction *)0);

  memset(&saterm, 0, sizeof saterm);
  saterm.sa_handler = bye;
  sigaction(SIGTERM, &sawinch, (struct sigaction *)0);

  memset(&sachild, 0, sizeof sachild);
  sachild.sa_handler = bye;
  sigaction(SIGCHLD, &sachild, (struct sigaction *)0);

  oldtime = time(NULL);

  while (n > 0) {
    fd_set readfds;

    FD_ZERO(&readfds);
    FD_SET(pspair.mfd, &readfds);
    FD_SET(0, &readfds);

    gettimeofday((struct timeval *)&tv, NULL);
    if (select(pspair.mfd + 1, &readfds, (fd_set *)0, (fd_set *)0,
               (struct timeval *)0) < 0) {

      if (errno == EINTR)
        continue;

      perror("select");
      bye(EXIT_FAILURE);
    }

    if (FD_ISSET(pspair.mfd, &readfds)) {
      if ((n = read(pspair.mfd, iobuf, sizeof(iobuf))) > 0) {
        DO_WRITE(1, iobuf, n);
        script.bytes += DO_WRITE(script.fd, iobuf, n);
      }
      newtime = tv.tv_sec + (double)tv.tv_usec / 1000000;
      snprintf(timing.str, BUFSIZ - 1, "%f %i\n", newtime - oldtime, n);
      timing.bytes += DO_WRITE(timing.fd, &timing.str, strlen(timing.str));
      oldtime = newtime;
    }

    if (FD_ISSET(0, &readfds)) {
      if ((n = read(0, iobuf, BUFSIZ)) > 0) {
        DO_WRITE(pspair.mfd, iobuf, n);
#ifdef RECORDINPUT
        switch (*iobuf) {
        case '\r':
          snprintf(input.str, BUFSIZ - 1, "\n");
          break;
        case 0x003:
          snprintf(input.str, BUFSIZ - 1, "(CTRL-C)");
          break;
        case 0x004:
          snprintf(input.str, BUFSIZ - 1, "(CTRL-D)\n");
          break;
        case 0x1a:
          snprintf(input.str, BUFSIZ - 1, "(CTRL-Z)\n");
          break;
        case 0x1b:
          snprintf(input.str, BUFSIZ - 1, "(ESC)");
          break;
        default:
          DO_WRITE(input.fd, iobuf, 1);
          written = 1;
          break;
        }

        if (written == 0) {
          DO_WRITE(input.fd, &input.str, strlen(input.str));
        }
#endif
      }
    }
  }

  bye(EXIT_SUCCESS);
  return (0);
}

static int findms(struct pst *p) {
  char *sname;

  if ((p->mfd = open("/dev/ptmx", O_RDWR)) == -1) {
    if ((p->mfd = open("/dev/ptc", O_RDWR)) == -1) {
      perror("Cannot open cloning master pty");
      return -1;
    }
  }

  (void)unlockpt(p->mfd);
  (void)grantpt(p->mfd);

  sname = (char *)ptsname(p->mfd);

  if ((p->sfd = open(sname, O_RDWR)) == -1) {
    perror("open slave pty");
    close(p->mfd);
    return -1;
  }

  p->master = (char *)0;

  p->slave = malloc(strlen(sname) + 1);
  strcpy(p->slave, sname);

#ifdef I_LIST
  if (ioctl(p->sfd, I_LIST, NULL) > 0) {
#ifdef I_FIND
    if (ioctl(p->sfd, I_FIND, "ldterm") != 1 &&
        ioctl(p->sfd, I_FIND, "ldtty") != 1) {
#ifdef I_PUSH
      (void)ioctl(p->sfd, I_PUSH, "ptem");
      (void)ioctl(p->sfd, I_PUSH, "ldterm");
#endif
    }
#endif
  }
#endif

  return p->mfd;
}

static void prepchild(struct pst *pst) {
  int i;
  char *b = NULL;
  char newargv[BUFSIZ];
  char *env_list[] = {user.term.str,    user.home.str, user.shell.str,
                      user.logname.str, user.path.str, NULL};

  close(0);
  close(1);
  close(2);

  setsid();
#ifdef TIOCSCTTY
  (void)ioctl(pst->sfd, TIOCSCTTY, 0);
#endif

  if ((pst->sfd = open(pst->slave, O_RDWR)) < 0)
    exit(EXIT_FAILURE);
  i = dup(0);
  i = dup(0);
  for (i = 3; i < 100; ++i)
    close(i);

#ifdef TCSETS
  (void)ioctl(0, TCSETS, &termorig);
#endif
  (void)ioctl(0, TIOCSWINSZ, &winorig);

  if (setuid(getuid()) != 0) {
    perror("setuid failed");
    bye(EXIT_FAILURE);
  }

  snprintf(newargv, BUFSIZ, "%s", user.shell.ptr);

  if ((b = strrchr(newargv, '/')) == NULL)
    b = newargv;
  *b = '-';

  snprintf(user.shell.str, BUFSIZ - 1, "SHELL=%s", user.shell.ptr);
  snprintf(user.logname.str, BUFSIZ - 1, "LOGNAME=%.128s", user.to);
  if (!strcmp(user.to, "root"))
    snprintf(user.path.str, BUFSIZ - 1, "PATH=/sbin:/bin:/usr/sbin:/usr/bin");
  else
    snprintf(user.path.str, BUFSIZ - 1, "PATH=/usr/bin:/bin:/usr/local/bin");

#ifdef HAVE_SETPENV
  /* I love AIX - setpenv takes care of everything, including chdir() and the
   * env */
  setpenv(user.to, PENV_INIT, (char **)0, (char *)0);
#endif
  if (chdir(user.to_home.str) == -1) {
    fprintf(stderr, "Unable to chdir to %s, using /tmp instead.\n",
            user.pw->pw_dir);
    if (chdir("/tmp") == -1)
      fprintf(stderr, "Unable to chdir to /tmp\n");
  }

  if (sudosh_option.clearenvironment == 0)
    execl(user.shell.ptr, b, (char *)0);
  else
    execle(user.shell.ptr, b, (char *)0, env_list);

  abort();
}

static void rawmode(int ttyfd) {
  static struct termios termnew;

#ifdef TCGETS
  if (ioctl(ttyfd, TCGETS, &termorig) == -1) {
    perror("ioctl TCGETS failed");
    exit(EXIT_FAILURE);
  }
#endif

  if (ioctl(ttyfd, TIOCGWINSZ, &winorig) == -1) {
    perror("ioctl TIOCGWINSZ failed");
    exit(EXIT_FAILURE);
  }

  termnew.c_cc[VEOF] = 1;
  termnew.c_iflag = BRKINT | IXON | IXANY;
  termnew.c_oflag = 0;
  termnew.c_cflag = termorig.c_cflag;
  termnew.c_lflag = 0;

#ifdef TCSETS
  (void)ioctl(ttyfd, TCSETS, &termnew);
#endif
}

static void bye(int signum) {
#ifdef TCSETS
  (void)ioctl(0, TCSETS, &termorig);
#endif

  if (seteuid(orig_euid) != 0) {
    perror("Reacquiring uid 0 failed");
    exit(signum);
  }
  set_perms_and_close_file(&timing);
  set_perms_and_close_file(&script);
#ifdef RECORDINPUT
  set_perms_and_close_file(&input);
#endif
  if (setuid(getuid()) != 0) {
    perror("Dropping setuid 0 failed");
    exit(signum);
  }

  if (sudosh_option.priority != -1)
    mysyslog(sudosh_option.priority,
             "stopping session for %s as %s, tty %s, shell %s", user.from,
             user.to, ttyname(0), user.shell.ptr);
  if (signum == SIGCHLD)
    exit(0);
  exit(signum);
}

static void newwinsize(int signum) {
  int fd;

  if (ioctl(0, TIOCGWINSZ, &winorig) != -1) {
    if ((fd = open(pspair.slave, O_RDWR)) >= 0) {
      (void)ioctl(fd, TIOCSWINSZ, &winorig);
      close(fd);
    }
  }
}

void mysyslog(int pri, const char *fmt, ...) {
  va_list ap;
  char buf[BUFSIZ + 1];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);

  openlog(progname, 0, sudosh_option.facility);
  syslog(pri, "%s", buf);
  closelog();
}

int do_write(int fd, void *buf, size_t size, char *file, unsigned int line) {
  char str[BUFSIZ];
  int s;

  if (fd < 0)
    return -1;

  if ((s = write(fd, buf, size)) < 0) {
    snprintf(str, BUFSIZ - 1, "%s [%s, line %i]: %s\n", progname, file, line,
             strerror(errno));
    perror(str);
    exit(EXIT_FAILURE);
  }

  return s;
}

#ifdef __linux__
void set_file_flag(struct s_file *file, int flag) {
  int flags = 0;
  if (ioctl(file->fd, FS_IOC_GETFLAGS, &flags) == -1) {
    printf("Unable to get inode flags for %s: %s\n", file->name,
           strerror(errno));
    exit(EXIT_FAILURE);
  }
  flags |= flag;
  if (ioctl(file->fd, FS_IOC_SETFLAGS, &flags) == -1) {
    printf("Unable to set inode flag %d for %s: %s\n", flag, file->name,
           strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void unset_file_flag(struct s_file *file, int flag) {
  int flags = 0;
  if (ioctl(file->fd, FS_IOC_GETFLAGS, &flags) == -1) {
    printf("Unable to get inode flags for %s: %s\n", file->name,
           strerror(errno));
    exit(EXIT_FAILURE);
  }
  flags &= ~(flag);
  if (ioctl(file->fd, FS_IOC_SETFLAGS, &flags) == -1) {
    printf("Unable to unset inode flag %d for %s: %s\n", flag, file->name,
           strerror(errno));
    exit(EXIT_FAILURE);
  }
}
#endif

void set_perms_and_close_file(struct s_file *file) {
#ifdef __linux__
  if (sudosh_option.immutable_recordings)
    unset_file_flag(file, FS_APPEND_FL);
#endif
  if (fchmod(file->fd, S_IRUSR | S_IRGRP) == -1) {
    printf("Unable to chmod file %s: %s\n", file->name, strerror(errno));
  }
#ifdef __linux__
  if (sudosh_option.immutable_recordings)
    set_file_flag(file, FS_IMMUTABLE_FL);
#endif
  if (close(file->fd) == -1) {
    printf("Unable to close file %s: %s\n", file->name, strerror(errno));
  }
}

void set_perms_and_open_file(struct s_file *file) {
  file->bytes = 0;
  if ((file->fd = open(file->name, O_WRONLY | O_CREAT | O_EXCL,
                       S_IRUSR | S_IWUSR)) == -1) {
    printf("Unable to open file %s: %s\n", file->name, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (fstat(file->fd, &file->stat) == -1) {
    printf("Unable to stat file %s: %s\n", file->name, strerror(errno));
    exit(EXIT_FAILURE);
  }
#ifdef __linux__
  if (sudosh_option.immutable_recordings)
    set_file_flag(file, FS_APPEND_FL);
#endif
}
