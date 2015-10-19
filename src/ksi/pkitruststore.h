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

#ifndef KSI_TRUSTSTORE_H_
#define KSI_TRUSTSTORE_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup pkitruststore PKI Truststore
 * This module is used for PKI operations.
 *
 * The main components are:
 * 1) #KSI_PKITruststore - contains and verifies certificates.
 * 2) #KSI_PKICertificate - an implementation independent PKI certificate object.
 * 3) #KSI_PKISignature - an implementation independent PKI signature object.
 * @{
 */

	/**
	 * PKI Truststore constructor. If <tt>\c setDefaults == 1</tt>, the truststore is initiated
	 * with default settings, otherwise a blank truststore is created.
	 * \param[in]	ctx				KSI context.
	 * \param[in]	setDefaults		With or without default system settings.
	 * \param[out]	store			Pointer to the receiving pointer.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKITruststore_new(KSI_CTX *ctx, int setDefaults, KSI_PKITruststore **store);

	/**
	 * Destructor for the PKI Truststore object.
	 * \param[in]	store			PKI Truststore object.
	 */
	void KSI_PKITruststore_free(KSI_PKITruststore *store);

	/**
	 * PKI Certificate constructor.
	 * \param[in]	ctx			KSI context.
	 * \param[in]	der			Pointer to DER encoded certificate.
	 * \param[in]	der_len		Length of the certificate.
	 * \param[out]	cert		Pointer to the receiving pointer.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKICertificate_new(KSI_CTX *ctx, const void *der, size_t der_len, KSI_PKICertificate **cert);

	/**
	 * Destructor for the PKI Certificate object.
	 * \param[in]	cert		PKI Certificate object.
	 */
	void KSI_PKICertificate_free(KSI_PKICertificate *cert);

	KSI_DEFINE_FN_FROM_TLV(KSI_PKICertificate);
	KSI_DEFINE_FN_TO_TLV(KSI_PKICertificate);

	/**
	 * Function to serialise the KSI Certificate object as DER.
	 * \param[in]	cert		PKI Certificate object.
	 * \param[out]	raw			Pointer to the receiving pointer.
	 * \param[out]	raw_len		Serialized length.
	 */
	int KSI_PKICertificate_serialize(KSI_PKICertificate *cert, unsigned char **raw, size_t *raw_len);

	/**
	 * Constructor for PKI Signature object.
	 * \param[in]	ctx			KSI context.
	 * \param[in]	raw			PKCS7 encoded signature.
	 * \param[in]	raw_len		Signature len.
	 * \param[out]	signature	Pointer to the receiving pointer.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
	 */
	int KSI_PKISignature_new(KSI_CTX *ctx, const void *raw, size_t raw_len, KSI_PKISignature **signature);

	/**
	 * Destructor for the PKI Signature object.
	 * \param[in]	sig			PKI Signature object.
	 */
	void KSI_PKISignature_free(KSI_PKISignature *sig);

	/**
	 * Extracts a PKI certificate from PKI signature.
	 * \param[in]	signature			PKI Signature object.
	 * \param[out]	cert				Pointer to the receiving pointer.
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
     */
	int KSI_PKISignature_extractCertificate(const KSI_PKISignature *signature, KSI_PKICertificate **cert);

	
	/**
	 * Serializes the #KSI_PKISignature object.
	 * \param[in]	sig			Pointer to the PKI signature.
	 * \param[out]	raw			Pointer to the receiving pointer.
	 * \param[out]	raw_len		Serialized value length.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
	 */
	int KSI_PKISignature_serialize(KSI_PKISignature *sig, unsigned char **raw, size_t *raw_len);

	KSI_DEFINE_FN_FROM_TLV(KSI_PKISignature);
	KSI_DEFINE_FN_TO_TLV(KSI_PKISignature);

	/**
	 * Function for verifying a raw PKCS#1 signature. TODO! - explain in detail!
	 * \param[in]	ctx				KSI context.
	 * \param[in]	data			Pointer to input data.
	 * \param[in]	data_len		Input data len.
	 * \param[in]	algoOid			Algorithm OID used to sign the input data.
	 * \param[in]	signature		Pointer to the raw PKCS#1 signature.
	 * \param[in]	signature_len	Signature length.
	 * \param[in]	cert			PKI Certificate object.
	 *
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKITruststore_verifyRawSignature(KSI_CTX *ctx, const unsigned char *data, size_t data_len, const char *algoOid, const unsigned char *signature, size_t signature_len, const KSI_PKICertificate *cert);

	/**
	 * Function for verifying the data with PKI Signature.
	 * \param[in]	pki			PKI Truststore.
	 * \param[in]	data		Pointer to signed data.
	 * \param[in]	data_len	Length of the signed data.
	 * \param[in]	signature	PKI signature object.
	 *
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKITruststore_verifySignature(KSI_PKITruststore *pki, const unsigned char *data, size_t data_len, const KSI_PKISignature *signature);

	/**
	 * Add trusted certificate lookup file.
	 * \param[in]	store		PKI truststore.
	 * \param[in]	path		Null-terminated sting path to the file containing trusted certificates.
	 *
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKITruststore_addLookupFile(KSI_PKITruststore *store, const char *path);

	/**
	 * Add trusted certificate lookup directory.
	 * \param[in]	store		PKI truststore.
	 * \param[in]	path		Null-terminated sting path to the directory containing trusted certificates..
	 *
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKITruststore_addLookupDir(KSI_PKITruststore *store, const char *path);
	 
	/**
	 * Retrieves the beginning of PKI certificates validity period as the number of seconds
	 * since 1970-01-01 00:00:00 UTC.
     * \param[in]	cert	input certificate object.
     * \param[out]	time	retrieved time value.
	 * 	
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKICertificate_getValidityNotBefore(const KSI_PKICertificate *cert, KSI_uint64_t *time);

	/**
	 * Retrieves the end of PKI certificates validity period as the number of seconds
	 * since 1970-01-01 00:00:00 UTC.
     * \param[in]	cert	input certificate object.
     * \param[out]	time	retrieved time value.
	 * 	
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_PKICertificate_getValidityNotAfter(const KSI_PKICertificate *cert, KSI_uint64_t *time);

	KSI_DEFINE_GET_CTX(KSI_PKICertificate);
	
	/**
	 * Creates a string representation of PKI Certificate issuer name.
     * \param[in]	cert		input certificate object.
     * \param[out]	buf			pointer to the receiving buffer.
     * \param[in]	buf_len		length of the receiving buffer.
     * \return buf if successful, NULL otherwise;
     */
	char* KSI_PKICertificate_issuerToString(const KSI_PKICertificate *cert, char *buf, size_t buf_len);

	/**
	 * Creates a string representation of PKI Certificate subject name.
     * \param[in]	cert		input certificate object.
     * \param[out]	buf			pointer to the receiving buffer.
     * \param[in]	buf_len		length of the receiving buffer.
     * \return buf if successful, NULL otherwise;
     */
	char* KSI_PKICertificate_subjectToString(const KSI_PKICertificate *cert, char *buf, size_t buf_len);
	
	/**
	 * Creates a string representation of PKI Certificate issuer OID object.
     * \param[in]	cert		input certificate object.
	 * \param[in]	OID			pointer to NULL terminated OID string.
     * \param[out]	buf			pointer to the receiving buffer.
     * \param[in]	buf_len		length of the receiving buffer.
     * \return buf if successful, NULL otherwise;
     */
	char* KSI_PKICertificate_issuerOIDToString(KSI_PKICertificate *cert, const char *OID, char *buf, size_t buf_len);
	
	/**
	 * Creates a string representation of PKI Certificate subject OID object.
     * \param[in]	cert		input certificate object.
	 * \param[in]	OID			pointer to NULL terminated OID string.
     * \param[out]	buf			pointer to the receiving buffer.
     * \param[in]	buf_len		length of the receiving buffer.
     * \return buf if successful, NULL otherwise;
     */
	char* KSI_PKICertificate_subjectOIDToString(KSI_PKICertificate *cert, const char *OID, char *buf, size_t buf_len);
	
	/**
	 * Calculates the CRC32 of PKI certificates that can be used as the certificate ID in the publications file.
     * \param[in]	cert		input certificate object.
     * \param[out]	crc			pointer to receiving pointer. 
	 * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 * \note Function caller must free the \c #KSI_OctetString object.
     */
	int KSI_PKICertificate_calculateCRC32(KSI_PKICertificate *cert, KSI_OctetString **crc);
	
	/**
	 * Getter for PKI Certificate serial number.
     * \param[in]	cert			input certificate object.
     * \param[out]	serial_number	pointer to receiving long integer.
     * \return status code (\c #KSI_OK, when operation succeeded, otherwise an
	 * error code).
     */
	int KSI_PKICertificate_getSerialNumber(const KSI_PKICertificate *cert, unsigned long *serial_number);
	
	/**
	 * Creates a string representation of a PKI Certificate.
	 *
     * \param[in]	cert		input certificate object.
     * \param[out]	buf			pointer to the receiving buffer.
     * \param[in]	buf_len		length of the receiving buffer.
     * \return buf if successful, NULL otherwise;
     */
	char* KSI_PKICertificate_toString(KSI_PKICertificate *cert, char *buf, size_t buf_len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* KSI_TRUSTSTORE_H_ */
