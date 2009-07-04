/*
 * Copyright (c) 2005, 2006 Tama Communications Corporation
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
#include <stdio.h>
#include <ctype.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#include "abs2rel.h"
#include "checkalloc.h"
#include "die.h"
#include "format.h"
#include "gparam.h"
#include "gpathop.h"
#include "pathconvert.h"
#include "strbuf.h"
#include "strlimcpy.h"

/*
 * Path filter for the output of global(1).
 */
static const char *
convert_pathname(CONVERT *cv, const char *path)
{
	static char buf[MAXPATHLEN+1];
	const char *a, *b;

	/*
	 * print without conversion.
	 */
	if (cv->type == PATH_THROUGH)
		return path;
	/*
	 * make absolute path name.
	 * 'path + 1' means skipping "." at the head.
	 */
	strbuf_setlen(cv->abspath, cv->start_point);
	strbuf_puts(cv->abspath, path + 1);
	/*
	 * print path name with converting.
	 */
	switch (cv->type) {
	case PATH_ABSOLUTE:
		path = strbuf_value(cv->abspath);
		break;
	case PATH_RELATIVE:
		a = strbuf_value(cv->abspath);
		b = cv->basedir;
#if defined(_WIN32) || defined(__DJGPP__)
		while (*a != '/')
			a++;
		while (*b != '/')
			b++;
#endif
		if (!abs2rel(a, b, buf, sizeof(buf)))
			die("abs2rel failed. (path=%s, base=%s).", a, b);
		path = buf;
		break;
	default:
		die("unknown path type.");
		break;
	}
	return (const char *)path;
}
/*
 * convert_open: open convert filter
 *
 *	i)	type	PATH_ABSOLUTE, PATH_RELATIVE, PATH_THROUGH
 *	i)	format	tag record format
 *	i)	root	root directory of source tree
 *	i)	cwd	current directory
 *	i)	dbpath	dbpath directory
 *	i)	op	output file
 */
CONVERT *
convert_open(int type, int format, const char *root, const char *cwd, const char *dbpath, FILE *op)
{
	CONVERT *cv = (CONVERT *)check_calloc(sizeof(CONVERT), 1);
	/*
	 * set base directory.
	 */
	cv->abspath = strbuf_open(MAXPATHLEN);
	strbuf_puts(cv->abspath, root);
	strbuf_unputc(cv->abspath, '/');
	cv->start_point = strbuf_getlen(cv->abspath);
	/*
	 * copy elements.
	 */
	if (strlen(cwd) > MAXPATHLEN)
		die("current directory name too long.");
	strlimcpy(cv->basedir, cwd, sizeof(cv->basedir));
	cv->type = type;
	cv->format = format;
	cv->op = op;
	/*
	 * open GPATH.
	 */
	if (gpath_open(dbpath, 0) < 0)
		die("GPATH not found.");
	return cv;
}
/*
 * convert_put: convert path into relative or absolute and print.
 *
 *	i)	cv	CONVERT structure
 *	i)	tagline	output record
 */
void
convert_put(CONVERT *cv, const char *tagline)
{
	char *tagnextp = NULL;
	int tagnextc = 0;
	char *tag = NULL, *lineno = NULL, *path, *rest = NULL;

	/*
	 * parse tag line.
	 * Don't use split() function not to destroy line image.
	 */
	if (cv->format == FORMAT_PATH) {
		path = (char *)tagline;
	} else {
		char *p = (char *)tagline;
		/*
		 * tag name
		 */
		tag = p;
		for (; *p && !isspace(*p); p++)
			;
		if (*p == '\0')
			die("illegal ctags-x format (line number not found).");
		tagnextp = p;
		tagnextc = *p;
		*p++ = '\0';
		/* skip blanks */
		for (; *p && isspace(*p); p++)
			;
		if (*p == '\0')
			die("illegal ctags-x format (line number not found).");
		/*
		 * line number
		 */
		lineno = p;
		for (; *p && !isspace(*p); p++)
			;
		if (*p == '\0')
			die("illegal ctags-x format (path name not found).");
		*p++ = '\0';
		/* skip blanks */
		for (; *p && isspace(*p); p++)
			;
		if (*p == '\0')
			die("illegal ctags-x format (path name not found).");
		/*
		 * path name
		 */
		path = p;
		for (; *p && !isspace(*p); p++)
			;
		if (*p == '\0')
			die("illegal ctags-x format (line image not found).");
		*p++ = '\0';
		rest = p;
	}
	switch (cv->format) {
	case FORMAT_PATH:
		fputs(convert_pathname(cv, path), cv->op);
		break;
	case FORMAT_CTAGS:
		fputs(tag, cv->op);
		fputc('\t', cv->op);
		fputs(convert_pathname(cv, path), cv->op);
		fputc('\t', cv->op);
		fputs(lineno, cv->op);
		break;
	case FORMAT_CTAGS_XID:
		fputs(gpath_path2fid(path, NULL), cv->op);
		fputc(' ', cv->op);
		/* PASS THROUGH */
	case FORMAT_CTAGS_X:
		/*
		 * print until path name.
		 */
		*tagnextp = tagnextc;
		fputs(tagline, cv->op);
		fputc(' ', cv->op);
		/*
		 * print path name and the rest.
		 */
		fputs(convert_pathname(cv, path), cv->op);
		fputc(' ', cv->op);
		fputs(rest, cv->op);
		break;
	case FORMAT_GREP:
		fputs(convert_pathname(cv, path), cv->op);
		fputc(':', cv->op);
		fputs(lineno, cv->op);
		fputc(':', cv->op);
		fputs(rest, cv->op);
		break;
	case FORMAT_CSCOPE:
		fputs(convert_pathname(cv, path), cv->op);
		fputc(' ', cv->op);
		fputs(tag, cv->op);
		fputc(' ', cv->op);
		fputs(lineno, cv->op);
		fputc(' ', cv->op);
		for (; *rest && isspace(*rest); rest++)
			;
		fputs(rest, cv->op);
		break;
	default:
		die("unknown format type.");
	}
	(void)fputc('\n', cv->op);
}
/*
 * convert_put_using: convert path into relative or absolute and print.
 *
 *	i)	cv	CONVERT structure
 *      i)      tag     tag name
 *      i)      path    path name
 *      i)      lineno  line number
 *      i)      line    line image
 */
void
convert_put_using(CONVERT *cv, const char *tag, const char *path, int lineno, const char *rest)
{
	switch (cv->format) {
	case FORMAT_PATH:
		fputs(convert_pathname(cv, path), cv->op);
		break;
	case FORMAT_CTAGS:
		fputs(tag, cv->op);
		fputc('\t', cv->op);
		fputs(convert_pathname(cv, path), cv->op);
		fputc('\t', cv->op);
		fprintf(cv->op, "%d", lineno);
		break;
	case FORMAT_CTAGS_XID:
		fputs(gpath_path2fid(path, NULL), cv->op);
		fputc(' ', cv->op);
		/* PASS THROUGH */
	case FORMAT_CTAGS_X:
		fprintf(cv->op, "%-16s %4d %-16s %s",
			tag, lineno, convert_pathname(cv, path), rest);
		break;
	case FORMAT_GREP:
		fputs(convert_pathname(cv, path), cv->op);
		fputc(':', cv->op);
		fprintf(cv->op, "%d", lineno);
		fputc(':', cv->op);
		fputs(rest, cv->op);
		break;
	case FORMAT_CSCOPE:
		fputs(convert_pathname(cv, path), cv->op);
		fputc(' ', cv->op);
		fputs(tag, cv->op);
		fputc(' ', cv->op);
		fprintf(cv->op, "%d", lineno);
		fputc(' ', cv->op);
		for (; *rest && isspace(*rest); rest++)
			;
		fputs(rest, cv->op);
		break;
	default:
		die("unknown format type.");
	}
	(void)fputc('\n', cv->op);
}
void
convert_close(CONVERT *cv)
{
	strbuf_close(cv->abspath);
	gpath_close();
	free(cv);
}
