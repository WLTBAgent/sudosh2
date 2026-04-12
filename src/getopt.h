/* Declarations for getopt.
   Copyright (C) 1989, 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this program; if not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _GETOPT_H
#define _GETOPT_H 1

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Modern systems (glibc 2.x, musl, BSDs, macOS) all provide getopt,
 * getopt_long, and getopt_long_only in their system headers via <unistd.h>
 * or <getopt.h>. Only use our bundled declarations on truly ancient systems
 * that lack native getopt support.
 *
 * The old code declared `extern int getopt();` (K&R, no prototype) which
 * conflicts with modern glibc's `int getopt(int, char *const *, const char *)`
 * when both getopt.h and <unistd.h> are included (gcc-15/glibc-2.39+).
 *
 * Fixes: https://github.com/WLTBAgent/sudosh2/issues/53
 */
#if defined(__GNU_LIBRARY__) || defined(__linux__) || defined(__FreeBSD__) || \
    defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || \
    defined(__DragonFly__)

/* Use system getopt — <unistd.h> provides getopt on glibc/musl systems.
 * Some systems have a separate <getopt.h> for getopt_long. */
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#else
/* Legacy/ancient systems — use bundled getopt declarations */

/* For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

    extern char *optarg;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `getopt'.

   On entry to `getopt', zero means this is the first call; initialize.

   When `getopt' returns EOF, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `optind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

    extern int optind;

/* Callers store zero here to inhibit the error message `getopt' prints
   for unrecognized options.  */

    extern int opterr;

/* Set to an option character which was unrecognized.  */

    extern int optopt;

/* Describe the long-named options requested by the application.
   The LONG_OPTIONS argument to getopt_long or getopt_long_only is a vector
   of `struct option' terminated by an element containing a name which is
   zero.

   The field `has_arg' is:
   no_argument		(or 0) if the option does not take an argument,
   required_argument	(or 1) if the option requires an argument,
   optional_argument 	(or 2) if the option takes an optional argument.

   If the field `flag' is not NULL, it points to a variable that is set
   to the value given in the field `val' when the option is found, but
   left unchanged if the option is not found.

   To have a long-named option do something other than set an `int' to
   a compiled-in constant, such as set a value from `optarg', set the
   option's `flag' field to zero and its `val' field to a nonzero
   value (the equivalent single-letter option character, if there is
   one).  For long options that have a zero `flag' field, `getopt'
   returns the contents of the `val' field.  */

    struct option {
#if	__STDC__
	const char *name;
#else
	char *name;
#endif
	/* has_arg can't be an enum because some compilers complain about
	   type mismatches in all the code that assumes it is an int.  */
	int has_arg;
	int *flag;
	int val;
    };

/* Names for the values of the `has_arg' field of `struct option'.  */

#define	no_argument		0
#define required_argument	1
#define optional_argument	2

#if __STDC__
    extern int getopt(int argc, char *const *argv, const char *shortopts);
    extern int getopt_long(int argc, char *const *argv,
			   const char *shortopts,
			   const struct option *longopts, int *longind);
    extern int getopt_long_only(int argc, char *const *argv,
				const char *shortopts,
				const struct option *longopts,
				int *longind);

/* Internal only. Users should not call this directly.  */
    extern int _getopt_internal(int argc, char *const *argv,
				const char *shortopts,
				const struct option *longopts,
				int *longind, int long_only);
#else				/* not __STDC__ */
    extern int getopt();
    extern int getopt_long();
    extern int getopt_long_only();

    extern int _getopt_internal();
#endif				/* not __STDC__ */

#endif /* modern vs legacy systems */

#ifdef	__cplusplus
}
#endif
#endif				/* _GETOPT_H */
