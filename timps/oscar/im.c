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

#include <string.h>
#include <stdlib.h>

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/naftlv.h>
#include <naf/nafbufutils.h>
#include <gnr/gnrmsg.h>

#include "oscar_internal.h"
#include "snac.h"
#include "im.h"
#include "flap.h"

#define MSGCOOKIELEN 8

static char *
toscar_icbm__extractmsgtext(struct nafmodule *mod, naf_tlv_t *msgtlv)
{
	naf_sbuf_t sb;
	char *msgtext = NULL, *msgtextend = NULL;
	naf_u16_t featlen;

	if (naf_sbuf_init(mod, &sb, msgtlv->tlv_value, msgtlv->tlv_length) == -1)
		return NULL;

	/* 0501 */
	if (naf_sbuf_get8(&sb) != 0x05)
		goto out;
	if (naf_sbuf_get8(&sb) != 0x01)
		goto out;

	/* features */
	featlen = naf_sbuf_get16(&sb);
	naf_sbuf_advance(&sb, featlen);

	while (naf_sbuf_bytesremaining(&sb) > 0) {
		naf_u16_t plen, f1, f2;

		/* 0101 */
		if (naf_sbuf_get8(&sb) != 0x01)
			goto out;
		if (naf_sbuf_get8(&sb) != 0x01)
			goto out;

		plen = naf_sbuf_get16(&sb);
		if (!plen)
			continue;

		/* encoding flags */
		f1 = naf_sbuf_get16(&sb);
		f2 = naf_sbuf_get16(&sb);

		if ((f1 == 0x0000) || (f2 == 0x0003)) { /* ASCII7, ISO-8859-1 */

			/* XXX it would help if there were a naf_realloc(). */
			if (!msgtext) {
				if (!(msgtext = naf_malloc(mod, plen - 2 - 2 + 1)))
					goto out;
				*msgtext = '\0';
				msgtextend = msgtext;
			} else {
				char *nmsgtext;

				if (!(nmsgtext = naf_malloc(mod, (msgtextend - msgtext) + (plen - 2 - 2) + 1)))
					goto out;
				memcpy(nmsgtext, msgtext, (msgtextend - msgtext) + 1);
				msgtextend = nmsgtext + (msgtextend - msgtext);
				naf_free(mod, msgtext);
				msgtext = nmsgtext;
			}
			naf_sbuf_getrawbuf(&sb, (naf_u8_t *)msgtextend, plen - 2 - 2);
			msgtextend += plen - 2 - 2;
			*msgtextend = '\0';

		} else
			naf_sbuf_advance(&sb, plen - 2 - 2);

		/* XXX translate UTF16 parts into &#nnnn; entities */
	}

out:
	naf_sbuf_free(mod, &sb);
	return msgtext;
}

static int
_naf_tlv_addoscarmsgblock(struct nafmodule *mod, naf_tlv_t **tlvh, naf_u16_t type, const char *msgtext)
{
	naf_u8_t *buf;
	int buflen;
	naf_sbuf_t sb;

	if (!mod || !tlvh || !msgtext)
		return -1;

	/* I hope you like (seemingly) magic numbers. */

	buflen = 2 + 2 + 4 + 2 + 2 + 4 + strlen(msgtext);
	if (!(buf = naf_malloc(mod, buflen)))
		return -1;
	naf_sbuf_init(mod, &sb, buf, buflen);

	naf_sbuf_put8(&sb, 0x05);
	naf_sbuf_put8(&sb, 0x01);

	naf_sbuf_put16(&sb, 0x0004);
		naf_sbuf_put8(&sb, 0x01);
		naf_sbuf_put8(&sb, 0x01);
		naf_sbuf_put8(&sb, 0x01);
		naf_sbuf_put8(&sb, 0x02);

	naf_sbuf_put8(&sb, 0x01);
	naf_sbuf_put8(&sb, 0x01);
	naf_sbuf_put16(&sb, strlen(msgtext) + 4);
		naf_sbuf_put16(&sb, 0x0000); /* assume ASCII encoding */
		naf_sbuf_put16(&sb, 0x0000);
		naf_sbuf_putstr(&sb, msgtext);

	naf_tlv_addraw(mod, tlvh, type, naf_sbuf_getpos(&sb), buf);

	naf_free(mod, buf);

	return 0;
}

static int
toscar_icbm__parsechan1(struct nafmodule *mod, struct nafconn *conn, struct gnrmsg *gm, naf_tlv_t **tlvh)
{
	naf_tlv_t *tlv;

	if ((tlv = naf_tlv_remove(mod, tlvh, 0x0002))) {

		gm->msgtexttype = "text/html";
		gm->msgtext = toscar_icbm__extractmsgtext(mod, tlv);

		/*
		 * Since there can be a lot here that isn't easily expressable
		 * in another form, we'll pass the whole block along to hint
		 * us later.
		 */
		if (gnr_msg_tag_add(mod, gm, "gnrmsg.oscarmsgtlv", 'V', (void *)tlv) == -1)
			naf_tlv_free(mod, tlv);
		tlv = NULL;
	}

	 /* XXX handle the bug related to host acks */
	if ((tlv = naf_tlv_remove(mod, tlvh, 0x0003)))
		gm->msgflags |= GNR_MSG_MSGFLAG_ACKREQUESTED;

	if ((tlv = naf_tlv_remove(mod, tlvh, 0x0004)))
		gm->msgflags |= GNR_MSG_MSGFLAG_AUTORESPONSE;

	return 0;
}

static int
toscar_icbm__renderchan1(struct nafmodule *mod, struct gnrmsg *gm, naf_sbuf_t *sb)
{
	naf_tlv_t *msgtlv = NULL;
	naf_tlv_t *tlvh = NULL;
	naf_tlv_t *extratlvs = NULL;


	/*
	 * Hopefully no one modified gm->msgtext on us, so we can just
	 * send the cached msgtlv without regenerating it.
	 */
	gnr_msg_tag_fetch(mod, gm, "gnrmsg.oscarmsgtlv", NULL, (void **)&msgtlv);
	if (msgtlv)
		naf_tlv_render(mod, msgtlv, sb);
	else {
		/* If we don't have one, make one... */
		_naf_tlv_addoscarmsgblock(mod, &msgtlv, 0x0002, gm->msgtext);
		naf_tlv_render(mod, tlvh, sb);
		naf_tlv_free(mod, msgtlv);
	}


	if (gm->msgflags & GNR_MSG_MSGFLAG_ACKREQUESTED)
		naf_tlv_addnoval(mod, &tlvh, 0x0003);
	if (gm->msgflags & GNR_MSG_MSGFLAG_AUTORESPONSE)
		naf_tlv_addnoval(mod, &tlvh, 0x0004);
	naf_tlv_render(mod, tlvh, sb);
	naf_tlv_free(mod, tlvh);


	gnr_msg_tag_fetch(mod, gm, "gnrmsg.extraoscartlvs", NULL, (void **)&extratlvs);
	if (extratlvs)
		naf_tlv_render(mod, extratlvs, sb);

	return 0;
}

/*
 * 0004/0006 (client->server) Outgoing IM (ICBM)
 *
 * An instant message from the client to the host.
 */
int
toscar_snachandler_0004_0006(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	int ret = HRET_DIGESTED;

	naf_u8_t *msgck = NULL;
	naf_u16_t msgchan;
	naf_u8_t destsnlen;
	char *destsn = NULL, *srcsn = NULL;
	naf_tlv_t *tlvh = NULL;

	struct gnrmsg *gm = NULL;


	if ((naf_conn_tag_fetch(mod, conn->endpoint, "conn.screenname", NULL, (void **)&srcsn) == -1) || !srcsn) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] 0004/0006: unable to find conn.screenname tag\n", conn->cid);
		ret = HRET_ERROR;
		goto out;
	}


	if (!(msgck = naf_sbuf_getraw(mod, &snac->payload, MSGCOOKIELEN))) {
		ret = HRET_ERROR;
		goto out;
	}
	msgchan = naf_sbuf_get16(&snac->payload);
	destsnlen = naf_sbuf_get8(&snac->payload);
	if (!(destsn = naf_sbuf_getstr(mod, &snac->payload, destsnlen))) {
		ret = HRET_ERROR;
		goto out;
	}
	tlvh = naf_tlv_parse(mod, &snac->payload);


	if (!(gm = gnr_msg_new(mod))) {
		ret = HRET_ERROR;
		goto out;
	}

	/* Add various hints to build context for rewriting the message later */
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.snacid", 'I', (void *)snac->id) == -1) {
		ret = HRET_ERROR;
		goto out;
	}
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.oscarmsgcookie", 'V', (void *)msgck) == -1) {
		ret = HRET_ERROR;
		goto out;
	}
	msgck = NULL;

	/* generic routing info */
	gm->srcname = srcsn;
	gm->srcnameservice = OSCARSERVICE;
	gm->destname = destsn;
	gm->destnameservice = OSCARSERVICE;

	/* extract channel-specific data */
	if (msgchan == 0x0001) {
		if (toscar_icbm__parsechan1(mod, conn, gm, &tlvh) == -1) {
			ret = HRET_ERROR;
			goto out;
		}
	} else {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] [%s] ignoring outgoing message to '%s' on unknown channel %u\n", conn->cid, srcsn, destsn, msgchan);
		ret = HRET_FORWARD;
		goto out;
	}

	/*
	 * We assume that the channel-specific handler has removed all the
	 * TLVs it knows how to parse, so now we send the other, unknown TLVs
	 * along as hints.  These probably include silly things like user
	 * icon data, etc, and aren't a big deal.
	 */
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.extraoscartlvs", 'V', (void *)tlvh) != -1)
		tlvh = NULL;


	if (!gnr_node_findbyname(gm->destname, OSCARSERVICE)) {
		/*
		 * If the target user is not obviously here, create them.
		 *
		 * XXX It's unclear that this is the right way to do things.
		 */
		gnr_node_online(mod, gm->destname, OSCARSERVICE, GNR_NODE_FLAG_NONE, GNR_NODE_METRIC_MAX);
	}


	gnr_msg_route(mod, gm);


out:
	if (gm)
		naf_free(mod, gm->msgtext);
	gnr_msg_free(mod, gm);
	naf_tlv_free(mod, tlvh);
	naf_free(mod, destsn);
	naf_free(mod, msgck);
	return ret;
}


/*
 * 0004/0007 (server->client) Incoming IM (ICBM)
 *
 * An instant message from the host to the client.
 *
 */
int
toscar_snachandler_0004_0007(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	int ret = HRET_DIGESTED;

	naf_u8_t *msgck = NULL;
	naf_u16_t msgchan;
	char *destsn = NULL;
	struct touserinfo *srcinfo = NULL;
	naf_tlv_t *tlvh = NULL;

	struct gnrmsg *gm = NULL;


	if ((naf_conn_tag_fetch(mod, conn, "conn.screenname", NULL, (void **)&destsn) == -1) || !destsn) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] 0004/0007: unable to find conn.screenname tag\n", conn->cid);
		ret = HRET_ERROR;
		goto out;
	}


	if (!(msgck = naf_sbuf_getraw(mod, &snac->payload, MSGCOOKIELEN))) {
		ret = HRET_ERROR;
		goto out;
	}
	msgchan = naf_sbuf_get16(&snac->payload);
	if (!(srcinfo = touserinfo_extract(mod, &snac->payload))) {
		ret = HRET_ERROR;
		goto out;
	}
	tlvh = naf_tlv_parse(mod, &snac->payload);


	if (!(gm = gnr_msg_new(mod))) {
		ret = HRET_ERROR;
		goto out;
	}

	/* Add various hints to build context for rewriting the message later */
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.snacid", 'I', (void *)snac->id) == -1) {
		ret = HRET_ERROR;
		goto out;
	}
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.oscarmsgcookie", 'V', (void *)msgck) == -1) {
		ret = HRET_ERROR;
		goto out;
	}
	msgck = NULL;

	/* generic routing info */
	gm->srcname = srcinfo->sn;
	gm->srcnameservice = OSCARSERVICE;
	gm->destname = destsn;
	gm->destnameservice = OSCARSERVICE;

	/* extract channel-specific data */
	if (msgchan == 0x0001) {
		if (toscar_icbm__parsechan1(mod, conn, gm, &tlvh) == -1) {
			ret = HRET_ERROR;
			goto out;
		}
	} else {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] [%s] ignoring incoming message from '%s' on unknown channel %u\n", conn->cid, destsn, srcinfo->sn, msgchan);
		ret = HRET_FORWARD;
		goto out;
	}

	if (gnr_msg_tag_add(mod, gm, "gnrmsg.srcuserinfo", 'V', (void *)srcinfo) == -1) {
		ret = HRET_ERROR;
		goto out;
	}
	srcinfo = NULL;

	/*
	 * Pass along remaining unknown TLVs;
	 */
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.extraoscartlvs", 'V', (void *)tlvh) != -1)
		tlvh = NULL;

	if (!gnr_node_findbyname(gm->srcname, OSCARSERVICE)) {
		/*
		 * If we receive a message from a user through a local user,
		 * we can assume they are external, but accessible.
		 *
		 * XXX It's unclear that this is the right way to do things.
		 */
		gnr_node_online(mod, gm->srcname, OSCARSERVICE, GNR_NODE_FLAG_NONE, GNR_NODE_METRIC_MAX);
	}

	gnr_msg_route(mod, gm);


out:
	if (gm)
		naf_free(mod, gm->msgtext);
	gnr_msg_free(mod, gm);
	naf_tlv_free(mod, tlvh);
	touserinfo_free(mod, srcinfo);
	naf_free(mod, msgck);
	return ret;
	return HRET_FORWARD;
}

int
toscar_icbm_sendoutgoing(struct nafmodule *mod, struct nafconn *conn, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{
	naf_u32_t snacid = 0x42424242;
	naf_u8_t *msgck = NULL;
	naf_u16_t icbmchan;

	naf_sbuf_t sb;


	if (gm->type == GNR_MSG_MSGTYPE_IM)
		icbmchan = 0x0001;
	else if ((gm->type == GNR_MSG_MSGTYPE_GROUPINVITE) ||
			(gm->type == GNR_MSG_MSGTYPE_RENDEZVOUS))
		icbmchan = 0x0002;
	else
		return -1; /* not valid for this context */
	
	if (icbmchan != 0x0001)
		return -1; /* XXX generate the other types... */


	gnr_msg_tag_fetch(mod, gm, "gnrmsg.snacid", NULL, (void **)&snacid);
	gnr_msg_tag_fetch(mod, gm, "gnrmsg.oscarmsgcookie", NULL, (void **)&msgck);


	snacid &= ~0x80000000; /* unset server bit */

	if (toscar_newsnacsb(mod, &sb, 0x0004, 0x0006, 0x0000, snacid) == -1)
		return -1;

	if (msgck)
		naf_sbuf_putraw(&sb, msgck, MSGCOOKIELEN);
	else {
		int i;

		/* make one up */
		for (i = 0; i < MSGCOOKIELEN; i++)
			naf_sbuf_put8(&sb, '0' + ((naf_u8_t) rand() % 10));
	}

	naf_sbuf_put16(&sb, icbmchan);

	naf_sbuf_put8(&sb, strlen(gm->destname));
	naf_sbuf_putstr(&sb, gm->destname);

	if (icbmchan == 0x0001) {
		if (toscar_icbm__renderchan1(mod, gm, &sb) == -1)
			goto errout;
	} else if (icbmchan == 0x0002)
		; /* XXX rendezvous / chat invites / icons / whatever */


	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1)
		goto errout;

	return 0;
errout:
	naf_sbuf_free(mod, &sb);
	return -1;
}

int
toscar_icbm_sendincoming(struct nafmodule *mod, struct nafconn *conn, struct gnrmsg *gm, struct gnrmsg_handler_info *gmhi)
{
	naf_u32_t snacid = 0x42424242;
	naf_u8_t *msgck = NULL;
	struct touserinfo *srcinfo = NULL;
	naf_u16_t icbmchan;

	naf_sbuf_t sb;


	if (gm->type == GNR_MSG_MSGTYPE_IM)
		icbmchan = 0x0001;
	else if ((gm->type == GNR_MSG_MSGTYPE_GROUPINVITE) ||
			(gm->type == GNR_MSG_MSGTYPE_RENDEZVOUS))
		icbmchan = 0x0002;
	else
		return -1; /* not valid for this context */
	
	if (icbmchan != 0x0001)
		return -1; /* XXX generate the other types... */


	gnr_msg_tag_fetch(mod, gm, "gnrmsg.snacid", NULL, (void **)&snacid);
	gnr_msg_tag_fetch(mod, gm, "gnrmsg.oscarmsgcookie", NULL, (void **)&msgck);
	gnr_msg_tag_fetch(mod, gm, "gnrmsg.srcuserinfo", NULL, (void **)&srcinfo);


	snacid |= 0x80000000; /* server bit */

	if (toscar_newsnacsb(mod, &sb, 0x0004, 0x0007, 0x0000, snacid) == -1)
		return -1;

	if (msgck)
		naf_sbuf_putraw(&sb, msgck, MSGCOOKIELEN);
	else {
		int i;

		/* make one up */
		for (i = 0; i < MSGCOOKIELEN; i++)
			naf_sbuf_put8(&sb, '0' + ((naf_u8_t) rand() % 10));
	}

	naf_sbuf_put16(&sb, icbmchan);

	if (srcinfo)
		touserinfo_render(mod, srcinfo, &sb);
	else {
		struct touserinfo *toui;

		if (!(toui = touserinfo_new(mod, gm->srcname)))
			goto errout;

		/* XXX need to add fake some info here? */

		touserinfo_render(mod, toui, &sb);

		touserinfo_free(mod, toui);
	}

	/* don't send this to clients */
	gm->msgflags &= ~GNR_MSG_MSGFLAG_ACKREQUESTED;

	if (icbmchan == 0x0001) {
		if (toscar_icbm__renderchan1(mod, gm, &sb) == -1)
			goto errout;
	} else if (icbmchan == 0x0002)
		; /* XXX rendezvous / chat invites / icons / whatever */


	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1)
		goto errout;

	return 0;
errout:
	naf_sbuf_free(mod, &sb);
	return -1;
}


