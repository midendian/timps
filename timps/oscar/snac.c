/*
 * timps - Transparent Instant Messaging Proxy Server
 * Copyright (c) 2003-2005 Adam Fritzler <mid@zigamorph.net>
 *
 * timps is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (version 2) as published by the Free
 * Software Foundation.
 *
 * timps is distributed in the hope that it will be useful, but WITHOUT ANY
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
#include <string.h>
#endif

#include <naf/nafmodule.h>
#include <naf/naftlv.h>
#include <gnr/gnrnode.h>

#include "oscar_internal.h"
#include "snac.h"
#include "flap.h"
#include "ckcache.h"
#include "im.h"

int
toscar_newsnacsb(struct nafmodule *mod, naf_sbuf_t *sb, naf_u16_t group, naf_u16_t subtype, naf_u16_t flags, naf_u32_t id)
{

	/* automatic buffer, dynamically resized */
	if (naf_sbuf_init(mod, sb, NULL, 0) == -1)
		return -1;

	toscar_flap_puthdr(sb, 0x02); /* SNAC is always channel 0x02 */

	naf_sbuf_put16(sb, group);
	naf_sbuf_put16(sb, subtype);
	naf_sbuf_put16(sb, flags);
	naf_sbuf_put32(sb, id);

	return 0;
}


static struct touserinfo *
touserinfo__alloc(struct nafmodule *mod)
{
	struct touserinfo *toui;

	if (!(toui = naf_malloc(mod, sizeof(struct touserinfo))))
		return NULL;
	memset(toui, 0, sizeof(struct touserinfo));

	return toui;
}

void
touserinfo_free(struct nafmodule *mod, struct touserinfo *toui)
{

	if (!mod || !toui)
		return;

	naf_tlv_free(mod, toui->tlvh);
	naf_free(mod, toui->sn);
	naf_free(mod, toui);

	return;
}

struct touserinfo *
touserinfo_new(struct nafmodule *mod, const char *sn)
{
	struct touserinfo *toui;

	if (!mod || !sn)
		return NULL;

	if (!(toui = touserinfo__alloc(mod)))
		return NULL;
	if (!(toui->sn = naf_strdup(mod, sn))) {
		touserinfo_free(mod, toui);
		return NULL;
	}

	return toui;
}

struct touserinfo *
touserinfo_extract(struct nafmodule *mod, naf_sbuf_t *sb)
{
	struct touserinfo *toui;
	naf_u8_t snlen;
	naf_u16_t tlvcnt;

	if (!(toui = touserinfo__alloc(mod)))
		return NULL;

	snlen = naf_sbuf_get8(sb);
	if (!(toui->sn = naf_sbuf_getstr(mod, sb, snlen)))
		goto errout;
	toui->evillevel = naf_sbuf_get16(sb);
	tlvcnt = naf_sbuf_get16(sb);
	toui->tlvh = naf_tlv_parse_limit(mod, sb, tlvcnt);

	return toui;
errout:
	touserinfo_free(mod, toui);
	return NULL;
}

int touserinfo_render(struct nafmodule *mod, struct touserinfo *toui, naf_sbuf_t *sb)
{

	if (!mod || !toui || !sb)
		return -1;

	naf_sbuf_put8(sb, (naf_u8_t)strlen(toui->sn));
	naf_sbuf_putstr(sb, toui->sn);
	naf_sbuf_put16(sb, toui->evillevel);
	naf_sbuf_put16(sb, (naf_u16_t)naf_tlv_gettotallength(mod, toui->tlvh));
	naf_tlv_render(mod, toui->tlvh, sb);

	return 0;
}


/*
 * 0001/0002 (client->server) Client Online
 *
 * Although the client will probably make more requests to learn about its
 * environment after this, reception of the Client Online message is when the
 * server will consider the client to be online, and, in the case of a BOS
 * connection, light them up on the buddy lists.
 */
static int
toscar_snachandler_0001_0002(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	char *sn = NULL;
	struct gnrnode *node;

	if (!(conn->type & NAF_CONN_TYPE_CLIENT))
		return HRET_ERROR;

	if ((naf_conn_tag_fetch(mod, conn->endpoint, "conn.screenname", NULL, (void **)&sn) == -1) || !sn) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] received Client Online on connection with no user info; killing\n", conn->cid);
		return HRET_ERROR;
	}

	node = gnr_node_online(mod, sn, OSCARSERVICE,
				GNR_NODE_FLAG_NONE, GNR_NODE_METRIC_LOCAL);
	if (!node) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] [%s] unable to create gnrnode; disconnecting\n", conn->cid, sn);
		return HRET_ERROR;
	}

	return HRET_FORWARD;
}

/*
 * 0001/000b (server->client) Server Pause
 *
 * This is the first step in migration, where the server moves the client to a
 * different server.
 *
 */
static int
toscar_snachandler_0001_000b(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	char *sn = NULL;

	naf_conn_tag_fetch(mod, conn, "conn.screenname", NULL, (void **)&sn);

	dvprintf(mod, "[cid %lu] [%s] received server pause; disconnecting for safety\n", conn->cid, sn);

	return HRET_ERROR;
}

static int
toscar_auth_sendfail(struct nafmodule *mod, struct nafconn *conn, const char *sn, naf_u16_t code, const char *errurl)
{
	naf_sbuf_t sb;
	naf_tlv_t *tlvh = NULL;

	if (toscar_newsnacsb(mod, &sb, 0x0017, 0x0003, 0x0000, 0x00000000) == -1)
		return -1;

	naf_tlv_addstring(mod, &tlvh, 0x0001, sn ? sn : "");
	naf_tlv_addu16(mod, &tlvh, 0x0008, code);
	if (errurl)
		naf_tlv_addstring(mod, &tlvh, 0x0004, errurl);

	naf_tlv_render(mod, tlvh, &sb);

	naf_tlv_free(mod, tlvh);


	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1) {
		naf_sbuf_free(mod, &sb);
		return -1;
	}
	return 0;
}

int
toscar_auth_sendauthinforequest(struct nafmodule *mod, struct nafconn *conn, naf_u32_t snacid, naf_tlv_t *tlvh)
{
	naf_sbuf_t sb;

	if (toscar_newsnacsb(mod, &sb, 0x0017, 0x0006, 0x0000, snacid) == -1)
		return -1;

	naf_tlv_render(mod, tlvh, &sb);


	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1) {
		naf_sbuf_free(mod, &sb);
		return -1;
	}
	return 0;
}

/*
 * 0017/0003 (server->client) Authentication success.
 *
 * In response to the client's 0017/0002, which contained the password hash.
 * This response will contain a cookie and an IP address, and is the last
 * transaction on the connection.  The next step is connecting to BOS at the
 * listed IP and authenticating with the provided cookie.
 */
static int
toscar_snachandler_0017_0003(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	char *sn = NULL, *ip = NULL;
	naf_tlv_t *tlvh, *cktlv = NULL, *iptlv = NULL;
	int ret = HRET_DIGESTED;

	/*
	 * We need to do two things here:
	 *   1) Keep the cookie/IP pair (with attached canonical SN) in a cache
	 *   2) Form a new version of this SNAC modified with our IP address,
	 *      but everything else the same, including any unknown TLVs.
	 */

	tlvh = naf_tlv_parse(mod, &snac->payload);

	/* check for login failure */
	if (naf_tlv_get(mod, tlvh, 0x0004 /* error URL */) ||
			naf_tlv_get(mod, tlvh, 0x0008 /* error code */)) {
		ret = HRET_FORWARD;
		goto out;
	}

	sn = naf_tlv_getasstring(mod, tlvh, 0x0001); /* canonical SN */
	cktlv = naf_tlv_get(mod, tlvh, 0x0006);
	iptlv = naf_tlv_remove(mod, &tlvh, 0x0005); /* remove this one */
	ip = naf_tlv_getasstring(mod, iptlv, 0x0005);
	if (!cktlv || !ip || !sn) {
		ret = HRET_ERROR;
		goto out;
	}

	if (toscar_ckcache_add(mod, cktlv->tlv_value, cktlv->tlv_length, ip, sn, TOSCAR_SERVTYPE_BOS) == -1) {
		ret = HRET_ERROR;
		goto out;
	}

	{ /* make a new IP TLV... */
		char *las;

		if (!(las = naf_conn_getlocaladdrstr(mod, conn->endpoint))) {
			ret = HRET_ERROR;
			goto out;
		}

		naf_tlv_addstring(mod, &tlvh, 0x0005, las);

		naf_free(mod, las);
	}

	{ /* make a new 17/3... */
		naf_sbuf_t sb;

		if (toscar_newsnacsb(mod, &sb, 0x0017, 0x0003, 0x0000, snac->id) == -1) {
			ret = HRET_ERROR;
			goto out;
		}

		naf_tlv_render(mod, tlvh, &sb);

		if (toscar_flap_sendsbuf_consume(mod, conn->endpoint, &sb) == -1) {
			naf_sbuf_free(mod, &sb);
			ret = HRET_ERROR;
			goto out;
		}
	}

out:
	naf_free(mod, sn);
	naf_free(mod, ip);
	naf_tlv_free(mod, iptlv);
	naf_tlv_free(mod, tlvh);
	return ret;
}

/*
 *
 * 0017/0006 (client->server) Request authentication data.
 *
 * This contains the screen name.  The server will respond with a 0017/0007
 * containing a key to be used in the password hash.
 */
static int
toscar_snachandler_0017_0006(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	naf_tlv_t *tlvh;
	char *sn;
	int ret = HRET_DIGESTED;

	/* XXX rate limit login attempts like AIM */

	tlvh = naf_tlv_parse(mod, &snac->payload);

	sn = naf_tlv_getasstring(mod, tlvh, 0x0001);

	/*
	 * Although the registration limits are 3 <= snlen <= 32, the server
	 * limits are actually 2 <= snlen <= 96.  Registration also requires
	 * the screen name to start with a letter, but we don't enforce that
	 * since it would pointless exclude ICQ, which uses the same protocol
	 * but all-numeric screen names.
	 *
	 * The only reason we do this check is to avoid any local problems 
	 * an "allow anything" policy might incur.
	 */
	if (!sn || (strlen(sn) < 2) || (strlen(sn) > 96)) {
		/* invalid screen name */
		toscar_auth_sendfail(mod, conn, sn, 0x0004, "http://www.aol.com?ccode=us&lang=en");
		toscar_flap_sendconnclose(mod, conn, 0, NULL);
		naf_conn_schedulekill(conn);
		ret = HRET_DIGESTED;
		goto out;
	}

	if (0 /* XXX !isuserallowed(sn) */) {
		/* screen name not permitted to use this server */
		toscar_auth_sendfail(mod, conn, sn, 0x0011 /* actually for 'suspended' */, "http://www.aol.com?ccode=us&lang=en");
		toscar_flap_sendconnclose(mod, conn, 0, NULL);
		naf_conn_schedulekill(conn);
		ret = HRET_DIGESTED;
		goto out;
	}

	if (!conn->endpoint) {
		if (naf_conn_startconnect(mod, conn,
					timps_oscar__authorizer,
					TIMPS_OSCAR_DEFAULTPORT) == -1) {
			ret = HRET_ERROR;
			goto out;
		}

		conn->servtype = TOSCAR_SERVTYPE_AUTH;
		conn->endpoint->servtype = TOSCAR_SERVTYPE_AUTH;

		if (naf_conn_tag_add(mod, conn->endpoint, "conn.logintlvs", 'V', (void *)tlvh) == -1) {
			ret = HRET_ERROR;
			goto out;
		}
		tlvh = NULL; /* now owned by the tag */

		if (naf_conn_tag_add(mod, conn->endpoint, "conn.loginsnacid", 'I', (void *)snac->id) == -1) {
			ret = HRET_ERROR;
			goto out;
		}
	}

out:
	naf_free(mod, sn);
	naf_tlv_free(mod, tlvh);
	return ret;
}


typedef int (*toscar_snachandler_t)(struct nafmodule *, struct nafconn *, struct toscar_snac *);
static struct snachandler {
	naf_u16_t group;
	naf_u16_t subtype;
	toscar_snachandler_t handler;
} toscar__snachandlers[] = {
	{0x0001, 0x0002, toscar_snachandler_0001_0002},
	{0x0001, 0x000b, toscar_snachandler_0001_000b},
	{0x0004, 0x0006, toscar_snachandler_0004_0006},
	{0x0004, 0x0007, toscar_snachandler_0004_0007},
	{0x0017, 0x0003, toscar_snachandler_0017_0003},
	{0x0017, 0x0006, toscar_snachandler_0017_0006},
	{0x0000, 0x0000, NULL}
};

int
toscar_flap_handlesnac(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen)
{
	naf_u16_t exthdrlen = 0;
	struct toscar_snac snac;
	int hret = HRET_FORWARD;

#define SNACHDRLEN 10
	if (buflen < SNACHDRLEN) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %ld] runt SNAC\n", conn->cid);
		hret = HRET_ERROR;
		goto out;
	}

	memset(&snac, 0, sizeof(struct toscar_snac));
	snac.group = naf_byte_get16(buf);
	snac.subtype = naf_byte_get16(buf + 2);
	snac.flags = naf_byte_get16(buf + 4);
	snac.id = naf_byte_get32(buf + 6);
	if (snac.flags & 0x8000) { /* extended SNAC header */
		exthdrlen = naf_byte_get16(buf + SNACHDRLEN);
		if (exthdrlen)
			naf_sbuf_init(mod, &snac.extinfo, buf + SNACHDRLEN + 2, exthdrlen);
	}
	if (naf_sbuf_init(mod, &snac.payload,
				buf + SNACHDRLEN + (exthdrlen ? (2 + exthdrlen) : 0),
				(naf_u16_t)(buflen - SNACHDRLEN - (exthdrlen ? (2 + exthdrlen) : 0))) == -1) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %ld] failed to allocate sbuf for SNAC parsing\n", conn->cid);
		hret = HRET_ERROR;
		goto out;
	}

	if (timps_oscar__debug > 1) {
		dvprintf(mod, "[cid %lu] received SNAC %lx / %lx / %lx / %lx, %d bytes payload\n",
				conn->cid,
				snac.group, snac.subtype,
				snac.flags, snac.id,
				naf_sbuf_bytesremaining(&snac.payload));
	}

	/*
	 * XXX We need a generalized way of saying "only these snacs are
	 * permitted right now".  As it stands, a rogue client can, say,
	 * send messages before even logging in, or at any time during the
	 * login process.  They won't make it to the server ('cause it
	 * actually does these checks), but if they send to a local user,
	 * that message will go through without authentication.
	 */

	{ /* dispatch */
		struct snachandler *i;

		for (i = toscar__snachandlers; i->group != 0x0000; i++) {
			if ((i->group == snac.group) &&
					(i->subtype == snac.subtype)) {
				break;
			}
		}

		if (i && i->handler)
			hret = i->handler(mod, conn, &snac);
	}

out:
	if (timps_oscar__debug > 1) {
		dvprintf(mod, "[cid %lu] SNAC handler returned %s%s%s (%d)\n",
				conn->cid,
				(hret == HRET_ERROR) ? "ERROR" : "",
				(hret == HRET_DIGESTED) ? "DIGESTED" : "",
				(hret == HRET_FORWARD) ? "FORWARD" : "",
				hret);
	}
	return hret;
}


