
/*
 * Wrappers / debugging for memory management.
 *
 */

#include <naf/nafmodule.h>
#include <naf/naftypes.h>
#include <naf/nafrpc.h>

#include "memory.h"
#include "module.h" /* naf_module_iter() */

/* Undefine the breaking macros and pull in the real ones */
#undef malloc
#undef free
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <sys/mman.h> /* m(un)map() */

#define CHATTYFLMP


struct memory_stats {
	naf_u32_t regions;
	naf_u32_t current; 
	naf_u32_t maximum;
};

struct module_memory_stats {
	struct memory_stats totals;
	struct memory_stats type_generic;
	struct memory_stats type_netbuf;
};


static int naf_memory__module_init(struct nafmodule *mod)
{

	if (mod->memorystats)
		return 0;

	if (!(mod->memorystats = malloc(sizeof(struct module_memory_stats))))
		return -1;
	memset(mod->memorystats, 0, sizeof(struct module_memory_stats));

	return 0;
}

void naf_memory__module_free(struct nafmodule *mod)
{

	free(mod->memorystats);

	return;
}


static int memuseiter(struct nafmodule *mod, struct nafmodule *cur, void *udata)
{
	naf_rpc_arg_t **top = (naf_rpc_arg_t **)udata;
	naf_rpc_arg_t **ptop;

	if ((ptop = naf_rpc_addarg_array(mod, top, cur->name)) && cur->memorystats) {
		struct module_memory_stats *pms = (struct module_memory_stats *)cur->memorystats;
		naf_rpc_arg_t **ms;

		if ((ms = naf_rpc_addarg_array(mod, ptop, "total"))) {
			naf_rpc_addarg_scalar(mod, ms, "regions_outstanding", pms->totals.regions);
			naf_rpc_addarg_scalar(mod, ms, "current_outstanding", pms->totals.current);
			naf_rpc_addarg_scalar(mod, ms, "maximum_outstanding", pms->totals.maximum);
		}

		if ((ms = naf_rpc_addarg_array(mod, ptop, "type_generic"))) {
			naf_rpc_addarg_scalar(mod, ms, "regions_outstanding", pms->type_generic.regions);
			naf_rpc_addarg_scalar(mod, ms, "current_outstanding", pms->type_generic.current);
			naf_rpc_addarg_scalar(mod, ms, "maximum_outstanding", pms->type_generic.maximum);
		}

		if ((ms = naf_rpc_addarg_array(mod, ptop, "type_netbuf"))) {
			naf_rpc_addarg_scalar(mod, ms, "regions_outstanding", pms->type_netbuf.regions);
			naf_rpc_addarg_scalar(mod, ms, "current_outstanding", pms->type_netbuf.current);
			naf_rpc_addarg_scalar(mod, ms, "maximum_outstanding", pms->type_netbuf.maximum);
		}
	}

	return 0;
}

/*
 * core->modmemoryuse()
 *   IN:
 *      [optional] string module;
 *
 *   OUT:
 *      [optional] array modules {
 *          array modulename {
 *              scalar current_outstanding;
 *              scalar maximum_outstanding;
 *          }
 *      }
 */
void __rpc_core_modmemoryuse(struct nafmodule *mod, naf_rpc_req_t *req)
{
	naf_rpc_arg_t *module, **modules;

	if ((module = naf_rpc_getarg(req->inargs, "module"))) {
		if (module->type != NAF_RPC_ARGTYPE_STRING) {
			req->status = NAF_RPC_STATUS_INVALIDARGS;
			return;
		}
	}

	if ((modules = naf_rpc_addarg_array(mod, &req->returnargs, "modules"))) {

		if (module) {
			struct nafmodule *pmod;

			if ((pmod = naf_module_findbyname(mod, module->data.string)))
				memuseiter(mod, pmod, (void *)modules);

		} else
			naf_module_iter(mod, memuseiter, (void *)modules);
	}

	req->status = NAF_RPC_STATUS_SUCCESS;

	return;
}


/* most unoriginal magic numbers evar. */
#define HDR_MAGIC_START 0xbeefbeef
#define HDR_MAGIC_END   0xfeebfeeb
#define FTR_MAGIC_END   {0xde, 0xad, 0xbe, 0xef}
struct naf_mem_header { /* try to keep sizeof naf_mem_header at multiple of 4 */
	naf_u32_t hdrmagic1;
	naf_u16_t hdrlen; /* so we can analyze cores of old versions */
	naf_u16_t type;
	struct nafmodule *owner;
	naf_u32_t buflen;
	naf_u32_t hdrmagic2;
};

void *naf_malloc_real(struct nafmodule *mod, int type, size_t reqsize, const char *file, int line)
{
	void *buf;
	int buflen;
	struct naf_mem_header *hdr;
	static const char ftrmatch[] = FTR_MAGIC_END;

	if (naf_memory_debug) {
		dvprintf(NULL, "[%s:%d] NAF_MALLOC(%p=%s, 0x%04x, %d)\n",
				file, line,
				mod,
				mod ? mod->name : "(none)",
				type,
				reqsize);
	}

	/* XXX have the option to turn on/off stats for certain modules */
	if (mod && !mod->memorystats)
		naf_memory__module_init(mod);

	buflen = sizeof(struct naf_mem_header) + reqsize + sizeof(ftrmatch);

	if (!(buf = malloc(buflen))) {

		if (naf_memory_debug) {
			dvprintf(NULL, "[%s:%d] NAF_MALLOC(%p=%s, 0x%04x, %d) FAILED\n",
					file, line,
					mod,
					mod ? mod->name : "(none)",
					type,
					reqsize);
		}

		return NULL;
	}

	hdr = (struct naf_mem_header *)buf;
	hdr->hdrmagic1 = HDR_MAGIC_START;
	hdr->hdrlen = sizeof(struct naf_mem_header);
	hdr->type = type;
	hdr->owner = mod;
	hdr->buflen = reqsize;
	hdr->hdrmagic2 = HDR_MAGIC_END;

	memcpy(buf + sizeof(struct naf_mem_header) + reqsize, ftrmatch, sizeof(ftrmatch));

	if (mod && mod->memorystats) {
		struct module_memory_stats *pms = (struct module_memory_stats *)mod->memorystats;

		pms->totals.regions++;
		pms->totals.current += reqsize;
		if (pms->totals.current > pms->totals.maximum)
			pms->totals.maximum = pms->totals.current;

		if (type == NAF_MEM_TYPE_GENERIC) {

			pms->type_generic.regions++;
			pms->type_generic.current += reqsize;
			if (pms->type_generic.current > pms->type_generic.maximum)
				pms->type_generic.maximum = pms->type_generic.current;

		} else if (type == NAF_MEM_TYPE_NETBUF) {

			pms->type_netbuf.regions++;
			pms->type_netbuf.current += reqsize;
			if (pms->type_netbuf.current > pms->type_netbuf.maximum)
				pms->type_netbuf.maximum = pms->type_netbuf.current;
		}
	}

	if (naf_memory_debug) {
		dvprintf(NULL, "[%s:%d] NAF_MALLOC(%p=%s, 0x%04x, %d) = %p/%p\n",
				file, line,
				mod,
				mod ? mod->name : "(none)",
				type,
				reqsize,
				buf, buf + sizeof(struct naf_mem_header));
	}

	return buf + sizeof(struct naf_mem_header);
}

void naf_free_real(struct nafmodule *mod, void *ptr, const char *file, int line)
{
	struct naf_mem_header *hdr;
	unsigned char *ftr;
	static const char ftrmatch[] = FTR_MAGIC_END;

	if (naf_memory_debug) {
		dvprintf(NULL, "[%s:%d] NAF_FREE(%p=%s, %p)\n",
				file, line,
				mod,
				mod ? mod->name : "(none)",
				ptr);
	}

	if (!ptr)
		return; /* nothing to do */

	hdr = (struct naf_mem_header *)(ptr - sizeof(struct naf_mem_header));
	/*
	 * This is sort of dangerous.  If the address was not allocated by us
	 * and it starts right at a protected page boundry, even checking one
	 * field before the start of the pointer will segfault (which would
	 * be incorrect behavior).  Do that check first, then if that's okay,
	 * go on to the first magic string at the beginning of the buffer.
	 *
	 * Unfortunately this keeps us from detecting an overrun...  Someday
	 * when everything is guarenteed to be using this appropriately, we
	 * can turn off this check and get the full benefit.
	 *
	 */
	if ((hdr->hdrmagic2 != HDR_MAGIC_END) ||
			(hdr->hdrmagic1 != HDR_MAGIC_START)) {

		if (naf_memory_debug) {
			dvprintf(NULL, "[%s:%d] NAF_FREE(%p=%s, %p) -- buffer was not allocated with naf_malloc!\n",
					file, line,
					mod,
					mod ? mod->name : "(none)",
					ptr);
		}

		/* assuming it's not ours, free the pointer as given */
		free(ptr);

		return;
	}

	if (naf_memory_debug) {
		if (hdr->owner != mod) {
			dvprintf(NULL, "[%s:%d] NAF_FREE(%p=%s, %p) -- buffer originally allocated by %p=%s... mismatch!\n",
					file, line,
					mod,
					mod ? mod->name : "(none)",
					ptr,
					hdr->owner,
					hdr->owner ? hdr->owner->name : "(none)");

		}
	}

	ftr = ((unsigned char *)hdr) + hdr->hdrlen + hdr->buflen;
	if (memcmp(ftr, ftrmatch, sizeof(ftrmatch)) != 0) {
		dvprintf(NULL, "[%s:%d] NAF_FREE(%p=%s, %p) -- footer magic corrupt, someone ran over us!\n",
				file, line,
				mod,
				mod ? mod->name : "(none)",
				ptr);

		abort(); /* yum. */
	}

	if (mod && mod->memorystats) {
		struct module_memory_stats *pms = (struct module_memory_stats *)mod->memorystats;

		pms->totals.current -= hdr->buflen;
		pms->totals.regions--;
		if (hdr->type == NAF_MEM_TYPE_GENERIC) {
			pms->type_generic.current -= hdr->buflen;
			pms->type_generic.regions--;
		} else if (hdr->type == NAF_MEM_TYPE_NETBUF) {
			pms->type_netbuf.current -= hdr->buflen;
			pms->type_netbuf.regions--;
		}
	}

	/* Finally, free it... */
	free(hdr);

	return;
}

char *naf_strdup_type(struct nafmodule *mod, int type, const char *s)
{
	char *r;

	if (!(r = naf_malloc_type(mod, type, strlen(s) + 1)))
		return NULL;
	memcpy(r, s, strlen(s) + 1);

	return r;
}

naf_flmempool_t *naf_flmp_alloc(struct nafmodule *owner, int memtype, int blklen, int blkcount)
{
	naf_flmempool_t *flmp;

	/*
	 * Prefer to have block size a power of two.
	 */
	if ((blklen <= 0) || ((blklen & (blklen - 1))))
		return NULL;

	/*
	 * In the allocmap, we use one bit to represent the free/allocated
	 * state of one block, therefore it is helpful if the block count is a
	 * multiple of eight.
	 */
	if ((blkcount <= 0) || ((blkcount % 8) > 0))
		return NULL;


	flmp = (naf_flmempool_t *)naf_malloc_type(owner, memtype, sizeof(naf_flmempool_t));
	if (!flmp)
		return NULL;

	flmp->flmp_blkcount = blkcount;
	flmp->flmp_blklen = blklen;
	flmp->flmp_regionlen = flmp->flmp_blklen * flmp->flmp_blkcount;
	flmp->flmp_allocmaplen = flmp->flmp_blkcount / 8;

#ifdef CHATTYFLMP
	dvprintf(NULL, "()()()() naf_flmp_alloc: allocating pool of %d blocks, %d bytes each, total region of %d bytes, map size of %d\n",
			flmp->flmp_blkcount,
			flmp->flmp_blklen,
			flmp->flmp_regionlen,
			flmp->flmp_allocmaplen);
#endif

	flmp->flmp_allocmap = (naf_u8_t *)naf_malloc_type(owner, memtype, flmp->flmp_allocmaplen);
	if (!flmp->flmp_allocmap) {
		naf_free(owner, flmp);
		return NULL;
	}

	/* XXX should be a naf_mmap() for stats */
	flmp->flmp_region = (naf_u8_t *)mmap(NULL,
			flmp->flmp_regionlen, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!flmp->flmp_region) {
		naf_free(owner, flmp->flmp_allocmap);
		naf_free(owner, flmp);
		return NULL;
	}

	/*
	 * Most-significant bit of flmp_allocmap[0] is state of first block.
	 */
	memset(flmp->flmp_allocmap, 0, flmp->flmp_allocmaplen);


	/* XXX attach it to the module for stats. */

	return flmp;
}

void naf_flmp_free(struct nafmodule *owner, naf_flmempool_t *flmp)
{

	if (!flmp)
		return;

	/* XXX naf_munmap() */
	munmap(flmp->flmp_region, flmp->flmp_regionlen);
	naf_free(owner, flmp->flmp_allocmap);
	naf_free(owner, flmp);

	return;
}

void *naf_flmp_blkalloc(struct nafmodule *owner, naf_flmempool_t *flmp)
{
	int i, j;
	void *block;

	for (i = 0; i < flmp->flmp_allocmaplen; i++) {
		if (flmp->flmp_allocmap[i] < 255 /* 2**sizeof(naf_u8_t)-1 */)
			break;
	}
	if (i == flmp->flmp_allocmaplen)
		return NULL; /* no free blocks left */

	/* start at left/top/MSb */
	for (j = 7; j >= 0; j--) {
		if (((flmp->flmp_allocmap[i] >> j) & 0x01) == 0x00)
			break;
	}
	if (j == -1)
		abort(); /* better not happen... */

	flmp->flmp_allocmap[i] |= 0x01 << j; /* mark allocated */
	block = flmp->flmp_region + (((i * 8) + (7 - j)) * flmp->flmp_blklen);

#ifdef CHATTYFLMP
	dvprintf(NULL, "()()()() naf_flmp_blkalloc: returning block number %d, ptr %p, map index [%d,%d]\n",
			(i * 8) + (7 - j),
			block,
			i, j);
#endif

	return block;
}

void naf_flmp_blkfree(struct nafmodule *owner, naf_flmempool_t *flmp, void *block)
{
	naf_u32_t blknum;
	int i, j;

	if (!flmp || !block)
		return;

	if ((naf_u8_t *)block < flmp->flmp_region)
		abort(); /* not inside this pool (below) */
	blknum = (naf_u32_t)((naf_u8_t *)block - flmp->flmp_region) / (naf_u32_t)flmp->flmp_blklen;
	if (blknum > flmp->flmp_blkcount)
		abort(); /* not inside this pool (above) */

	i = blknum / 8; j = blknum % 8;
#ifdef CHATTYFLMP
	dvprintf(NULL, "()()()() naf_flmp_blkfree: block = %p, blknum = %d, map index [%d, %d]\n",
			block,
			blknum,
			i, j);
#endif
	if (!((flmp->flmp_allocmap[i] >> (7 - j)) & 0x01))
		abort(); /* not allocated! whoops! */
	flmp->flmp_allocmap[i] ^= 0x01 << (7 - j); /* mark free */

	return;
}

