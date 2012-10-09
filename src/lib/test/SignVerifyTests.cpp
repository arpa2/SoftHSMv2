/* $Id$ */

/*
 * Copyright (c) 2012 SURFnet
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*****************************************************************************
 SignVerifyTests.cpp

 Contains test cases for:
	 C_SignInit
	 C_Sign
	 C_SignUpdate
	 C_SignFinal
	 C_VerifyInit
	 C_Verify
	 C_VerifyUpdate
	 C_VerifyFinal

 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <cppunit/extensions/HelperMacros.h>
#include "SignVerifyTests.h"
#include "testconfig.h"

// CKA_TOKEN
const CK_BBOOL ON_TOKEN = CK_TRUE;
const CK_BBOOL IN_SESSION = CK_FALSE;

// CKA_PRIVATE
const CK_BBOOL IS_PRIVATE = CK_TRUE;
const CK_BBOOL IS_PUBLIC = CK_FALSE;


CPPUNIT_TEST_SUITE_REGISTRATION(SignVerifyTests);

void SignVerifyTests::setUp()
{
//    printf("\nSignVerifyTests\n");

	setenv("SOFTHSM2_CONF", "./softhsm2.conf", 1);

	CK_RV rv;
	CK_UTF8CHAR pin[] = SLOT_0_USER1_PIN;
	CK_ULONG pinLength = sizeof(pin) - 1;
	CK_UTF8CHAR sopin[] = SLOT_0_SO1_PIN;
	CK_ULONG sopinLength = sizeof(sopin) - 1;
	CK_SESSION_HANDLE hSession;

	CK_UTF8CHAR label[32];
	memset(label, ' ', 32);
	memcpy(label, "token1", strlen("token1"));

	// (Re)initialize the token
	rv = C_Initialize(NULL_PTR);
	CPPUNIT_ASSERT(rv == CKR_OK);
	rv = C_InitToken(SLOT_INIT_TOKEN, sopin,sopinLength, label);
	CPPUNIT_ASSERT(rv == CKR_OK);

	// Open session
	rv = C_OpenSession(SLOT_INIT_TOKEN, CKF_SERIAL_SESSION | CKF_RW_SESSION, NULL_PTR, NULL_PTR, &hSession);
	CPPUNIT_ASSERT(rv == CKR_OK);

	// Login SO
	rv = C_Login(hSession,CKU_SO, sopin, sopinLength);
	CPPUNIT_ASSERT(rv == CKR_OK);

	// Initialize the user pin
	rv = C_InitPIN(hSession, pin, pinLength);
	CPPUNIT_ASSERT(rv == CKR_OK);
}

void SignVerifyTests::tearDown()
{
	C_Finalize(NULL_PTR);
}

CK_RV SignVerifyTests::generateRsaKeyPair(CK_SESSION_HANDLE hSession, CK_BBOOL bTokenPuk, CK_BBOOL bPrivatePuk, CK_BBOOL bTokenPrk, CK_BBOOL bPrivatePrk, CK_OBJECT_HANDLE &hPuk, CK_OBJECT_HANDLE &hPrk)
{
	CK_MECHANISM mechanism = { CKM_RSA_PKCS_KEY_PAIR_GEN, NULL_PTR, 0 };
	CK_ULONG bits = 1536;
	CK_BYTE pubExp[] = {0x01, 0x00, 0x01};
	CK_BYTE subject[] = { 0x12, 0x34 }; // dummy
	CK_BYTE id[] = { 123 } ; // dummy
	CK_BBOOL bFalse = CK_FALSE;
	CK_BBOOL bTrue = CK_TRUE;
	CK_ATTRIBUTE pukAttribs[] = {
		{ CKA_TOKEN, &bTokenPuk, sizeof(bTokenPuk) },
		{ CKA_PRIVATE, &bPrivatePuk, sizeof(bPrivatePuk) },
		{ CKA_ENCRYPT, &bFalse, sizeof(bFalse) },
		{ CKA_VERIFY, &bTrue, sizeof(bTrue) },
		{ CKA_WRAP, &bFalse, sizeof(bFalse) },
		{ CKA_MODULUS_BITS, &bits, sizeof(bits) },
		{ CKA_PUBLIC_EXPONENT, &pubExp[0], sizeof(pubExp) }
	};
	CK_ATTRIBUTE prkAttribs[] = {
		{ CKA_TOKEN, &bTokenPrk, sizeof(bTokenPrk) },
		{ CKA_PRIVATE, &bPrivatePrk, sizeof(bPrivatePrk) },
		{ CKA_SUBJECT, &subject[0], sizeof(subject) },
		{ CKA_ID, &id[0], sizeof(id) },
		{ CKA_SENSITIVE, &bTrue, sizeof(bTrue) },
		{ CKA_DECRYPT, &bFalse, sizeof(bFalse) },
		{ CKA_SIGN, &bTrue, sizeof(bTrue) },
		{ CKA_UNWRAP, &bFalse, sizeof(bFalse) }
	};

	hPuk = CK_INVALID_HANDLE;
	hPrk = CK_INVALID_HANDLE;
	return C_GenerateKeyPair(hSession, &mechanism,
							 pukAttribs, sizeof(pukAttribs)/sizeof(CK_ATTRIBUTE),
							 prkAttribs, sizeof(prkAttribs)/sizeof(CK_ATTRIBUTE),
							 &hPuk, &hPrk);
}

void SignVerifyTests::rsaPkcsSignVerify(CK_MECHANISM_TYPE mechanismType, CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hPublicKey, CK_OBJECT_HANDLE hPrivateKey)
{
	CK_RV rv;
	CK_MECHANISM mechanism = { mechanismType, NULL_PTR, 0 };
	CK_BYTE data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,0x0C, 0x0D, 0x0F };
	CK_BYTE signature[256];
	CK_ULONG ulSignatureLen = 0;

	rv = C_SignInit(hSession,&mechanism,hPrivateKey);
	CPPUNIT_ASSERT(rv==CKR_OK);

	ulSignatureLen = sizeof(signature);
	rv = C_Sign(hSession,data,sizeof(data),signature,&ulSignatureLen);
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv = C_VerifyInit(hSession,&mechanism,hPublicKey);
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv = C_Verify(hSession,data,sizeof(data),signature,ulSignatureLen);
	CPPUNIT_ASSERT(rv==CKR_OK);
}

void SignVerifyTests::digestRsaPkcsSignVerify(CK_MECHANISM_TYPE mechanismType, CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hPublicKey, CK_OBJECT_HANDLE hPrivateKey)
{
	CK_RV rv;
	CK_MECHANISM mechanism = { mechanismType, NULL_PTR, 0 };
	CK_BYTE data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,0x0C, 0x0D, 0x0F };
	CK_BYTE signature[256];
	CK_ULONG ulSignatureLen = 0;

	rv = C_SignInit(hSession,&mechanism,hPrivateKey);
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv =C_SignUpdate(hSession,data,sizeof(data));
	CPPUNIT_ASSERT(rv==CKR_OK);

	ulSignatureLen = sizeof(signature);
	rv =C_SignFinal(hSession,signature,&ulSignatureLen);
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv = C_VerifyInit(hSession,&mechanism,hPublicKey);
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv = C_VerifyUpdate(hSession,data,sizeof(data));
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv = C_VerifyFinal(hSession,signature,ulSignatureLen);
	CPPUNIT_ASSERT(rv==CKR_OK);

	// verify again, but now change the input that is being signed.
	rv = C_VerifyInit(hSession,&mechanism,hPublicKey);
	CPPUNIT_ASSERT(rv==CKR_OK);

	data[0] = 0xff;
	rv = C_VerifyUpdate(hSession,data,sizeof(data));
	CPPUNIT_ASSERT(rv==CKR_OK);

	rv = C_VerifyFinal(hSession,signature,ulSignatureLen);
	CPPUNIT_ASSERT(rv==CKR_SIGNATURE_INVALID);
}


void SignVerifyTests::testRsaSignVerify()
{
	CK_RV rv;
	CK_UTF8CHAR pin[] = SLOT_0_USER1_PIN;
	CK_ULONG pinLength = sizeof(pin) - 1;
	CK_UTF8CHAR sopin[] = SLOT_0_SO1_PIN;
	CK_ULONG sopinLength = sizeof(sopin) - 1;
	CK_SESSION_HANDLE hSessionRO;
	CK_SESSION_HANDLE hSessionRW;

	// Just make sure that we finalize any previous tests
	C_Finalize(NULL_PTR);

	// Open read-only session on when the token is not initialized should fail
	rv = C_OpenSession(SLOT_INIT_TOKEN, CKF_SERIAL_SESSION, NULL_PTR, NULL_PTR, &hSessionRO);
	CPPUNIT_ASSERT(rv == CKR_CRYPTOKI_NOT_INITIALIZED);

	// Initialize the library and start the test.
	rv = C_Initialize(NULL_PTR);
	CPPUNIT_ASSERT(rv == CKR_OK);

	// Open read-only session
	rv = C_OpenSession(SLOT_INIT_TOKEN, CKF_SERIAL_SESSION, NULL_PTR, NULL_PTR, &hSessionRO);
	CPPUNIT_ASSERT(rv == CKR_OK);

	// Open read-write session
	rv = C_OpenSession(SLOT_INIT_TOKEN, CKF_SERIAL_SESSION | CKF_RW_SESSION, NULL_PTR, NULL_PTR, &hSessionRW);
	CPPUNIT_ASSERT(rv == CKR_OK);

	// Login USER into the sessions so we can create a private objects
	rv = C_Login(hSessionRO,CKU_USER,pin,pinLength);
	CPPUNIT_ASSERT(rv==CKR_OK);

	CK_OBJECT_HANDLE hPuk = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE hPrk = CK_INVALID_HANDLE;

	// Public Session keys
	rv = generateRsaKeyPair(hSessionRW,IN_SESSION,IS_PUBLIC,IN_SESSION,IS_PUBLIC,hPuk,hPrk);
	CPPUNIT_ASSERT(rv == CKR_OK);

	rsaPkcsSignVerify(CKM_RSA_PKCS, hSessionRO, hPuk,hPrk);
	rsaPkcsSignVerify(CKM_RSA_X_509, hSessionRO, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_MD5_RSA_PKCS, hSessionRO, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA1_RSA_PKCS, hSessionRO, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA256_RSA_PKCS, hSessionRO, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA512_RSA_PKCS, hSessionRO, hPuk,hPrk);

	// Private Session Keys
	rv = generateRsaKeyPair(hSessionRW,IN_SESSION,IS_PRIVATE,IN_SESSION,IS_PRIVATE,hPuk,hPrk);
	CPPUNIT_ASSERT(rv == CKR_OK);

	rsaPkcsSignVerify(CKM_RSA_PKCS, hSessionRW, hPuk,hPrk);
	rsaPkcsSignVerify(CKM_RSA_X_509, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_MD5_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA1_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA256_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA512_RSA_PKCS, hSessionRW, hPuk,hPrk);

	// Public Token Keys
	rv = generateRsaKeyPair(hSessionRW,ON_TOKEN,IS_PUBLIC,ON_TOKEN,IS_PUBLIC,hPuk,hPrk);
	CPPUNIT_ASSERT(rv == CKR_OK);

	rsaPkcsSignVerify(CKM_RSA_PKCS, hSessionRW, hPuk,hPrk);
	rsaPkcsSignVerify(CKM_RSA_X_509, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_MD5_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA1_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA256_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA512_RSA_PKCS, hSessionRW, hPuk,hPrk);

	// Private Token Keys
	rv = generateRsaKeyPair(hSessionRW,ON_TOKEN,IS_PRIVATE,ON_TOKEN,IS_PRIVATE,hPuk,hPrk);
	CPPUNIT_ASSERT(rv == CKR_OK);

	rsaPkcsSignVerify(CKM_RSA_PKCS, hSessionRW, hPuk,hPrk);
	rsaPkcsSignVerify(CKM_RSA_X_509, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_MD5_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA1_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA256_RSA_PKCS, hSessionRW, hPuk,hPrk);
	digestRsaPkcsSignVerify(CKM_SHA512_RSA_PKCS, hSessionRW, hPuk,hPrk);
}
