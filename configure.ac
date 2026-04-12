#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.5])
AC_INIT([sudosh2],[1.0.8],[josh@shortcutsolutions.net])
AC_CONFIG_SRCDIR([src/])
AC_CANONICAL_TARGET

# Override default
# test "$sysconfdir" = '${prefix}/etc' && sysconfdir='/etc'

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
AC_PROG_CC

recordinput=0
AC_MSG_CHECKING(to see if you want to log input too)
AC_ARG_ENABLE(recordinput,
[  --enable-recordinput   Enable recording of input keystrokes, default off],
[recordinput=1])
if test $recordinput = "1"; then
  AC_MSG_RESULT(yes)
AC_DEFINE_UNQUOTED(RECORDINPUT,"$recordinput",[input will be recorded])
else
  AC_MSG_RESULT(no)
fi

logdir=
AC_MSG_CHECKING(for logdir)
AC_ARG_WITH(logdir,
[  --with-logdir=PATH      specify path where logfiles will be written],
[
  if test "x$withval" != "x" ; then
    logdir=$withval
  fi
])
if test "x$logdir" = "x" ; then
  case "$host_os" in
    *linux*)
      logdir="/var/log/sudosh"
      ;;
    *solaris*)
      logdir="/var/adm/sudosh"
      ;;
    *hp*)
      logdir="/var/adm/sudosh"
      ;;
    *aix*)
      logdir="/var/adm/sudosh"
      ;;
    *)
      logdir="/tmp/sudosh"
      ;;
  esac
fi
AC_MSG_RESULT($logdir)
AC_DEFINE_UNQUOTED(LOGDIR,"$logdir",[directory for session output])

# Check for /etc/shells
etcshells=
AC_MSG_CHECKING(for /etc/shells)
if test -f /etc/shells; then
  AC_MSG_RESULT(found)
  AC_DEFINE(HAS_ETC_SHELLS, 1,[have /etc/shells])
else
  AC_MSG_RESULT(not found)
  AC_DEFINE(HAS_ETC_SHELLS, 0,[does not have /etc/shells])
fi


# Checks for libraries.
if test "x$prefix" = "xNONE" ; then
    prefix="/usr/local"
    prefix_nondefault="NONE"
fi

# Checks for header files.
AC_HEADER_DIRENT
m4_warn([obsolete],
[The preprocessor macro `STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  ISO C90 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_PROG_EGREP


AC_CHECK_HEADERS(unistd.h stdlib.h string.h errno.h stdarg.h stdio.h)
AC_CHECK_HEADERS(sgtty.h pty.h signal.h time.h sys/time.h fcntl.h stropts.h)
AC_CHECK_HEADERS(util.h termios.h sys/termios.h sys/types.h libutil.h)
AC_CHECK_HEADERS(features.h sys/ioctl.h pwd.h syslog.h usersec.h)
AC_CHECK_HEADERS(limits.h sys/stat.h sys/param.h dirent.h ctype.h strings.h)
AC_CHECK_HEADERS([linux/fs.h], [], [], [#include <sys/ioctl.h>])

AC_CHECK_FUNCS(openpty, openpty=1, openpty=0)
AC_CHECK_FUNCS([getusershell gettimeofday])
AC_CHECK_FUNCS([mkdir rmdir])
AC_CHECK_FUNCS([setpenv])
AC_CHECK_FUNCS([atoi sscanf open read close fflush fclose fopen fread fputs fprintf])
AC_CHECK_FUNCS([strncpy snprintf strcmp strlen strftime isspace localtime])
AC_CHECK_FUNCS([opendir readdir closedir])
AC_CHECK_FUNCS([memchr memmove memset select strrchr bzero])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
m4_warn([obsolete],
[Update your code to rely only on HAVE_SYS_TIME_H,
then remove this warning and the obsolete code below it.
All current systems provide time.h; it need not be checked for.
Not all systems provide sys/time.h, but those that do, all allow
you to include it and time.h simultaneously.])dnl
AC_CHECK_HEADERS_ONCE([sys/time.h])
# Obsolete code to be removed.
if test $ac_cv_header_sys_time_h = yes; then
  AC_DEFINE([TIME_WITH_SYS_TIME],[1],[Define to 1 if you can safely include both <sys/time.h>
	     and <time.h>.  This macro is obsolete.])
fi
# End of obsolete code.


# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_SELECT_ARGTYPES
m4_warn([obsolete],
[your code may safely assume C89 semantics that RETSIGTYPE is void.
Remove this warning and the `AC_CACHE_CHECK' when you adjust the code.])dnl
AC_CACHE_CHECK([return type of signal handlers],[ac_cv_type_signal],[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <signal.h>
],
		 [return *(signal (0, 0)) (0) == 1;])],
		   [ac_cv_type_signal=int],
		   [ac_cv_type_signal=void])])
AC_DEFINE_UNQUOTED([RETSIGTYPE],[$ac_cv_type_signal],[Define as the return type of signal handlers
		    (`int' or `void').])


VLOGDIR=$logdir
VFDL=$filedelimit
PREFIX=$prefix
RECORDINPUT=$recordinput
AC_SUBST([RECORDINPUT])
AC_SUBST([VLOGDIR])
AC_SUBST([VFDL])
AC_SUBST([PREFIX])

temp_mandir=`eval echo $mandir` ; temp_mandir=`eval echo $temp_mandir`
temp_sysconfdir=`eval echo ${sysconfdir}`
sysconfdir=$temp_sysconfdir

AC_DEFINE_UNQUOTED(SYSCONFDIR,"$sysconfdir", [directory to place sudosh.conf])
AC_CONFIG_FILES([Makefile src/Makefile src/sudosh.conf sudosh.1 sudosh-replay.8 sudosh.conf.5])
AC_OUTPUT

echo "Configuration summary:"
echo "======================"
echo ""
echo "  Host type................: $target"
echo "  CC.......................: $CC"
echo "  CFLAGS...................: $CFLAGS"
echo ""
echo "  Package..................: $PACKAGE_NAME"
echo "  Version..................: $VERSION"
echo ""
echo "  Installation prefix......: $prefix"
echo "  Man directory............: $temp_mandir"
echo "  sysconfdir...............: ${sysconfdir}"
if test $recordinput = "1"; then
recording="yes"
else
recording="no"
fi
echo "  recording input..........: $recording"
echo ""
echo "If this summary looks good, type 'make' or 'gmake' to compile"
echo "then 'make install' or 'gmake install' as root to install the"
echo "programs to the above installation prefix."
