/*
 * Copyright 2013-2015 Guardtime, Inc.
 *
 * This file is part of the Guardtime client SDK.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES, CONDITIONS, OR OTHER LICENSES OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 * "Guardtime" and "KSI" are trademarks or registered trademarks of
 * Guardtime, Inc., and no license to trademarks is granted; Guardtime
 * reserves and retains all trademark rights.
 */

#include <string.h>

#include <ksi/hashchain.h>
#include <ksi/net.h>
#include <ksi/net_uri.h>
#include <ksi/pkitruststore.h>
#include <ksi/tree_builder.h>

#include "all_tests.h"

#include "../src/ksi/impl/ctx_impl.h"
#include "../src/ksi/impl/net_http_impl.h"
#include "../src/ksi/impl/net_tcp_impl.h"
#include "../src/ksi/impl/net_uri_impl.h"
#include "../src/ksi/impl/signature_impl.h"

extern KSI_CTX *ctx;

#define TEST_USER "anon"
#define TEST_PASS "anon"

static unsigned char mockImprint[] ={0x01,
		 0x11, 0xa7, 0x00, 0xb0, 0xc8, 0x06, 0x6c, 0x47,
		 0xec, 0xba, 0x05, 0xed, 0x37, 0xbc, 0x14, 0xdc,
		 0xad, 0xb2, 0x38, 0x55, 0x2d, 0x86, 0xc6, 0x59,
		 0x34, 0x2d, 0x1d, 0x7e, 0x87, 0xb8, 0x77, 0x2d};

static KSI_Config *callbackConf = NULL;
static size_t callbackCalls = 0;

static int Test_ConfigCallback(KSI_CTX *ctx, KSI_Config *conf) {
	callbackCalls++;
	if (ctx == NULL || conf == NULL) return KSI_INVALID_ARGUMENT;
	callbackConf = KSI_Config_ref(conf);
	return KSI_OK;
}

static void Test_ConfCallback_reset(void) {
	callbackConf = NULL;
	callbackCalls = 0;
}

static void preTest(void) {
	ctx->netProvider->requestCount = 0;

	/* Set PDU v2. */
	KSI_CTX_setOption(ctx, KSI_OPT_AGGR_PDU_VER, (void*)KSI_PDU_VERSION_2);
	KSI_CTX_setOption(ctx, KSI_OPT_EXT_PDU_VER, (void*)KSI_PDU_VERSION_2);

	/* Reset conf callback. */
	Test_ConfCallback_reset();
	KSI_CTX_setOption(ctx, KSI_OPT_AGGR_CONF_RECEIVED_CALLBACK, NULL);
	KSI_CTX_setOption(ctx, KSI_OPT_EXT_CONF_RECEIVED_CALLBACK, NULL);
}

static void postTest(void) {
	/* Restore default PDU version. */
	KSI_CTX_setOption(ctx, KSI_OPT_AGGR_PDU_VER, (void*)KSI_AGGREGATION_PDU_VERSION);
	KSI_CTX_setOption(ctx, KSI_OPT_EXT_PDU_VER, (void*)KSI_EXTENDING_PDU_VERSION);
	/* Restore default HMAC algorithm. */
	KSI_CTX_setOption(ctx, KSI_OPT_AGGR_HMAC_ALGORITHM, (void*)TEST_DEFAULT_AGGR_HMAC_ALGORITHM);
	KSI_CTX_setOption(ctx, KSI_OPT_EXT_HMAC_ALGORITHM, (void*)TEST_DEFAULT_EXT_HMAC_ALGORITHM);

	/* Reset conf callback. */
	KSI_CTX_setOption(ctx, KSI_OPT_AGGR_CONF_RECEIVED_CALLBACK, NULL);
	KSI_CTX_setOption(ctx, KSI_OPT_EXT_CONF_RECEIVED_CALLBACK, NULL);
}

static void testSigning(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-07-01.1.ksig"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	unsigned char *raw = NULL;
	size_t raw_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Unable to sign the hash.", res == KSI_OK && sig != NULL);

	res = KSI_Signature_serialize(sig, &raw, &raw_len);
	CuAssert(tc, "Unable to serialize signature.", res == KSI_OK && raw != NULL && raw_len > 0);

	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to load sample signature.", f != NULL);

	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	CuAssert(tc, "Failed to read sample.", expected_len > 0);

	CuAssert(tc, "Serialized signature length mismatch.", expected_len == raw_len);
	CuAssert(tc, "Serialized signature content mismatch.", !memcmp(expected, raw, raw_len));

	if (f != NULL) fclose(f);
	KSI_free(raw);
	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testSigning_hmacAlgorithmSha512(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-hmac_sha512.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-07-01.1.ksig"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	unsigned char *raw = NULL;
	size_t raw_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_CTX_setAggregatorHmacAlgorithm(ctx, KSI_HASHALG_SHA2_512);
	CuAssert(tc, "Unable to set aggregator HMAC algorithm.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Unable to sign the hash.", res == KSI_OK && sig != NULL);

	res = KSI_Signature_serialize(sig, &raw, &raw_len);
	CuAssert(tc, "Unable to serialize signature.", res == KSI_OK && raw != NULL && raw_len > 0);

	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to load sample signature.", f != NULL);

	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	CuAssert(tc, "Failed to read sample", expected_len > 0);

	CuAssert(tc, "Serialized signature length mismatch.", expected_len == raw_len);
	CuAssert(tc, "Serialized signature content mismatch.", !memcmp(expected, raw, raw_len));

	if (f != NULL) fclose(f);
	KSI_free(raw);
	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE

}

static void testSigning_hmacAlgorithmMismatch(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-07-01.1.ksig"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_CTX_setAggregatorHmacAlgorithm(ctx, KSI_HASHALG_SHA2_512);
	CuAssert(tc, "Unable to set aggregator HMAC algorithm.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Unable to sign the hash.", res == KSI_HMAC_ALGORITHM_MISMATCH && sig == NULL);

	KSI_DataHash_free(hsh);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testSigningHeaderNotFirst(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/nok-aggr-response-header-not-first.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should fail with incorrectly ordered aggregation response.", res == KSI_INVALID_FORMAT && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testSigningHmacNotLast(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/nok-aggr-response-hmac-not-last.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should fail with incorrectly ordered aggregation response.", res == KSI_INVALID_FORMAT && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testSigningResponsePduV1(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/ok-sig-2014-07-01.1-aggr_response-pduv1.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should fail with a different PDU version.", res == KSI_SERVICE_AGGR_PDU_V1_RESPONSE_TO_PDU_V2_REQUEST && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testSigningWrongResponse(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-07-01.1.ksig"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSITest_DataHash_fromStr(ctx, "010000000000000000000000000000000000000000000000000000000000000000", &hsh);
	CuAssert(tc, "Unable to create data hash.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should not succeed.", res == KSI_VERIFICATION_FAILURE && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testAggreAuthFailure(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/aggr_error_pdu.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Aggregation should fail with service error.", res == KSI_SERVICE_AUTHENTICATION_FAILURE && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}



static void testExtending(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-extended.ksig"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Unable to extend the signature.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);
	CuAssert(tc, "Unexpected extended signature.", !KSITest_memcmp(expected, serialized, expected_len));

	KSI_free(serialized);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testExtending_hmacAlgorithmSha512(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-hmac_sha512.tlv"
#define TEST_EXT_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-extended.ksig"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_CTX_setExtenderHmacAlgorithm(ctx, KSI_HASHALG_SHA2_512);
	CuAssert(tc, "Unable to set aggregator HMAC algorithm.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Unable to extend the signature.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_EXT_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);
	CuAssert(tc, "Unexpected extended signature.", !KSITest_memcmp(expected, serialized, expected_len));

	KSI_free(serialized);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_EXT_SIGNATURE_FILE
}

static void testExtending_hmacAlgorithmMismatch(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_CTX_setExtenderHmacAlgorithm(ctx, KSI_HASHALG_SHA2_512);
	CuAssert(tc, "Unable to set extender HMAC algorithm.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Unable to extend the signature.", res == KSI_HMAC_ALGORITHM_MISMATCH && ext == NULL);

	KSI_Signature_free(sig);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingHeaderNotFirst(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/nok-extender-response-header-not-first.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Extending should fail with incorrectly ordered response PDU.", res == KSI_INVALID_FORMAT && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingHmacNotLast(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/nok-extender-response-hmac-not-last.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Extending should fail with incorrectly ordered response PDU.", res == KSI_INVALID_FORMAT && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingResponsePduV1(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/ok-sig-2014-04-30.1-extend_response-pduv1.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should fail with a different PDU version.", res == KSI_SERVICE_EXTENDER_PDU_V1_RESPONSE_TO_PDU_V2_REQUEST && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendTo(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-extended_1400112000.ksig"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;
	KSI_Integer *to = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	KSI_Integer_new(ctx, 1400112000, &to);

	res = KSI_Signature_extendTo(sig, ctx, to, &ext);
	CuAssert(tc, "Unable to extend the signature.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);
	CuAssert(tc, "Unexpected extended signature.", !KSITest_memcmp(expected, serialized, expected_len));

	KSI_free(serialized);

	KSI_Integer_free(to);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testExtendSigNoCalChain(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1-no-cal-hashchain.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-extended.ksig"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Unable to extend the signature.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);

	KSI_free(serialized);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testExtenderWrongData(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	KSI_Integer *to = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	/* Create a random date that is different from the response. */
	KSI_Integer_new(ctx, 1400112222, &to);

	res = KSI_Signature_extendTo(sig, ctx, to, &ext);
	CuAssert(tc, "Wrong answer from extender should not be tolerated.", res == KSI_INVALID_ARGUMENT && ext == NULL);

	KSI_Integer_free(to);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtAuthFailure(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ext_error_pdu.tlv"
#define TEST_CRT_FILE           "resource/crt/mock.crt"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	KSI_PKITruststore *pki = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_CTX_getPKITruststore(ctx, &pki);
	CuAssert(tc, "Unable to get PKI Truststore.", res == KSI_OK && pki != NULL);

	res = KSI_PKITruststore_addLookupFile(pki, getFullResourcePath(TEST_CRT_FILE));
	CuAssert(tc, "Unable to add test certificate to truststore.", res == KSI_OK);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Extend should fail with service error.", res == KSI_SERVICE_AUTHENTICATION_FAILURE && ext == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_CRT_FILE
}

static void testExtendingWithoutPublication(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-head.ksig"
#define TEST_CRT_FILE           "resource/crt/mock.crt"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;
	KSI_PKITruststore *pki = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_CTX_getPKITruststore(ctx, &pki);
	CuAssert(tc, "Unable to get PKI Truststore.", res == KSI_OK && pki != NULL);

	res = KSI_PKITruststore_addLookupFile(pki, getFullResourcePath(TEST_CRT_FILE));
	CuAssert(tc, "Unable to add test certificate to truststore.", res == KSI_OK);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_Signature_extend(sig, ctx, NULL, &ext);
	CuAssert(tc, "Unable to extend the signature to the head.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);
	KSI_LOG_logBlob(ctx, KSI_LOG_DEBUG, "Signature extended to head.", serialized, serialized_len);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);
	CuAssert(tc, "Unexpected extended signature.", !memcmp(expected, serialized, expected_len));

	KSI_free(serialized);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
#undef TEST_CRT_FILE
}

static void testExtendingToNULL(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-head.ksig"
#define TEST_CRT_FILE           "resource/crt/mock.crt"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;
	KSI_PKITruststore *pki = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_CTX_getPKITruststore(ctx, &pki);
	CuAssert(tc, "Unable to get PKI Truststore.", res == KSI_OK && pki != NULL);

	res = KSI_PKITruststore_addLookupFile(pki, getFullResourcePath(TEST_CRT_FILE));
	CuAssert(tc, "Unable to add test certificate to truststore.", res == KSI_OK);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_Signature_extendTo(sig, ctx, NULL, &ext);
	CuAssert(tc, "Unable to extend the signature to the head.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);
	KSI_LOG_logBlob(ctx, KSI_LOG_DEBUG, "Signature extended to head.", serialized, serialized_len);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);
	CuAssert(tc, "Unexpected extended signature.", !memcmp(expected, serialized, expected_len));

	KSI_free(serialized);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
#undef TEST_CRT_FILE
}

static void testSigningInvalidResponse(CuTest* tc){
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/nok_aggr_response_missing_header.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signature should not be created with invalid aggregation response.", res == KSI_INVALID_FORMAT && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testSigningInvalidAggrChainReturned(CuTest* tc){
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/nok_aggr_response-invalid-aggr-chain.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	unsigned char imprint[] = {0x01, 0xc5, 0xf3, 0x30, 0x84, 0x32, 0x8a, 0x04, 0xa4, 0xee, 0x5c, 0x75, 0xa9, 0xeb, 0x8c, 0x9a, 0xe0, 0x0c, 0x22, 0x14, 0xdf, 0x70, 0x4c, 0x7c, 0xf6, 0x8b, 0xb3, 0x09, 0x5c, 0xec, 0xbc, 0x71, 0xca};


	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, imprint, sizeof(imprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signature should not be created with invalid aggregation response.", res == KSI_VERIFICATION_FAILURE);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testSigningErrorResponse(CuTest *tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok_aggr_err_response-1.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signature should not be created due to server error.", res == KSI_SERVICE_INVALID_PAYLOAD && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testExtendingErrorResponse(CuTest *tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok_extend_err_response-1.tlv"
#define TEST_CRT_FILE           "resource/crt/mock.crt"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	KSI_PKITruststore *pki = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_CTX_getPKITruststore(ctx, &pki);
	CuAssert(tc, "Unable to get PKI Truststore.", res == KSI_OK && pki != NULL);

	res = KSI_PKITruststore_addLookupFile(pki, getFullResourcePath(TEST_CRT_FILE));
	CuAssert(tc, "Unable to add test certificate to truststore.", res == KSI_OK);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_Signature_extend(sig, ctx, NULL, &ext);
	CuAssert(tc, "Extend should fail with server error.", res == KSI_SERVICE_INVALID_PAYLOAD && ext == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_CRT_FILE
}

static void verifySignatureWithLevel(CuTest* tc, KSI_Signature *sig, int lvl,
		KSI_VerificationResultCode rCode, KSI_VerificationErrorCode eCode) {
	int res;
	KSI_VerificationContext verifier;
	KSI_PolicyVerificationResult *result = NULL;

	KSI_VerificationContext_init(&verifier, ctx);

	verifier.signature = sig;
	verifier.docAggrLevel = lvl;

	res = KSI_SignatureVerifier_verify(KSI_VERIFICATION_POLICY_GENERAL, &verifier, &result);
	CuAssert(tc, "Locally aggregated signature was not verifiable due to an error.", res == KSI_OK);
	CuAssert(tc, "Signature verification result mismatch.", result->resultCode == rCode && result->finalResult.errorCode == eCode);

	KSI_VerificationContext_clean(&verifier);
	KSI_PolicyVerificationResult_free(result);
}

static void testLocalAggregationSigning(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-local_aggr_lvl4_resp.tlv"
#define TEST_AGGR_LEVEL 4

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_Signature_signAggregated(ctx, hsh, TEST_AGGR_LEVEL, &sig);
	CuAssert(tc, "Unable to sign the hash.", res == KSI_OK && sig != NULL);

	res = KSI_verifySignature(ctx, sig);
	CuAssert(tc, "Locally aggregated signature was not verifiable due to an error.", res == KSI_OK);

	verifySignatureWithLevel(tc, sig, TEST_AGGR_LEVEL,     KSI_VER_RES_OK,   KSI_VER_ERR_NONE);
	verifySignatureWithLevel(tc, sig, TEST_AGGR_LEVEL - 1, KSI_VER_RES_OK,   KSI_VER_ERR_NONE);
	verifySignatureWithLevel(tc, sig, TEST_AGGR_LEVEL + 1, KSI_VER_RES_FAIL, KSI_VER_ERR_GEN_3);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_AGGR_LEVEL
}

static void testExtendExtended(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.2-extended.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
#define TEST_RES_SIGNATURE_FILE "resource/tlv/ok-sig-2014-04-30.1-extended.ksig"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	unsigned char *serialized = NULL;
	size_t serialized_len = 0;
	unsigned char expected[0x1ffff];
	size_t expected_len = 0;
	FILE *f = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Unable to extend the signature.", res == KSI_OK && ext != NULL);

	res = KSI_Signature_serialize(ext, &serialized, &serialized_len);
	CuAssert(tc, "Unable to serialize extended signature.", res == KSI_OK && serialized != NULL && serialized_len > 0);

	/* Read in the expected result. */
	f = fopen(getFullResourcePath(TEST_RES_SIGNATURE_FILE), "rb");
	CuAssert(tc, "Unable to read expected result file.", f != NULL);
	expected_len = (unsigned)fread(expected, 1, sizeof(expected), f);
	fclose(f);

	CuAssert(tc, "Expected result length mismatch.", expected_len == serialized_len);
	CuAssert(tc, "Unexpected extended signature.", !KSITest_memcmp(expected, serialized, expected_len));

	KSI_free(serialized);
	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
#undef TEST_RES_SIGNATURE_FILE
}

static void testCreateAggregated(CuTest *tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/test_create_aggregated_response.tlv"
	int res;
	const char data[] = "Test";
	const char clientStr[] = "Dummy";

	KSI_DataHash *docHash = NULL;
	KSI_MetaData *metaData = NULL;
	KSI_Utf8String *clientId = NULL;

	KSI_AggregationHashChain *chn = NULL;

	KSI_TreeBuilder *tb = NULL;
	KSI_TreeLeafHandle *leaf = NULL;

	unsigned char *raw = NULL;
	size_t raw_len = 0;

	KSI_Signature *sig = NULL;

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	/* Create the hash for the initial document. */
	res = KSI_DataHash_create(ctx, data, sizeof(data), KSI_HASHALG_SHA2_256, &docHash);
	CuAssert(tc, "Unable to create data hash.", res == KSI_OK && docHash != NULL);

	/* Create client id object. */
	res = KSI_Utf8String_new(ctx, clientStr, sizeof(clientStr), &clientId);
	CuAssert(tc, "Unable to create client id.", res == KSI_OK && clientId != NULL);

	/* Create the metadata object. */
	res = KSI_MetaData_new(ctx, &metaData);
	CuAssert(tc, "Unable to create metadata.", res == KSI_OK && metaData != NULL);

	res = KSI_MetaData_setClientId(metaData, clientId);
	CuAssert(tc, "Unable to set meta data client id.", res == KSI_OK);

	/* Create a tree builder. */
	res = KSI_TreeBuilder_new(ctx, KSI_HASHALG_SHA2_256, &tb);
	CuAssert(tc, "Unable to create tree builder.", res == KSI_OK && tb != NULL);

	/* Add the document hash as the first leaf. */
	res = KSI_TreeBuilder_addDataHash(tb, docHash, 0, &leaf);
	CuAssert(tc, "Unable to add leaf to the tree builder.", res == KSI_OK && leaf != NULL);

	res = KSI_TreeBuilder_addMetaData(tb, metaData, 0, NULL);
	CuAssert(tc, "Unable to add meta data to the tree builder.", res == KSI_OK);

	/* Finalize the tree. */
	res = KSI_TreeBuilder_close(tb);
	CuAssert(tc, "Unable to close the tree.", res == KSI_OK);

	/* Extract the aggregation hash chain. */
	res = KSI_TreeLeafHandle_getAggregationChain(leaf, &chn);
	CuAssert(tc, "Unable to extract the aggregation hash chain.", res == KSI_OK && chn != NULL);

	res = KSI_Signature_signAggregationChain(ctx, 0, chn, &sig);
	CuAssert(tc, "Unable to sign aggregation chain.", res == KSI_OK && sig != NULL);

	/* Serialize the signature. */
	res = KSI_Signature_serialize(sig, &raw, &raw_len);
	CuAssert(tc, "Unable to serialize signature.", res == KSI_OK && raw != NULL && raw_len > 0);
	KSI_LOG_logBlob(ctx, KSI_LOG_DEBUG, "Serialized", raw, raw_len);

	KSI_Signature_free(sig);
	sig = NULL;

	/* Parse the signature. */
	res = KSI_Signature_parse(ctx, raw, raw_len, &sig);
	CuAssert(tc, "Unable to parse the serialized signature.", res == KSI_OK && sig != NULL);

	KSI_AggregationHashChain_free(chn);
	KSI_TreeBuilder_free(tb);
	KSI_TreeLeafHandle_free(leaf);
	KSI_DataHash_free(docHash);
	KSI_MetaData_free(metaData);
	KSI_Signature_free(sig);
	KSI_free(raw);
	KSI_Utf8String_free(clientId);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testExtendingBackgroundVerification(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/all-wrong-hash-chains-in-signature.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/all-wrong-hash-chains-in-signature-extend_response.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_Signature_extendTo(sig, ctx, NULL, &ext);
	CuAssert(tc, "Wrong answer from extender should not be tolerated.", res == KSI_INCOMPATIBLE_HASH_CHAIN && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testSigningBackgroundVerification(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/aggr-response-no-cal-auth-and-invalid-cal.tlv"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/extender-response-no-cal-auth-and-invalid-cal.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_DataHash *hsh = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSITest_DataHash_fromStr(ctx, "0111a700b0c8066c47ecba05ed37bc14dcadb238552d86c659342d1d7e87b8772d", &hsh);
	CuAssert(tc, "Unable to get hash from string.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_Signature_signWithPolicy(ctx, hsh, KSI_VERIFICATION_POLICY_CALENDAR_BASED, NULL, &sig);
	CuAssert(tc, "Unable to sign hash.", res == KSI_VERIFICATION_FAILURE && sig == NULL);

	KSI_Signature_free(sig);
	KSI_DataHash_free(hsh);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testSigningBackgroundVerification_verifyResult(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/aggr-response-no-cal-auth-and-invalid-cal.tlv"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/extender-response-no-cal-auth-and-invalid-cal.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_DataHash *hsh = NULL;
	KSI_PolicyVerificationResult *result = NULL;
	KSI_VerificationContext context;

	KSI_LOG_debug(ctx, "%s", __FUNCTION__);

	KSI_ERR_clearErrors(ctx);

	res = KSITest_DataHash_fromStr(ctx, "0111a700b0c8066c47ecba05ed37bc14dcadb238552d86c659342d1d7e87b8772d", &hsh);
	CuAssert(tc, "Unable to get hash from string.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_VerificationContext_init(&context, ctx);
	CuAssert(tc, "Verification context creation failed.", res == KSI_OK);

	res = KSI_Signature_signWithPolicy(ctx, hsh, KSI_VERIFICATION_POLICY_EMPTY, &context, &sig);
	CuAssert(tc, "Unable to sign hash.", sig != NULL);
	context.signature = sig;
	context.extendingAllowed = 1;

	res = KSI_SignatureVerifier_verify(KSI_VERIFICATION_POLICY_CALENDAR_BASED, &context, &result);
	CuAssert(tc, "Policy verification failed.", res == KSI_OK);
	CuAssert(tc, "Unexpected verification result.", result->finalResult.resultCode == KSI_VER_RES_FAIL);
	CuAssert(tc, "Unexpected verification error code.", result->finalResult.errorCode == KSI_VER_ERR_CAL_4);

	KSI_PolicyVerificationResult_free(result);
	KSI_VerificationContext_clean(&context);
	KSI_Signature_free(sig);
	KSI_DataHash_free(hsh);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testNonCriticalPayloadElementInAggregationResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-non-critical-unknown-payload.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have not failed.", res == KSI_OK && sig != NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}


static void testCriticalPayloadElementInAggregationResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-critical-unknown-payload.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should fail with invalid format due to critical unknown element in PDU.", res == KSI_INVALID_FORMAT && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}


static void testFlagsInAggregationResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-with-flags.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have not failed.", res == KSI_OK && sig != NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}


static void testErrorStatusWithSignatureElementsInResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-with-status-301.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have failed with service upstream timeout error.", res == KSI_SERVICE_UPSTREAM_TIMEOUT && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testNonCriticalPayloadElementInExtenderResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-non-critical-payload-element.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have not failed.", res == KSI_OK && ext != NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testCriticalPayloadElementInExtenderResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-critical-payload-element.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should fail invalid fromat.", res == KSI_INVALID_FORMAT && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testFlagsInExtenderResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-with-flags.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have not failed.", res == KSI_OK && ext != NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testErrorStatusWithCalendarHashChainInResponse(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-with-status-301.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should fail with service upstream timeout error.", res == KSI_SERVICE_UPSTREAM_TIMEOUT && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingResponseWithConf(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-with-conf.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have not failed.", res == KSI_OK && ext != NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingResponseWithConfCallback(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-with-conf.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	KSI_Config *conf = NULL;
	KSI_Integer *intVal = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_CTX_setOption(ctx, KSI_OPT_EXT_CONF_RECEIVED_CALLBACK, Test_ConfigCallback);
	CuAssert(tc, "Unable to set extender conf callback.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have not failed.", res == KSI_OK && ext != NULL);

	CuAssert(tc, "Conf callback has not been invoked.", callbackCalls > 0);

	conf = callbackConf;
	CuAssert(tc, "Push conf is not set.", conf != NULL);

	res = KSI_Config_getMaxRequests(conf, &intVal);
	CuAssert(tc, "Conf max requests value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 4);
	intVal = NULL;

	res = KSI_Config_getCalendarFirstTime(conf, &intVal);
	CuAssert(tc, "Conf calendar time value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 1398866256);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);
	KSI_Config_free(conf);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingResponseWithConfAndAck(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-with-conf-and-ack.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed due to unknow critical element [05] in PDU.", res == KSI_INVALID_FORMAT && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtenderConfRequestConfWithExtResponse(CuTest* tc) {
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-with-conf.tlv"

	int res;
	KSI_Config *conf = NULL;
	KSI_Integer *intVal = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_receiveExtenderConfig(ctx, &conf);
	CuAssert(tc, "Conf request should have not failed.", res == KSI_OK && conf != NULL);

	CuAssert(tc, "Conf callback should have not been called.", callbackCalls == 0 && callbackConf == NULL);

	res = KSI_Config_getMaxRequests(conf, &intVal);
	CuAssert(tc, "Conf max requests value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 4);
	intVal = NULL;

	res = KSI_Config_getCalendarFirstTime(conf, &intVal);
	CuAssert(tc, "Conf calendar time value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 1398866256);

	KSI_Config_free(conf);

#undef TEST_EXT_RESPONSE_FILE
}

static void testAggregationResponseWithConfAndAck(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-with-conf-and-ack.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have not failed.", res == KSI_OK && sig != NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testAggregationResponseWithConfCallback(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-with-conf-and-ack.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	KSI_Config *conf = NULL;
	KSI_Integer *intVal = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_CTX_setOption(ctx, KSI_OPT_AGGR_CONF_RECEIVED_CALLBACK, Test_ConfigCallback);
	CuAssert(tc, "Unable to set extender conf callback.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have not failed.", res == KSI_OK && sig != NULL);

	CuAssert(tc, "Conf callback has not been invoked.", callbackCalls > 0);

	conf = callbackConf;
	CuAssert(tc, "Push conf is not set.", conf != NULL);

	res = KSI_Config_getMaxRequests(conf, &intVal);
	CuAssert(tc, "Conf max requests value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 4);
	intVal = NULL;

	res = KSI_Config_getAggrPeriod(conf, &intVal);
	CuAssert(tc, "Conf aggregation period value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 3);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
	KSI_Config_free(conf);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testAggreConfRequestConfWithSig(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-with-conf-and-ack.tlv"
	int res;
	KSI_Config *conf = NULL;
	KSI_Integer *intVal = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_receiveAggregatorConfig(ctx, &conf);
	CuAssert(tc, "Conf request should have not failed.", res == KSI_OK && conf != NULL);

	CuAssert(tc, "Conf callback should have not been called.", callbackCalls == 0 && callbackConf == NULL);

	res = KSI_Config_getMaxRequests(conf, &intVal);
	CuAssert(tc, "Conf max requests value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 4);
	intVal = NULL;

	res = KSI_Config_getAggrPeriod(conf, &intVal);
	CuAssert(tc, "Conf aggregation period value mismatch.", res == KSI_OK && KSI_Integer_getUInt64(intVal) == 3);

	KSI_Config_free(conf);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testAggregationResponseWithInvalidId(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-wrong-id.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have failed because of invalid request ID.", res == KSI_REQUEST_ID_MISMATCH && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
#undef TEST_PDU_VERSION
}


static void testExtendingResponseWithInvalidId(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-wrong-id.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed because of invalid request ID.", res == KSI_REQUEST_ID_MISMATCH && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingResponseMultiplePayload(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-multi-payload.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed with invalid format.", res == KSI_INVALID_FORMAT && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testExtendingResponseWithResponseAndErrorPayload(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response-response-with-error-payload.tlv"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set extend response from file.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed with invalid request error.", res == KSI_SERVICE_INVALID_REQUEST && ext == NULL);

	KSI_Signature_free(sig);
	KSI_Signature_free(ext);

#undef TEST_SIGNATURE_FILE
#undef TEST_EXT_RESPONSE_FILE
}

static void testAggregationResponseMultiplePayload(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-multi-payload.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have failed with invalid response format.", res == KSI_INVALID_FORMAT && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testAggregationResponseWithResponseAndErrorPayload(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/ok-sig-2014-07-01.1-aggr_response-response-with-error-payload.tlv"
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Signing should have failed with invalid request error.", res == KSI_SERVICE_INVALID_REQUEST && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);

#undef TEST_AGGR_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testSigningWithLevel(CuTest* tc) {
#define TEST_AGGR_RESPONSE_FILE "resource/tlv/v2/signing-request-with-level-response.tlv"

	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;
	KSI_VerificationContext context;
	KSI_PolicyVerificationResult *result = NULL;
	int level = 3;
	KSI_AggregationHashChain *aggr = NULL;
	KSI_LIST(KSI_HashChainLink) *chain = NULL;
	KSI_HashChainLink *link = NULL;
	KSI_Integer *sigLvl = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSITest_DataHash_fromStr(ctx, "016338656636643537616332386431623465393561353133393539663566636464", &hsh);
	CuAssert(tc, "Unable to create data hash object.", res == KSI_OK && hsh != NULL);

	res = KSI_CTX_setAggregator(ctx, getFullResourcePathUri(TEST_AGGR_RESPONSE_FILE), TEST_USER, TEST_PASS);
	CuAssert(tc, "Unable to set aggregator file URI.", res == KSI_OK);

	res = KSI_Signature_signAggregated(ctx, hsh, level, &sig);
	CuAssert(tc, "Unable to sign the hash with level.", res == KSI_OK && sig != NULL);

	res = KSI_AggregationHashChainList_elementAt(sig->aggregationChainList, 0, &aggr);
	CuAssert(tc, "Unable to get aggregation hash chain.", res == KSI_OK && aggr != NULL);

	res = KSI_AggregationHashChain_getChain(aggr, &chain);
	CuAssert(tc, "Unable to get aggregation hash chain links.", res == KSI_OK && chain != NULL);

	res = KSI_HashChainLinkList_elementAt(chain, 0, &link);
	CuAssert(tc, "Unable to get first chain link.", res == KSI_OK && link != NULL);

	res = KSI_HashChainLink_getLevelCorrection(link, &sigLvl);
	CuAssert(tc, "Unable to get level corrector value.", res == KSI_OK && sigLvl != NULL);

	CuAssert(tc, "Signature first link level does not match with signing level.", level == KSI_Integer_getUInt64(sigLvl));

	res = KSI_VerificationContext_init(&context, ctx);
	CuAssert(tc, "Unable to init verification context.", res == KSI_OK);

	context.signature = sig;

	res = KSI_SignatureVerifier_verify(KSI_VERIFICATION_POLICY_INTERNAL, &context, &result);
	CuAssert(tc, "Unable to verify created signature.", res == KSI_OK && result != NULL);
	CuAssert(tc, "Unexpected verification result.", result->finalResult.resultCode == KSI_VER_RES_OK);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
	KSI_PolicyVerificationResult_free(result);

#undef TEST_AGGR_RESPONSE_FILE
}

static void testSigning_hmacAlgorithmDeprecated(CuTest* tc) {
	int res;
	KSI_DataHash *hsh = NULL;
	KSI_Signature *sig = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_DataHash_fromImprint(ctx, mockImprint, sizeof(mockImprint), &hsh);
	CuAssert(tc, "Unable to create data hash object from raw imprint.", res == KSI_OK && hsh != NULL);


	res = KSI_CTX_setAggregatorHmacAlgorithm(ctx, KSI_HASHALG_SHA1);
	CuAssert(tc, "Unable to set aggregator HMAC algorithm.", res == KSI_OK);

	res = KSI_createSignature(ctx, hsh, &sig);
	CuAssert(tc, "Unable to sign the hash.", res == KSI_UNTRUSTED_HASH_ALGORITHM && sig == NULL);

	KSI_DataHash_free(hsh);
	KSI_Signature_free(sig);
}


static void testExtending_hmacAlgorithmDeprecated(CuTest* tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/ok-sig-2014-04-30.1.ksig"

	int res;
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;

	KSI_ERR_clearErrors(ctx);

	res = KSI_Signature_fromFile(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), &sig);
	CuAssert(tc, "Unable to load signature from file.", res == KSI_OK && sig != NULL);

	res = KSI_CTX_setExtenderHmacAlgorithm(ctx, KSI_HASHALG_SHA1);
	CuAssert(tc, "Unable to set aggregator HMAC algorithm.", res == KSI_OK);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "The extending of the signature should not succeed.", res == KSI_UNTRUSTED_HASH_ALGORITHM && ext == NULL);

	KSI_Signature_free(sig);
#undef TEST_SIGNATURE_FILE
}

static void testExtendWithExtraRightLink(CuTest *tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/nok-sig-2014-04-30.1-extra-right-link.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	int res;

	/* The signature is broken, but would be fixed after the extending process, if the calendar hash chain compatibility would not be checked. */
	KSI_Signature_fromFileWithPolicy(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), KSI_VERIFICATION_POLICY_EMPTY, NULL, &sig);
	KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed as the original calendar hash chain has an extra right-link.", res == KSI_INCOMPATIBLE_HASH_CHAIN && ext == NULL);

	KSI_Signature_free(sig);

#undef TEST_EXT_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testExtendWithMissingRightLink(CuTest *tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/nok-sig-2014-04-30.1-missing-right-link.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	int res;

	/* The signature is broken, but would be fixed after the extending process, if the calendar hash chain compatibility would not be checked. */
	KSI_Signature_fromFileWithPolicy(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), KSI_VERIFICATION_POLICY_EMPTY, NULL, &sig);
	KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed as the original calendar hash chain has a missing right-link.", res == KSI_INCOMPATIBLE_HASH_CHAIN && ext == NULL);

	KSI_Signature_free(sig);

#undef TEST_EXT_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}

static void testExtendWithWrongRightLink(CuTest *tc) {
#define TEST_SIGNATURE_FILE     "resource/tlv/nok-sig-2014-04-30.1-wrong-right-link.ksig"
#define TEST_EXT_RESPONSE_FILE  "resource/tlv/v2/ok-sig-2014-04-30.1-extend_response.tlv"
	KSI_Signature *sig = NULL;
	KSI_Signature *ext = NULL;
	int res;

	/* The signature is broken, but would be fixed after the extending process, if the calendar hash chain compatibility would not be checked. */
	KSI_Signature_fromFileWithPolicy(ctx, getFullResourcePath(TEST_SIGNATURE_FILE), KSI_VERIFICATION_POLICY_EMPTY, NULL, &sig);
	KSI_CTX_setExtender(ctx, getFullResourcePathUri(TEST_EXT_RESPONSE_FILE), TEST_USER, TEST_PASS);

	res = KSI_extendSignature(ctx, sig, &ext);
	CuAssert(tc, "Signature extending should have failed as the original calendar hash chain has a missing right-link.", res == KSI_INCOMPATIBLE_HASH_CHAIN && ext == NULL);

	KSI_Signature_free(sig);

#undef TEST_EXT_RESPONSE_FILE
#undef TEST_SIGNATURE_FILE
}


CuSuite* KSITest_NetPduV2_getSuite(void) {
	CuSuite* suite = CuSuiteNew();

	suite->preTest = preTest;
	suite->postTest = postTest;

	SUITE_ADD_TEST(suite, testSigning);
	SUITE_ADD_TEST(suite, testSigning_hmacAlgorithmSha512);
	SUITE_ADD_TEST(suite, testSigning_hmacAlgorithmMismatch);
	SUITE_ADD_TEST(suite, testSigningHeaderNotFirst);
	SUITE_ADD_TEST(suite, testSigningHmacNotLast);
	SUITE_ADD_TEST(suite, testSigningResponsePduV1);
	SUITE_ADD_TEST(suite, testSigningWrongResponse);
	SUITE_ADD_TEST(suite, testAggreAuthFailure);
	SUITE_ADD_TEST(suite, testExtending);
	SUITE_ADD_TEST(suite, testExtending_hmacAlgorithmSha512);
	SUITE_ADD_TEST(suite, testExtending_hmacAlgorithmMismatch);
	SUITE_ADD_TEST(suite, testExtendingHeaderNotFirst);
	SUITE_ADD_TEST(suite, testExtendingHmacNotLast);
	SUITE_ADD_TEST(suite, testExtendingResponsePduV1);
	SUITE_ADD_TEST(suite, testExtendTo);
	SUITE_ADD_TEST(suite, testExtendSigNoCalChain);
	SUITE_ADD_TEST(suite, testExtenderWrongData);
	SUITE_ADD_TEST(suite, testExtAuthFailure);
	SUITE_ADD_TEST(suite, testExtendingWithoutPublication);
	SUITE_ADD_TEST(suite, testExtendingToNULL);
	SUITE_ADD_TEST(suite, testSigningInvalidResponse);
	SUITE_ADD_TEST(suite, testSigningInvalidAggrChainReturned);
	SUITE_ADD_TEST(suite, testSigningErrorResponse);
	SUITE_ADD_TEST(suite, testExtendingErrorResponse);
	SUITE_ADD_TEST(suite, testLocalAggregationSigning);
	SUITE_ADD_TEST(suite, testExtendExtended);
	SUITE_ADD_TEST(suite, testCreateAggregated);
	SUITE_ADD_TEST(suite, testExtendingBackgroundVerification);
	SUITE_ADD_TEST(suite, testSigningBackgroundVerification);
	SUITE_ADD_TEST(suite, testSigningBackgroundVerification_verifyResult);
	SUITE_ADD_TEST(suite, testNonCriticalPayloadElementInAggregationResponse);
	SUITE_ADD_TEST(suite, testCriticalPayloadElementInAggregationResponse);
	SUITE_ADD_TEST(suite, testFlagsInAggregationResponse);
	SUITE_ADD_TEST(suite, testErrorStatusWithSignatureElementsInResponse);
	SUITE_ADD_TEST(suite, testNonCriticalPayloadElementInExtenderResponse);
	SUITE_ADD_TEST(suite, testCriticalPayloadElementInExtenderResponse);
	SUITE_ADD_TEST(suite, testFlagsInExtenderResponse);
	SUITE_ADD_TEST(suite, testErrorStatusWithCalendarHashChainInResponse);
	SUITE_ADD_TEST(suite, testExtendingResponseWithConf);
	SUITE_ADD_TEST(suite, testExtendingResponseWithConfCallback);
	SUITE_ADD_TEST(suite, testExtendingResponseWithConfAndAck);
	SUITE_ADD_TEST(suite, testExtenderConfRequestConfWithExtResponse);
	SUITE_ADD_TEST(suite, testAggregationResponseWithConfAndAck);
	SUITE_ADD_TEST(suite, testAggregationResponseWithConfCallback);
	SUITE_ADD_TEST(suite, testAggreConfRequestConfWithSig);
	SUITE_ADD_TEST(suite, testAggregationResponseWithInvalidId);
	SUITE_ADD_TEST(suite, testExtendingResponseWithInvalidId);
	SUITE_ADD_TEST(suite, testExtendingResponseMultiplePayload);
	SUITE_ADD_TEST(suite, testExtendingResponseWithResponseAndErrorPayload);
	SUITE_ADD_TEST(suite, testAggregationResponseMultiplePayload);
	SUITE_ADD_TEST(suite, testAggregationResponseWithResponseAndErrorPayload);
	SUITE_ADD_TEST(suite, testSigningWithLevel);
	SUITE_ADD_TEST(suite, testSigning_hmacAlgorithmDeprecated);
	SUITE_ADD_TEST(suite, testExtending_hmacAlgorithmDeprecated);
	SUITE_ADD_TEST(suite, testExtendWithExtraRightLink);
	SUITE_ADD_TEST(suite, testExtendWithMissingRightLink);
	SUITE_ADD_TEST(suite, testExtendWithWrongRightLink);

	return suite;
}

