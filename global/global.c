/*
 * Copyright (c) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2006,
 *	2007, 2008 Tama Communications Corporation
 *
 * This file is part of GNU GLOBAL.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <ctype.h>
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "getopt.h"

#include "global.h"
#include "regex.h"
#include "const.h"

static void usage(void);
static void help(void);
static void setcom(int);
int decide_tag_by_context(const char *, const char *, const char *);
int main(int, char **);
void completion(const char *, const char *, const char *);
void idutils(const char *, const char *);
void grep(const char *, const char *);
void pathlist(const char *, const char *);
void parsefile(int, char **, const char *, const char *, const char *, int);
int search(const char *, const char *, const char *, const char *, int);
void tagsearch(const char *, const char *, const char *, const char *, int);
void encode(char *, int, const char *);

const char *localprefix;		/* local prefix		*/
int aflag;				/* [option]		*/
int cflag;				/* command		*/
int fflag;				/* command		*/
int gflag;				/* command		*/
int Gflag;				/* [option]		*/
int iflag;				/* [option]		*/
int Iflag;				/* command		*/
int lflag;				/* [option]		*/
int nflag;				/* [option]		*/
int oflag;				/* [option]		*/
int Oflag;				/* [option]		*/
int pflag;				/* command		*/
int Pflag;				/* command		*/
int qflag;				/* [option]		*/
int rflag;				/* [option]		*/
int sflag;				/* [option]		*/
int tflag;				/* [option]		*/
int Tflag;				/* [option]		*/
int uflag;				/* command		*/
int vflag;				/* [option]		*/
int xflag;				/* [option]		*/
int show_version;
int show_help;
int nofilter;
int nosource;				/* undocumented command */
int debug;
int format;
int type;				/* path conversion type */
char cwd[MAXPATHLEN+1];			/* current directory	*/
char root[MAXPATHLEN+1];		/* root of source tree	*/
char dbpath[MAXPATHLEN+1];		/* dbpath directory	*/
char *context_file;
char *context_lineno;

static void
usage(void)
{
	if (!qflag)
		fputs(usage_const, stderr);
	exit(2);
}
static void
help(void)
{
	fputs(usage_const, stdout);
	fputs(help_const, stdout);
	exit(0);
}

#define RESULT		128
#define FROM_HERE	129
#define SORT_FILTER     1
#define PATH_FILTER     2
#define BOTH_FILTER     (SORT_FILTER|PATH_FILTER)

static struct option const long_options[] = {
	{"absolute", no_argument, NULL, 'a'},
	{"completion", no_argument, NULL, 'c'},
	{"regexp", required_argument, NULL, 'e'},
	{"file", no_argument, NULL, 'f'},
	{"local", no_argument, NULL, 'l'},
	{"nofilter", optional_argument, NULL, 'n'},
	{"grep", no_argument, NULL, 'g'},
	{"basic-regexp", no_argument, NULL, 'G'},
	{"ignore-case", no_argument, NULL, 'i'},
	{"idutils", no_argument, NULL, 'I'},
	{"other", no_argument, NULL, 'o'},
	{"only-other", no_argument, NULL, 'O'},
	{"print-dbpath", no_argument, NULL, 'p'},
	{"path", no_argument, NULL, 'P'},
	{"quiet", no_argument, NULL, 'q'},
	{"reference", no_argument, NULL, 'r'},
	{"rootdir", no_argument, NULL, 'r'},
	{"symbol", no_argument, NULL, 's'},
	{"tags", no_argument, NULL, 't'},
	{"through", no_argument, NULL, 'T'},
	{"update", no_argument, NULL, 'u'},
	{"verbose", no_argument, NULL, 'v'},
	{"cxref", no_argument, NULL, 'x'},

	/* long name only */
	{"from-here", required_argument, NULL, FROM_HERE},
	{"debug", no_argument, &debug, 1},
	{"version", no_argument, &show_version, 1},
	{"help", no_argument, &show_help, 1},
	{"result", required_argument, NULL, RESULT},
	{"nosource", no_argument, &nosource, 1},
	{ 0 }
};

static int command;
static void
setcom(int c)
{
	if (command == 0)
		command = c;
	else if (command != c)
		usage();
}
/*
 * decide_tag_by_context: decide tag type by context
 *
 *	i)	tag	tag name
 *	i)	file	context file
 *	i)	lineno	context lineno
 *	r)		GTAGS, GRTAGS, GSYMS
 */
int
decide_tag_by_context(const char *tag, const char *file, const char *lineno)
{
	char path[MAXPATHLEN+1], s_fid[32];
	const char *tagline, *p;
	DBOP *dbop;
	int db = GSYMS;

	if (normalize(file, get_root_with_slash(), cwd, path, sizeof(path)) == NULL)
		die("'%s' is out of source tree.", file);
	/*
	 * get file id
	 */
	if (gpath_open(dbpath, 0) < 0)
		die("GPATH not found.");
	if ((p = gpath_path2fid(path, NULL)) == NULL)
		die("path name in the context is not found.");
	strlimcpy(s_fid, p, sizeof(s_fid));
	gpath_close();
	/*
	 * read btree records directly to avoid the overhead.
	 */
	dbop = dbop_open(makepath(dbpath, dbname(GTAGS), NULL), 0, 0, 0);
	tagline = dbop_first(dbop, tag, NULL, 0);
	if (tagline)
		db = GTAGS;
	for (; tagline; tagline = dbop_next(dbop)) {
		/*
		 * examine whether the definition record include the context.
		 */
		p = locatestring(tagline, s_fid, MATCH_AT_FIRST);
		if (p != NULL && *p == ' ') {
			for (p++; *p && *p != ' '; p++)
				;
			if (*p++ != ' ')
				die("Impossible!");
			if ((p = locatestring(p, lineno, MATCH_AT_FIRST)) != NULL && *p == ' ') {
				db = GRTAGS;
				break;
			}
		}
	}
        dbop_close(dbop);
	return db;
}
int
main(int argc, char **argv)
{
	const char *av = NULL;
	int db;
	int optchar;
	int option_index = 0;

	while ((optchar = getopt_long(argc, argv, "ace:ifgGIlnoOpPqrstTuvx", long_options, &option_index)) != EOF) {
		switch (optchar) {
		case 0:
			break;
		case 'a':
			aflag++;
			break;
		case 'c':
			cflag++;
			setcom(optchar);
			break;
		case 'e':
			av = optarg;
			break;
		case 'f':
			fflag++;
			xflag++;
			setcom(optchar);
			break;
		case 'l':
			lflag++;
			break;
		case 'n':
			nflag++;
			if (optarg) {
				if (!strcmp(optarg, "sort"))
					nofilter |= SORT_FILTER;
				else if (!strcmp(optarg, "path"))
					nofilter |= PATH_FILTER;
			} else {
				nofilter = BOTH_FILTER;
			}
			break;
		case 'g':
			gflag++;
			setcom(optchar);
			break;
		case 'G':
			Gflag++;
			break;
		case 'i':
			iflag++;
			break;
		case 'I':
			Iflag++;
			setcom(optchar);
			break;
		case 'o':
			oflag++;
			break;
		case 'O':
			Oflag++;
			break;
		case 'p':
			pflag++;
			setcom(optchar);
			break;
		case 'P':
			Pflag++;
			setcom(optchar);
			break;
		case 'q':
			qflag++;
			setquiet();
			break;
		case 'r':
			rflag++;
			break;
		case 's':
			sflag++;
			break;
		case 't':
			tflag++;
			break;
		case 'T':
			Tflag++;
			break;
		case 'u':
			uflag++;
			setcom(optchar);
			break;
		case 'v':
			vflag++;
			break;
		case 'x':
			xflag++;
			break;
		case FROM_HERE:
			{
			char *p = optarg;
			const char *usage = "usage: global --from-here=lineno:path.";

			context_lineno = p;
			while (*p && isdigit(*p))
				p++;
			if (*p != ':')
				die_with_code(2, usage);
			*p++ = '\0';
			if (!*p)
				die_with_code(2, usage);
			context_file = p;
			}
			break;
		case RESULT:
			if (!strcmp(optarg, "ctags-x"))
				format = FORMAT_CTAGS_X;
			else if (!strcmp(optarg, "ctags-xid"))
				format = FORMAT_CTAGS_XID;
			else if (!strcmp(optarg, "ctags"))
				format = FORMAT_CTAGS;
			else if (!strcmp(optarg, "path"))
				format = FORMAT_PATH;
			else if (!strcmp(optarg, "grep"))
				format = FORMAT_GREP;
			else if (!strcmp(optarg, "cscope"))
				format = FORMAT_CSCOPE;
			else
				die_with_code(2, "unknown format type for the --result option.");
			break;
		default:
			usage();
			break;
		}
	}
	if (qflag)
		vflag = 0;
	if (show_help)
		help();

	argc -= optind;
	argv += optind;
	/*
	 * At first, we pickup pattern from -e option. If it is not found
	 * then use argument which is not option.
	 */
	if (!av)
		av = (argc > 0) ? *argv : NULL;

	if (show_version)
		version(av, vflag);
	/*
	 * invalid options.
	 */
	if (sflag && rflag)
		die_with_code(2, "both of -s and -r are not allowed.");
	/*
	 * only -c, -u, -P and -p allows no argument.
	 */
	if (!av) {
		switch (command) {
		case 'c':
		case 'u':
		case 'p':
		case 'P':
			break;
		default:
			usage();
			break;
		}
	}
	/*
	 * -u and -p cannot have any arguments.
	 */
	if (av) {
		switch (command) {
		case 'u':
		case 'p':
			usage();
		default:
			break;
		}
	}
	if (tflag)
		xflag = 0;
	if (nflag > 1)
		nosource = 1;	/* to keep compatibility */
	/*
	 * remove leading blanks.
	 */
	if (!Iflag && !gflag && av)
		for (; *av == ' ' || *av == '\t'; av++)
			;
	if (cflag && av && isregex(av))
		die_with_code(2, "only name char is allowed with -c option.");
	/*
	 * get path of following directories.
	 *	o current directory
	 *	o root of source tree
	 *	o dbpath directory
	 *
	 * if GTAGS not found, getdbpath doesn't return.
	 */
	getdbpath(cwd, root, dbpath, (pflag && vflag));
	if (Iflag && !test("f", makepath(root, "ID", NULL)))
		die("You must have idutils's index at the root of source tree.");
	/*
	 * print dbpath or rootdir.
	 */
	if (pflag) {
		fprintf(stdout, "%s\n", (rflag) ? root : dbpath);
		exit(0);
	}
	/*
	 * incremental update of tag files.
	 */
	if (uflag) {
		STRBUF	*sb = strbuf_open(0);
		char *gtags = usable("gtags");

		if (!gtags)
			die("gtags command not found.");
		if (chdir(root) < 0)
			die("cannot change directory to '%s'.", root);
		strbuf_puts(sb, gtags);
		strbuf_puts(sb, " -i");
		if (vflag)
			strbuf_putc(sb, 'v');
		strbuf_putc(sb, ' ');
		strbuf_puts(sb, dbpath);
		if (system(strbuf_value(sb)))
			exit(1);
		strbuf_close(sb);
		exit(0);
	}
	/*
	 * complete function name
	 */
	if (cflag) {
		completion(dbpath, root, av);
		exit(0);
	}
	/*
	 * make local prefix.
	 * local prefix must starts with './' and ends with '/'.
	 */
	if (lflag) {
		STRBUF *sb = strbuf_open(0);

		strbuf_putc(sb, '.');
		if (strcmp(root, cwd) != 0) {
			char *p = cwd + strlen(root);
			if (*p != '/')
				strbuf_putc(sb, '/');
			strbuf_puts(sb, p);
		}
		strbuf_putc(sb, '/');
		localprefix = check_strdup(strbuf_value(sb));
		strbuf_close(sb);
#ifdef DEBUG
		fprintf(stderr, "root=%s\n", root);
		fprintf(stderr, "cwd=%s\n", cwd);
		fprintf(stderr, "localprefix=%s\n", localprefix);
#endif
	}
	/*
	 * decide tag type.
	 */
	if (context_file) {
		if (isregex(av))
			die_with_code(2, "regular expression is not allowed with the --from-here option.");
		db = decide_tag_by_context(av, context_file, context_lineno);
	} else {
		db = (rflag) ? GRTAGS : ((sflag) ? GSYMS : GTAGS);
	}
	/*
	 * decide format.
	 * The --result option is given to priority more than the -t and -x option.
	 */
	if (format == 0) {
		if (tflag) { 			/* ctags format */
			format = FORMAT_CTAGS;
		} else if (xflag) {		/* print details */
			format = FORMAT_CTAGS_X;
		} else {			/* print just a file name */
			format = FORMAT_PATH;
		}
	}
	/*
	 * decide path conversion type.
	 */
	if (nofilter & PATH_FILTER)
		type = PATH_THROUGH;
	else if (aflag)
		type = PATH_ABSOLUTE;
	else
		type = PATH_RELATIVE;
	/*
	 * exec lid(idutils).
	 */
	if (Iflag) {
		chdir(root);
		idutils(av, dbpath);
	}
	/*
	 * search pattern (regular expression).
	 */
	else if (gflag) {
		chdir(root);
		grep(av, dbpath);
	}
	/*
	 * locate paths including the pattern.
	 */
	else if (Pflag) {
		chdir(root);
		pathlist(av, dbpath);
	}
	/*
	 * parse source files.
	 */
	else if (fflag) {
		parsefile(argc, argv, cwd, root, dbpath, db);
	}
	/*
	 * tag search.
	 */
	else {
		tagsearch(av, cwd, root, dbpath, db);
	}
	return 0;
}
/*
 * completion: print completion list of specified prefix
 *
 *	i)	dbpath	dbpath directory
 *	i)	root	root directory
 *	i)	prefix	prefix of primary key
 */
void
completion(const char *dbpath, const char *root, const char *prefix)
{
	int flags = GTOP_KEY;
	GTOP *gtop;
	GTP *gtp;
	int db;

	flags |= GTOP_NOREGEX;
	if (prefix && *prefix == 0)	/* In the case global -c '' */
		prefix = NULL;
	if (prefix)
		flags |= GTOP_PREFIX;
	db = (sflag) ? GSYMS : GTAGS;
	gtop = gtags_open(dbpath, root, db, GTAGS_READ, 0);
	for (gtp = gtags_first(gtop, prefix, flags); gtp; gtp = gtags_next(gtop)) {
		fputs(gtp->tag, stdout);
		fputc('\n', stdout);
	}
	gtags_close(gtop);
}
/*
 * Output filter
 *
 * (1) Old architecture (- GLOBAL-4.7.8)
 *
 * process1          process2       process3
 * +=============+  +===========+  +===========+
 * |global(write)|->|sort filter|->|path filter|->[stdout]
 * +=============+  +===========+  +===========+
 *
 * (2) Recent architecture (GLOBAL-5.0 - 5.3)
 *
 * 1 process
 * +===========================================+
 * |global(write)->[sort filter]->[path filter]|->[stdout]
 * +===========================================+
 *
 * (3) Current architecture (GLOBAL-5.4 -)
 *
 * 1 process
 * +===========================================+
 * |[sort filter]->global(write)->[path filter]|->[stdout]
 * +===========================================+
 *
 * Sort filter is implemented in gtagsop module (libutil/gtagsop.c).
 * Path filter is implemented in pathconvert module (libutil/pathconvert.c).
 */
/*
 * print number of object.
 *
 * This procedure is commonly used except for the -P option.
 */
void
print_count(int number)
{
	const char *target = format == FORMAT_PATH ? "file" : "object";

	switch (number) {
	case 0:
		fprintf(stderr, "object not found");
		break;
	case 1:
		fprintf(stderr, "1 %s located", target);
		break;
	default:
		fprintf(stderr, "%d %ss located", number, target);
		break;
	}
}
/*
 * idutils:  lid(idutils) pattern
 *
 *	i)	pattern	POSIX regular expression
 *	i)	dbpath	GTAGS directory
 */
void
idutils(const char *pattern, const char *dbpath)
{
	FILE *ip;
	CONVERT *cv;
	STRBUF *ib = strbuf_open(0);
	char edit[IDENTLEN+1];
	const char *path, *lno, *lid;
	int linenum, count;
	char *p, *grep;

	lid = usable("lid");
	if (!lid)
		die("lid(idutils) not found.");
	/*
	 * convert spaces into %FF format.
	 */
	encode(edit, sizeof(edit), pattern);
	/*
	 * make lid command line.
	 * Invoke lid with the --result=grep option to generate grep format.
	 */
	strbuf_puts(ib, lid);
	strbuf_puts(ib, " --separator=newline");
	if (!tflag && !xflag)
		strbuf_puts(ib, " --result=filenames --key=none");
	else
		strbuf_puts(ib, " --result=grep");
	if (iflag)
		strbuf_puts(ib, " --ignore-case");
	strbuf_putc(ib, ' ');
	strbuf_puts(ib, quote_string(pattern));
	if (debug)
		fprintf(stderr, "idutils: %s\n", strbuf_value(ib));
	if (!(ip = popen(strbuf_value(ib), "r")))
		die("cannot execute '%s'.", strbuf_value(ib));
	cv = convert_open(type, format, root, cwd, dbpath, stdout);
	count = 0;
	while ((grep = strbuf_fgets(ib, ip, STRBUF_NOCRLF)) != NULL) {
		p = grep;
		/* extract path name */
		path = p;
		while (*p && *p != ':')
			p++;
		if ((xflag || tflag) && !*p)
			die("invalid lid(idutils) output format. '%s'", grep);
		*p++ = 0;
		/* normalize path name */
		path = makepath(".", path, NULL);
		if (lflag) {
			if (!locatestring(path, localprefix, MATCH_AT_FIRST))
				continue;
		}
		count++;
		switch (format) {
		case FORMAT_PATH:
			convert_put(cv, path);
			break;
		default:
			/* extract line number */
			while (*p && isspace(*p))
				p++;
			lno = p;
			while (*p && isdigit(*p))
				p++;
			if (*p != ':')
				die("invalid lid(idutils) output format. '%s'", grep);
			*p++ = 0;
			linenum = atoi(lno);
			if (linenum <= 0)
				die("invalid lid(idutils) output format. '%s'", grep);
			/*
			 * print out.
			 */
			convert_put_using(cv, edit, path, linenum, p);
			break;
		}
	}
	if (pclose(ip) < 0)
		die("terminated abnormally.");
	convert_close(cv);
	strbuf_close(ib);
	if (vflag) {
		print_count(count);
		fprintf(stderr, " (using idutils index in '%s').\n", dbpath);
	}
}
/*
 * grep: grep pattern
 *
 *	i)	pattern	POSIX regular expression
 */
void
grep(const char *pattern, const char *dbpath)
{
	FILE *fp;
	CONVERT *cv;
	GFIND *gp;
	STRBUF *ib = strbuf_open(MAXBUFLEN);
	const char *path;
	char edit[IDENTLEN+1];
	const char *buffer;
	int linenum, count;
	int flags = 0;
	int target = GPATH_SOURCE;
	regex_t	preg;

	/*
	 * convert spaces into %FF format.
	 */
	encode(edit, sizeof(edit), pattern);

	if (oflag)
		target = GPATH_BOTH;
	if (Oflag)
		target = GPATH_OTHER;
	if (!Gflag)
		flags |= REG_EXTENDED;
	if (iflag)
		flags |= REG_ICASE;
	if (regcomp(&preg, pattern, flags) != 0)
		die("invalid regular expression.");
	cv = convert_open(type, format, root, cwd, dbpath, stdout);
	count = 0;

	gp = gfind_open(dbpath, localprefix, target);
	while ((path = gfind_read(gp)) != NULL) {
		if (!(fp = fopen(path, "r")))
			die("'%s' not found. Please remake tag files by invoking gtags(1).", path);
		linenum = 0;
		while ((buffer = strbuf_fgets(ib, fp, STRBUF_NOCRLF)) != NULL) {
			linenum++;
			if (regexec(&preg, buffer, 0, 0, 0) == 0) {
				count++;
				if (format == FORMAT_PATH) {
					convert_put(cv, path);
					break;
				} else {
					convert_put_using(cv, edit, path, linenum, buffer);
				}
			}
		}
		fclose(fp);
	}
	gfind_close(gp);
	convert_close(cv);
	strbuf_close(ib);
	regfree(&preg);
	if (vflag) {
		print_count(count);
		fprintf(stderr, " (no index used).\n");
	}
}
/*
 * pathlist: print candidate path list.
 *
 *	i)	dbpath
 */
void
pathlist(const char *pattern, const char *dbpath)
{
	GFIND *gp;
	CONVERT *cv;
	const char *path, *p;
	regex_t preg;
	int count;
	int target = GPATH_SOURCE;

	if (oflag)
		target = GPATH_BOTH;
	if (Oflag)
		target = GPATH_OTHER;
	if (pattern) {
		int flags = 0;
		char edit[IDENTLEN+1];

		if (!Gflag)
			flags |= REG_EXTENDED;
		if (iflag || getconfb("icase_path"))
			flags |= REG_ICASE;
#ifdef _WIN32
		flags |= REG_ICASE;
#endif /* _WIN32 */
		/*
		 * We assume '^aaa' as '^/aaa'.
		 */
		if (*pattern == '^' && *(pattern + 1) != '/') {
			snprintf(edit, sizeof(edit), "^/%s", pattern + 1);
			pattern = edit;
		}
		if (regcomp(&preg, pattern, flags) != 0)
			die("invalid regular expression.");
	}
	if (!localprefix)
		localprefix = "./";
	cv = convert_open(type, format, root, cwd, dbpath, stdout);
	count = 0;

	gp = gfind_open(dbpath, localprefix, target);
	while ((path = gfind_read(gp)) != NULL) {
		/*
		 * skip localprefix because end-user doesn't see it.
		 */
		p = path + strlen(localprefix) - 1;
		if (pattern && regexec(&preg, p, 0, 0, 0) != 0)
			continue;
		if (format == FORMAT_PATH)
			convert_put(cv, path);
		else
			convert_put_using(cv, "path", path, 1, " ");
		count++;
	}
	gfind_close(gp);
	convert_close(cv);
	if (pattern)
		regfree(&preg);
	if (vflag) {
		switch (count) {
		case 0:
			fprintf(stderr, "file not found");
			break;
		case 1:
			fprintf(stderr, "1 file located");
			break;
		default:
			fprintf(stderr, "%d files located", count);
			break;
		}
		fprintf(stderr, " (using '%s').\n", makepath(dbpath, dbname(GPATH), NULL));
	}
}
/*
 * parsefile: parse file to pick up tags.
 *
 *	i)	argc
 *	i)	argv
 *	i)	cwd	current directory
 *	i)	root	root directory of source tree
 *	i)	dbpath	dbpath
 *	i)	db	type of parse
 */
void
parsefile(int argc, char **argv, const char *cwd, const char *root, const char *dbpath, int db)
{
	CONVERT *cv;
	int count = 0;
	int file_count = 0;
	STRBUF *comline = strbuf_open(0);
	STRBUF *path_list = strbuf_open(MAXPATHLEN);
	XARGS *xp;
	char *ctags_x;

	/*
	 * teach parser where is dbpath.
	 */
	set_env("GTAGSDBPATH", dbpath);
	/*
	 * teach parser language mapping.
	 */
	{
		STRBUF *sb = strbuf_open(0);

		if (getconfs("langmap", sb))
			set_env("GTAGSLANGMAP", strbuf_value(sb));
		strbuf_close(sb);
	}
	/*
	 * get parser.
	 */
	if (!getconfs(dbname(db), comline))
		die("cannot get parser for %s.", dbname(db));
	cv = convert_open(type, format, root, cwd, dbpath, stdout);
	if (gpath_open(dbpath, 0) < 0)
		die("GPATH not found.");
	/*
	 * Make a path list while checking the validity of path name.
	 */
	for (; argc > 0; argv++, argc--) {
		const char *av = argv[0];
		char path[MAXPATHLEN+1];

		/*
		 * convert the path into relative from the root directory of source tree.
		 */
		if (normalize(av, get_root_with_slash(), cwd, path, sizeof(path)) == NULL)
			if (!qflag)
				fprintf(stderr, "'%s' is out of source tree.\n", path + 2);
		if (!gpath_path2fid(path, NULL)) {
			if (!qflag)
				fprintf(stderr, "'%s' not found in GPATH.\n", path + 2);
			continue;
		}
		if (!test("f", makepath(root, path, NULL))) {
			if (test("d", NULL)) {
				if (!qflag)
					fprintf(stderr, "'%s' is a directory.\n", av);
			} else {
				if (!qflag)
					fprintf(stderr, "'%s' not found.\n", av);
			}
			continue;
		}
		if (lflag && !locatestring(path, localprefix, MATCH_AT_FIRST))
			continue;
		/*
		 * Add a path to the path list.
		 */
		strbuf_puts0(path_list, path);
		file_count++;
	}
	if (file_count > 0) {
		/*
		 * Execute parser in the root directory of source tree.
		 */
		if (chdir(root) < 0)
			die("cannot move to '%s' directory.", root);
		xp = xargs_open_with_strbuf(strbuf_value(comline), 0, path_list);
		if (format == FORMAT_PATH) {
			SPLIT ptable;
			char curpath[MAXPATHLEN+1];

			curpath[0] = '\0';
			while ((ctags_x = xargs_read(xp)) != NULL) {
				if (split((char *)ctags_x, 4, &ptable) < 4) {
					recover(&ptable);
					die("too small number of parts.\n'%s'", ctags_x);
				}
				if (strcmp(curpath, ptable.part[PART_PATH].start)) {
					strlimcpy(curpath, ptable.part[PART_PATH].start, sizeof(curpath));
					convert_put(cv, curpath);
					count++;
				}
			}
		} else {
			while ((ctags_x = xargs_read(xp)) != NULL) {
				convert_put(cv, ctags_x);
				count++;
			}
		}
		xargs_close(xp);
		if (chdir(cwd) < 0)
			die("cannot move to '%s' directory.", cwd);
	}
	/*
	 * Settlement
	 */
	gpath_close();
	convert_close(cv);
	strbuf_close(comline);
	strbuf_close(path_list);
	if (file_count == 0)
		die("file not found.");
	if (vflag) {
		print_count(count);
		fprintf(stderr, " (no index used).\n");
	}
}
/*
 * search: search specified function 
 *
 *	i)	pattern		search pattern
 *	i)	root		root of source tree
 *	i)	cwd		current directory
 *	i)	dbpath		database directory
 *	i)	db		GTAGS,GRTAGS,GSYMS
 *	r)			count of output lines
 */
/* get next number and seek to the next character */
#define GET_NEXT_NUMBER(p) do {                                                 \
                if (!isdigit(*p))                                              \
                        p++;                                                    \
                for (n = 0; isdigit(*p); p++)                                  \
                        n = n * 10 + (*p - '0');                                \
        } while (0)
int
search(const char *pattern, const char *root, const char *cwd, const char *dbpath, int db)
{
	CONVERT *cv;
	int count = 0;
	GTOP *gtop;
	GTP *gtp;
	int flags = 0;
	STRBUF *sb = NULL, *ib = NULL;
	char curpath[MAXPATHLEN+1], curtag[IDENTLEN+1];
	FILE *fp = NULL;
	const char *src = "";
	int lineno, last_lineno;

	lineno = last_lineno = 0;
	curpath[0] = curtag[0] = '\0';
	/*
	 * open tag file.
	 */
	gtop = gtags_open(dbpath, root, db, GTAGS_READ, 0);
	cv = convert_open(type, format, root, cwd, dbpath, stdout);
	/*
	 * search through tag file.
	 */
	if (nofilter & SORT_FILTER)
		flags |= GTOP_NOSORT;
	if (iflag) {
		if (!isregex(pattern)) {
			sb = strbuf_open(0);
			strbuf_putc(sb, '^');
			strbuf_puts(sb, pattern);
			strbuf_putc(sb, '$');
			pattern = strbuf_value(sb);
		}
		flags |= GTOP_IGNORECASE;
	}
	if (Gflag)
		flags |= GTOP_BASICREGEX;
	if (format == FORMAT_PATH)
		flags |= GTOP_PATH;
	if (gtop->format & GTAGS_COMPACT)
		ib = strbuf_open(0);
	for (gtp = gtags_first(gtop, pattern, flags); gtp; gtp = gtags_next(gtop)) {
		if (lflag && !locatestring(gtp->path, localprefix, MATCH_AT_FIRST))
			continue;
		if (format == FORMAT_PATH) {
			convert_put(cv, gtp->path);
			count++;
		} else if (gtop->format & GTAGS_COMPACT) {
			/*
			 * Compact format:
			 *                    a          b
			 * tagline = <file id> <tag name> <line no>,...
			 */
			char *p = (char *)gtp->tagline;
			const char *tagname;
			int n = 0;

			while (*p != ' ')
				p++;
			*p++ = '\0';			/* a */
			tagname = p;
			while (*p != ' ')
				p++;
			*p++ = '\0';			/* b */
			/*
			 * Reopen or rewind source file.
			 */
			if (!nosource) {
				if (strcmp(gtp->path, curpath) != 0) {
					if (curpath[0] != '\0' && fp != NULL)
						fclose(fp);
					strlimcpy(curtag, tagname, sizeof(curtag));
					strlimcpy(curpath, gtp->path, sizeof(curpath));
					/*
					 * Use absolute path name to support GTAGSROOT
					 * environment variable.
					 */
					fp = fopen(makepath(root, curpath, NULL), "r");
					if (fp == NULL)
						warning("source file '%s' is not available.", curpath);
					last_lineno = lineno = 0;
				} else if (strcmp(gtp->tag, curtag) != 0) {
					strlimcpy(curtag, gtp->tag, sizeof(curtag));
					if (atoi(p) < last_lineno && fp != NULL) {
						rewind(fp);
						lineno = 0;
					}
					last_lineno = 0;
				}
			}
			/*
			 * Unfold compact format.
			 */
			if (!isdigit(*p))
				die("illegal compact format.");
			if (gtop->format & GTAGS_COMPLINE) {
				/*
				 *
				 * If GTAGS_COMPLINE flag is set, each line number is expressed as
				 * the difference from the previous line number except for the head.
				 * Please see flush_pool() in libutil/gtagsop.c for the details.
				 */
				int last = 0, cont = 0;

				while (*p || cont > 0) {
					if (cont > 0) {
						n = last + 1;
						if (n > cont) {
							cont = 0;
							continue;
						}
					} else if (isdigit(*p)) {
						GET_NEXT_NUMBER(p);
					}  else if (*p == '-') {
						GET_NEXT_NUMBER(p);
						cont = n + last;
						n = last + 1;
					} else if (*p == ',') {
						GET_NEXT_NUMBER(p);
						n += last;
					}
					if (last_lineno != n && fp) {
						while (lineno < n) {
							if (!(src = strbuf_fgets(ib, fp, STRBUF_NOCRLF))) {
								src = "";
								fclose(fp);
								fp = NULL;
								break;
							}
							lineno++;
						}
					}
					if (gtop->format & GTAGS_COMPNAME)
						tagname = (char *)uncompress(tagname, gtp->tag);
					convert_put_using(cv, tagname, gtp->path, n, src);
					count++;
					last_lineno = last = n;
				}
			} else {
				/*
				 * In fact, when GTAGS_COMPACT is set, GTAGS_COMPLINE is allways set.
				 * Therefore, the following code are not actually used.
				 * However, it is left for some test.
				 */
				while (*p) {
					for (n = 0; isdigit(*p); p++)
						n = n * 10 + *p - '0';
					if (*p == ',')
						p++;
					if (last_lineno == n)
						continue;
					if (last_lineno != n && fp) {
						while (lineno < n) {
							if (!(src = strbuf_fgets(ib, fp, STRBUF_NOCRLF))) {
								src = "";
								fclose(fp);
								fp = NULL;
								break;
							}
							lineno++;
						}
					}
					if (gtop->format & GTAGS_COMPNAME)
						tagname = (char *)uncompress(tagname, gtp->tag);
					convert_put_using(cv, tagname, gtp->path, n, src);
					count++;
					last_lineno = n;
				}
			}
		} else {
			/*
			 * Standard format:
			 *                    a          b         c
			 * tagline = <file id> <tag name> <line no> <line image>
			 */
			char *p = (char *)gtp->tagline;
			char namebuf[IDENTLEN+1];
			const char *tagname, *image;

			while (*p != ' ')
				p++;
			*p++ = '\0';			/* a */
			tagname = p;
			while (*p != ' ')
				p++;
			*p++ = '\0';			/* b */
			if (gtop->format & GTAGS_COMPNAME) {
				strlimcpy(namebuf, (char *)uncompress(tagname, gtp->tag), sizeof(namebuf));
				tagname = namebuf;
			}
			if (nosource) {
				image = " ";
			} else {
				while (*p != ' ')
					p++;
				image = p + 1;		/* c + 1 */
				if (gtop->format & GTAGS_COMPRESS)
					image = (char *)uncompress(image, gtp->tag);
			}
			convert_put_using(cv, tagname, gtp->path, gtp->lineno, image);
			count++;
		}
	}
	convert_close(cv);
	if (sb)
		strbuf_close(sb);
	if (ib)
		strbuf_close(ib);
	if (fp)
		fclose(fp);
	gtags_close(gtop);
	return count;
}
/*
 * tagsearch: execute tag search
 *
 *	i)	pattern		search pattern
 *	i)	cwd		current directory
 *	i)	root		root of source tree
 *	i)	dbpath		database directory
 *	i)	db		GTAGS,GRTAGS,GSYMS
 */
void
tagsearch(const char *pattern, const char *cwd, const char *root, const char *dbpath, int db)
{
	int count, total = 0;
	char libdbpath[MAXPATHLEN+1];

	/*
	 * search in current source tree.
	 */
	count = search(pattern, root, cwd, dbpath, db);
	total += count;
	/*
	 * search in library path.
	 */
	if (db == GTAGS && getenv("GTAGSLIBPATH") && (count == 0 || Tflag) && !lflag) {
		STRBUF *sb = strbuf_open(0);
		char *libdir, *nextp = NULL;

		strbuf_puts(sb, getenv("GTAGSLIBPATH"));
		/*
		 * search for each tree in the library path.
		 */
		for (libdir = strbuf_value(sb); libdir; libdir = nextp) {
			if ((nextp = locatestring(libdir, PATHSEP, MATCH_FIRST)) != NULL)
				*nextp++ = 0;
			if (!gtagsexist(libdir, libdbpath, sizeof(libdbpath), 0))
				continue;
			if (!strcmp(dbpath, libdbpath))
				continue;
			if (!test("f", makepath(libdbpath, dbname(db), NULL)))
				continue;
			/*
			 * search again
			 */
			count = search(pattern, libdir, cwd, libdbpath, db);
			total += count;
			if (count > 0 && !Tflag) {
				/* for verbose message */
				dbpath = libdbpath;
				break;
			}
		}
		strbuf_close(sb);
	}
	if (vflag) {
		print_count(total);
		if (!Tflag)
			fprintf(stderr, " (using '%s')", makepath(dbpath, dbname(db), NULL));
		fputs(".\n", stderr);
	}
}
/*
 * encode: string copy with converting blank chars into %ff format.
 *
 *	o)	to	result
 *	i)	size	size of 'to' buffer
 *	i)	from	string
 */
void
encode(char *to, int size, const char *from)
{
	const char *p;
	char *e = to;

	for (p = from; *p; p++) {
		if (*p == '%' || *p == ' ' || *p == '\t') {
			if (size <= 3)
				break;
			snprintf(e, size, "%%%02x", *p);
			e += 3;
			size -= 3;
		} else {
			if (size <= 1)
				break;
			*e++ = *p;
			size--;
		}
	}
	*e = 0;
}
