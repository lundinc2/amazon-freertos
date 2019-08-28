/*
 * Amazon FreeRTOS PKCS#11 V1.0.8
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

 //------------------------------------------------------------------------------
 //
 // This code was auto-generated by a tool.
 //
 // Changes to this file may cause incorrect behavior and will be 
 // lost if the code is regenerated.
 // 
 //-----------------------------------------------------------------------------

#include "MBT_GenerationMachine.h"

TEST_SETUP(Full_PKCS11_ModelBased_GenerationMachine)
{
	CK_RV rv = xInitializePKCS11();
	TEST_ASSERT_EQUAL_MESSAGE(CKR_OK, rv, "Failed to initialize PKCS #11 module.");
	rv = xInitializePkcs11Session(&xGlobalSession);
	TEST_ASSERT_EQUAL_MESSAGE(CKR_OK, rv, "Failed to open PKCS #11 session.");
}

TEST_TEAR_DOWN(Full_PKCS11_ModelBased_GenerationMachine)
{
	pxGlobalFunctionList->C_Finalize(NULL_PTR);
}

void runAllGenerationTestCases() {
	pxGlobalFunctionList->C_Finalize(NULL_PTR);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_0);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_1);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_2);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_3);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_4);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_5);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_6);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_7);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_8);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_9);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_10);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_11);
	RUN_TEST_CASE(Full_PKCS11_ModelBased_GenerationMachine, path_12);
}

TEST_GROUP_RUNNER(Full_PKCS11_ModelBased_GenerationMachine)
{
	xGlobalSlotId = 1; // TODO

	CK_RV rv = prvBeforeRunningTests();

	if (rv == CKR_CRYPTOKI_NOT_INITIALIZED) {
		rv = CKR_OK; 
	}

	TEST_ASSERT_EQUAL_MESSAGE(CKR_OK, rv, "Setup for the PKCS #11 routine failed.  Test module will start in an unknown state.");

	runAllGenerationTestCases();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_0)
{
	C_GenerateKeyPair_normal_behavior();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_1)
{
	C_GenerateKeyPair_exceptional_behavior_0();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_2)
{
	C_GenerateKeyPair_exceptional_behavior_1();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_3)
{
	C_GenerateKeyPair_exceptional_behavior_2();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_4)
{
	C_GenerateKeyPair_exceptional_behavior_3();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_5)
{
	C_GenerateKeyPair_exceptional_behavior_4();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_6)
{
	C_GenerateKeyPair_exceptional_behavior_1();
	C_GenerateKeyPair_exceptional_behavior_2();
	C_GenerateKeyPair_normal_behavior();
	C_GenerateKeyPair_exceptional_behavior_3();
	C_GenerateKeyPair_exceptional_behavior_4();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_7)
{
	C_GenerateKeyPair_normal_behavior();
	C_GenerateKeyPair_normal_behavior();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_8)
{
	C_GenerateKeyPair_normal_behavior();
	C_GenerateKeyPair_exceptional_behavior_2();
	C_GenerateKeyPair_exceptional_behavior_2();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_9)
{
	C_GenerateKeyPair_normal_behavior();
	C_GenerateKeyPair_exceptional_behavior_1();
	C_GenerateKeyPair_exceptional_behavior_1();
	C_GenerateKeyPair_normal_behavior();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_10)
{
	C_GenerateRandom_normal_behavior();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_11)
{
	C_GenerateRandom_exceptional_behavior_1();
}

TEST(Full_PKCS11_ModelBased_GenerationMachine, path_12)
{
	C_GenerateRandom_exceptional_behavior_2();
}