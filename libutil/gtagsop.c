/*
 * Copyright (c) 1997, 1998, 1999, 2000, 2001, 2002, 2005, 2006,
 *	2007, 2009 Tama Communications Corporation
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
#include <assert.h>
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

#include "char.h"
#include "checkalloc.h"
#include "conf.h"
#include "compress.h"
#include "dbop.h"
#include "die.h"
#include "format.h"
#include "gparam.h"
#include "gtagsop.h"
#include "locatestring.h"
#include "makepath.h"
#include "path.h"
#include "gpathop.h"
#include "split.h"
#include "strbuf.h"
#include "strhash.h"
#include "strlimcpy.h"
#include "strmake.h"
#include "varray.h"

#define HASHBUCKETS	2048

static int compare_path(const void *, const void *);
static int compare_lineno(const void *, const void *);
static int compare_tags(const void *, const void *);
static const char *seekto(const char *, int);
static void flush_pool(GTOP *);
static void segment_read(GTOP *);

/*
 * compare_path: compare function for sorting path names.
 */
static int
compare_path(const void *s1, const void *s2)
{
	return strcmp(*(char **)s1, *(char **)s2);
}
/*
 * compare_lineno: compare function for sorting line number.
 */
static int
compare_lineno(const void *s1, const void *s2)
{
	return *(const int *)s1 - *(const int *)s2;
}
/*
 * compare_tags: compare function for sorting tags.
 */
static int
compare_tags(const void *v1, const void *v2)
{
	const GTP *e1 = v1, *e2 = v2;
	int ret;

	if ((ret = strcmp(e1->path, e2->path)) != 0)
		return ret;
	return e1->lineno - e2->lineno;
}
/*
 * seekto: seek to the specified item of tag record.
 *
 * Usage:
 *           0         1          2
 * tagline = <file id> <tag name> <line number>
 *
 * <file id>     = seekto(tagline, SEEKTO_FILEID);
 * <tag name>    = seekto(tagline, SEEKTO_TAGNAME);
 * <line number> = seekto(tagline, SEEKTO_LINENO);
 */
#define SEEKTO_FILEID	0
#define SEEKTO_TAGNAME	1
#define SEEKTO_LINENO	2

static const char *
seekto(const char *string, int n)
{
	const char *p = string;
	while (n--) {
		p = strchr(p, ' ');
		if (p == NULL)
			return NULL;
		p++;
	}
	return p;
}
/*
 * Tag format
 *
 * [Specification of format version 4]
 * 
 * Standard format (for GTAGS)
 * 
 *         <file id> <tag name> <line number> <line image>
 * 
 *                 * Separator is single blank.
 * 
 *         [example]
 *         +------------------------------------
 *         |110 func 10 int func(int a)
 *         |110 func 30 func(int a1, int a2)
 * 
 *         Line image might be compressed (GTAGS_COMPRESS).
 *
 * With compact option for GRTAGS, GSYMS (GTAGS_COMPACT)
 * 
 *         <file id> <tag name> <line number>,...
 * 
 *                 * Separator is single blank.
 * 
 *         [example]
 *         +------------------------------------
 *         |110 func 10,30
 * 
 *         Line numbers are sorted in a line.
 * 
 * [Specification of format version 5]
 * 
 * Standard format for GTAGS
 * 
 *         <file id> <tag name> <line number> <line image>
 * 
 *                 * Separator is single blank.
 * 
 *         [example]
 *         +------------------------------------
 *         |110 func 10 int func(int a)
 *         |110 func 30 func(int a1, int a2)
 * 
 *         Line image might be compressed (GTAGS_COMPRESS).
 *         Tag name might be compressed (GTAGS_COMPNAME).
 *
 * With compact option for GRTAGS, GSYMS (GTAGS_COMPACT)
 * 
 *         <file id> <tag name> <line number>,...
 * 
 *                 * Separator is single blank.
 * 
 *         [example]
 *         +------------------------------------
 *         |110 func 10,20
 * 
 *	   Each line number might be expressed as difference from the previous
 *	   line number except for the head (GTAGS_COMPLINE).
 *           ex: 10,3,2 means '10 13 15'.
 *	   In addition,successive line numbers are expressed as a range.
 *           ex: 10-3 means '10 11 12 13'.
 *
 * [Description]
 * 
 * o Standard format is applied to GTAGS, and compact format is applied
 *   to GRTAGS and GSYMS by default.
 * o Above two formats are same to the first line number. So, we can use
 *   common function to sort them.
 * o Separator is single blank.
 *   This decrease disk space used a little, and make it easy to parse
 *   tag record.
 * o Use file id instead of path name.
 *   This allows blanks in path name at least in tag files.
 * o Put file id at the head of tag record.
 *   We can access file id without string processing.
 *   This is advantageous for deleting tag record when incremental updating.
 * 
 * [Concept of format version]
 *
 * Since GLOBAL's tag files are machine independent, they can be distributed
 * apart from GLOBAL itself. For example, if some network file system available,
 * client may execute global using server's tag files. In this case, both
 * GLOBAL are not necessarily the same version. So, we should assume that
 * older version of GLOBAL might access the tag files which generated
 * by new GLOBAL. To deal in such case, we decided to buried a version number
 * to both global(1) and tag files. The conclete procedure is like follows:
 *
 * 1. Gtags(1) bury the version number in tag files.
 * 2. Global(1) pick up the version number from a tag file. If the number
 *    is larger than its acceptable version number then global give up work
 *    any more and display error message.
 * 3. If version number is not found then it assumes version 1.
 *
 * [History of format version]
 *
 * GLOBAL-1.0 - 1.8     no idea about format version.
 * GLOBAL-1.9 - 2.24    understand format version.
 *                      support format version 1 (default).
 *                      if (format > 1) then print error message.
 * GLOBAL-3.0 - 4.5     support format version 1 and 2.
 *                      if (format > 2) then print error message.
 * GLOBAL-4.5.1 - 4.8.7 support format version 1, 2 and 3.
 *                      if (format > 3) then print error message.
 * GLOBAL-5.0 -	5.3	support format version only 4.
 *                      if (format !=  4) then print error message.
 * GLOBAL-5.4 -		support format version 4 and 5
 *                      if (format > 5 || format < 4) then print error message.
 *
 * In GLOBAL-5.0, we threw away the compatibility with the past formats.
 * Though we could continue the support for older formats, it seemed
 * not to be worthy. Because keeping maintaining the all formats hinders
 * new optimization and the function addition in the future.
 * Instead, the following error messages are displayed in a wrong usage.
 *       [older global and new tag file]
 *       $ global -x main
 *       GTAGS seems new format. Please install the latest GLOBAL.
 *       [new global and older tag file]
 *       $ global -x main
 *       GTAGS seems older format. Please remake tag files.
 */
static int upper_bound_version = 5;	/* acceptable format version (upper bound) */
static int lower_bound_version = 4;	/* acceptable format version (lower bound) */
static const char *tagslist[] = {"GPATH", "GTAGS", "GRTAGS", "GSYMS"};
/*
 * dbname: return db name
 *
 *	i)	db	0: GPATH, 1: GTAGS, 2: GRTAGS, 3: GSYMS
 *	r)		dbname
 */
const char *
dbname(int db)
{
	assert(db >= 0 && db < GTAGLIM);
	return tagslist[db];
}
/*
 * gtags_open: open global tag.
 *
 *	i)	dbpath	dbpath directory
 *	i)	root	root directory (needed when compact format)
 *	i)	db	GTAGS, GRTAGS, GSYMS
 *	i)	mode	GTAGS_READ: read only
 *			GTAGS_CREATE: create tag
 *			GTAGS_MODIFY: modify tag
 *	i)	flags	GTAGS_COMPACT: compact format
 *	r)		GTOP structure
 *
 * when error occurred, gtagopen doesn't return.
 */
GTOP *
gtags_open(const char *dbpath, const char *root, int db, int mode, int flags)
{
	GTOP *gtop;
	int dbmode;

	gtop = (GTOP *)check_calloc(sizeof(GTOP), 1);
	gtop->db = db;
	gtop->mode = mode;
	gtop->openflags = flags;
	gtop->format_version = 4;
	/*
	 * Open tag file allowing duplicate records.
	 */
	switch (gtop->mode) {
	case GTAGS_READ:
		dbmode = 0;
		break;
	case GTAGS_CREATE:
		dbmode = 1;
		break;
	case GTAGS_MODIFY:
		dbmode = 2;
		break;
	default:
		assert(0);
	}
	gtop->dbop = dbop_open(makepath(dbpath, dbname(db), NULL), dbmode, 0644, DBOP_DUP);
	if (gtop->dbop == NULL) {
		if (dbmode == 1)
			die("cannot make %s.", dbname(db));
		die("%s not found.", dbname(db));
	}
	if (gtop->mode == GTAGS_CREATE) {
		/*
		 * Decide format.
		 */
		gtop->format = 0;
		gtop->format_version = 5;
		/*
		 * GRTAGS and GSYSM always use compact format.
		 * GTAGS uses compact format only when the -c option specified.
		 */
		if (gtop->db == GRTAGS || gtop->db == GSYMS || gtop->openflags & GTAGS_COMPACT) {
			gtop->format |= GTAGS_COMPACT;
			gtop->format |= GTAGS_COMPLINE;
		} else {
			/* standard format */
			gtop->format |= GTAGS_COMPRESS;
		}
		gtop->format |= GTAGS_COMPNAME;
		if (gtop->format & GTAGS_COMPACT)
			dbop_putoption(gtop->dbop, COMPACTKEY, NULL);
		if (gtop->format & GTAGS_COMPRESS) {
			dbop_putoption(gtop->dbop, COMPRESSKEY, DEFAULT_ABBREVIATION);
			abbrev_open(DEFAULT_ABBREVIATION);
		}
		if (gtop->format & GTAGS_COMPLINE)
			dbop_putoption(gtop->dbop, COMPLINEKEY, NULL);
		if (gtop->format & GTAGS_COMPNAME)
			dbop_putoption(gtop->dbop, COMPNAMEKEY, NULL);
		dbop_putversion(gtop->dbop, gtop->format_version); 
	} else {
		/*
		 * recognize format version of GTAGS. 'format version record'
		 * is saved as a META record in GTAGS and GRTAGS.
		 * if 'format version record' is not found, it's assumed
		 * version 1.
		 */
		const char *p;
		/*
		 * check format version.
		 */
		gtop->format_version = dbop_getversion(gtop->dbop);
		if (gtop->format_version > upper_bound_version)
			die("%s seems new format. Please install the latest GLOBAL.", dbname(gtop->db));
		else if (gtop->format_version < lower_bound_version)
			die("%s seems older format. Please remake tag files.", dbname(gtop->db));
		gtop->format = 0;
		if (dbop_getoption(gtop->dbop, COMPACTKEY) != NULL)
			gtop->format |= GTAGS_COMPACT;
		if ((p = dbop_getoption(gtop->dbop, COMPRESSKEY)) != NULL) {
			abbrev_open(p);
			gtop->format |= GTAGS_COMPRESS;
		}
		if (dbop_getoption(gtop->dbop, COMPLINEKEY) != NULL)
			gtop->format |= GTAGS_COMPLINE;
		if (dbop_getoption(gtop->dbop, COMPNAMEKEY) != NULL)
			gtop->format |= GTAGS_COMPNAME;
	}
	if (gpath_open(dbpath, dbmode) < 0) {
		if (dbmode == 1)
			die("cannot create GPATH.");
		else
			die("GPATH not found.");
	}
	if (gtop->mode != GTAGS_READ)
		gtop->sb = strbuf_open(0);	/* This buffer is used for working area. */
	/*
	 * Stuff for compact format.
	 */
	if (gtop->format & GTAGS_COMPACT) {
		assert(root != NULL);
		strlimcpy(gtop->root, root, sizeof(gtop->root));
		if (gtop->mode != GTAGS_READ)
			gtop->path_hash = strhash_open(HASHBUCKETS);
	}
	return gtop;
}
/*
 * gtags_put: put tag record with packing.
 *
 *	i)	gtop	descripter of GTOP
 *	i)	key	tag name
 *	i)	ctags_x	tag line (ctags -x format)
 */
void
gtags_put(GTOP *gtop, const char *key, const char *ctags_x)	/* virtually const */
{
	SPLIT ptable;

	if (split((char *)ctags_x, 4, &ptable) != 4) {
		recover(&ptable);
		die("illegal tag format.\n'%s'", ctags_x);
	}
	if (gtop->format & GTAGS_COMPACT) {
		int *lno;
		struct sh_entry *entry;
		/*
		 * Flush the pool when path is changed.
		 * Line numbers in the pool will be sorted and duplicated
		 * records will be combined.
		 *
		 * pool    "funcA"   | 1| 3| 7|23|11| 2|...
		 *           v
		 * output  funcA 33 1,2,3,7,11,23...
		 */
		if (gtop->cur_path[0] && strcmp(gtop->cur_path, ptable.part[PART_PATH].start)) {
			flush_pool(gtop);
			strhash_reset(gtop->path_hash);
		}
		strlimcpy(gtop->cur_path, ptable.part[PART_PATH].start, sizeof(gtop->cur_path));
		/*
		 * Register each record into the pool.
		 *
		 * Pool image:
		 *
		 * tagname   lno
		 * ------------------------------
		 * "funcA"   | 1| 3| 7|23|11| 2|...
		 * "funcB"   |34| 2| 5|66| 3|...
		 * ...
		 */
		entry = strhash_assign(gtop->path_hash, key, 1);
		if (entry->value == NULL)
			entry->value = varray_open(sizeof(int), 100);
		lno = varray_append((VARRAY *)entry->value);
		*lno = atoi(ptable.part[PART_LNO].start);
	} else {
		const char *s_fid = gpath_path2fid(ptable.part[PART_PATH].start, NULL);

		if (s_fid == NULL)
			die("GPATH is corrupted.('%s' not found)", ptable.part[PART_PATH].start);
		strbuf_reset(gtop->sb);
		strbuf_puts(gtop->sb, s_fid);
		strbuf_putc(gtop->sb, ' ');
		if (gtop->format & GTAGS_COMPNAME)
			strbuf_puts(gtop->sb, compress(ptable.part[PART_TAG].start, key));
		else
			strbuf_puts(gtop->sb, ptable.part[PART_TAG].start);
		strbuf_putc(gtop->sb, ' ');
		strbuf_puts(gtop->sb, ptable.part[PART_LNO].start);
		strbuf_putc(gtop->sb, ' ');
		strbuf_puts(gtop->sb, gtop->format & GTAGS_COMPRESS ?
			compress(ptable.part[PART_LINE].start, key) :
			ptable.part[PART_LINE].start);
		dbop_put(gtop->dbop, key, strbuf_value(gtop->sb));
	}
	recover(&ptable);
}
/*
 * gtags_delete: delete records belong to set of fid.
 *
 *	i)	gtop	GTOP structure
 *	i)	deleteset bit array of fid
 */
void
gtags_delete(GTOP *gtop, IDSET *deleteset)
{
	const char *tagline;
	int fid;

	for (tagline = dbop_first(gtop->dbop, NULL, NULL, 0); tagline; tagline = dbop_next(gtop->dbop)) {
		/*
		 * Extract path from the tag line.
		 */
		fid = atoi(tagline);
		/*
		 * If the file id exists in the deleteset, delete the tagline.
		 */
		if (idset_contains(deleteset, fid))
			dbop_delete(gtop->dbop, NULL);
	}
}
/*
 * gtags_first: return first record
 *
 *	i)	gtop	GTOP structure
 *	i)	pattern	tag name
 *		o may be regular expression
 *		o may be NULL
 *	i)	flags	GTOP_PREFIX	prefix read
 *			GTOP_KEY	read key only
 *			GTOP_PATH	read path only
 *			GTOP_NOREGEX	don't use regular expression.
 *			GTOP_IGNORECASE	ignore case distinction.
 *			GTOP_BASICREGEX	use basic regular expression.
 *			GTOP_NOSORT	don't sort
 *	r)		record
 */
GTP *
gtags_first(GTOP *gtop, const char *pattern, int flags)
{
	int dbflags = 0;
	int regflags = 0;
	char prefix[IDENTLEN+1];
	static regex_t reg;
	regex_t *preg = &reg;
	const char *key = NULL;
	const char *tagline;

	/* Settlement for last time if any */
	if (gtop->path_hash) {
		strhash_close(gtop->path_hash);
		gtop->path_hash = NULL;
	}
	if (gtop->path_array) {
		free(gtop->path_array);
		gtop->path_array = NULL;
	}

	gtop->flags = flags;
	if (flags & GTOP_PREFIX && pattern != NULL)
		dbflags |= DBOP_PREFIX;
	if (flags & GTOP_KEY)
		dbflags |= DBOP_KEY;

	if (!(flags & GTOP_BASICREGEX))
		regflags |= REG_EXTENDED;
	if (flags & GTOP_IGNORECASE)
		regflags |= REG_ICASE;
	if (gtop->format & GTAGS_COMPACT)
		gtop->fp = NULL;
	/*
	 * Get key and compiled regular expression for dbop_xxxx().
	 */
	if (flags & GTOP_NOREGEX) {
		key = pattern;
		preg = NULL;
	} else if (pattern == NULL || !strcmp(pattern, ".*")) {
		/*
		 * Since the regular expression '.*' matches to any record,
		 * we take sequential read method.
		 */
		key = NULL;
		preg = NULL;
	} else if (isregex(pattern) && regcomp(preg, pattern, regflags) == 0) {
		const char *p;
		/*
		 * If the pattern include '^' + some non regular expression
		 * characters like '^aaa[0-9]', we take prefix read method
		 * with the non regular expression part as the prefix.
		 */
		if (!(flags & GTOP_IGNORECASE) && *pattern == '^' && *(p = pattern + 1) && !isregexchar(*p)) {
			int i = 0;

			while (*p && !isregexchar(*p) && i < IDENTLEN)
				prefix[i++] = *p++;
			prefix[i] = '\0';
			key = prefix;
			dbflags |= DBOP_PREFIX;
		} else {
			key = NULL;
		}
	} else {
		key = pattern;
		preg = NULL;
	}
	/*
	 * If GTOP_PATH is set, at first, we collect all path names in a pool and
	 * sort them. gtags_first() and gtags_next() returns one of the pool.
	 */
	if (gtop->flags & GTOP_PATH) {
		struct sh_entry *entry;
		char *p;
		const char *cp;
		unsigned long i;

		gtop->path_hash = strhash_open(HASHBUCKETS);
		/*
		 * Pool path names.
		 *
		 * fid		path name
		 * +--------------------------
		 * |100		./aaa/a.c
		 * |105		./aaa/b.c
		 *  ...
		 */
		for (tagline = dbop_first(gtop->dbop, key, preg, dbflags);
		     tagline != NULL;
		     tagline = dbop_next(gtop->dbop))
		{
			/* extract file id */
			p = locatestring(tagline, " ", MATCH_FIRST);
			if (p == NULL)
				die("Illegal tag record. '%s'\n", tagline);
			*p = '\0';
			entry = strhash_assign(gtop->path_hash, tagline, 1);
			/* new entry: get path name and set. */
			if (entry->value == NULL) {
				cp = gpath_fid2path(tagline, NULL);
				if (cp == NULL)
					die("GPATH is corrupted.(file id '%s' not found)", tagline);
				entry->value = strhash_strdup(gtop->path_hash, cp, 0);
			}
		}
		/*
		 * Sort path names.
		 *
		 * fid		path name	path_array (sort)
		 * +--------------------------	+---+
		 * |100		./aaa/a.c <-------* |
		 * |105		./aaa/b.c <-------* |
		 *  ...				...
		 */
		gtop->path_array = (char **)check_malloc(gtop->path_hash->entries * sizeof(char *));
		i = 0;
		for (entry = strhash_first(gtop->path_hash); entry != NULL; entry = strhash_next(gtop->path_hash))
			gtop->path_array[i++] = entry->value;
		if (i != gtop->path_hash->entries)
			die("Something is wrong. 'i = %lu, entries = %lu'" , i, gtop->path_hash->entries);
		if (!(gtop->flags & GTOP_NOSORT))
			qsort(gtop->path_array, gtop->path_hash->entries, sizeof(char *), compare_path);
		gtop->path_count = gtop->path_hash->entries;
		gtop->path_index = 0;

		if (gtop->path_index >= gtop->path_count)
			return NULL;
		gtop->gtp.path = gtop->path_array[gtop->path_index++];
		return &gtop->gtp;
	} else if (gtop->flags & GTOP_KEY) {
		return ((gtop->gtp.tag = dbop_first(gtop->dbop, key, preg, dbflags)) == NULL)
			? NULL : &gtop->gtp;
	} else {
		if (gtop->vb == NULL)
			gtop->vb = varray_open(sizeof(GTP), 200);
		else
			varray_reset(gtop->vb);
		if (gtop->segment_pool == NULL)
			gtop->segment_pool = pool_open();
		else
			pool_reset(gtop->segment_pool);
		if (gtop->path_hash == NULL)
			gtop->path_hash = strhash_open(HASHBUCKETS);
		else
			strhash_reset(gtop->path_hash);
		tagline = dbop_first(gtop->dbop, key, preg, dbflags);
		if (tagline == NULL)
			return NULL;
		/*
		 * Dbop_next() wil read the same record again.
		 */
		dbop_unread(gtop->dbop);
		/*
		 * Read a tag segment with sorting.
		 */
		segment_read(gtop);
		return  &gtop->gtp_array[gtop->gtp_index++];
	}
}
/*
 * gtags_next: return next record.
 *
 *	i)	gtop	GTOP structure
 *	r)		record
 *			NULL end of tag
 */
GTP *
gtags_next(GTOP *gtop)
{
	if (gtop->flags & GTOP_PATH) {
		if (gtop->path_index >= gtop->path_count)
			return NULL;
		gtop->gtp.path = gtop->path_array[gtop->path_index++];
		return &gtop->gtp;
	} else if (gtop->flags & GTOP_KEY) {
		return ((gtop->gtp.tag = dbop_next(gtop->dbop)) == NULL)
			? NULL : &gtop->gtp;
	} else {
		/*
		 * End of segment.
		 * Reset resources and read new segment again.
		 */
		if (gtop->gtp_index >= gtop->gtp_count) {
			varray_reset(gtop->vb);
			pool_reset(gtop->segment_pool);
			/* strhash_reset(gtop->path_hash); */
			segment_read(gtop);
		}
		if (gtop->gtp_index >= gtop->gtp_count)
			return NULL;
		return &gtop->gtp_array[gtop->gtp_index++];
	}
}
/*
 * gtags_close: close tag file
 *
 *	i)	gtop	GTOP structure
 */
void
gtags_close(GTOP *gtop)
{
	if (gtop->format & GTAGS_COMPACT && gtop->fp != NULL)
		fclose(gtop->fp);
	if (gtop->format & GTAGS_COMPRESS)
		abbrev_close();
	if (gtop->format & GTAGS_COMPACT && gtop->cur_path[0])
		flush_pool(gtop);
	if (gtop->segment_pool)
		pool_close(gtop->segment_pool);
	if (gtop->path_array)
		free(gtop->path_array);
	if (gtop->sb)
		strbuf_close(gtop->sb);
	if (gtop->vb)
		varray_close(gtop->vb);
	if (gtop->path_hash)
		strhash_close(gtop->path_hash);
	gpath_close();
	dbop_close(gtop->dbop);
	free(gtop);
}
/*
 * flush_pool: flush the pool and write is as compact format.
 *
 *	i)	gtop	descripter of GTOP
 */
static void
flush_pool(GTOP *gtop)
{
	struct sh_entry *entry;
	const char *s_fid = gpath_path2fid(gtop->cur_path, NULL);
	int header_offset;
	int i, last;

	if (s_fid == NULL)
		die("GPATH is corrupted.('%s' not found)", gtop->cur_path);
	/*
	 * Write records as compact format and free line number table
	 * for each entry in the pool.
	 */
	for (entry = strhash_first(gtop->path_hash); entry; entry = strhash_next(gtop->path_hash)) {
		VARRAY *vb = (VARRAY *)entry->value;
		int *lno_array = varray_assign(vb, 0, 0);
		const char *key = entry->name;

		/*
		 * extract method when class method definition.
		 *
		 * Ex: Class::method(...)
		 *
		 * key	= 'method'
		 * data = 'Class::method  103 ./class.cpp ...'
		 */
		if (gtop->flags & GTAGS_EXTRACTMETHOD) {
			if ((key = locatestring(entry->name, ".", MATCH_LAST)) != NULL)
				key++;
			else if ((key = locatestring(entry->name, "::", MATCH_LAST)) != NULL)
				key += 2;
			else
				key = entry->name;
		}
		/* Sort line number table */
		qsort(lno_array, vb->length, sizeof(int), compare_lineno); 

		strbuf_reset(gtop->sb);
		strbuf_puts(gtop->sb, s_fid);
		strbuf_putc(gtop->sb, ' ');
		if (gtop->format & GTAGS_COMPNAME) {
			strbuf_puts(gtop->sb, compress(entry->name, key));
		} else {
			strbuf_puts(gtop->sb, entry->name);
		}
		strbuf_putc(gtop->sb, ' ');
		header_offset = strbuf_getlen(gtop->sb);
		/*
		 * If GTAGS_COMPLINE flag is set, each line number is expressed as the
		 * difference from the previous line number except for the head.
		 * GTAGS_COMPLINE is set by default in format version 5.
		 */
		if (gtop->format & GTAGS_COMPLINE) {
			int cont = 0;

			last = 0;			/* line 0 doesn't exist */
			for (i = 0; i < vb->length; i++) {
				int n = lno_array[i];

				if (n == last)
					continue;
				if (last > 0 && n == last + 1) {
					if (!cont) {
						/*
						 * Don't use range expression at the head.
						 */
						if (strbuf_getlen(gtop->sb) == header_offset)
							strbuf_putn(gtop->sb, n);
						else
							cont = last;
					}
				} else {
					/*
					 * Range expression. ex: 10-2 means 10 11 12
					 */
					if (cont) {
						strbuf_putc(gtop->sb, '-');
						strbuf_putn(gtop->sb, last - cont);
						cont = 0;
					}
					if (strbuf_getlen(gtop->sb) > header_offset) {
						strbuf_putc(gtop->sb, ',');
						strbuf_putn(gtop->sb, n - last);
					} else {
						strbuf_putn(gtop->sb, n);
					}
					if (strbuf_getlen(gtop->sb) > DBOP_PAGESIZE / 4) {
						dbop_put(gtop->dbop, key, strbuf_value(gtop->sb));
						strbuf_setlen(gtop->sb, header_offset);
					}
				}
				last = n;
			}
			if (cont) {
				strbuf_putc(gtop->sb, '-');
				strbuf_putn(gtop->sb, last - cont);
			}
		} else {
			/*
			 * This code is to support older format (version 4).
			 */
			last = 0;			/* line 0 doesn't exist */
			for (i = 0; i < vb->length; i++) {
				int n = lno_array[i];

				if (n == last)
					continue;
				if (strbuf_getlen(gtop->sb) > header_offset)
					strbuf_putc(gtop->sb, ',');
				strbuf_putn(gtop->sb, n);
				if (strbuf_getlen(gtop->sb) > DBOP_PAGESIZE / 4) {
					dbop_put(gtop->dbop, key, strbuf_value(gtop->sb));
					strbuf_setlen(gtop->sb, header_offset);
				}
				last = n;
			}
		}
		if (strbuf_getlen(gtop->sb) > header_offset)
			dbop_put(gtop->dbop, key, strbuf_value(gtop->sb));
		/* Free line number table */
		varray_close(vb);
	}
}
/*
 * Read a tag segment with sorting.
 *
 *	i)	gtop	GTOP structure
 *	o)	gtop->gtp_array		segment table
 *	o)	gtop->gtp_count		segment table size
 *	o)	gtop->gtp_index		segment table index (initial value = 0)
 *	o)	gtop->cur_tagname	current tag name
 *
 * A segment is a set of tag records which have same tag name.
 * This function read a segment from tag file, sort it and put it on segment table.
 * This function can treat both of standard format and compact format.
 *
 * Sorting is done by three keys.
 *	1st key: tag name
 *	2nd key: file name
 *	3rd key: line number
 * Since all records in a segment have same tag name, you need not think about 1st key.
 */
void
segment_read(GTOP *gtop)
{
	const char *tagline, *fid, *path, *lineno;
	GTP *gtp;
	struct sh_entry *sh;

	/*
	 * Save tag lines.
	 */
	gtop->cur_tagname[0] = '\0';
	while ((tagline = dbop_next(gtop->dbop)) != NULL) {
		/*
		 * get tag name and line number.
		 *
		 * tagline = <file id> <tag name> <line number>
		 */
		if (gtop->cur_tagname[0] == '\0') {
			strlimcpy(gtop->cur_tagname, gtop->dbop->lastkey, sizeof(gtop->cur_tagname));
		} else if (strcmp(gtop->cur_tagname, gtop->dbop->lastkey) != 0) {
			/*
			 * Dbop_next() wil read the same record again.
			 */
			dbop_unread(gtop->dbop);
			break;
		}
		gtp = varray_append(gtop->vb);
		gtp->tagline = pool_strdup(gtop->segment_pool, tagline, 0);
		gtp->tag = (const char *)gtop->cur_tagname;
		/*
		 * convert fid into hashed path name to save memory.
		 */
		fid = (const char *)strmake(tagline, " ");
		path = gpath_fid2path(fid, NULL);
		if (path == NULL)
			die("gtags_first: path not found. (fid=%s)", fid);
		sh = strhash_assign(gtop->path_hash, path, 1);
		gtp->path = sh->name;
		lineno = seekto(gtp->tagline, SEEKTO_LINENO);
		if (lineno == NULL)
			die("illegal tag record.\n%s", tagline);
		gtp->lineno = atoi(lineno);
	}
	/*
	 * Sort tag lines.
	 */
	gtop->gtp_array = varray_assign(gtop->vb, 0, 0);
	gtop->gtp_count = gtop->vb->length;
	gtop->gtp_index = 0;
	if (!(gtop->flags & GTOP_NOSORT))
		qsort(gtop->gtp_array, gtop->gtp_count, sizeof(GTP), compare_tags);
}
