
#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <naf/nafmodule.h>
#include <naf/naftag.h>


#define MAXTAGNAMELEN 64
typedef struct naf_tag_s {
	struct nafmodule *owner;
	char name[MAXTAGNAMELEN+1];
	char type;
	void *data;
	struct naf_tag_s *next;
} naf_tag_t;

int naf_tag_add(void **taglistv, struct nafmodule *mod, const char *name, char type, void *data)
{
	naf_tag_t *tag, **taglist;

	/* Having no data is valid. */
	if (!mod || !taglistv || !name || !strlen(name))
		return -1;

	if (naf_tag_ispresent(taglistv, mod, name) == 1)
		return -1; /* already exists */

	if (!(tag = malloc(sizeof(naf_tag_t))))
		return -1;
	memset(tag, 0, sizeof(naf_tag_t));

	tag->owner = mod;
	strncpy(tag->name, name, MAXTAGNAMELEN);
	tag->type = type;
	tag->data = data;

	taglist = (naf_tag_t **)taglistv;
	tag->next = *taglist;
	*taglist = tag;

	return 0;
}

int naf_tag_cloneall(void **desttaglistv, void **srctaglistv)
{
	naf_tag_t *cur;

	for (cur = *((naf_tag_t **)srctaglistv); cur; cur = cur->next) {

		if (cur->type == 'S') {
			char *newstr;

			if ((newstr = strdup((char *)cur->data))) {
				if (naf_tag_add(desttaglistv, cur->owner, cur->name, cur->type, (void *)newstr) == -1)
					free(newstr);
			}
		} else if (cur->type == 'I') {
			
			naf_tag_add(desttaglistv, cur->owner, cur->name, cur->type, (void *)cur->data);
		} else
			; /* don't clone non-primitive types */
	}

	return 0;
}

int naf_tag_remove(void **taglistv, struct nafmodule *mod, const char *name, char *typeret, void **dataret)
{
	naf_tag_t *tag, **prev;

	if (!mod || !taglistv || !name || !strlen(name))
		return -1;

	for (prev = (naf_tag_t **)taglistv; (tag = *prev); ) {
		if ((tag->owner == mod ) && (strcmp(tag->name, name) == 0)) {
			*prev = tag->next;

			if (typeret)
				*typeret = tag->type;
			if (dataret)
				*dataret = tag->data;
			free(tag);

			return 0;

		} else
			prev = &tag->next;
	}

	return -1;
}

int naf_tag_ispresent(void **taglistv, struct nafmodule *mod, const char *name)
{
	naf_tag_t *tag;

	if (!mod || !taglistv || !name || !strlen(name))
		return -1;

	for (tag = *((naf_tag_t **)taglistv); tag; tag = tag->next) {
		if ((tag->owner == mod) && (strcmp(tag->name, name) == 0))
			return 1;
	}

	return 0;
}

/* secret feature: pass mod as NULL to get any tag by that name */
int naf_tag_fetch(void **taglistv, struct nafmodule *mod, const char *name, char *typeret, void **dataret)
{
	naf_tag_t *tag;

	if (!taglistv || !name || !strlen(name))
		return -1;

	for (tag = *((naf_tag_t **)taglistv); tag; tag = tag->next) {

		if (mod && (tag->owner != mod))
			continue;

		if (strcmp(tag->name, name) == 0) {

			if (typeret)
				*typeret = tag->type;
			if (dataret)
				*dataret = tag->data;

			return 0;
		}
	}

	return -1;
}

void naf_tag_iter(void **taglistv, struct nafmodule *mod, int (*uf)(struct nafmodule *mod, void *ud, const char *tagname, char tagtype, void *tagdata), void *ud)
{
	naf_tag_t *tag;

	if (!taglistv)
		return;

	for (tag = *((naf_tag_t **)taglistv); tag; tag = tag->next) {
		if (uf(mod, ud, tag->name, tag->type, tag->data))
			break;
	}

	return;
}

void naf_tag_freelist(void **taglistv, void *user)
{
	naf_tag_t *tag;

	for (tag = *((naf_tag_t **)taglistv); tag; ) {
		naf_tag_t *tmp;

		tmp = tag->next;

		/* XXX this is confusing for non-conn tags */
		if (tag->owner && tag->owner->freetag)
			tag->owner->freetag(tag->owner, user, tag->name, tag->type, tag->data);
		free(tag);

		tag = tmp;
	}
	taglistv = NULL;

	return;
}


