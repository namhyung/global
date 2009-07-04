/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2003
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

#include "token.h"
/*
 * target type.
 */
#define DEF	1
#define REF	2
#define SYM	3

#define NOTFUNCTION	".notfunction"
#ifdef __DJGPP__
#define DOS_NOTFUNCTION "_notfunction"
#endif
#define YACC	1

extern int bflag;
extern int eflag;
extern int nflag;
extern int rflag;
extern int sflag;
extern int vflag;
extern int wflag;
extern int debug;

#define PUT(tag, lno, line) do {					\
	DBG_PRINT(level, line);						\
	if (!nflag)							\
		printf("%-16s %4d %-16s %s\n",tag, lno, curfile, line);	\
} while (0)

#define DBG_PRINT(level, a) dbg_print(level, a)

/* parser procedures */
void C(const char *);
void yacc(const char *);
void Cpp(const char *);
void java(const char *);
void php(const char *);
void assembly(const char *);

void dbg_print(int, const char *);
int isnotfunction(char *);
int cmp(const void *, const void *);
