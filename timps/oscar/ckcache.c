
#include <time.h>
#include <string.h>

#include <naf/nafmodule.h>
#include <naf/naftypes.h>

#include "oscar_internal.h"
#include "ckcache.h"

#define CKCACHE_TIMEOUT 60

struct ckcache {

	/* key */
	naf_u8_t *ck;
	naf_u16_t cklen;

	/* values */
	char *ip;
	char *sn;
	naf_u16_t servtype;

	time_t addtime;

	struct ckcache *next;
};
static struct ckcache *timps_oscar__ckcache = NULL;


static void
ckc__free(struct nafmodule *mod, struct ckcache *ckc)
{

	naf_free(mod, ckc->ck);
	naf_free(mod, ckc->sn);
	naf_free(mod, ckc->ip);
	naf_free(mod, ckc);

	return;
}

static struct ckcache *
ckc__alloc(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, const char *ip, const char *sn, naf_u16_t servtype)
{
	struct ckcache *ckc;

	if (!(ckc = naf_malloc(mod, sizeof(struct ckcache))))
		return NULL;
	memset(ckc, 0, sizeof(struct ckcache));

	if (!(ckc->ck = naf_malloc(mod, cklen)) ||
			!(ckc->ip = naf_strdup(mod, ip)) ||
			!(ckc->sn = naf_strdup(mod, sn))) {
		ckc__free(mod, ckc);
		return NULL;
	}
	memcpy(ckc->ck, ck, cklen);
	ckc->cklen = cklen;

	return ckc;
}

static struct ckcache *
toscar_ckcache__find(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen)
{
	struct ckcache *ckc;

	for (ckc = timps_oscar__ckcache; ckc; ckc = ckc->next) {
		if ((ckc->cklen == cklen) &&
				(memcmp(ckc->ck, ck, cklen) == 0))
			return ckc;
	}

	return NULL;
}

int
toscar_ckcache_add(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, const char *ip, const char *sn, naf_u16_t servtype)
{
	struct ckcache *ckc;

	if (!mod || !ck || !cklen || !ip)
		return -1;

	if (toscar_ckcache__find(mod, ck, cklen)) {
		if (timps_oscar__debug > 0)
			dprintf(mod, "ckcache: attempted to add duplicate cookie\n");
		return -1; /* dups are bad... */
	}

	if (!(ckc = ckc__alloc(mod, ck, cklen, ip, sn, servtype)))
		return -1;
	ckc->addtime = time(NULL);

	ckc->next = timps_oscar__ckcache;
	timps_oscar__ckcache = ckc;

	if (timps_oscar__debug > 1) {
		dvprintf(mod, "ckcache: added cookie %02x %02x %02x %02x\n",
				ckc->ck[0], ckc->ck[1],
				ckc->ck[2], ckc->ck[3]);
	}

	return 0;
}

static struct ckcache *
toscar_ckcache__remove(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen)
{
	struct ckcache *cur, **prev;

	for (prev = &timps_oscar__ckcache; (cur = *prev); ) {
		if ((cur->cklen == cklen) &&
				(memcmp(cur->ck, ck, cklen) == 0)) {
			*prev = cur->next;
			return cur;
		}
		prev = &cur->next;
	}

	return NULL;
}

int
toscar_ckcache_rem(struct nafmodule *mod, naf_u8_t *ck, naf_u16_t cklen, char **ipret, char **snret, naf_u16_t *servtyperet)
{
	struct ckcache *ckc;

	if (timps_oscar__debug > 1) {
		dvprintf(mod, "ckcache: looking to remove cookie %02x %02x %02x %02x\n",
				ck[0], ck[1],
				ck[2], ck[3]);
	}

	if (!(ckc = toscar_ckcache__remove(mod, ck, cklen)))
		return -1;

	if (ipret) {
		*ipret = ckc->ip;
		ckc->ip = NULL;
	}
	if (snret) {
		*snret = ckc->sn;
		ckc->sn = NULL;
	}
	if (servtyperet)
		*servtyperet = ckc->servtype;

	ckc__free(mod, ckc);

	return 0;
}

void
toscar_ckcache_timer(struct nafmodule *mod, time_t now)
{
	struct ckcache *cur, **prev;

	for (prev = &timps_oscar__ckcache; (cur = *prev); ) {
		if ((now - cur->addtime) > CKCACHE_TIMEOUT) {
			*prev = cur->next;
			ckc__free(mod, cur);
		} else
			prev = &cur->next;
	}

	return;
}

