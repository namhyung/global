/*
 * Copyright (c) 2011  Namhyung Kim <namhyung@gmail.com>
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "internal.h"
#include "die.h"
#include "strbuf.h"
#include "strlimcpy.h"
#include "token.h"

enum kconfig_keywords {
	KCONFIG_ITEM = 2001,
	KCONFIG_TYPE,
	KCONFIG_HELP,
	KCONFIG_MENU,
	KCONFIG_DEFAULT,
	KCONFIG_DEPENDS,
	KCONFIG_SELECTS,
	KCONFIG_CONDITION,
};

int
kconfig_reserved_word(const char *str, int len)
{
	if (!strncmp("config", str, len) ||
	    !strncmp("menuconfig", str, len))
		return KCONFIG_ITEM;

	return 0;
}

void
kconfig(const struct parser_param *param)
{
	int cc;
	int level = 0;			/* implicitly used in PUT() */
	int items = 0;
	int lines = 0;
	int configs = 0;
	int symbols = 0;
	int config_item = 0;
	int line_start = 1;
	STRBUF *sb = strbuf_open(0);

	if (!opentoken(param->file))
		die("'%s' cannot open.", param->file);

	crflag = 1;		/* require nexttoken() returns '\n' */

	while ((cc = nexttoken("\n", kconfig_reserved_word)) != EOF) {
		switch (cc) {
		case KCONFIG_ITEM:
			if (line_start)
				config_item = 1;
			line_start = 0;
			configs++;
			break;

		case '\n':
			config_item = 0;
			line_start = 1;
			lines++;
			break;

		case SYMBOL:
			if (config_item) {
				char buf[256];
				snprintf(buf, 256, "CONFIG_%s", token);
				PUT(PARSER_DEF, buf, lineno, sp);
				items++;
			}
			symbols++;
			/* fall through */
		default:
			config_item = 0;
			line_start = 0;
			break;
		}
	}

	if (param->flags & PARSER_DEBUG) {
		printf("number of items   found: %d\n", items);
		printf("number of configs found: %d\n", configs);
		printf("number of symbols found: %d\n", symbols);
		printf("number of total   lines: %d\n", lines);
	}

	strbuf_close(sb);

	closetoken();
}
