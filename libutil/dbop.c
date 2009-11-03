/*
 * Copyright (c) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2006, 2009
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
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
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
#include "dbop.h"
#include "die.h"
#include "locatestring.h"
#include "strbuf.h"
#include "strlimcpy.h"
#include "test.h"

int print_statistics = 0;

#define ismeta(p)	(*((char *)(p)) == ' ')

/*
 * dbop_open: open db database.
 *
 *	i)	path	database name
 *	i)	mode	0: read only, 1: create, 2: modify
 *	i)	perm	file permission
 *	i)	flags
 *			DBOP_DUP: allow duplicate records.
 *			DBOP_REMOVE: remove on closed.
 *	r)		descripter for dbop_xxx()
 */
DBOP *
dbop_open(const char *path, int mode, int perm, int flags)
{
	DB *db;
	int rw = 0;
	DBOP *dbop;
	BTREEINFO info;

	/*
	 * setup arguments.
	 */
	switch (mode) {
	case 0:
		rw = O_RDONLY;
		break;
	case 1:
		rw = O_RDWR|O_CREAT|O_TRUNC;
		break;
	case 2:
		rw = O_RDWR;
		break;
	default:
		assert(0);
	}
	memset(&info, 0, sizeof(info));
	if (flags & DBOP_DUP)
		info.flags |= R_DUP;
	info.psize = DBOP_PAGESIZE;
	/*
	 * Decide cache size. The default value is 5MB.
	 * See libutil/gparam.h for the details.
	 */
	info.cachesize = GTAGSCACHE;
	if (getenv("GTAGSCACHE") != NULL)
		info.cachesize = atoi(getenv("GTAGSCACHE"));
	if (info.cachesize < GTAGSMINCACHE)
		info.cachesize = GTAGSMINCACHE;

	/*
	 * if unlink do job normally, those who already open tag file can use
	 * it until closing.
	 */
	if (path != NULL && mode == 1 && test("f", path))
		(void)unlink(path);
	db = dbopen(path, rw, 0600, DB_BTREE, &info);
	if (!db)
		return NULL;
	dbop = (DBOP *)check_calloc(sizeof(DBOP), 1);
	if (path == NULL)
		dbop->dbname[0] = '\0';
	else
		strlimcpy(dbop->dbname, path, sizeof(dbop->dbname));
	dbop->db	= db;
	dbop->openflags	= flags;
	dbop->perm	= (mode == 1) ? perm : 0;
	dbop->lastdat	= NULL;
	dbop->lastsize	= 0;

	return dbop;
}
/*
 * dbop_get: get data by a key.
 *
 *	i)	dbop	descripter
 *	i)	name	name
 *	r)		pointer to data
 */
const char *
dbop_get(DBOP *dbop, const char *name)
{
	DB *db = dbop->db;
	DBT key, dat;
	int status;

	key.data = (char *)name;
	key.size = strlen(name)+1;

	status = (*db->get)(db, &key, &dat, 0);
	dbop->lastdat = (char *)dat.data;
	dbop->lastsize = dat.size;
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
		die("cannot read from database.");
	case RET_SPECIAL:
		return (NULL);
	}
	return (dat.data);
}
/*
 * dbop_put: put data by a key.
 *
 *	i)	dbop	descripter
 *	i)	name	key
 *	i)	data	data
 */
void
dbop_put(DBOP *dbop, const char *name, const char *data)
{
	DB *db = dbop->db;
	DBT key, dat;
	int status;
	int len;

	if (!(len = strlen(name)))
		die("primary key size == 0.");
	if (len > MAXKEYLEN)
		die("primary key too long.");
	key.data = (char *)name;
	key.size = strlen(name)+1;
	dat.data = (char *)data;
	dat.size = strlen(data)+1;

	status = (*db->put)(db, &key, &dat, 0);
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
	case RET_SPECIAL:
		die("cannot write to database.");
	}
}
/*
 * dbop_put_withlen: put data by a key.
 *
 *	i)	dbop	descripter
 *	i)	name	key
 *	i)	data	data
 *	i)	length	length of data
 */
void
dbop_put_withlen(DBOP *dbop, const char *name, const char *data, int length)
{
	DB *db = dbop->db;
	DBT key, dat;
	int status;
	int len;

	if (!(len = strlen(name)))
		die("primary key size == 0.");
	if (len > MAXKEYLEN)
		die("primary key too long.");
	key.data = (char *)name;
	key.size = strlen(name)+1;
	dat.data = (char *)data;
	dat.size = length;

	status = (*db->put)(db, &key, &dat, 0);
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
	case RET_SPECIAL:
		die("cannot write to database.");
	}
}
/*
 * dbop_delete: delete record by path name.
 *
 *	i)	dbop	descripter
 *	i)	path	path name
 */
void
dbop_delete(DBOP *dbop, const char *path)
{
	DB *db = dbop->db;
	DBT key;
	int status;

	if (path) {
		key.data = (char *)path;
		key.size = strlen(path)+1;
		status = (*db->del)(db, &key, 0);
	} else
		status = (*db->del)(db, &key, R_CURSOR);
	if (status == RET_ERROR)
		die("cannot delete record.");
}
/*
 * dbop_update: update record.
 *
 *	i)	dbop	descripter
 *	i)	key	key
 *	i)	dat	data
 */
void
dbop_update(DBOP *dbop, const char *key, const char *dat)
{
	dbop_put(dbop, key, dat);
}
/*
 * dbop_first: get first record. 
 * 
 *	i)	dbop	dbop descripter
 *	i)	name	key value or prefix
 *			!=NULL: indexed read by key
 *			==NULL: sequential read
 *	i)	preg	compiled regular expression if any.
 *	i)	flags	following dbop_next call take over this.
 *			DBOP_KEY	read key part
 *			DBOP_PREFIX	prefix read
 *					only valied when sequential read
 *	r)		data
 */
const char *
dbop_first(DBOP *dbop, const char *name, regex_t *preg, int flags)
{
	DB *db = dbop->db;
	DBT key, dat;
	int status;

	dbop->preg = preg;
	if (flags & DBOP_PREFIX && !name)
		flags &= ~DBOP_PREFIX;
	if (name) {
		if (strlen(name) > MAXKEYLEN)
			die("primary key too long.");
		strlimcpy(dbop->key, name, sizeof(dbop->key));
		key.data = (char *)name;
		key.size = strlen(name);
		/*
		 * includes NULL character unless prefix read.
		 */
		if (!(flags & DBOP_PREFIX))
			key.size++;
		dbop->keylen = key.size;
		for (status = (*db->seq)(db, &key, &dat, R_CURSOR);
			status == RET_SUCCESS;
			status = (*db->seq)(db, &key, &dat, R_NEXT)) {
			if (flags & DBOP_PREFIX) {
				if (strncmp((char *)key.data, dbop->key, dbop->keylen))
					return NULL;
			} else {
				if (strcmp((char *)key.data, dbop->key))
					return NULL;
			}
			if (preg && regexec(preg, (char *)key.data, 0, 0, 0) != 0)
				continue;
			break;
		}
	} else {
		dbop->keylen = dbop->key[0] = 0;
		for (status = (*db->seq)(db, &key, &dat, R_FIRST);
			status == RET_SUCCESS;
			status = (*db->seq)(db, &key, &dat, R_NEXT)) {
			/* skip meta records */
			if (ismeta(key.data) && !(dbop->openflags & DBOP_RAW))
				continue;
			if (preg && regexec(preg, (char *)key.data, 0, 0, 0) != 0)
				continue;
			break;
		}
	}
	dbop->lastdat = (char *)dat.data;
	dbop->lastsize = dat.size;
	dbop->lastkey = (char *)key.data;
	dbop->lastkeysize = key.size;
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
		die("dbop_first failed.");
	case RET_SPECIAL:
		return (NULL);
	}
	dbop->ioflags = flags;
	if (flags & DBOP_KEY) {
		strlimcpy(dbop->prev, (char *)key.data, sizeof(dbop->prev));
		return (char *)key.data;
	}
	return ((char *)dat.data);
}
/*
 * dbop_next: get next record. 
 * 
 *	i)	dbop	dbop descripter
 *	r)		data
 *
 * Db_next always skip meta records.
 */
const char *
dbop_next(DBOP *dbop)
{
	DB *db = dbop->db;
	int flags = dbop->ioflags;
	DBT key, dat;
	int status;

	if (dbop->unread) {
		dbop->unread = 0;
		return dbop->lastdat;
	}
	while ((status = (*db->seq)(db, &key, &dat, R_NEXT)) == RET_SUCCESS) {
		assert(dat.data != NULL);
		/* skip meta records */
		if (!(dbop->openflags & DBOP_RAW)) {
			if (flags & DBOP_KEY && ismeta(key.data))
				continue;
			else if (ismeta(dat.data))
				continue;
		}
		if (flags & DBOP_KEY) {
			if (!strcmp(dbop->prev, (char *)key.data))
				continue;
			if (strlen((char *)key.data) > MAXKEYLEN)
				die("primary key too long.");
			strlimcpy(dbop->prev, (char *)key.data, sizeof(dbop->prev));
		}
		dbop->lastdat	= (char *)dat.data;
		dbop->lastsize	= dat.size;
		dbop->lastkey = (char *)key.data;
		dbop->lastkeysize = key.size;
		if (flags & DBOP_PREFIX) {
			if (strncmp((char *)key.data, dbop->key, dbop->keylen))
				return NULL;
		} else if (dbop->keylen) {
			if (strcmp((char *)key.data, dbop->key))
				return NULL;
		}
		if (dbop->preg && regexec(dbop->preg, (char *)key.data, 0, 0, 0) != 0)
			continue;
		return (flags & DBOP_KEY) ? (char *)key.data : (char *)dat.data;
	}
	if (status == RET_ERROR)
		die("dbop_next failed.");
	return NULL;
}
/*
 * dbop_unread: unread record to read again.
 * 
 *	i)	dbop	dbop descripter
 *
 * Dbop_next will read this record later.
 */
void
dbop_unread(DBOP *dbop)
{
	dbop->unread = 1;
}
/*
 * dbop_lastdat: get last data
 * 
 *	i)	dbop	dbop descripter
 *	r)		last data
 */
const char *
dbop_lastdat(DBOP *dbop, int *size)
{
	if (size)
		*size = dbop->lastsize;
	return dbop->lastdat;
}
/*
 * get_flag: get flag value
 */
const char *
dbop_getflag(DBOP *dbop)
{
	int size;
	const char *dat = dbop_lastdat(dbop, &size);
	const char *flag = "";
	/*
	 * Dat format is like follows.
	 * dat 'xxxxxxx\0ffff\0'
	 *      (data)   (flag)
	 */
	if (dat) {
		int i = strlen(dat) + 1;
		if (size > i)
			flag = dat + i;
	}
	return flag;
}
/*
 * dbop_getoption: get option
 */
const char *
dbop_getoption(DBOP *dbop, const char *key)
{
	static char buf[1024];
	const char *p;

	if ((p = dbop_get(dbop, key)) == NULL)
		return NULL;
	if (dbop->lastsize <= strlen(key))
		die("illegal format (dbop_getoption).");
	for (p += strlen(key); *p && isspace((unsigned char)*p); p++)
		;
	strlimcpy(buf, p, sizeof(buf));
	return buf;
}
/*
 * dbop_putoption: put option
 */
void
dbop_putoption(DBOP *dbop, const char *key, const char *string)
{
	char buf[1024];

	if (string)
		snprintf(buf, sizeof(buf), "%s %s", key, string);
	else
		snprintf(buf, sizeof(buf), "%s", key);
	dbop_put(dbop, key, buf);
}
/*
 * dbop_getversion: get format version
 */
int
dbop_getversion(DBOP *dbop)
{
	int format_version = 1;			/* default format version */
	const char *p;

	if ((p = dbop_getoption(dbop, VERSIONKEY)) != NULL)
		format_version = atoi(p);
	return format_version;
}
/*
 * dbop_putversion: put format version
 */
void
dbop_putversion(DBOP *dbop, int version)
{
	char number[32];

	snprintf(number, sizeof(number), "%d", version);
	dbop_putoption(dbop, VERSIONKEY, number);
}
/*
 * dbop_close: close db
 * 
 *	i)	dbop	dbop descripter
 */
void
dbop_close(DBOP *dbop)
{
	DB *db = dbop->db;

#ifdef USE_DB185_COMPAT
	(void)db->close(db);
#else
	/*
	 * If DBOP_REMOVE is specified, omit writing to the disk in __bt_close().
	 */
	(void)db->close(db, (dbop->openflags & DBOP_REMOVE || dbop->dbname[0] == '\0') ? 1 : 0);
#endif
	if (dbop->dbname[0] != '\0') {
		if (dbop->openflags & DBOP_REMOVE)
			(void)unlink(dbop->dbname);
		else if (dbop->perm && chmod(dbop->dbname, dbop->perm) < 0)
			die("cannot change file mode.");
	}
	(void)free(dbop);
}
