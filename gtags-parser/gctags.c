/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002, 2003, 2005
 *	Tama Communications Corporation
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
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
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
#include "gctags.h"
#include "const.h"

int main(int, char **);
static void usage(void);
static void help(void);

struct words {
        const char *name;
        int val;
};
struct words *words;
static int tablesize;

static char *langmap;

int bflag;			/* -b: force level 1 block start */
int eflag;			/* -e: force level 1 block end */
int nflag;			/* -n: doen't print tag */
int qflag;			/* -q: quiet mode */
int rflag;			/* -r: function reference */
int sflag;			/* -s: collect symbols */
int wflag;			/* -w: warning message */
int vflag;			/* -v: verbose mode */
int show_version;
int show_help;
int debug;

/*----------------------------------------------------------------------*/
/* Parser switch                                                        */
/*----------------------------------------------------------------------*/
/*
 * This is the linkage section of each parsers.
 * If you want to support new language, you must define parser procedure
 * which requires file name as an argument.
 */
struct lang_entry {
	const char *lang_name;
	void (*parser)(const char *);		/* parser procedure */
};

/*
 * The first entry is default language.
 */
static struct lang_entry lang_switch[] = {
	/* lang_name    parser_proc	*/
	{"c",		C},			/* DEFAULT */
	{"yacc",	yacc},
	{"cpp",		Cpp},
	{"java",	java},
	{"php",		php},
	{"asm",		assembly}
};
#define DEFAULT_ENTRY &lang_switch[0]
/*
 * get language entry.
 *
 *      i)      lang    language name (NULL means 'not specified'.)
 *      r)              language entry
 */
static struct lang_entry *
get_lang_entry(const char *lang)
{
        int i, size = sizeof(lang_switch) / sizeof(struct lang_entry);

        /*
         * if language not specified, it assumes default language.
         */
        if (lang == NULL)
                return DEFAULT_ENTRY;
        for (i = 0; i < size; i++)
                if (!strcmp(lang, lang_switch[i].lang_name))
                        return &lang_switch[i];
        /*
         * if specified language not found, it assumes default language.
         */
        return DEFAULT_ENTRY;
}

static void
usage(void)
{
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
/*
 * Though the -d(--define) and -t(--typedef) option was removed,
 * the entries have been left not to bring error messages.
 */
static struct option const long_options[] = {
	{"begin-block", no_argument, NULL, 'b'},
	{"define", no_argument, NULL, 'd'},
	{"end-block", no_argument, NULL, 'e'},
	{"no-tags", no_argument, NULL, 'n'},
	{"quiet", no_argument, NULL, 'q'},
	{"reference", no_argument, NULL, 'r'},
	{"symbol", no_argument, NULL, 's'},
	{"typedef", no_argument, NULL, 't'},
	{"verbose", no_argument, NULL, 'v'},
	{"warning", no_argument, NULL, 'w'},

	/* long name only */
	{"debug", no_argument, &debug, 1},
	{"langmap", required_argument, NULL, 0},
	{"version", no_argument, &show_version, 1},
	{"help", no_argument, &show_help, 1},
	{ 0 }
};

int
main(int argc, char **argv)
{
	char *p;
	int optchar;
	int option_index = 0;

	while ((optchar = getopt_long(argc, argv, "bdenqrstvw", long_options, &option_index)) != EOF) {
		switch(optchar) {
		case 0:
			p = (char *)long_options[option_index].name;
			if (!strcmp(p, "langmap"))
				langmap = optarg;	
			break;
		case 'b':
			bflag++;
			break;
		case 'd':
			break;
		case 'e':
			eflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'q':
			qflag++;
			break;
		case 'r':
			rflag++;
			sflag = 0;
			break;
		case 's':
			sflag++;
			rflag = 0;
			break;
		case 't':
			break;
		case 'v':
			vflag++;
			break;
		case 'w':
			wflag++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	if (show_version)
		version(NULL, vflag);
	else if (show_help)
		help();
	/*
	 * If langmap is not passed as argument, environment variable
	 * GTAGSLANGMAP should be checked. Gtags(1) call gtags-parser
	 * in this method.
	 */
	if (langmap == NULL)
		langmap = getenv("GTAGSLANGMAP");
	if (langmap == NULL)
		langmap = DEFAULTLANGMAP;
	setup_langmap(langmap);

        argc -= optind;
        argv += optind;

	if (argc < 1)
		usage();
	if (getenv("GTAGSWARNING"))
		wflag++;	
	/*
	 * This is a hack for FreeBSD.
	 * In the near future, it will be removed.
	 */
#ifdef __DJGPP__
	if (test("r", NOTFUNCTION) || test("r", DOS_NOTFUNCTION))
#else
	if (test("r", NOTFUNCTION))
#endif
	{
		FILE *ip;
		STRBUF *sb = strbuf_open(0);
		STRBUF *ib = strbuf_open(0);
		char *p;
		int i;

		if ((ip = fopen(NOTFUNCTION, "r")) == 0)
#ifdef __DJGPP__
			if ((ip = fopen(DOS_NOTFUNCTION, "r")) == 0)
#endif
			die("'%s' cannot read.", NOTFUNCTION);
		for (tablesize = 0; (p = strbuf_fgets(ib, ip, STRBUF_NOCRLF)) != NULL; tablesize++)
			strbuf_puts0(sb, p);
		fclose(ip);
		words = (struct words *)check_malloc(sizeof(struct words) * tablesize);
		/*
		 * Don't free *p.
		 */
		p = (char *)check_malloc(strbuf_getlen(sb) + 1);
		memcpy(p, strbuf_value(sb), strbuf_getlen(sb) + 1);
		for (i = 0; i < tablesize; i++) {
			words[i].name = p;
			p += strlen(p) + 1;
		}
		qsort(words, tablesize, sizeof(struct words), cmp);
		strbuf_close(sb);
		strbuf_close(ib);
	}

	/*
	 * pick up files and parse them.
	 */
	for (; argc > 0; argv++, argc--) {
		const char *lang, *suffix;
		struct lang_entry *ent;

		/* get suffix of the path. */
		suffix = locatestring(argv[0], ".", MATCH_LAST);
		if (!suffix)
			continue;
		lang = decide_lang(suffix);
		if (lang == NULL)
			continue;
		if (vflag)
			fprintf(stderr, "suffix '%s' assumed language '%s'.\n", suffix, lang);
		/*
		 * Select parser.
		 * If lang == NULL then default parser is selected.
		 */
		ent = get_lang_entry(lang);
		/*
		 * call language specific parser.
		 */
		ent->parser(argv[0]);
	}
	return 0;
}

int
cmp(const void *s1, const void *s2)
{
	return strcmp(((struct words *)s1)->name, ((struct words *)s2)->name);
}

int
isnotfunction(char *name)
{
	struct words tmp;
	struct words *result;

	if (words == NULL)
		return 0;
	tmp.name = name;
	result = (struct words *)bsearch(&tmp, words, tablesize, sizeof(struct words), cmp);
	return (result != NULL) ? 1 : 0;
}

void
dbg_print(int level, const char *s)
{
	if (!debug)
		return;
	fprintf(stderr, "[%04d]", lineno);
	for (; level > 0; level--)
		fprintf(stderr, "    ");
	fprintf(stderr, "%s\n", s);
}
