
#include <string.h>

#include <naf/nafmodule.h>
#include <naf/nafconn.h>
#include <naf/naftlv.h>
#include <naf/nafbufutils.h>
#include <gnr/gnrmsg.h>

#include "oscar_internal.h"
#include "snac.h"
#include "im.h"

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

/*
 * 0004/0006 (client->server) Outgoing IM (ICBM)
 *
 * An instant message from the client to the host.
 */
int
toscar_snachandler_0004_0006(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	int ret = HRET_FORWARD;

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


	if (!(msgck = naf_sbuf_getraw(mod, &snac->payload, 8))) {
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
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.oscarmsgcookie", 'V', (void **)msgck) == -1) {
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

struct touserinfo {
	char *sn;
	naf_u16_t evillevel;
	naf_tlv_t *tlvh;
};

static struct touserinfo *
touserinfo__alloc(struct nafmodule *mod)
{
	struct touserinfo *toui;

	if (!(toui = naf_malloc(mod, sizeof(struct touserinfo))))
		return NULL;
	memset(toui, 0, sizeof(struct touserinfo));

	return toui;
}

static void
touserinfo_free(struct nafmodule *mod, struct touserinfo *toui)
{

	if (!mod || !toui)
		return;

	naf_tlv_free(mod, toui->tlvh);
	naf_free(mod, toui->sn);
	naf_free(mod, toui);

	return;
}

static struct touserinfo *
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

/*
 * 0004/0007 (server->client) Incoming IM (ICBM)
 *
 * An instant message from the host to the client.
 *
 */
int
toscar_snachandler_0004_0007(struct nafmodule *mod, struct nafconn *conn, struct toscar_snac *snac)
{
	int ret = HRET_FORWARD;

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


	if (!(msgck = naf_sbuf_getraw(mod, &snac->payload, 8))) {
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
	if (gnr_msg_tag_add(mod, gm, "gnrmsg.oscarmsgcookie", 'V', (void **)msgck) == -1) {
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


