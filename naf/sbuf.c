/*
 * naf - Networked Application Framework
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * naf is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * naf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef WIN32
#include <configwin32.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h> /* memcpy */
#endif

#include <naf/nafmodule.h>
#include <naf/nafbufutils.h>

/* default initial size of dynamic buffers */
#define NAF_SBUF_DEFAULT_BUFLEN 512
/* size of blocks that buffers grow in */
#define NAF_SBUF_DEFAULT_BLOCKSIZE 256
/* maximum size to allow dynamic buffers to grow to (should be evenly divisible by blocksize) */
#define NAF_SBUF_DEFAULT_MAXBUFLEN 32768

int naf_sbuf_init(struct nafmodule *mod, naf_sbuf_t *sbuf, naf_u8_t *buf, naf_u16_t buflen)
{

	memset(sbuf, 0, sizeof(naf_sbuf_t));

	sbuf->sbuf_owner = mod;
	sbuf->sbuf_flags = NAF_SBUF_FLAG_NONE;
	if (buf) {
		sbuf->sbuf_buf = buf;
		sbuf->sbuf_buflen = buflen;
		/* 
		 * If they pass in their own buffer, default to not freeing it
		 * when we are freed.
		 */
	} else {

		if (!buflen)
			buflen = NAF_SBUF_DEFAULT_BUFLEN;

		/* XXX don't assume NETBUF */
		if (!(sbuf->sbuf_buf = naf_malloc_type(mod, NAF_MEM_TYPE_NETBUF, buflen)))
			return -1;
		sbuf->sbuf_buflen = buflen;
		sbuf->sbuf_flags |= NAF_SBUF_FLAG_FREEBUF | NAF_SBUF_FLAG_AUTORESIZE;
	}
	sbuf->sbuf_pos = 0;

	return 0;
}

void naf_sbuf_free(struct nafmodule *mod, naf_sbuf_t *sbuf)
{

	if (sbuf->sbuf_flags & NAF_SBUF_FLAG_FREEBUF) {
		naf_free(sbuf->sbuf_owner, sbuf->sbuf_buf);
		sbuf->sbuf_buf = NULL;
	}

	return;
}

static int naf_sbuf__extendbuf(naf_sbuf_t *sbuf, naf_u16_t bytesneeded)
{
	naf_u8_t *nbuf;
	naf_u16_t nbuflen;

	if (!(sbuf->sbuf_flags & NAF_SBUF_FLAG_AUTORESIZE))
		return -1;

	/* (awkward because of unsigned) */
	for (nbuflen = sbuf->sbuf_buflen; 
			bytesneeded > NAF_SBUF_DEFAULT_BLOCKSIZE;
			nbuflen += NAF_SBUF_DEFAULT_BLOCKSIZE,
				bytesneeded -= NAF_SBUF_DEFAULT_BLOCKSIZE)
		;
	if (bytesneeded > 0)
		nbuflen += NAF_SBUF_DEFAULT_BLOCKSIZE;

	if (nbuflen > NAF_SBUF_DEFAULT_MAXBUFLEN)
		return -1;

	/* XXX XXX need a naf_realloc() */
	/* XXX don't assume NETBUF */
	if (!(nbuf = naf_malloc_type(sbuf->sbuf_owner, NAF_MEM_TYPE_NETBUF, nbuflen)))
		return -1;
	memcpy(nbuf, sbuf->sbuf_buf, sbuf->sbuf_buflen);
	naf_free(sbuf->sbuf_owner, sbuf->sbuf_buf);
	sbuf->sbuf_buf = nbuf;
	sbuf->sbuf_buflen = nbuflen;

	/* now has at least bytesneeded extra bytes at end */
	return 0;
}

int naf_sbuf_prepend(naf_sbuf_t *sbuf, naf_u16_t bytesneeded)
{

	if (naf_sbuf__extendbuf(sbuf, bytesneeded) == -1)
		return -1;
	memmove(sbuf->sbuf_buf + bytesneeded, sbuf->sbuf_buf, sbuf->sbuf_buflen);

	return 0;
}

int naf_sbuf_getpos(naf_sbuf_t *sbuf)
{
	return sbuf->sbuf_pos;
}

naf_u8_t *naf_sbuf_getposptr(naf_sbuf_t *sbuf)
{
	return sbuf->sbuf_buf + sbuf->sbuf_pos;
}

int naf_sbuf_bytesremaining(naf_sbuf_t *sbuf)
{
	return sbuf->sbuf_buflen - sbuf->sbuf_pos;
}

int naf_sbuf_setpos(naf_sbuf_t *sbuf, naf_u16_t npos)
{

	if ((npos > sbuf->sbuf_buflen) &&
			(naf_sbuf__extendbuf(sbuf, (naf_u16_t)(npos - sbuf->sbuf_buflen)) == -1))
		return -1; /* XXX assert somehow */

	sbuf->sbuf_pos = npos;

	return 0;
}

int naf_sbuf_rewind(naf_sbuf_t *sbuf)
{
	return naf_sbuf_setpos(sbuf, 0);
}

int naf_sbuf_advance(naf_sbuf_t *sbuf, naf_u16_t n)
{

	if ((naf_sbuf_bytesremaining(sbuf) < n) &&
			(naf_sbuf__extendbuf(sbuf, n) == -1))
		return -1; /* XXX assert somehow */

	sbuf->sbuf_pos += n;

	return 0;
}

naf_u8_t naf_sbuf_get8(naf_sbuf_t *sbuf)
{

	if (naf_sbuf_bytesremaining(sbuf) < 1)
		return 0; /* XXX assert somehow */

	sbuf->sbuf_pos += 1;
	return naf_byte_get8(sbuf->sbuf_buf + sbuf->sbuf_pos - 1);
}

naf_u16_t naf_sbuf_get16(naf_sbuf_t *sbuf)
{

	if (naf_sbuf_bytesremaining(sbuf) < 2)
		return 0; /* XXX assert somehow */

	sbuf->sbuf_pos += 2;
	return naf_byte_get16(sbuf->sbuf_buf + sbuf->sbuf_pos - 2);
}

naf_u32_t naf_sbuf_get32(naf_sbuf_t *sbuf)
{

	if (naf_sbuf_bytesremaining(sbuf) < 4)
		return 0; /* XXX assert somehow */

	sbuf->sbuf_pos += 4;
	return naf_byte_get32(sbuf->sbuf_buf + sbuf->sbuf_pos - 4);
}

int naf_sbuf_put8(naf_sbuf_t *sbuf, naf_u8_t v)
{

	if ((naf_sbuf_bytesremaining(sbuf) < 1) &&
			(naf_sbuf__extendbuf(sbuf, 1) == -1))
		return 0; /* XXX assert somehow */

	sbuf->sbuf_pos += naf_byte_put8(sbuf->sbuf_buf + sbuf->sbuf_pos, v);

	return 1;
}

int naf_sbuf_put16(naf_sbuf_t *sbuf, naf_u16_t v)
{

	if ((naf_sbuf_bytesremaining(sbuf) < 2) &&
			(naf_sbuf__extendbuf(sbuf, 2) == -1))
		return 0; /* XXX assert somehow */

	sbuf->sbuf_pos += naf_byte_put16(sbuf->sbuf_buf + sbuf->sbuf_pos, v);

	return 2;
}

int naf_sbuf_put32(naf_sbuf_t *sbuf, naf_u32_t v)
{

	if ((naf_sbuf_bytesremaining(sbuf) < 4) &&
			(naf_sbuf__extendbuf(sbuf, 4) == -1))
		return 0; /* XXX assert somehow */

	sbuf->sbuf_pos += naf_byte_put32(sbuf->sbuf_buf + sbuf->sbuf_pos, v);

	return 4;
}

int naf_sbuf_getrawbuf(naf_sbuf_t *sbuf, naf_u8_t *outbuf, naf_u16_t len)
{

	if (naf_sbuf_bytesremaining(sbuf) < len)
		return 0; /* XXX assert somehow */

	memcpy(outbuf, sbuf->sbuf_buf + sbuf->sbuf_pos, len);
	sbuf->sbuf_pos += len;

	return len;
}

naf_u8_t *naf_sbuf_getraw(struct nafmodule *mod, naf_sbuf_t *sbuf, naf_u16_t len)
{
	naf_u8_t *outbuf;

	if (!(outbuf = naf_malloc(mod, len)))
		return NULL;

	if (naf_sbuf_getrawbuf(sbuf, outbuf, len) < len) {
		naf_free(mod, outbuf);
		return NULL;
	}

	return outbuf;
}

char *naf_sbuf_getstr(struct nafmodule *mod, naf_sbuf_t *sbuf, naf_u16_t len)
{
	naf_u8_t *outbuf;

	if (!(outbuf = naf_malloc(mod, len + 1)))
		return NULL;

	if (naf_sbuf_getrawbuf(sbuf, outbuf, len) < len) {
		naf_free(mod, outbuf);
		return NULL;
	}
	outbuf[len] = '\0';

	return (char *)outbuf;

}

int naf_sbuf_putraw(naf_sbuf_t *sbuf, const naf_u8_t *inbuf, int inbuflen)
{

	if ((naf_sbuf_bytesremaining(sbuf) < inbuflen) &&
			(naf_sbuf__extendbuf(sbuf, (naf_u16_t)inbuflen) == -1))
		return 0; /* XXX assert somehow */

	memcpy(sbuf->sbuf_buf + sbuf->sbuf_pos, inbuf, inbuflen);
	sbuf->sbuf_pos += inbuflen;

	return inbuflen;
}

int naf_sbuf_putstr(naf_sbuf_t *sbuf, const char *instr)
{

	if (!sbuf || !instr)
		return 0;

	return naf_sbuf_putraw(sbuf, (const naf_u8_t *)instr, strlen(instr));
}

int naf_sbuf_cmp(naf_sbuf_t *sbuf, const naf_u8_t *cmpbuf, int cmpbuflen)
{

	if (naf_sbuf_bytesremaining(sbuf) < cmpbuflen)
		return -1; /* not enough to compare */

	if (memcmp(sbuf->sbuf_buf + sbuf->sbuf_pos, cmpbuf, cmpbuflen) == 0)
		return 0; /* match */

	return 1; /* no match */
}

