
#include <naf/nafmodule.h>
#include <naf/nafbufutils.h>
#include <naf/naftlv.h>

#include <string.h> /* memcpy */


naf_tlv_t *naf_tlv_new(struct nafmodule *mod, naf_u16_t type, naf_u16_t length, naf_u8_t *value)
{
	naf_tlv_t *tlv;

	if (!(tlv = (naf_tlv_t *)naf_malloc(mod, sizeof(naf_tlv_t))))
		return NULL;

	tlv->tlv_type = type;
	tlv->tlv_length = length;
	tlv->tlv_value = value;
	tlv->tlv_next = NULL;

	return tlv;
}

static void naf_tlv__free(struct nafmodule *mod, naf_tlv_t *tlv)
{

	naf_free(mod, tlv->tlv_value);
	naf_free(mod, tlv);

	return;
}

void naf_tlv_free(struct nafmodule *mod, naf_tlv_t *tlvhead)
{

	while (tlvhead) {
		naf_tlv_t *tmp;

		tmp = tlvhead->tlv_next;
		naf_tlv__free(mod, tlvhead);
		tlvhead = tmp;
	}

	return;
}

naf_tlv_t *naf_tlv_parse(struct nafmodule *mod, naf_sbuf_t *sbuf)
{
	naf_tlv_t *tlvhead = NULL, *tlvtail = NULL;

	while (naf_sbuf_bytesremaining(sbuf) > 0) {
		naf_tlv_t *tlv;
		naf_u16_t t, l;
		naf_u8_t *v = NULL;

		t = naf_sbuf_get16(sbuf);
		l = naf_sbuf_get16(sbuf);
		if (l) {
			if (!(v = naf_sbuf_getraw(mod, sbuf, l)))
				break;
		}

		if (!(tlv = naf_tlv_new(mod, t, l, v))) {
			naf_free(mod, v);
			break;
		}

		if (tlvtail) {
			tlvtail->tlv_next = tlv;
			tlvtail = tlv;
		} else
			tlvhead = tlvtail = tlv;
	}

	return tlvhead;
}

int naf_tlv_render(struct nafmodule *mod, naf_tlv_t *tlv, naf_sbuf_t *destsbuf)
{
	int n;

	for (n = 0; tlv && (naf_sbuf_bytesremaining(destsbuf) > 0); tlv = tlv->tlv_next) {

		n += naf_sbuf_put16(destsbuf, tlv->tlv_type);
		n += naf_sbuf_put16(destsbuf, tlv->tlv_length);
		if (tlv->tlv_value)
			n += naf_sbuf_putraw(destsbuf, tlv->tlv_value, tlv->tlv_length);
	}

	return n;
}

int naf_tlv_getrenderedsize(struct nafmodule *mod, naf_tlv_t *tlv)
{
	int n;

	for (n = 0; tlv; tlv = tlv->tlv_next)
		n += 2 + 2 + tlv->tlv_length;

	return n;
}

int naf_tlv_gettotallength(struct nafmodule *mod, naf_tlv_t *tlv)
{
	int n;

	for (n = 0; tlv; tlv = tlv->tlv_next)
		n++;

	return n;
}

naf_tlv_t *naf_tlv_remove(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type)
{
	naf_tlv_t *cur, **prev;

	for (prev = head; (cur = *prev); ) {
		if (cur->tlv_type == type) {
			*prev = cur->tlv_next;
			return cur;
		}
		prev = &cur->tlv_next;
	}

	return NULL;
}

int naf_tlv_addraw(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u16_t length, const naf_u8_t *value)
{
	naf_tlv_t *tlv;
	naf_u8_t *vcopy = NULL;

	if (length) {
		if (!(vcopy = (naf_u8_t *)naf_malloc(mod, length)))
			return -1;
		memcpy(vcopy, value, length);
	}

	if (!(tlv = naf_tlv_new(mod, type, length, vcopy))) {
		naf_free(mod, vcopy);
		return -1;
	}


	if (!*head)
		*head = tlv;
	else {
		naf_tlv_t *tcur;

		for (tcur = *head; tcur->tlv_next; tcur = tcur->tlv_next)
			;
		tcur->tlv_next = tlv;
	}


	return 0;
}

int naf_tlv_addtlvraw(struct nafmodule *mod, naf_tlv_t **desthead, naf_tlv_t *srctlv)
{

	if (!srctlv)
		return -1;

	return naf_tlv_addraw(mod, desthead, srctlv->tlv_type, srctlv->tlv_length, srctlv->tlv_value); 
}

int naf_tlv_addstring(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, const char *str)
{
	return naf_tlv_addraw(mod, head, type, strlen(str) + 1, (const naf_u8_t *)str);
}

int naf_tlv_addu8(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u8_t value)
{
	naf_u8_t v8[1];

	naf_byte_put8(v8, value);

	return naf_tlv_addraw(mod, head, type, 1, v8);
}

int naf_tlv_addu16(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u16_t value)
{
	naf_u8_t v16[2];

	naf_byte_put16(v16, value);

	return naf_tlv_addraw(mod, head, type, 2, v16);
}

int naf_tlv_addu32(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_u32_t value)
{
	naf_u8_t v32[4];

	naf_byte_put32(v32, value);

	return naf_tlv_addraw(mod, head, type, 4, v32);
}

int naf_tlv_addnoval(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type)
{
	return naf_tlv_addraw(mod, head, type, 0, NULL);
}

int naf_tlv_addtlv(struct nafmodule *mod, naf_tlv_t **head, naf_u16_t type, naf_tlv_t *tlv)
{
	naf_u8_t *buf;
	int buflen;
	naf_sbuf_t sb;

	buflen = naf_tlv_getrenderedsize(mod, tlv);
	if (!(buf = (naf_u8_t *)naf_malloc(mod, buflen)))
		return -1;
	naf_sbuf_init(mod, &sb, buf, buflen);

	naf_tlv_render(mod, tlv, &sb);

	naf_tlv_addraw(mod, head, type, buflen, buf);

	naf_sbuf_free(mod, &sb);

	return 0;
}

naf_tlv_t *naf_tlv_get(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type)
{

	while (head) {
		if (head->tlv_type == type)
			return head;
		head = head->tlv_next;
	}

	return NULL;
}

char *naf_tlv_getasstring(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type)
{
	naf_tlv_t *tlv;
	char *str;

	if (!(tlv = naf_tlv_get(mod, head, type)))
		return NULL;

	if (!(str = (char *)naf_malloc(mod, tlv->tlv_length + 1)))
		return NULL;
	memcpy(str, tlv->tlv_value, tlv->tlv_length);
	str[tlv->tlv_length] = '\0';

	return str;
}

naf_u8_t naf_tlv_getasu8(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type)
{
	naf_tlv_t *tlv;

	if (!(tlv = naf_tlv_get(mod, head, type)))
		return 0;

	if (tlv->tlv_length < 1)
		return 0;

	return naf_byte_get8(tlv->tlv_value);
}

naf_u16_t naf_tlv_getasu16(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type)
{
	naf_tlv_t *tlv;

	if (!(tlv = naf_tlv_get(mod, head, type)))
		return 0;

	if (tlv->tlv_length < 2)
		return 0;

	return naf_byte_get16(tlv->tlv_value);
}

naf_u32_t naf_tlv_getasu32(struct nafmodule *mod, naf_tlv_t *head, naf_u16_t type)
{
	naf_tlv_t *tlv;

	if (!(tlv = naf_tlv_get(mod, head, type)))
		return 0;

	if (tlv->tlv_length < 4)
		return 0;

	return naf_byte_get32(tlv->tlv_value);
}

