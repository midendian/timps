
#ifndef __TAGS_H__
#define __TAGS_H__

int naf_tag_add(void **taglistv, struct nafmodule *mod, const char *name, char type, void *data);
int naf_tag_remove(void **taglistv, struct nafmodule *mod, const char *name, char *typeret, void **dataret);
int naf_tag_ispresent(void **taglistv, struct nafmodule *mod, const char *name);
int naf_tag_fetch(void **taglistv, struct nafmodule *mod, const char *name, char *typeret, void **dataret);
void naf_tag_iter(void **taglistv, struct nafmodule *mod, int (*uf)(struct nafmodule *mod, void *ud, const char *tagname, char tagtype, void *tagdata), void *ud);
void naf_tag_freelist(void **taglistv, void *user);
int naf_tag_cloneall(void **desttaglistv, void **srctaglistv);


#endif /* __TAGS_H__ */

