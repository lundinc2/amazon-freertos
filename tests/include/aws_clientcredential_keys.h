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

#ifndef AWS_CLIENT_CREDENTIAL_KEYS_H
#define AWS_CLIENT_CREDENTIAL_KEYS_H

/*
 * @brief PEM-encoded client certificate.
 *
 * @todo If you are running one of the FreeRTOS demo projects, set this
 * to the certificate that will be used for TLS client authentication.
 *
 * @note Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 */
<<<<<<< Updated upstream
#define keyCLIENT_CERTIFICATE_PEM                   NULL
||||||| constructed merge base
#define keyCLIENT_CERTIFICATE_PEM                   ""
=======
#define keyCLIENT_CERTIFICATE_PEM                   "" \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDfzCCAmcCFFQ1+VCMeqgXwISQAQUsFqR86pxHMA0GCSqGSIb3DQEBCwUAMHwx\n" \
"CzAJBgNVBAYTAlVTMQswCQYDVQQIDAJXQTEMMAoGA1UEBwwDU2VhMQ8wDQYDVQQK\n" \
"DAZBbWF6b24xDDAKBgNVBAsMA0lvVDEQMA4GA1UEAwwHbHVuZGluYzEhMB8GCSqG\n" \
"SIb3DQEJARYSbHVuZGluY0BhbWF6b24uY29tMB4XDTIwMDMwNTE4MDcxMFoXDTIx\n" \
"MDMwNTE4MDcxMFowfDELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAldBMQwwCgYDVQQH\n" \
"DANTZWExDzANBgNVBAoMBkFtYXpvbjEMMAoGA1UECwwDSW9UMRAwDgYDVQQDDAds\n" \
"dW5kaW5jMSEwHwYJKoZIhvcNAQkBFhJsdW5kaW5jQGFtYXpvbi5jb20wggEiMA0G\n" \
"CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCzf0+HUZFWE5HFl7XRk7OvX4zqOk/J\n" \
"XWp4K6AmzWjS6CMCPnLwqXNq94Bt9GHKVNboOuR1NvLBDzvllRUpYqM9tbmfw+6M\n" \
"svolG7tj+/+vaHlvGhy3sFeRGqLiL8x+msPWODzp+UGybNhNOAD28ePbLP/oDn2+\n" \
"ZFBcqZL4RC2+hu3kyGkxdyAfHUm1a/duEtlhGM9bXc11ma50jJl+gT/xuUQW0PlM\n" \
"wP00sgB0goPi91zQBjrzzI/VvTlEbnXu1nDizrrLTibd49vhOUJQew9DaA6oJtBo\n" \
"NAWYAEnKUPMNuKe+Im7CrzvDEr54qgANbKWu135jZlfMpdJQwVCxLnwHAgMBAAEw\n" \
"DQYJKoZIhvcNAQELBQADggEBAHT7d+cdamZkFktXPdrSZA7UZr51pNBHJawYZGzX\n" \
"c14iP9ZMv3+l8oaVv99DsYiX2Axp2qBBk6Cs5fh6CBUHBg+3HVFfIw6pkEAYLzrE\n" \
"AB9Gg0gXEKuXq3tZRY8WRYoDTrx3skEiN1kgKWNJKWJ8IfGygeDK16o1pYWmuonL\n" \
"JTLk9dlXme08IuoY8zp8Dwy8OQTHLPI1Xuwp2enVEfQxQk+QjO5R3JH3rsE9rKV5\n" \
"1ESOOHr5SyVSDQ+Bibv3QYqr6I2tTmSV6EggOomwYCWChNctHPjqlU56lq+RUx7A\n" \
"XEgEqhm8Bf/v9DAglI5U9oMOPR9yOjwMt0A5g3FsKhAAf6Q=\n" \
"-----END CERTIFICATE-----"
>>>>>>> Stashed changes

/*
 * @brief PEM-encoded issuer certificate for AWS IoT Just In Time Registration (JITR).
 *
 * @todo If you are using AWS IoT Just in Time Registration (JITR), set this to
 * the issuer (Certificate Authority) certificate of the client certificate above.
 *
 * @note This setting is required by JITR because the issuer is used by the AWS
 * IoT gateway for routing the device's initial request. (The device client
 * certificate must always be sent as well.) For more information about JITR, see:
 *  https://docs.aws.amazon.com/iot/latest/developerguide/jit-provisioning.html,
 *  https://aws.amazon.com/blogs/iot/just-in-time-registration-of-device-certificates-on-aws-iot/.
 *
 * If you're not using JITR, set below to NULL.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 */
#define keyJITR_DEVICE_CERTIFICATE_AUTHORITY_PEM    NULL

/*
 * @brief PEM-encoded client private key.
 *
 * @todo If you are running one of the FreeRTOS demo projects, set this
 * to the private key that will be used for TLS client authentication.
 *
 * @note Must include the PEM header and footer:
 * "-----BEGIN RSA PRIVATE KEY-----\n"\
 * "...base64 data...\n"\
 * "-----END RSA PRIVATE KEY-----\n"
 */
<<<<<<< Updated upstream
#define keyCLIENT_PRIVATE_KEY_PEM                   NULL
||||||| constructed merge base
#define keyCLIENT_PRIVATE_KEY_PEM                   ""
=======
#define keyCLIENT_PRIVATE_KEY_PEM                   "" \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"MIIEowIBAAKCAQEAs39Ph1GRVhORxZe10ZOzr1+M6jpPyV1qeCugJs1o0ugjAj5y\n" \
"8KlzaveAbfRhylTW6DrkdTbywQ875ZUVKWKjPbW5n8PujLL6JRu7Y/v/r2h5bxoc\n" \
"t7BXkRqi4i/MfprD1jg86flBsmzYTTgA9vHj2yz/6A59vmRQXKmS+EQtvobt5Mhp\n" \
"MXcgHx1JtWv3bhLZYRjPW13NdZmudIyZfoE/8blEFtD5TMD9NLIAdIKD4vdc0AY6\n" \
"88yP1b05RG517tZw4s66y04m3ePb4TlCUHsPQ2gOqCbQaDQFmABJylDzDbinviJu\n" \
"wq87wxK+eKoADWylrtd+Y2ZXzKXSUMFQsS58BwIDAQABAoIBABGhTolm/JRsxaOX\n" \
"vBcCn+J4yxlycsW/dCarekZ1ZHdar52XzqhOrHR0LNwf5b7+yED1D91ncT5/JY3u\n" \
"L67p7kiwYbQMhZCyP5mHeMdgSAPNiifcc7ejDWbGK/t1YGuK/fM7gNgmUEqbID+t\n" \
"YYLfzVaIu/Xp/nXF2pLPYQ0bfTa6VauEU6YtcLxfcg5Jet7GZ6UjDTSdNh5wQBsE\n" \
"jfFJ3q6gXidh45BTiYuJlH7OtyK7VKHa9EZvhc44JTxLzS14qCSO/zbKAbMSjUZW\n" \
"EuNTL/VB+rjXwewVWERRhdwjqVIe2lb4Wkr9aYL+wyoXFznJpWLjlircAPeG7QQT\n" \
"sr8yq7kCgYEA7bxazwbhXJFhhXt904uMYHdYk3PiipPpM9gsjiOfzD0bLYIv4sN5\n" \
"BM4x+3c9SDeE/bi2dRwRX+x/nDGWgLziwekFqScoiNuV7EKhvezIYDiqFFl7tS8C\n" \
"eOpq5hAyupZvAZUFjVi2tG4XSudFQbM6eLv/z/Ag3e9ZFIMKXVKDKQMCgYEAwUmO\n" \
"hnXJY45c0Xr5B+qWcXRcmKlN5Os4MU3Nkp6Je1JX+3eimffCJiw7YAH0dlpB6sGV\n" \
"cd9gBkg2rOyfgVWDTwCAQkncLqZOZkISourBEbvNDSpW18eZfpzjc31J8wk8qmxL\n" \
"9TSwzaEXXRqM5hZkowthuJ0aSuomV3mcv8rJl60CgYAAgvA7E8u2VEW+cMaThvBV\n" \
"YMxa/NvW6nyM9QEbiS4V1WfSkD4kIcGH5h2radVC64OovBYAaIANEcgwgNbPDhj6\n" \
"y9KMS55FtRs8d+Q7MWA4/MY45vxiJmi989spBY3mYt54RWbOqAs0liwMqDS48HbG\n" \
"vbjOLLkVYSdy6NlD3CKWGwKBgDKWDbcjHJHxsFki1go8WyNWUOWjab9/0DUXJ7Y3\n" \
"x8N+yYgGx4eEUEutR9zYpiJTfOzzvSkQTRFX1Pds9lHjD3qdpvOyYO3UmLAqmrYI\n" \
"un7pp8DKU/AlTQbWCLExGSmCQV5Y+YgzQhKPFo5HZJjTQ4NodyrZ8weoQGCkc2G+\n" \
"sQQBAoGBAM9rK3tXBJs2m7YyelV+oIq6EusJEIv68b+g1wCIKCRfRYBukBkkQ1Cm\n" \
"WVNzH4OM3gYDJHQ9R617x8Qjp/HFaADD1A/CdsYVYvl96KK91KNbX2Zt+oDSkj7s\n" \
"DM5rIPMHVzMHFNFDt5IDBaW/qbOntvyLYtRW6EMKxgokf6/tXwXa\n" \
"-----END RSA PRIVATE KEY-----"

>>>>>>> Stashed changes

#endif /* AWS_CLIENT_CREDENTIAL_KEYS_H */
