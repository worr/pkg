#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "pkg_compat.h"

static void
str_lowercase(char *str)
{
	while (*str) {
		*str = tolower(*str);
		++str;
	}
}


static struct plist *
pkg_compat_new_plist_entry(void)
{
	struct plist *ret;

	ret = (struct plist *)malloc(sizeof(struct plist));
	bzero(ret, sizeof(struct plist));
	return ret;
}

static void
pkg_compat_delete_plist(struct oldpackage *pkg, bool all, enum plist_t type, const char *name)
{
	struct plist *p = pkg->head;

	while (p) {
		struct plist *pnext = p->next;

		if (p->type == type && (!name || !strcmp(name, p->name))) {
			free(p->name);
			if (p->prev)
				p->prev->next = pnext;
			else
				pkg->head = pnext;
			if (pnext)
				pnext->prev = p->prev;
			else
				pkg->tail = p->prev;
			free(p);
			if (!all)
				return;
			p = pnext;
		}
		else
			p = p->next;
	}
}

static void
pkg_compat_free_plist(struct oldpackage *pkg)
{
	struct plist *p = pkg->head;

	while (p) {
		struct plist *p1 = p->next;

		free(p->name);
		free(p);
		p = p1;
	}
	pkg->head = pkg->tail = NULL;
}

static int
pkg_compat_plist_cmd(const char *s, char **arg)
{
	char cmd[FILENAME_MAX + 20];    /* 20 == fudge for max cmd len */
	char *cp;
	const char *sp;

	strlcpy(cmd, s, sizeof(cmd));
	str_lowercase(cmd);
	cp = cmd;
	sp = s;
	while (*cp) {
		if (isspace(*cp)) {
			*cp = '\0';
			while (isspace(*sp)) /* Never sure if macro, increment later */
				++sp;
			break;
		}
		++cp, ++sp;
	}
	if (arg)
		*arg = (char *)sp;
	if (!strcmp(cmd, "cwd"))
		return PLIST_CWD;
	else if (!strcmp(cmd, "srcdir"))
		return PLIST_SRC;
	else if (!strcmp(cmd, "cd"))
		return PLIST_CWD;
	else if (!strcmp(cmd, "exec"))
		return PLIST_CMD;
	else if (!strcmp(cmd, "unexec"))
		return PLIST_UNEXEC;
	else if (!strcmp(cmd, "mode"))
		return PLIST_CHMOD;
	else if (!strcmp(cmd, "owner"))
		return PLIST_CHOWN;
	else if (!strcmp(cmd, "group"))
		return PLIST_CHGRP;
	else if (!strcmp(cmd, "noinst"))
		return PLIST_NOINST;
	else if (!strcmp(cmd, "comment")) {
		if (!strncmp(*arg, "ORIGIN:", 7)) {
			*arg += 7;
			return PLIST_ORIGIN;
		} else if (!strncmp(*arg, "DEPORIGIN:", 10)) {
			*arg += 10;
			return PLIST_DEPORIGIN;
		}
		return PLIST_COMMENT;
	} else if (!strcmp(cmd, "ignore"))
		return PLIST_IGNORE;
	else if (!strcmp(cmd, "ignore_inst"))
		return PLIST_IGNORE_INST;
	else if (!strcmp(cmd, "name"))
		return PLIST_NAME;
	else if (!strcmp(cmd, "display"))
		return PLIST_DISPLAY;
	else if (!strcmp(cmd, "pkgdep"))
		return PLIST_PKGDEP;
	else if (!strcmp(cmd, "conflicts"))
		return PLIST_CONFLICTS;
	else if (!strcmp(cmd, "mtree"))
		return PLIST_MTREE;
	else if (!strcmp(cmd, "dirrm"))
		return PLIST_DIR_RM;
	else if (!strcmp(cmd, "option"))
		return PLIST_OPTION;
	else
		return -1;
}

static void
pkg_compat_add_plist(cJSON *p, enum plist_t type, const char *arg)
{
	char *tmp;

	switch (type) {
		case PLIST_NAME:
			tmp = strrchr(arg, '-');
			tmp[0] = '\0';
			tmp++;
			cJSON_AddStringToObject(p, "name", arg);
			cJSON_AddStringToObject(p, "version", tmp);
			break;

		case PLIST_ORIGIN:
			cJSON_AddStringToObject(p, "origin", arg);
			break;
		
		case PLIST_COMMENT:
			cJSON_AddStringToObject(p, "comment", arg);
			break;

		default:
			break;
	}
}

static void
pkg_compat_read_plist(cJSON *pkg, char *plist_str)
{
	int cmd;
	char *buf, *next, *cp;

	buf = plist_str;
	while ((next = strchr(buf, '\n')) != NULL) {
		next[0] = '\0';

		while (strlen(buf) > 0 && isspace(buf[strlen(buf) - 1]))
			buf[strlen(buf) - 1] = '\0';

		if (buf[0] != '@') {
			cmd = PLIST_FILE;
			goto bottom;
		}

		cmd = pkg_compat_plist_cmd(buf + 1, &cp);
		if (cmd == -1) {
			warnx("%s: unknown command '%s'",
					__func__, buf);
			goto bottom;
		}
		if (*cp == '\0') {
			cp = NULL;
			if (cmd == PLIST_PKGDEP) {
				warnx("corrupted record (pkgdep line without argument), ignoring");
				cmd = -1;
			}
			goto bottom;
		}
bottom:
		pkg_compat_add_plist(pkg, cmd, cp);
		buf = next;
		buf++;
	}
}

cJSON *
pkg_compat_converter(char *plist_str)
{
	cJSON *rootpkg = cJSON_CreateObject();
	pkg_compat_read_plist(rootpkg, plist_str);
	return rootpkg;
}
