# PKCS \#11 Test Plan
## Introduction
This document summarizes the testing plan for the PKCS #11 stack. 

If you are working on qualifying a PKCS #11 stack it is sufficient to only read the "Qualification Tests" section. There is an "outline" section that outlines that describes the tests.

## Qualification Tests
The PKCS #11 stack can be qualified for three different provisioning use cases. The tests are split so that an implementation or device can specify a provisioning model to support. **A device / port only needs to support one of these three mechanisms.**

The tests in this group are always run regardless of the provisioning strategy the PKCS #11 stack is being qualified for. These tests don't require any objects and outline the expectations various libraries have.

**These tests must always pass, there is no exception that can be made here for qualification.**

The test groups in this category are called:
- Full_PKCS11_Management
    - This group focuses on the various management APIs in PKCS #11 as C_Initialize, C_Finalize, and sessions. It is always required ot pass this group, as all the libraries using PKCS #11 rely on the behavior under test.
- Full_PKCS11_NoObject
    - This group focuses on crypto operations that don't require an object. For example creating a message digest or generating a random number. Once again this group is always required.
- Full_PKCS11_SignVerify
    - This group focuses on signing a message digest and seeing if the signature can be verified. Note that it tests the PKCS #11 implementation's ability to trust itself, as well as validating this property using mbed TLS.
    - Warning: This test group assumes that there exists objects that can be used for the tests. If there is no public and private key on the device, this test case will fail.

#### Qualifying For Credential Importing
The test group in this category is called Full_PKCS11_ImportCredentials. This test group can be enabled by setting *pkcs11configIMPORT_SUPPORTED* to 1, in *iot_pkcs11_config.h*.

Should this test group pass, it is assumed that the PKCS #11 implementation has the capability of provisioning a x509 certificate, public, and private key that were all generated on a different device. The device must be capable of retaining these in flash, if the port is being created using the PKCS11_PAL.

#### Qualifying For Generating Credentials 
The test group in this category is called Full_PKCS11_GenerateCredentials. This test group can be enabled by setting *pkcs11configGENERATE_SUPPORTED* to 1. Should this test group pass it is assumed that the PKCS #11 implementation has the capability of creating a private and public key pair. 

Once again it is expected that the credentials will persist between flash cycles.

#### Qualifying For Pre-provisioned Credentials

This is for secure elements that are pre-provisioned with credentials, and cannot import or generate new ones. This is enabled by setting *pkcs11configMULTI_ACCOUNT_AUTH_SUPPORTED* to 1.

There is no test group associated with this configuration, but the tests will check that at least one of the configurations is chosen. The expectation is that the Sign and Verify capabilities will be tested in the Full_PKCS11_SignVerify test group, to prove that the secure element has real credentials stored.

It is expected that the certificates are able to connect to AWS IoT Core, using multi account registration, or another form of trust. This capability will be tested in separate tests, and is not within the scope of the PKCS #11 test plan.

## Unit Tests
Unit tests exist for *iot_pkcs11_mbedtls.c*, *iot_pkcs11.c*, and *iot_pki_utils.c*. 

If you are developing for any of these files, please submit an update to these unit tests as well, to  ensure that the code coverage in the unit tests is maintained.

The goal for these unit tests is to, at the minimum, maintain 100% line coverage for the above files. 

The unit tests can be found in a folder called "utest", usually located in the same directory as the source code.

See for more documentation [here](https://github.com/aws/amazon-freertos/tree/master/tests/unit_test/linux) on setting up the unit tests.


## Model Based Tests
Model based tests exist for PKCS #11 as well. They can be found in the test directory, with the prefix MBT. These tests are complementary tests, and are not necessary for qualification. The purpose of these tests is to test that conformance of iot_pkcs11_mbedtls.c to the PKCS #11 specification.

Currently only the windows simulator has been used with these tests. In order to run them add the MBT prefixed files, and iot_test_pkcs11_globals.h to the aws_test project for the windows platform and define testrunnerFULL_PKCS11_MODEL_ENABLED to 1 in the *aws_test_runner_config.h*.

## System Test outline
### Assumptions
#### Start State Assumptions
Most of the test groups will assume that the PKCS #11 module is uninitialized, and will initialize it as a first step. It is important that the PKCS #11 implementation is tolerant to multiple initializations.
#### End State Assumptions
Most of the test groups will un-initialize the PKCS #11 module. Some groups may modify the device's "real" credential objects, if they are not using a different label. 

If this is the case, it is the responsibility of the test harness to correct the credentials back to a good state, in order to ensure the success of other test groups that are reliant on the real credentials.

The test group Full_PKCS11_SignVerify makes an assumption that a private key and public key exist in flash or on the secure elements with a known test label. This test will fail if this is not the case.

### Verification
#### Using mbed TLS
(Currently this is the only method we support, I hope to update that before merging this report)
TODO/TBD:
- Create a macro to disable mbed TLS verification
- Document what is being verified 
- 

#### Using OpenSSL
This is an alternative to cross-verify the capabilities of the PKCS #11 implementation / device. Instead of using the device to cross-verify it's own capabilities, the proposal is to use OpenSSL and a separate device to make sure that the tests can be trusted. This could be done in a python script or something similar, or manual verification. 

Should we go ahead with this will need to supply some automated way to run this for IDT. Ideally IDT would use a script similar to the one that would be hosted in reference integrations repo.

Why:
- Some devices may be too small to link two crypto stacks.
- All of our crypto/tls is very tightly coupled to mbed TLS, decoupling the tests from mbed TLS is another step towards being agnostic to the implementation.
- This moves some responsibility from the tests to the device that is verifying the qualification. 
- Remove one dependency from the tests (or support either method).

TODO/TBD:
- Propose this as a mechanism of verification
- Introduce support to IDT if this is the case
- Create scripts that automate the cross-verification
- Determine the proper mechanism for delivering the data to verify
-


### Test Order
Generally the order of these tests does not matter. The exception is the Full_PKCS11_SignVerify test group. This group has a dependency for the credentials used in the test to already exist. Depending on what provisioning mechanism the device / PKCS #11 port supports, there are various ways to load these credentials onto the device.

| Test Number 	| Test Group 	| Test Case 	| Function under test 	| Expected Device Behavior 	| Possible Failure 	|
|-	|-	|-	|-	|-	|-	|
| 1 	| Full_PKCS11_Management 	| AFQP_GetFunctionList 	| C_GetFunctionList 	| Return a CK_FUNCTION_LIST_PTR object that points to the implemented PKCS #11 functions 	| C_GetFunctionList is not implemented 	|
| 2 	| Full_PKCS11_Management 	| AFQP_InitializeFinalize 	| C_Initialize, C_Finalize 	| Can initialize the PKCS #11 module without failure. Can handle multiple initialization calls without a call to C_Finalize. Can un-initialize PKCS #11 module with C_Finalize. Can tolerate multiple calls to C_Finalize 	| The device is unable to initialize the PKCS #11 stack as expected. The device is unable to un-initialize the PKCS #11 stack as expected. 	|
| 3 	| Full_PKCS11_Management 	| AFQP_GetSlotList 	| C_GetSlotList 	| Can return slot count when NULL parameter is used. Can return a list of slots. Can check to see if the received buffer is too small. 	| The device can't describe how many slots it has. The device cannot return a list of onboard slots. The device cannot do error handling as described by the PKCS #11 standard. 	|
| 4 	| Full_PKCS11_Management 	| AFQP_OpenSessionCloseSession 	| C_OpenSession, C_OpenSession 	| Can open a session on one of the slots returned by C_GetSlotList. Can close a session that was opened. 	| The device can't open a session bound to a slot. The device can't clean up after creating a session. 	|
| 5 	| Full_PKCS11_Management 	| AFQP_Capabilities 	| C_GetMechanismInfo 	| Can query device for PKCS #11 features supported on a token slot. The device outputs what mechanisms are supported by the slot under test.  	| The device cannot do either RSA or ECDSA verification. The device cannot perform a SHA256 digest. 	|
| 6 	| Full_PKCS11_NoObject 	| AFQP_Digest 	| C_DigestInit,C_DigestUpdate,C_DigestFinal 	| Can hash a buffer with the SHA256 algorithm.  	| The device cannot perform the digest, or the digest generated does not align with the known test vector 	|
| 7 	| Full_PKCS11_NoObject 	| AFQP_Digest_ErrorConditions 	| C_DigestInit,C_DigestUpdate,C_DigestFinal 	| Verify PKCS #11 stack handles bad input to digest operations 	| The PKCS #11 stack does not behave as expected per the spec, when doing a digest operation 	|
| 8 	| Full_PKCS11_NoObject 	| AFQP_GenerateRandom 	| C_GenerateRandom 	| Fill three 10 byte buffers with random values, first two filled consecutively, and the third filled after opening a new PKCS #11 sessions, test the variance in the random numbers 	| The port doesn't have RNG or has a bad implementation of an RNG source 	|
| 9 	| Full_PKCS11_NoObject 	| AFQP_GenerateRandomMultiThread 	| C_GenerateRandomMultiThread 	| Spawn multiple threads and generate random numbers in them.	| The port issues errors when multiple threads are querying for random numbers. 	|
| 10 	| Full_PKCS11_ImportKeyPair (Sub test cases for EC and RSA) 	| AFQP_CreateObject 	| C_CreateObject 	| Known credentials are imported into the PKCS #11 module and can be accessed through it, as well as across flash cycles. 	| The port cannot import either RSA or EC keys and certificates. 	|
| 11 	| Full_PKCS11_CreateKeyPair (Sub test cases for EC and RSA) 	| AFQP_GenerateKeyPair 	| C_GenerateKeyPair 	| This test case will generate a key pair on the device.  	| The port can handle creating a new public and private key pair on the device. 	|
| 12 	| Full_PKCS11_VerifySign 	| AFQP_FindObject 	| C_FindObjects,C_FindObjectsInit, C_CreateObject 	| This test will see if an object can be found using the C_FindObjects API 	| This test case will fail if the device cannot find an object. (Note we will need a separate test to check if we found the correct object, this one just checks that find can be used).  	|
| 13 	| Full_PKCS11_VerifySign 	| AFQP_GetAttributeValue 	| C_GetAttributeValue 	| This test will use the parameters we used when we created our object, and see if they match up with the object we find 	| This test case will fail if the API does not behave as expected, and if the object we found lacks the known parameters that we requested when it was created. 	|
| 14 	| Full_PKCS11_VerifySign 	| AFQP_SignVerify 	| C_SignInit,C_Sign,C_VerifyInit,C_Verify 	| This test will take a known digest, and sign it with the key on the device 	| This test will fail if the known test vector creates a different signature than what is expected. 	|
| 15 	| Full_PKCS11_VerifySign 	| AFQP_Verify 	| C_VerifyInit,C_Verify 	| This test will take a signature of a known digest, created by a known private key, and see if it can be verified with the public key created from the private key.  	| This test will fail if the device is unable to create a good signature on a known test vector 	|