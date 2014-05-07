#include <string.h>

#include "ksi_internal.h"
#include "ksi_tlv_easy.h"

typedef struct aggrChainRec_st AggrChainRec;
typedef struct headerRec_st HeaderRec;
typedef struct calAuthRec_st CalAuthRec;
typedef struct aggrAuthRec_st AggrAuthRec;
typedef struct pubDataRec_st PubDataRec;
typedef struct sigDataRec_st SigDataRec;
typedef struct chainIndex_st ChainIndex;

KSI_IMPORT_TLV_TEMPLATE(KSI_CalendarHashChain);
KSI_IMPORT_TLV_TEMPLATE(KSI_HashChainLink)

KSI_DEFINE_LIST(AggrChainRec);

struct chainIndex_st {
	KSI_Integer *index;
};

struct calAuthRec_st {
	KSI_CTX *ctx;

	PubDataRec *pubData;
	char *sigAlgo;
	SigDataRec *sigData;
};

struct aggrAuthRec_st {
	KSI_CTX *ctx;
	KSI_Integer *aggregationTime;
	KSI_LIST(KSI_Integer) *chainIndexesList;
	KSI_DataHash *intputHash;

	char *sigAlgo;

	SigDataRec *sigData;
};

struct pubDataRec_st {
	KSI_CTX *ctx;
	unsigned char *raw;
	int raw_len;
	KSI_Integer *pubTime;
	KSI_DataHash *pubHash;
};

struct sigDataRec_st {
	KSI_CTX *ctx;

	unsigned char *sigValue;
	int sigValue_len;

	unsigned char *cert;
	int cert_len;

	unsigned char *certId;
	int certId_len;

	char *certRepUri;
};

struct aggrChainRec_st {
	KSI_CTX *ctx;
	KSI_Integer *aggregationTime;
	KSI_LIST(KSI_Integer) *chainIndex;
	unsigned char *inputData;
	int inputData_len;
	KSI_DataHash *inputHash;
	int aggrHashId;
	KSI_LIST(KSI_HashChainLink) *chain;
};

struct calChainRec_st {
	KSI_Integer *publicationTime;
	KSI_Integer *aggregationTime;
	KSI_DataHash *inputHash;
	KSI_LIST(KSI_HashChainLink) *chain;
};

struct headerRec_st {
	KSI_CTX *ctx;
	KSI_Integer *instanceId;
	KSI_Integer *messageId;
	unsigned char *clientId;
	int clientId_length;
};

/**
 * KSI Signature object
 */
struct KSI_Signature_st {
	KSI_CTX *ctx;

	/* Base TLV - when serialized, this value will be used. */
	KSI_TLV *baseTlv;

	KSI_CalendarHashChain *calendarChain;

	KSI_LIST(AggrChainRec) *aggregationChainList;

	CalAuthRec *calAuth;
	AggrAuthRec *aggrAuth;

};

static void PubDataRec_free (PubDataRec *pdc) {
	if (pdc != NULL) {
		KSI_free(pdc->raw);
		KSI_Integer_free(pdc->pubTime);
		KSI_DataHash_free(pdc->pubHash);
		KSI_free(pdc);
	}
}

static void SigDataRec_free(SigDataRec *sdc) {
	if (sdc != NULL) {
		KSI_free(sdc->sigValue);
		KSI_free(sdc->cert);
		KSI_free(sdc->certId);
		KSI_free(sdc->certRepUri);
		KSI_free(sdc);
	}
}

static void AggrAuthRec_free(AggrAuthRec *aar) {
	if (aar != NULL) {
		KSI_Integer_free(aar->aggregationTime);
		KSI_IntegerList_free(aar->chainIndexesList);
		KSI_DataHash_free(aar->intputHash);
		KSI_free(aar->sigAlgo);
		SigDataRec_free(aar->sigData);
		KSI_free(aar);
	}
}

static void CalAuthRec_free(CalAuthRec *calAuth) {
	if (calAuth != NULL) {
		PubDataRec_free(calAuth->pubData);
		KSI_free(calAuth->sigAlgo);
		SigDataRec_free(calAuth->sigData);

		KSI_free(calAuth);
	}
}

static void HeaderRec_free(HeaderRec *hdr) {
	if (hdr != NULL) {
		KSI_Integer_free(hdr->instanceId);
		KSI_Integer_free(hdr->messageId);
		KSI_free(hdr->clientId);
		KSI_free(hdr);
	}
}

static void AggrChainRec_free(AggrChainRec *aggr) {
	if (aggr != NULL) {
		KSI_Integer_free(aggr->aggregationTime);
		KSI_IntegerList_free(aggr->chainIndex);
		KSI_free(aggr->inputData);
		KSI_DataHash_free(aggr->inputHash);
		KSI_HashChainLinkList_free(aggr->chain);
		KSI_free(aggr);
	}
}

KSI_IMPLEMENT_LIST(AggrChainRec, AggrChainRec_free);

static int KSI_Signature_new(KSI_CTX *ctx, KSI_Signature **sig) {
	KSI_ERR err;
	int res;
	KSI_Signature *tmp = NULL;
	KSI_LIST(AggrChainRec) *list = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	tmp = KSI_new(KSI_Signature);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->calendarChain = NULL;
	tmp->baseTlv = NULL;

	res = AggrChainRecList_new(ctx, &list);
	KSI_CATCH(&err, res) goto cleanup;

	tmp->aggregationChainList = list;

	*sig = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_Signature_free(tmp);

	return KSI_RETURN(&err);

}

static int CalAuthRec_validate(CalAuthRec *calAuth) {
	KSI_ERR err;
	int res;
	KSI_PKICertificate *cert = NULL;
	const KSI_PKICertificate *certp = NULL;

	KSI_PRE(&err, calAuth != NULL) goto cleanup;
	KSI_BEGIN(calAuth->ctx, &err);

	if (calAuth->sigData->certId == NULL) {
		res = KSI_PKICertificate_new(calAuth->ctx, calAuth->sigData->cert, calAuth->sigData->cert_len, &cert);
		KSI_CATCH(&err, res) goto cleanup;
		certp = cert;
	} else {
		res = KSI_PKICertificate_find(calAuth->ctx, calAuth->sigData->certId, calAuth->sigData->certId_len, &certp);
		KSI_CATCH(&err, res) goto cleanup;
	}

	res = KSI_PKITruststore_validateSignature(calAuth->pubData->raw, calAuth->pubData->raw_len, calAuth->sigAlgo, calAuth->sigData->sigValue, calAuth->sigData->sigValue_len, certp);
	KSI_CATCH(&err, res) goto cleanup;


	KSI_SUCCESS(&err);

cleanup:

	KSI_PKICertificate_free(cert);
	return KSI_RETURN(&err);
}

static int HeaderRec_new(KSI_CTX *ctx, HeaderRec **hdr) {
	KSI_ERR err;
	HeaderRec *tmp = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	tmp = KSI_new(HeaderRec);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->clientId = NULL;
	tmp->clientId_length = 0;
	tmp->instanceId = NULL;
	tmp->messageId = NULL;

	*hdr = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	HeaderRec_free(tmp);

	return KSI_RETURN(&err);

}

static int AggChainRec_new(KSI_CTX *ctx, AggrChainRec **out) {
	KSI_ERR err;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	AggrChainRec *tmp = NULL;
	tmp = KSI_new(AggrChainRec);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->aggrHashId = 0;
	tmp->aggregationTime = NULL;
	tmp->chain = NULL;
	tmp->chainIndex = NULL;
	tmp->inputData = NULL;
	tmp->inputData_len = 0;
	tmp->inputHash = NULL;

	*out = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	AggrChainRec_free(tmp);

	return KSI_RETURN(&err);
}

static int PubDataRed_new(KSI_CTX *ctx, PubDataRec **out) {
	KSI_ERR err;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	PubDataRec *tmp = NULL;
	tmp = KSI_new(PubDataRec);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->raw = NULL;
	tmp->pubHash = NULL;
	tmp->pubTime = NULL;

	*out = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	PubDataRec_free(tmp);

	return KSI_RETURN(&err);

}

static int CalAuthRec_new(KSI_CTX *ctx, CalAuthRec **out) {
	KSI_ERR err;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	CalAuthRec *tmp = NULL;
	tmp = KSI_new(CalAuthRec);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->pubData = NULL;
	tmp->sigAlgo = NULL;
	tmp->sigData = NULL;

	*out = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	CalAuthRec_free(tmp);

	return KSI_RETURN(&err);

}

static int AggrAuthRec_new(KSI_CTX *ctx, AggrAuthRec **out) {
	KSI_ERR err;
	int res;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, out != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	AggrAuthRec *tmp = NULL;
	tmp = KSI_new(AggrAuthRec);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	res = KSI_IntegerList_new(ctx, &tmp->chainIndexesList);
	KSI_CATCH(&err, res) goto cleanup;

	tmp->intputHash = NULL;
	tmp->ctx = ctx;
	tmp->sigAlgo = NULL;
	tmp->sigData = NULL;
	tmp->aggregationTime = NULL;

	*out = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	AggrAuthRec_free(tmp);

	return KSI_RETURN(&err);
}

static int SigDataRec_new(KSI_CTX *ctx, SigDataRec **out) {
	KSI_ERR err;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	SigDataRec *tmp = NULL;
	tmp = KSI_new(SigDataRec);
	if (tmp == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->cert = NULL;
	tmp->certId = NULL;
	tmp->certId_len = 0;
	tmp->certRepUri = NULL;
	tmp->cert_len = 0;
	tmp->sigValue = NULL;
	tmp->sigValue_len = 0;

	*out = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	SigDataRec_free(tmp);

	return KSI_RETURN(&err);

}


static int AggrChainRec_addIndex(KSI_CTX *ctx, KSI_TLV *tlv, AggrChainRec *aggr) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_Integer *item = NULL;
	KSI_LIST(KSI_Integer) *list = NULL;

	if (aggr == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	list = aggr->chainIndex;

	if (list == NULL) {
		res = KSI_IntegerList_new(ctx, &list);
	}

	res = KSI_TLV_cast(tlv, KSI_TLV_PAYLOAD_INT);
	if (res != KSI_OK) goto cleanup;

	res = KSI_TLV_getInteger(tlv, &item);
	if (res != KSI_OK) goto cleanup;

	res = KSI_IntegerList_append(list, item);
	if (res != KSI_OK) goto cleanup;
	item = NULL;

	aggr->chainIndex = list;
	list = NULL;



cleanup:
	if (list != aggr->chainIndex) KSI_IntegerList_free(list);
	KSI_Integer_free(item);

	return res;
}

static int AggrChainRec_addLink(KSI_CTX *ctx, KSI_TLV *tlv, AggrChainRec *aggr) {
	KSI_ERR err;
	int res = KSI_UNKNOWN_ERROR;
	int isLeft;

	KSI_HashChainLink *link = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, aggr != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	switch (KSI_TLV_getTag(tlv)) {
		case 0x07:
			isLeft = 1;
			break;
		case 0x08:
			isLeft = 0;
			break;
		default:
			KSI_FAIL(&err, KSI_INVALID_ARGUMENT, NULL);
			goto cleanup;
	}

	res = KSI_HashChainLink_new(ctx, &link);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_HashChainLink_setIsLeft(link, isLeft);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TlvTemplate_extract(ctx, link, tlv, KSI_TLV_TEMPLATE(KSI_HashChainLink), NULL);
	KSI_CATCH(&err, res) goto cleanup;

	/* Create a new list of links if this is the first link. */
	if (aggr->chain == NULL) {
		res = KSI_HashChainLinkList_new(ctx, &aggr->chain);
		KSI_CATCH(&err, res) goto cleanup;
	}

	/* Append the link to the chain */
	res = KSI_HashChainLinkList_append(aggr->chain, link);
	KSI_CATCH(&err, res) goto cleanup;
	link = NULL;

	res = KSI_OK;

cleanup:

	KSI_HashChainLink_free(link);

	return res;
}

static int CalChainRec_addLink(KSI_CTX *ctx, KSI_TLV *tlv, KSI_CalendarHashChain *cal) {
	int res;
	KSI_DataHash *hsh = NULL;
	const unsigned char *imprint = NULL;
	int imprint_len = 0;
	int isLeft;
	KSI_LIST(KSI_HashChainLink) *chain = NULL;

	/* Validate arguments. */
	switch (KSI_TLV_getTag(tlv)) {
		case 0x07:
			isLeft = 1;
			break;
		case 0x08:
			isLeft = 0;
			break;
		default:
			res = KSI_INVALID_ARGUMENT;
			goto cleanup;
	}

	res = KSI_TLV_cast(tlv, KSI_TLV_PAYLOAD_RAW);
	if (res != KSI_OK) goto cleanup;

	/* Extract the raw value from the tlv */
	res = KSI_TLV_getRawValue(tlv, &imprint, &imprint_len);
	if (res != KSI_OK) goto cleanup;

	res = KSI_DataHash_fromImprint(ctx, imprint, imprint_len, &hsh);
	if (res != KSI_OK) goto cleanup;

	res = KSI_CalendarHashChain_getHashChain(cal, &chain);
	if (res != KSI_OK) goto cleanup;

	res = KSI_HashChain_appendLink(ctx, hsh, NULL, NULL, isLeft, 0, &chain);
	if (res != KSI_OK) goto cleanup;

	res = KSI_CalendarHashChain_setHashChain(cal, chain);
	if (res != KSI_OK) goto cleanup;

	hsh = NULL;

	res = KSI_OK;

cleanup:

	KSI_nofree(imprint);
	KSI_DataHash_free(hsh);

	return res;
}

static int parseAggregationChainRec(KSI_CTX *ctx, KSI_TLV *tlv, KSI_Signature *sig) {
	KSI_ERR err;
	int res;

	AggrChainRec *aggr = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);


	if (KSI_TLV_getTag(tlv) != 0x0801) {
		KSI_FAIL(&err, KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = AggChainRec_new(ctx, &aggr);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_TLV_PARSE_BEGIN(ctx, tlv)
		KSI_PARSE_TLV_ELEMENT_INTEGER	(0x02, &aggr->aggregationTime)
		KSI_PARSE_TLV_ELEMENT_CB		(0x03, AggrChainRec_addIndex, aggr)
		KSI_PARSE_TLV_ELEMENT_RAW		(0x04, &aggr->inputData, &aggr->inputData_len)
		KSI_PARSE_TLV_ELEMENT_IMPRINT	(0x05, &aggr->inputHash)
		KSI_PARSE_TLV_ELEMENT_UINT8		(0x06, &aggr->aggrHashId)
		KSI_PARSE_TLV_ELEMENT_CB		(0x07, AggrChainRec_addLink, aggr)
		KSI_PARSE_TLV_ELEMENT_CB		(0x08, AggrChainRec_addLink, aggr)
		KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_IGNORE
	KSI_TLV_PARSE_END(res);
	KSI_CATCH(&err, res) goto cleanup;

	res = AggrChainRecList_append(sig->aggregationChainList, aggr);
	KSI_CATCH(&err, res) goto cleanup;

	aggr = NULL;

	KSI_SUCCESS(&err);

cleanup:

	AggrChainRec_free(aggr);

	return KSI_RETURN(&err);
}

static int parsePublDataRecord(KSI_CTX *ctx, KSI_TLV *tlv, PubDataRec **pdr) {
	KSI_ERR err;
	int res;
	PubDataRec *tmp = NULL;
	unsigned char *raw;
	int raw_len;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, pdr != NULL) goto cleanup;

	KSI_BEGIN(ctx, &err);

	res = PubDataRed_new(ctx, &tmp);
	KSI_CATCH(&err, res) goto cleanup;
	if (*pdr != NULL) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Multiple publication data records.");
		goto cleanup;
	}

	/* Keep the serialized value. */
	res = KSI_TLV_serialize(tlv, &raw, &raw_len);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_TLV_PARSE_BEGIN(ctx, tlv)
		KSI_PARSE_TLV_ELEMENT_INTEGER(0x02, &tmp->pubTime)
		KSI_PARSE_TLV_ELEMENT_IMPRINT(0x04, &tmp->pubHash)
		KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_IGNORE
	KSI_TLV_PARSE_END(res);
	KSI_CATCH(&err, res) goto cleanup;;

	if (tmp->pubTime == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Published Data: Missing publication time.");
		goto cleanup;

	}

	if (tmp->pubHash == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Published Data: Missing publication hash.");
		goto cleanup;

	}

	tmp->raw = raw;
	tmp->raw_len = raw_len;
	raw = NULL;

	*pdr = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_free(raw);
	PubDataRec_free(tmp);

	return KSI_RETURN(&err);
}

static int parseSigDataRecord(KSI_CTX *ctx, KSI_TLV *tlv, SigDataRec **sdr) {
	KSI_ERR err;
	int res;
	int count;
	SigDataRec *tmp = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, sdr != NULL) goto cleanup;

	KSI_BEGIN(ctx, &err);

	res = SigDataRec_new(ctx, &tmp);
	KSI_CATCH(&err, res) goto cleanup;
	if (*sdr != NULL) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Multiple signature data records.");
		goto cleanup;
	}

	KSI_TLV_PARSE_BEGIN(ctx, tlv)
		KSI_PARSE_TLV_ELEMENT_RAW(0x01, &tmp->sigValue, &tmp->sigValue_len)
		KSI_PARSE_TLV_ELEMENT_RAW(0x02, &tmp->cert, &tmp->cert_len)
		KSI_PARSE_TLV_ELEMENT_RAW(0x03, &tmp->certId, &tmp->certId_len)
		KSI_PARSE_TLV_ELEMENT_UTF8STR(0x04, &tmp->certRepUri)
		KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_IGNORE
	KSI_TLV_PARSE_END(res);
	KSI_CATCH(&err, res) goto cleanup;

	/* Check mandatory parameters. */
	if (tmp->sigValue == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signed Data: Missing signed value");
		goto cleanup;
	}

	/* Manual xor */
	count = 0 +
			(tmp->cert != NULL ? 1 : 0) +
			(tmp->certId != NULL ? 1 : 0) +
			(tmp->certRepUri != NULL ? 1 : 0);

	if (count == 0) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signed Data: Incomplete signed data.");
		goto cleanup;
	} else if (count != 1 ) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signed Data: More than one certificate specified.");
		goto cleanup;
	}

	*sdr = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	SigDataRec_free(tmp);

	return KSI_RETURN(&err);

}

static int parseAggrAuthRecChainIndex(KSI_CTX *ctx, KSI_TLV *tlv, AggrAuthRec **aar) {
	KSI_ERR err;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, aar != NULL) goto cleanup;

	KSI_BEGIN(ctx, &err);

	if (KSI_TLV_getTag(tlv) != 0x03)

	KSI_SUCCESS(&err);

cleanup:

	return KSI_RETURN(&err);
}

static int parseAggrAuthRec(KSI_CTX *ctx, KSI_TLV *tlv, AggrAuthRec **aar) {
	KSI_ERR err;
	int res;
	AggrAuthRec *auth = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, aar != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	res = AggrAuthRec_new(ctx, &auth);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_TLV_PARSE_BEGIN(ctx, tlv)
		KSI_PARSE_TLV_ELEMENT_INTEGER(0x02, &auth->aggregationTime)
		KSI_PARSE_TLV_ELEMENT_CB(0x03, parseAggrAuthRecChainIndex, &auth)
		KSI_PARSE_TLV_ELEMENT_IMPRINT(0x05, &auth->intputHash)

		KSI_PARSE_TLV_ELEMENT_UTF8STR(0x0b, &auth->sigAlgo)

		KSI_PARSE_TLV_ELEMENT_CB(0x0c, parseSigDataRecord, &auth->sigData)

		KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_IGNORE
	KSI_TLV_PARSE_END(res);




	KSI_SUCCESS(&err);

cleanup:

	return KSI_RETURN(&err);
}

static int parseCalAuthRec(KSI_CTX *ctx, KSI_TLV *tlv, CalAuthRec **car) {
	KSI_ERR err;
	int res;
	CalAuthRec *auth = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, car != NULL) goto cleanup;

	KSI_BEGIN(ctx, &err);

	if (*car != NULL) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Multiple calendar auth records.");
		goto cleanup;
	}

	res = CalAuthRec_new(ctx, &auth);
	KSI_CATCH(&err, res);

	KSI_TLV_PARSE_BEGIN(ctx, tlv)
		KSI_PARSE_TLV_ELEMENT_CB(0x10, parsePublDataRecord, &auth->pubData)
		KSI_PARSE_TLV_ELEMENT_UTF8STR(0x0b, &auth->sigAlgo)
		KSI_PARSE_TLV_ELEMENT_CB(0x0c, parseSigDataRecord, &auth->sigData)
		KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_IGNORE
	KSI_TLV_PARSE_END(res);
	KSI_CATCH(&err, res) goto cleanup;

	/* Check mandatory parameters. */
	if (auth->pubData == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Calendar Auth Record: Missing publication data.");
		goto cleanup;
	}

	if (auth->sigAlgo == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Calendar Auth Record: Missing algorithm.");
		goto cleanup;
	}

	if (auth->sigData == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Calendar Auth Record: Missing signed data.");
		goto cleanup;
	}

	*car = auth;
	auth = NULL;

	KSI_SUCCESS(&err);

cleanup:

	CalAuthRec_free(auth);

	return KSI_RETURN(&err);
}

static int extractSignature(KSI_CTX *ctx, KSI_TLV *tlv, KSI_Signature **signature) {
	KSI_ERR err;
	int res;

	KSI_Signature *sig = NULL;
	KSI_CalendarHashChain *cal = NULL;
	KSI_Integer *aggregationTime = NULL;
	KSI_Integer *publicationTime = NULL;
	KSI_DataHash *inputHash = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, tlv != NULL) goto cleanup;
	KSI_PRE(&err, signature != NULL) goto cleanup;

	KSI_BEGIN(ctx, &err);

	if (KSI_TLV_getTag(tlv) != 0x800) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, NULL);
		goto cleanup;
	}

	res = KSI_Signature_new(ctx, &sig);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_CalendarHashChain_new(ctx, &cal);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_LOG_debug(ctx, "Starting to parse signature.");

	KSI_TLV_PARSE_BEGIN(ctx, tlv)
		KSI_PARSE_TLV_ELEMENT_CB(0x801, parseAggregationChainRec, sig) // Aggregation hash chain

		KSI_PARSE_TLV_NESTED_ELEMENT_BEGIN(0x802) // Calendar hash chain
			KSI_PARSE_TLV_ELEMENT_INTEGER(0x01, &publicationTime)
			KSI_PARSE_TLV_ELEMENT_INTEGER(0x02, &aggregationTime)
			KSI_PARSE_TLV_ELEMENT_IMPRINT(0x05, &inputHash)
			KSI_PARSE_TLV_ELEMENT_CB(0x07, CalChainRec_addLink, cal)
			KSI_PARSE_TLV_ELEMENT_CB(0x08, CalChainRec_addLink, cal)
			KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_REMOVE
		KSI_PARSE_TLV_NESTED_ELEMENT_END

		KSI_PARSE_TLV_ELEMENT_CB(0x0804, parseAggrAuthRec, &sig->aggrAuth);
		KSI_PARSE_TLV_ELEMENT_CB(0x0805, parseCalAuthRec, &sig->calAuth)

		KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_REMOVE
	KSI_TLV_PARSE_END(res);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_CalendarHashChain_setAggregationTime(cal, aggregationTime);
	KSI_CATCH(&err, res) goto cleanup;
	aggregationTime = NULL;

	res = KSI_CalendarHashChain_setPublicationTime(cal, publicationTime);
	KSI_CATCH(&err, res) goto cleanup;
	publicationTime = NULL;

	res = KSI_CalendarHashChain_setInputHash(cal, inputHash);
	KSI_CATCH(&err, res) goto cleanup;
	inputHash = NULL;
	sig->calendarChain = cal;
	cal = NULL;

	res = KSI_Signature_validate(sig);
	KSI_CATCH(&err, res) goto cleanup;


	*signature = sig;
	sig = NULL;

	KSI_LOG_debug(ctx, "Finished parsing successfully.");
	KSI_SUCCESS(&err);

cleanup:

	KSI_Integer_free(aggregationTime);
	KSI_Integer_free(publicationTime);
	KSI_DataHash_free(inputHash);

	KSI_CalendarHashChain_free(cal);
	KSI_Signature_free(sig);

	return KSI_RETURN(&err);
}


void KSI_Signature_free(KSI_Signature *sig) {
	if (sig != NULL) {
		KSI_TLV_free(sig->baseTlv);
		KSI_CalendarHashChain_free(sig->calendarChain);
		AggrChainRecList_free(sig->aggregationChainList);
		CalAuthRec_free(sig->calAuth);
		AggrAuthRec_free(sig->aggrAuth);
		KSI_free(sig);
	}
}

int KSI_parseAggregationResponse(KSI_CTX *ctx, unsigned char *response, int response_len, KSI_Signature **signature) {
	KSI_ERR err;
	int res;
	KSI_TLV *sigTlv = NULL;
	KSI_TLV *tmpTlv = NULL;
	KSI_Signature *tmp = NULL;

	/* PDU Specific objects */
	KSI_Integer *status = NULL;
	KSI_Integer *requestId = NULL;
	char *errorMessage = NULL;
	HeaderRec *hdr = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, response != NULL) goto cleanup;
	KSI_PRE(&err, response_len > 0) goto cleanup;

	KSI_BEGIN(ctx, &err);

	res = HeaderRec_new(ctx, &hdr);
	KSI_CATCH(&err, res) goto cleanup;

	/* Parse the pdu */
	res = KSI_TLV_parseBlob(ctx, response, response_len, &tmpTlv);
	KSI_CATCH(&err, res) goto cleanup;

	/* Validate tag value */
	if (KSI_TLV_getTag(tmpTlv) != 0x200) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, NULL);
		goto cleanup;
	}

	/* Make it a composite object */
	res = KSI_TLV_cast(tmpTlv, KSI_TLV_PAYLOAD_TLV);
	KSI_CATCH(&err, res) goto cleanup;

	/* Create signature TLV */
	res = KSI_TLV_new(ctx, KSI_TLV_PAYLOAD_TLV, 0x800, 0, 0, &sigTlv);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_TLV_PARSE_BEGIN(ctx, tmpTlv)
		KSI_PARSE_TLV_NESTED_ELEMENT_BEGIN(0x202)
			KSI_PARSE_TLV_NESTED_ELEMENT_BEGIN(0x01)
				KSI_PARSE_TLV_ELEMENT_INTEGER(0x05, &hdr->instanceId)
				KSI_PARSE_TLV_ELEMENT_INTEGER(0x06, &hdr->messageId)
				KSI_PARSE_TLV_ELEMENT_RAW(0x07, &hdr->clientId, &hdr->clientId_length);
				KSI_PARSE_TLV_ELEMENT_UNKNONW_NON_CRITICAL_IGNORE
			KSI_PARSE_TLV_NESTED_ELEMENT_END

			KSI_PARSE_TLV_ELEMENT_INTEGER(0x02, &requestId)
			KSI_PARSE_TLV_ELEMENT_INTEGER(0x05, &status)
			KSI_PARSE_TLV_ELEMENT_UTF8STR(0x06, &errorMessage)

			KSI_PARSE_TLV_ELEMENT_UNKNOWN_FWD(sigTlv)
		KSI_PARSE_TLV_NESTED_ELEMENT_END
	KSI_TLV_PARSE_END(res);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_LOG_logTlv(ctx, KSI_LOG_DEBUG, "Signature", sigTlv);


	/* NB! By casting the TLV into raw we force it to decouple from the
	 * base TLV structure (i.e nested values may use the values from
	 * the parents.*/
	res = KSI_TLV_cast(sigTlv, KSI_TLV_PAYLOAD_RAW);
	KSI_CATCH(&err, res) goto cleanup;

	// TODO What else can we do with message header ?
	KSI_LOG_debug(ctx, "Aggregation response: instanceId = %ld, messageId = %ld",
			(unsigned long long) KSI_Integer_getUInt64(hdr->instanceId),
			(unsigned long long) KSI_Integer_getUInt64(hdr->messageId));

	if (status != NULL && !KSI_Integer_equalsUInt(status, 0)) {
		char msg[1024];

		snprintf(msg, sizeof(msg), "Aggregation failed: %s", errorMessage);
		KSI_FAIL_EXT(&err, KSI_AGGREGATOR_ERROR, (unsigned long long)KSI_Integer_getUInt64(status), errorMessage);
		goto cleanup;
	}

	res = extractSignature(ctx, sigTlv, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	/* The tlv is referenced from the signature now */
	tmp->baseTlv = sigTlv;
	sigTlv = NULL;

	*signature = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_TLV_free(sigTlv);
	HeaderRec_free(hdr);
	KSI_Signature_free(tmp);
	KSI_TLV_free(tmpTlv);

	return KSI_RETURN(&err);

}

int KSI_Signature_getDataHash(KSI_Signature *sig, const KSI_DataHash **hsh) {
	KSI_ERR err;

	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_BEGIN(sig->ctx, &err);

	// TODO!

	KSI_SUCCESS(&err);

cleanup:

	KSI_nofree(h);

	return KSI_RETURN(&err);
}

int KSI_Signature_getSigningTime(const KSI_Signature *sig, KSI_Integer **signTime) {
	KSI_ERR err;
	int res;
	KSI_Integer *signingTime = NULL;

	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_BEGIN(sig->ctx, &err);

	if (sig->calendarChain == NULL) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, NULL);
		goto cleanup;
	}

	res = KSI_CalendarHashChain_getAggregationTime(sig->calendarChain, &signingTime);
	KSI_CATCH(&err, res) goto cleanup;

	if (signingTime == NULL) {
		res = KSI_CalendarHashChain_getPublicationTime(sig->calendarChain, &signingTime);
		KSI_CATCH(&err, res) goto cleanup;

		if (signTime == NULL){
			KSI_FAIL(&err, KSI_INVALID_SIGNATURE, NULL);
			goto cleanup;
		}

	}

	*signTime = signingTime;

	KSI_SUCCESS(&err);

cleanup:

	return KSI_RETURN(&err);
}

int KSI_Signature_getSignerIdentity(KSI_Signature *sig, char ** identity) {
	*identity = "TODO!";
	return KSI_OK;
}

int KSI_Signature_validateInternal(KSI_Signature *sig) {
	KSI_ERR err;
	KSI_DataHash *hsh = NULL;
	uint32_t utc_time;
	int res;
	int level;
	int i;
	KSI_Integer *aggregationTime = NULL;
	KSI_Integer *publicationTime = NULL;
	KSI_LIST(KSI_HashChainLink) *chain = NULL;
	KSI_DataHash *inputHash = NULL;

	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_BEGIN(sig->ctx, &err);

	if (sig->aggregationChainList == NULL || AggrChainRecList_length(sig->aggregationChainList) == 0) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signature does not contain any aggregation chains.");
		goto cleanup;
	}

	if (sig->calendarChain == NULL) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signature does not contain a calendar chain.");
		goto cleanup;
	}

	if (sig->calAuth == NULL) { // Add aggr auth
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signature does not contain any authentication record.");
		goto cleanup;
	}

	res = KSI_CalendarHashChain_getHashChain(sig->calendarChain, &chain);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_CalendarHashChain_getPublicationTime(sig->calendarChain, &publicationTime);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_CalendarHashChain_getAggregationTime(sig->calendarChain, &aggregationTime);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_CalendarHashChain_getInputHash(sig->calendarChain, &inputHash);
	KSI_CATCH(&err, res) goto cleanup;

	/* Validate aggregation time */
	res = KSI_HashChain_getCalendarAggregationTime(chain, publicationTime, &utc_time);
	KSI_CATCH(&err, res) goto cleanup;

	if (aggregationTime == NULL) aggregationTime = publicationTime;


	if (!KSI_Integer_equalsUInt(aggregationTime, utc_time)) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Aggregation time mismatch.");
		goto cleanup;
	}

	/* Aggregate aggregation chains. */
	hsh = NULL;
	level = 0;

	for (i = 0; i < AggrChainRecList_length(sig->aggregationChainList); i++) {
		const AggrChainRec* aggregationChain = NULL;
		KSI_DataHash *tmpHash = NULL;

		res = AggrChainRecList_elementAt(sig->aggregationChainList, i, (AggrChainRec **)&aggregationChain);
		KSI_CATCH(&err, res) goto cleanup;

		if (aggregationChain == NULL) break;

		if (hsh != NULL) {
			/* Validate input hash */
			if (!KSI_DataHash_equals(hsh, aggregationChain->inputHash)) {
				KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Aggregation chain mismatch,");
			}
		}

		res = KSI_HashChain_aggregate(aggregationChain->chain, aggregationChain->inputHash, level, aggregationChain->aggrHashId, &level, &tmpHash);
		KSI_CATCH(&err, res) {
			KSI_FAIL(&err, res, "Failed to calculate aggregation chain.");
			goto cleanup;
		}

		if (hsh != NULL) {
			KSI_DataHash_free(hsh);
		}
		hsh = tmpHash;
	}

	/* Validate calendar input hash */
	if (!KSI_DataHash_equals(hsh, inputHash)) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Calendar chain input hash mismatch.");
		goto cleanup;
	}

	KSI_DataHash_free(hsh);
	hsh = NULL;

	/* Aggregate calendar chain */
	res = KSI_HashChain_aggregateCalendar(chain, inputHash, &hsh);
	KSI_CATCH(&err, res) goto cleanup;

	/* Validate calendar root hash */
	if (!KSI_DataHash_equals(hsh, sig->calAuth->pubData->pubHash)) {
		KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Calendar chain root hash mismatch.");
		goto cleanup;
	}

	if (sig->calAuth != NULL) {
		res = CalAuthRec_validate(sig->calAuth);
		KSI_CATCH(&err, res) goto cleanup;
	}

	if (sig->aggrAuth != NULL) {
		/* TODO! */
		KSI_FAIL(&err, KSI_UNKNOWN_ERROR, "Validation using aggregation auth record not implemented.");
		goto cleanup;
	}

	KSI_SUCCESS(&err);

cleanup:

	KSI_DataHash_free(hsh);

	return KSI_RETURN(&err);
}

int KSI_Signature_validate(KSI_Signature *sig) {
	int res;
	res = KSI_Signature_validateInternal(sig);
	if (res != KSI_OK) goto cleanup;

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_Signature_clone(const KSI_Signature *sig, KSI_Signature **clone) {
	KSI_ERR err;
	KSI_TLV *tlv = NULL;
	KSI_Signature *tmp = NULL;
	int res;

	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_PRE(&err, clone != NULL) goto cleanup;

	KSI_BEGIN(sig->ctx, &err);

	res = KSI_TLV_clone(sig->baseTlv, &tlv);
	KSI_CATCH(&err, res) goto cleanup;

	KSI_LOG_logTlv(sig->ctx, KSI_LOG_DEBUG, "Original TLV", sig->baseTlv);
	KSI_LOG_logTlv(sig->ctx, KSI_LOG_DEBUG, "Cloned TLV", tlv);

	res = extractSignature(sig->ctx, tlv, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	tmp->baseTlv = tlv;
	tlv = NULL;

	*clone = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_TLV_free(tlv);
	KSI_Signature_free(tmp);

	return KSI_RETURN(&err);
}

int KSI_Signature_parse(KSI_CTX *ctx, unsigned char *raw, int raw_len, KSI_Signature **sig) {
	KSI_ERR err;
	KSI_TLV *tlv = NULL;
	KSI_Signature *tmp = NULL;
	int res;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, raw != NULL) goto cleanup;
	KSI_PRE(&err, raw_len > 0) goto cleanup;
	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	res = KSI_TLV_parseBlob(ctx, raw, raw_len, &tlv);
	KSI_CATCH(&err, res) goto cleanup;

	res = extractSignature(ctx, tlv, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	tmp->baseTlv = tlv;
	tlv = NULL;

	*sig = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_TLV_free(tlv);
	KSI_Signature_free(tmp);

	return KSI_RETURN(&err);
}

int KSI_Signature_fromFile(KSI_CTX *ctx, const char *fileName, KSI_Signature **sig) {
	KSI_ERR err;
	int res;
	FILE *f = NULL;
	unsigned char *raw = NULL;
	int raw_size = 0xfffff;
	size_t raw_len = 0;
	KSI_Signature *tmp = NULL;

	KSI_PRE(&err, ctx != NULL) goto cleanup;
	KSI_PRE(&err, fileName != NULL) goto cleanup;
	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_BEGIN(ctx, &err);

	raw = KSI_calloc(raw_size, 1);
	if (raw == NULL) {
		KSI_FAIL(&err, KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	f = fopen(fileName, "rb");
	if (f == NULL) {
		KSI_FAIL(&err, KSI_IO_ERROR, "Unable to open file.");
		goto cleanup;
	}

	raw_len = fread(raw, 1, raw_size, f);
	if (raw_len == 0) {
		KSI_FAIL(&err, KSI_IO_ERROR, "Unable to read file.");
		goto cleanup;
	}

	if (!feof(f)) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Input too long for a valid signature.");
		goto cleanup;
	}

	res = KSI_Signature_parse(ctx, raw, (int)raw_len, &tmp);
	KSI_CATCH(&err, res) goto cleanup;

	*sig = tmp;
	tmp = NULL;

	KSI_SUCCESS(&err);

cleanup:

	if (f != NULL) fclose(f);
	KSI_Signature_free(tmp);
	KSI_free(raw);

	return KSI_RETURN(&err);
}

int KSI_Signature_serialize(KSI_Signature *sig, unsigned char **raw, int *raw_len) {
	KSI_ERR err;
	int res;
	unsigned char *tmp = NULL;
	int tmp_len;

	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_PRE(&err, raw != NULL) goto cleanup;
	KSI_PRE(&err, raw_len != NULL) goto cleanup;
	KSI_BEGIN(sig->ctx, &err);

	/* We assume that the baseTlv tree is up to date! */
	res = KSI_TLV_serialize(sig->baseTlv, &tmp, &tmp_len);
	KSI_CATCH(&err, res) goto cleanup;

	*raw = tmp;
	tmp = NULL;

	*raw_len = tmp_len;

	KSI_SUCCESS(&err);

cleanup:

	KSI_free(tmp);

	return KSI_RETURN(&err);

}

int KSI_Signature_replaceCalendarChain(KSI_Signature *sig, KSI_CalendarHashChain *calendarHashChain) {
	KSI_ERR err;
	int res;
	// FIXME: const KSI_DataHash *inputHash = NULL;
	KSI_TLV *oldCalChainTlv = NULL;
	KSI_TLV *newCalChainTlv = NULL;

	KSI_PRE(&err, calendarHashChain != NULL) goto cleanup;
	KSI_PRE(&err, sig != NULL) goto cleanup;

	KSI_BEGIN(sig->ctx, &err);

	if (sig->calendarChain == NULL) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Signature does not contain a hash chain.");
		goto cleanup;
	}

/*	res = KSI_CalendarHashChain_getInputHash(chain, &inputHash);
	KSI_CATCH(&err, res) goto cleanup;

	if (inputHash == NULL) {
		KSI_FAIL(&err, KSI_INVALID_FORMAT, "Given calendar hash chain does not contain an input hash.");
		goto cleanup;
	}
*/
	/* The output hash and input hash have to be equal */
/*	if (!KSI_DataHash_equals(inputHash, sig->calendarChain->inputHash)) {
		KSI_FAIL(&err, KSI_EXTEND_WRONG_CAL_CHAIN, NULL);
		goto cleanup;
	}
*/
	res = KSI_TLV_iterNested(sig->baseTlv);
	KSI_CATCH(&err, res) goto cleanup;

	while (1) {
		res = KSI_TLV_getNextNestedTLV(sig->baseTlv, &oldCalChainTlv);
		KSI_CATCH(&err, res) goto cleanup;

		if (oldCalChainTlv == NULL) {
			KSI_FAIL(&err, KSI_INVALID_SIGNATURE, "Signature does not contain calendar chain.");
			goto cleanup;
		}

		if (KSI_TLV_getTag(oldCalChainTlv) == KSI_TAG_CALENDAR_CHAIN) break;
	}

	res = KSI_TLV_new(sig->ctx, KSI_TLV_PAYLOAD_TLV, KSI_TAG_CALENDAR_CHAIN, 0, 0, &newCalChainTlv);
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TlvTemplate_construct(sig->ctx, newCalChainTlv, calendarHashChain, KSI_TLV_TEMPLATE(KSI_CalendarHashChain));
	KSI_CATCH(&err, res) goto cleanup;

	res = KSI_TLV_replaceNestedTlv(sig->baseTlv, oldCalChainTlv, newCalChainTlv);
	KSI_CATCH(&err, res) goto cleanup;
	newCalChainTlv = NULL;

	KSI_CalendarHashChain_free(sig->calendarChain);
	sig->calendarChain = calendarHashChain;


	KSI_TLV_free(oldCalChainTlv);

	KSI_SUCCESS(&err);

cleanup:

	KSI_TLV_free(newCalChainTlv);

	KSI_nofree(inputHash);

	return KSI_RETURN(&err);
}

int KSI_Signature_getSignedIdentity(KSI_Signature *sig, char **signerIdentity) {
	KSI_ERR err;
	int res;
	int i, j;
	KSI_LIST(KSI_Utf8String) *idList = NULL;
	KSI_Utf8String *clientId = NULL;
	char *signerId = NULL;
	int signerId_size = 0;
	int signerId_len = 0;

	KSI_PRE(&err, sig != NULL) goto cleanup;
	KSI_PRE(&err, signerIdentity != NULL) goto cleanup;
	KSI_BEGIN(sig->ctx, &err);

	/* Create a list of separate signer identities. */
	res = KSI_Utf8StringList_new(sig->ctx, &idList);
	KSI_CATCH(&err, res) goto cleanup;

	/* Extract all identities from all aggregation chains. */
	for (i = 0; i < AggrChainRecList_length(sig->aggregationChainList); i++) {
		AggrChainRec *aggrRec = NULL;

		res = AggrChainRecList_elementAt(sig->aggregationChainList, i, &aggrRec);
		KSI_CATCH(&err, res) goto cleanup;

		for (j = 0; j < KSI_HashChainLinkList_length(aggrRec->chain); j++) {
			KSI_HashChainLink *link = NULL;
			KSI_MetaData *metaData = NULL;

			res = KSI_HashChainLinkList_elementAt(aggrRec->chain, j, &link);
			KSI_CATCH(&err, res) goto cleanup;

			KSI_HashChainLink_getMetaData(link, &metaData);
			KSI_CATCH(&err, res) goto cleanup;

			/* Exit inner loop if this chain link does not contain a metadata block. */
			if (metaData == NULL) continue;

			res = KSI_MetaData_getClientId(metaData, &clientId);
			KSI_CATCH(&err, res) goto cleanup;

			signerId_size += strlen((char *)clientId) + 1; /* +1 for dot (.) or ending zero character. */

			res = KSI_Utf8StringList_append(idList, clientId);
			clientId = NULL;

		}
	}

	/* Concatenate all together. */
	for (i = 0; i < KSI_Utf8StringList_length(idList); i++) {
		KSI_Utf8String *tmp = NULL;

		res = KSI_Utf8StringList_elementAt(idList, i, &tmp);
		KSI_CATCH(&err, res) goto cleanup;

		signerId_len += sprintf(signerId + signerId_len, "%s%s", signerId_len > 0 ? ".": "", (char *) tmp);
	}

	*signerIdentity = signerId;
	signerId = NULL;

	KSI_SUCCESS(&err);

cleanup:

	KSI_Utf8String_free(signerId);
	KSI_Utf8StringList_free(idList);
	KSI_free(clientId);
	KSI_free(clientId);
	KSI_Utf8StringList_free(idList);

	return KSI_RETURN(&err);
}

KSI_IMPLEMENT_GET_CTX(KSI_Signature);
