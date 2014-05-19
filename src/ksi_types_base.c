#include <string.h>

#include "ksi_internal.h"

struct KSI_OctetString_st {
	KSI_CTX *ctx;
	unsigned char *data;
	int data_len;
};

struct KSI_Integer_st {
	KSI_CTX *ctx;
	KSI_uint64_t value;
};

/**
 * KSI_OctetString
 */
void KSI_OctetString_free(KSI_OctetString *t) {
	if(t != NULL) {
		KSI_free(t->data);
		KSI_free(t);
	}
}

int KSI_OctetString_new(KSI_CTX *ctx, const unsigned char *data, int data_len, KSI_OctetString **t) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_OctetString *tmp = NULL;
	tmp = KSI_new(KSI_OctetString);

	if(tmp == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->data = NULL;
	tmp->data_len = data_len;

	tmp->data = KSI_calloc(data_len, 1);
	if (tmp->data == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}

	memcpy(tmp->data, data, data_len);

	*t = tmp;
	tmp = NULL;
	res = KSI_OK;
cleanup:
	KSI_OctetString_free(tmp);
	return res;
}

int KSI_OctetString_extract(const KSI_OctetString *t, const unsigned char **data, int *data_len) {
	int res = KSI_UNKNOWN_ERROR;
	if(t == NULL || data == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	*data = t->data;
	*data_len = t->data_len;

	res = KSI_OK;
cleanup:
	 return res;
}

int KSI_OctetString_equals(const KSI_OctetString *left, const KSI_OctetString *right) {
	return left != NULL && right != NULL && left->data_len == right->data_len && !memcmp(left->data, right->data, left->data_len);
}

int KSI_OctetString_fromTlv(KSI_TLV *tlv, KSI_OctetString **oct) {
	KSI_ERR err;
	KSI_CTX *ctx = NULL;
	int res;
	const unsigned char *raw = NULL;
	int raw_len = 0;
	KSI_OctetString *tmp = NULL;

	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, oct != NULL) goto cleanup;

	ctx = KSI_TLV_getCtx(tlv);
	KSI_BEGIN(ctx, &err);

	res = KSI_TLV_cast(tlv, KSI_TLV_PAYLOAD_RAW);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TLV_getRawValue(tlv, &raw, &raw_len);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_OctetString_new(ctx, raw, raw_len, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	*oct = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_nofree(ctx);
	KSI_nofree(raw);
	KSI_OctetString_free(tmp);

	return KSI_RETURN(&err);
}

int KSI_OctetString_toTlv(KSI_OctetString *oct, int tag, int isNonCritical, int isForward, KSI_TLV **tlv) {
	KSI_ERR err;
	int res;
	KSI_TLV *tmp = NULL;

	KSI_PRE(&err, oct != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_BEGIN(oct->ctx, &err);

	res = KSI_TLV_new(oct->ctx, KSI_TLV_PAYLOAD_RAW, tag, isNonCritical, isForward, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TLV_setRawValue(tmp, oct->data, oct->data_len);
	KSI_CATCH(&err, res) goto cleanup;

	*tlv = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_nofree(raw);
	KSI_TLV_free(tmp);

	return KSI_RETURN(&err);
}


/**
 * Utf8String
 */

void KSI_Utf8String_free(KSI_Utf8String *t) {
	if (t != NULL) {
		KSI_free(t);
	}
}

int KSI_Utf8String_new(KSI_CTX *ctx, const char *str, KSI_Utf8String **t) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_Utf8String *tmp = NULL;

	tmp = KSI_calloc(strlen(str) + 1, 1);
	if (tmp == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}

	memcpy(tmp, str, strlen(str));

	*t = tmp;
	tmp = NULL;

	res = KSI_OK;
cleanup:

	KSI_Utf8String_free(tmp);
	return res;
}

void KSI_Integer_free(KSI_Integer *kint) {
	if (kint != NULL) {
		KSI_free(kint);
	}
}

KSI_Integer *KSI_Integer_clone(const KSI_Integer *val) {
	KSI_Integer *clone = NULL;
	KSI_Integer *tmp = NULL;

	int res;

	if (val == NULL) goto cleanup;

	res = KSI_Integer_new(val->ctx, val->value, &tmp);
	if (res != KSI_OK) goto cleanup;

	clone = tmp;
	tmp = NULL;

cleanup:

	KSI_Integer_free(tmp);

	return clone;
}

int KSI_Integer_getSize(const KSI_Integer *kint, int *size) {
	KSI_ERR err;
	KSI_PRE(&err, kint != NULL) goto cleanup;
	KSI_BEGIN(kint->ctx, &err);

	*size = KSI_UINT64_MINSIZE(kint->value);

	KSI_SUCCESS(&err);

cleanup:

	return KSI_RETURN(&err);
}

KSI_uint64_t KSI_Integer_getUInt64(const KSI_Integer *kint) {
	return kint != NULL ? kint->value : 0;
}

int KSI_Integer_equals(const KSI_Integer *a, const KSI_Integer *b) {
	return a != NULL && b != NULL && (a == b || a->value == b->value);
}

int KSI_Integer_equalsUInt(const KSI_Integer *o, KSI_uint64_t i) {
	return o != NULL && o->value == i;
}

int KSI_Integer_new(KSI_CTX *ctx, KSI_uint64_t value, KSI_Integer **kint) {
	KSI_ERR err;
	KSI_Integer *tmp = NULL;

	KSI_PRE(&err, ctx != NULL);
	KSI_BEGIN(ctx, &err);

	tmp = KSI_new(KSI_Integer);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->value = value;

	*kint = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_Integer_free(tmp);

	return KSI_RETURN(&err);
}

int KSI_Integer_fromTlv(KSI_TLV *tlv, KSI_OctetString **integer) {
	KSI_ERR err;
	KSI_CTX *ctx = NULL;
	int res;
	KSI_Integer *tmp = NULL;

	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, integer != NULL) goto cleanup;

	ctx = KSI_TLV_getCtx(tlv);
	KSI_BEGIN(ctx, &err);

	res = KSI_TLV_cast(tlv, KSI_TLV_PAYLOAD_INT);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TLV_getInteger(tlv, &tmp);

	*integer = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_nofree(ctx);
	KSI_Integer_free(tmp);

	return KSI_RETURN(&err);
}

int KSI_Integer_toTlv(KSI_Integer *integer, int tag, int isNonCritical, int isForward, KSI_TLV **tlv) {
	KSI_ERR err;
	int res;
	KSI_TLV *tmp = NULL;

	KSI_PRE(&err, integer != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_BEGIN(integer->ctx, &err);

	res = KSI_TLV_new(integer->ctx, KSI_TLV_PAYLOAD_INT, tag, isNonCritical, isForward, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TLV_setUintValue(tmp, integer->value);
	KSI_CATCH(&err, res) goto cleanup;

	*tlv = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_nofree(raw);
	KSI_TLV_free(tmp);

	return KSI_RETURN(&err);
}