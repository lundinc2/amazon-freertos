/*
 * FreeRTOS V202002.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

#ifndef AWS_TEST_TCP_H
#define AWS_TEST_TCP_H

/* Non-Encrypted Echo Server.
 * Update tcptestECHO_SERVER_ADDR# and
 * tcptestECHO_PORT with IP address
 * and port of unencrypted TCP echo server. */
#define tcptestECHO_SERVER_ADDR0         34
#define tcptestECHO_SERVER_ADDR1         214
#define tcptestECHO_SERVER_ADDR2         3
#define tcptestECHO_SERVER_ADDR3         53
#define tcptestECHO_PORT                 ( 9000 )

/* Encrypted Echo Server.
 * If tcptestSECURE_SERVER is set to 1, the following must be updated:
 * 1. aws_clientcredential.h to use a valid AWS endpoint.
 * 2. aws_clientcredential_keys.h with corresponding AWS keys.
 * 3. tcptestECHO_SERVER_TLS_ADDR0-3 with the IP address of an
 * echo server using TLS.
 * 4. tcptestECHO_PORT_TLS, with the port number of the echo server
 * using TLS.
 * 5. tcptestECHO_HOST_ROOT_CA with the trusted root certificate of the
 * echo server using TLS. */
#define tcptestSECURE_SERVER             1

#define tcptestECHO_SERVER_TLS_ADDR0     34
#define tcptestECHO_SERVER_TLS_ADDR1     214
#define tcptestECHO_SERVER_TLS_ADDR2     3
#define tcptestECHO_SERVER_TLS_ADDR3     53
#define tcptestECHO_PORT_TLS             ( 9001 )

/* Number of times to retry a connection if it fails. */
#define tcptestRETRY_CONNECTION_TIMES    6

/* The root certificate used for the encrypted echo server.
 * This certificate is self-signed, and not in the trusted catalog. */
static const char tcptestECHO_HOST_ROOT_CA[] = "" \
"-----BEGIN CERTIFICATE-----\n" \
"MIID2TCCAsGgAwIBAgIUerzk0hsdnbOXye6E7I9N+K/gfnswDQYJKoZIhvcNAQEL\n" \
"BQAwfDELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAldBMQwwCgYDVQQHDANTZWExDzAN\n" \
"BgNVBAoMBkFtYXpvbjEMMAoGA1UECwwDSW9UMRAwDgYDVQQDDAdsdW5kaW5jMSEw\n" \
"HwYJKoZIhvcNAQkBFhJsdW5kaW5jQGFtYXpvbi5jb20wHhcNMjAwMzA1MTgwNjI2\n" \
"WhcNMjEwMzA1MTgwNjI2WjB8MQswCQYDVQQGEwJVUzELMAkGA1UECAwCV0ExDDAK\n" \
"BgNVBAcMA1NlYTEPMA0GA1UECgwGQW1hem9uMQwwCgYDVQQLDANJb1QxEDAOBgNV\n" \
"BAMMB2x1bmRpbmMxITAfBgkqhkiG9w0BCQEWEmx1bmRpbmNAYW1hem9uLmNvbTCC\n" \
"ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANog4OrB3uLF43rJE/TDZjjS\n" \
"HmVY39gEn19Wb3HGBZMIS8zYyQScuTBOk55RAJaJVQFI+u/St7MqT+gs4cxgrfEx\n" \
"6DT/v7U2hIdQzTRea8fBbe3UkijrFg7XbUwor7ZRU9ODkMzUcBBedW9zjzqGX/T3\n" \
"Hi/XqEYFEVCUVZT0PU1zJOygIxd1bK0id7KmmANhzi/0tQSTZEQkuX5NkqSxbDzo\n" \
"TVhqA25RjLfcJBKKgSqK4/am1k65BFmvyARGG5VNfbEOnwWQC9WLsKBV0TrKoQEn\n" \
"dkscXZBrub7mqvnfR1VY9jOGURtM8oalpbUnWS1gSRx0WYsr59go8jMAottctWcC\n" \
"AwEAAaNTMFEwHQYDVR0OBBYEFFYgQVhRcZO63EE37je+BYpvtLgZMB8GA1UdIwQY\n" \
"MBaAFFYgQVhRcZO63EE37je+BYpvtLgZMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZI\n" \
"hvcNAQELBQADggEBAKe3rFPsJqGrkW8pv3CuKkZt2utLH/h5of8Ep/tHcuactS/H\n" \
"q2FSd6RKiN0vZzclQB7jsQ4el2yH3sfp877pznTteXTjtdRwA1G06Q+9SPb7OCoG\n" \
"qUl4dQgqonWEVcgZfGO0vlFypTMtnyPxqb2ECUh8f9CEdrFst/O8DW4+6t5HNaGD\n" \
"8r85KR4ix7BWCJrK+BwDWQhKxIa7fenhnpyRwBYj92UaHSeNrYWgPxhnCf0Ho0gG\n" \
"5WRwX/G7djj18z6kNoUi3Ju7NlRZrEkHe+XH1O0590C8qBScgWDtrUXRCYrBqKiz\n" \
"tfAoGLYfHR01UAW6QqmLENb3xLATi8dSu+G/ZxU=\n" \
"-----END CERTIFICATE-----";


#endif /* ifndef AWS_TEST_TCP_H */
