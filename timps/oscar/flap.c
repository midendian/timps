
#include <naf/nafmodule.h>
#include <naf/naftlv.h>

#include "oscar_internal.h"
#include "flap.h"
#include "snac.h"
#include "ckcache.h"

#define FLAPHDRLEN 6
#define MAXSNACLEN 8192
#define MAXFLAPLEN (FLAPHDRLEN + MAXSNACLEN) /* size of all flap buffers */

#define FLAP_MAGIC '*'
#define FLAPHDR_MAGIC(x) naf_byte_get8(x)
#define FLAPHDR_CHAN(x) naf_byte_get8((x) + 1)
#define FLAPHDR_SEQNUM(x) naf_byte_get16((x) + 2)
#define FLAPHDR_LEN(x) naf_byte_get16((x) + 4)

static int
toscar_flap__reqflap(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *oldbuf)
{
	naf_u8_t *buf;

	if (!oldbuf)
		buf = naf_malloc_type(mod, NAF_MEM_TYPE_NETBUF, MAXFLAPLEN);

	if (naf_conn_reqread(conn, oldbuf ? oldbuf : buf, FLAPHDRLEN, 0) == -1) {
		if (buf)
			naf_free(mod, buf);
		return -1;
	}

	return 0;
}

static int
toscar_flap__sendraw(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen)
{

	if (!conn)
		return -1;

	naf_byte_put8(buf, FLAP_MAGIC);
	naf_byte_put16(buf + 2, conn->nextseqnum);
	naf_byte_put16(buf + 4, buflen - FLAPHDRLEN);

	if (naf_conn_reqwrite(conn, buf, buflen) == -1)
		return -1; /* buffer not consumed */

	conn->nextseqnum++;

	return 0; /* buffer consumed */
}

int
toscar_flap_sendsbuf_consume(struct nafmodule *mod, struct nafconn *conn, naf_sbuf_t *sb)
{

	/*
	 * This uses sbuf_pos instead of sbuf_len, which obviously assumes
	 * that the caller has positioned itself at the end of the data it
	 * wants to send.  This assumption is usually valid, and making it
	 * allows us to use buffers which are bigger than the data (for
	 * example when using dynamic sbuf's that are arbitrarily sized).
	 */
	if (toscar_flap__sendraw(mod, conn, sb->sbuf_buf, sb->sbuf_pos) == -1)
		return -1; /* buffer not consumed */

	return 0; /* buffer consumed */
}

int
toscar_flap_puthdr(naf_sbuf_t *sb, naf_u8_t chan)
{

	if (!sb)
		return -1;

	naf_sbuf_put8(sb, FLAP_MAGIC);
	naf_sbuf_put8(sb, chan);
	naf_sbuf_put16(sb, 0x0000); /* seqnum filled in later */
	naf_sbuf_put16(sb, 4); /* length filled in later */

	return 0;
}

static int
toscar_flap__sendflapversion(struct nafmodule *mod, struct nafconn *conn)
{
	naf_sbuf_t sb;

	if (naf_sbuf_init(mod, &sb, NULL, FLAPHDRLEN + 4) == -1)
		return -1;

	toscar_flap_puthdr(&sb, 0x01); /* channel 1 */
	naf_sbuf_put32(&sb, 0x00000001); /* version 1 */

	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1) {
		naf_sbuf_free(mod, &sb);
		return -1;
	}

	return 0; /* sbuf consumed */
}

static int
toscar_flap_sendcookie(struct nafmodule *mod, struct nafconn *conn, naf_tlv_t *tlvh)
{
	naf_sbuf_t sb;

	if (naf_sbuf_init(mod, &sb, NULL, 0) == -1)
		return -1;

	toscar_flap_puthdr(&sb, 0x01); /* channel 1 */
	naf_sbuf_put32(&sb, 0x00000001); /* version 1 */
	naf_tlv_render(mod, tlvh, &sb);

	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1) {
		naf_sbuf_free(mod, &sb);
		return -1;
	}

	return 0; /* sbuf consumed */
}

int
toscar_flap_sendconnclose(struct nafmodule *mod, struct nafconn *conn, naf_u16_t reason, const char *reasonurl)
{
	naf_sbuf_t sb;

	if (naf_sbuf_init(mod, &sb, NULL, 0) == -1)
		return -1;

	toscar_flap_puthdr(&sb, 0x04); /* channel 4 */
	if (reason || reasonurl) {
		naf_tlv_t *tlvh = NULL;

		if (reason)
			naf_tlv_addu16(mod, &tlvh, 0x0009, reason);
		if (reasonurl)
			naf_tlv_addstring(mod, &tlvh, 0x000b, reasonurl);

		naf_tlv_render(mod, tlvh, &sb);

		naf_tlv_free(mod, tlvh);
	}

	if (toscar_flap_sendsbuf_consume(mod, conn, &sb) == -1) {
		naf_sbuf_free(mod, &sb);
		return -1;
	}

	return 0; /* sbuf consumed */
}

int
toscar_flap_prepareconn(struct nafmodule *mod, struct nafconn *conn)
{

	if (conn->type & NAF_CONN_TYPE_CLIENT) { /* act like a server */
		if (toscar_flap__sendflapversion(mod, conn) == -1)
			return -1;
	}

	return toscar_flap__reqflap(mod, conn, NULL);
}

static int
toscar_flap_handlechan1__conncomplete(struct nafmodule *mod, struct nafconn *conn)
{
	naf_tlv_t *wtlvs = NULL;

	if (naf_conn_tag_remove(mod, conn, "conn.logintlvs", NULL, (void **)&wtlvs) != -1) {
		naf_u32_t snacid = 0;

		naf_conn_tag_remove(mod, conn, "conn.loginsnacid", NULL, (void **)&snacid);

		toscar_flap__sendflapversion(mod, conn);
		toscar_auth_sendauthinforequest(mod, conn, snacid, wtlvs);

	} else if (naf_conn_tag_remove(mod, conn, "conn.cookietlvs", NULL, (void **)&wtlvs) != -1) {

		/* this sort of includes the flap version */
		toscar_flap_sendcookie(mod, conn, wtlvs);

	}

	naf_tlv_free(mod, wtlvs);

	return HRET_DIGESTED;
}

static int
toscar_flap_handlechan1__xorlogin(struct nafmodule *mod, struct nafconn *conn, naf_tlv_t **tlvh)
{
	/*
	 * Once upon a time, this is how login started.  libfaim
	 * refers to this as "XOR login", because of the method's
	 * choice of 'encryption'.  No modern client attempts this
	 * anymore.
	 *
	 * We don't do this, 'cause it sucks.  It sucks so much that
	 * we don't want you doing it either.
	 */
	return HRET_ERROR;
}

static int
toscar_flap_handlechan1__newconn(struct nafmodule *mod, struct nafconn *conn, naf_tlv_t **tlvh)
{
	naf_tlv_t *cktlv;
	char *ip = NULL, *sn = NULL;
	naf_u16_t servtype = TOSCAR_SERVTYPE_UNKNOWN;
	int ret = HRET_DIGESTED;

	if (!(cktlv = naf_tlv_get(mod, *tlvh, 0x0006))) {
		/* no cookie = wtf are we doing here? */
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] cookie FLAP missing cookie\n", conn->cid);
		return HRET_ERROR;
	}

	if (toscar_ckcache_rem(mod, cktlv->tlv_value, cktlv->tlv_length,
				&ip, &sn, &servtype) == -1) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] received unknown cookie\n", conn->cid);
		ret = HRET_ERROR;
		goto out;
	}

	if (timps_oscar__debug > 1)
		dvprintf(mod, "[cid %lu] matched cookie to sn '%s', ip '%s', servtype %d\n", conn->cid, sn, ip, servtype);

	if (naf_conn_startconnect(mod, conn, ip, TIMPS_OSCAR_DEFAULTPORT) == -1) {
		ret = HRET_ERROR;
		goto out;
	}

	conn->servtype = servtype;
	conn->endpoint->servtype = servtype;

	if (sn) {
		/* XXX store full userinfo here */
		if (naf_conn_tag_add(mod, conn, "conn.screenname", 'S', sn) == -1) {
			ret = HRET_ERROR;
			goto out;
		}
		sn = NULL;
	}

	/* need to resend these when connection completes */
	if (naf_conn_tag_add(mod, conn->endpoint, "conn.cookietlvs", 'V', (void *)*tlvh) == -1) {
		ret = HRET_ERROR;
		goto out;
	}
	*tlvh = NULL; /* will get freed with tag */

out:
	naf_free(mod, sn);
	naf_free(mod, ip);
	return ret;
}

static int
toscar_flap_handlechan1(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen)
{
	naf_sbuf_t sb;
	naf_u32_t flapver;
	naf_tlv_t *tlvh;
	int ret = HRET_FORWARD;

	naf_sbuf_init(mod, &sb, buf, buflen);

	naf_sbuf_advance(&sb, FLAPHDRLEN);

	flapver = naf_sbuf_get32(&sb);
	if (flapver != 0x00000001) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] %s sent invalid FLAP version\n", conn->cid, !(conn->type & NAF_CONN_TYPE_CLIENT) ? "server" : "client");
		return HRET_ERROR;
	}

	tlvh = naf_tlv_parse(mod, &sb);

	if (FLAPHDR_LEN(buf) == 4) { /* version only */
		if (conn->type & NAF_CONN_TYPE_SERVER) 
			ret = toscar_flap_handlechan1__conncomplete(mod, conn);
		else
			ret = HRET_DIGESTED; /* wait to see what else they have for us */
	} else if ((conn->type & NAF_CONN_TYPE_CLIENT) &&
			naf_tlv_get(mod, tlvh, 0x0001 /* screen name */))
		ret = toscar_flap_handlechan1__xorlogin(mod, conn, &tlvh);
	else if ((conn->type & NAF_CONN_TYPE_CLIENT) &&
			naf_tlv_get(mod, tlvh, 0x0006 /* cookie */))
		ret = toscar_flap_handlechan1__newconn(mod, conn, &tlvh);
	else {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] unable to determine purpose of channel 1 packet\n", conn->cid);
		ret = HRET_ERROR;
	}

	naf_tlv_free(mod, tlvh);
	return ret;
}

int
toscar_flap_handlechan4(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen)
{
	return HRET_FORWARD;
}

int
toscar_flap_handlechan5(struct nafmodule *mod, struct nafconn *conn, naf_u8_t *buf, naf_u16_t buflen)
{
	return HRET_FORWARD;
}

int
toscar_flap_handleread(struct nafmodule *mod, struct nafconn *conn)
{
	naf_u8_t *buf;
	int buflen;
	int hret = HRET_FORWARD;

	if (naf_conn_takeread(conn, &buf, &buflen) == -1)
		return -1;

	if (FLAPHDR_MAGIC(buf) != FLAP_MAGIC) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] FLAP packet did not start with correct magic\n", conn->cid);
		goto errout;
	}

	/* XXX check seqnums */

	if (! ( (FLAPHDR_CHAN(buf) == 0x01) ||
				(FLAPHDR_CHAN(buf) == 0x02) ||
				(FLAPHDR_CHAN(buf) == 0x04) ||
				(FLAPHDR_CHAN(buf) == 0x05) ) ) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] FLAP packet on invalid channel\n", conn->cid);
		goto errout;
	}

	if (FLAPHDR_LEN(buf) > MAXSNACLEN) {
		if (timps_oscar__debug > 0)
			dvprintf(mod, "[cid %lu] FLAP packet contained invalid length\n", conn->cid);
		goto errout;
	}

	if (buflen != (FLAPHDR_LEN(buf) + FLAPHDRLEN)) {
		if (naf_conn_reqread(conn, buf, buflen + FLAPHDR_LEN(buf), buflen) == -1) {
			if (timps_oscar__debug > 0)
				dvprintf(mod, "[cid %lu] naf refused further read request\n", conn->cid);
			goto errout;
		}
		return 0; /* continue later */
	}

	if (timps_oscar__debug > 1)
		dvprintf(mod, "[cid %lu] received full FLAP packet on channel 0x%02x, seqnum 0x%04lx, length 0x%04lx (%d bytes)\n", conn->cid, FLAPHDR_CHAN(buf), FLAPHDR_SEQNUM(buf), FLAPHDR_LEN(buf), FLAPHDR_LEN(buf));


	if (FLAPHDR_CHAN(buf) == 0x01)
		hret = toscar_flap_handlechan1(mod, conn, buf, buflen);
	else if (FLAPHDR_CHAN(buf) == 0x02)
		hret = toscar_flap_handlesnac(mod, conn, buf + FLAPHDRLEN, buflen - FLAPHDRLEN);
	else if (FLAPHDR_CHAN(buf) == 0x04)
		hret = toscar_flap_handlechan4(mod, conn, buf, buflen);
	else if (FLAPHDR_CHAN(buf) == 0x05)
		hret = toscar_flap_handlechan5(mod, conn, buf, buflen);


	if (hret == HRET_ERROR)
		goto errout;
	else if ((hret == HRET_FORWARD) && conn->endpoint) {
		if (toscar_flap__sendraw(mod, conn->endpoint, buf, buflen) == -1)
			goto errout;
		buf = NULL; /* consumed by sendraw */
	}
	/* 
	 * HRET_DIGESTED means the packet was processed but should not be
	 * forwarded -- it was not consumed, in the memory management sense, so
	 * we can reuse it.
	 */

	/* go again... */
	if (toscar_flap__reqflap(mod, conn, buf) == -1)
		goto errout;

	return 0;
errout:
	naf_free(mod, buf);
	return -1;
}

int
toscar_flap_handlewrite(struct nafmodule *mod, struct nafconn *conn)
{
	naf_u8_t *buf;
	int buflen;

	if (naf_conn_takewrite(conn, &buf, &buflen) == -1)
		return -1;

	naf_free(mod, buf);

	return 0;
}

