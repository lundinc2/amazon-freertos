/*
 * FreeRTOS V202007.00
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

/**
 * @file iot_demo_https_s3_download_sync.c
 * @brief Demonstrates synchronous usage of the HTTPS library by performing partial content GET requests on a file from
 * S3.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* C Standard includes. */
#include <stdbool.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* FreeRTOS includes. */
#include "iot_https_client.h"
#include "iot_https_utils.h"
#include "aws_demo_config.h"
#include "platform/iot_network.h"
#include "private/iot_error.h"
#include "iot_demo_https_common.h"
#include "platform/iot_clock.h"

/**
 * This demonstrates downloading a file from S3 using a pre-signed URL using the FreeRTOS HTTP Client library.
 * The HTTPS Client library is a generic HTTP/1.1 client library that be used to download files from other webservers as
 * well.
 *
 * A presigned URL is required to run this demo. Please see the demos/https/README.md for instructions on how to
 * generate one.
 *
 * The file is downloaded incrementally using HTTP Partial Content headers. This is done by requesting ranges of the
 * bytes in a file with the header: "Range: bytes=N-M", where N is the starting range and M is the ending range. The
 * S3 HTTP server will response with a 206 Partial Content type of response and the file byte range requested. Please
 * note that not all HTTP servers support a Partial Content download with a byte range.
 *
 * This demo cannot download a file larger than 2^32 - 1 bytes.
 */

/**
 * @cond DOXYGEN_IGNORE
 * Doxygen should ignore this section.
 *
 * Provide default values for undefined configuration settings.
 */

/* Presigned URL for S3 GET Object access. */
#define IOT_DEMO_HTTPS_PRESIGNED_GET_URL    "https://www.httpbin.org/anything/32"

/* TLS port for HTTPS. */
#ifndef IOT_DEMO_HTTPS_PORT
    #define IOT_DEMO_HTTPS_PORT    ( ( uint16_t ) 443 )
#endif

/* Size in bytes of the User Buffer used to store the internal connection context. The size presented here accounts for
 * storage of the internal connection context. The minimum size can be found in extern const unint32_t connectionUserBufferMinimumSize. */
#ifndef IOT_DEMO_HTTPS_CONN_BUFFER_SIZE
    #define IOT_DEMO_HTTPS_CONN_BUFFER_SIZE    ( ( int ) 512 )
#endif

/* Size in bytes of the user buffer used to store the internal request context and HTTP request header lines.
 * The size presented here accounts for the storage of the internal context, the first request line in the HTTP
 * formatted header and extra headers. The minimum size can be found in extern const uint32_t requestUserBufferMinimumSize. */
#ifndef IOT_DEMO_HTTPS_REQ_USER_BUFFER_SIZE
    #define IOT_DEMO_HTTPS_REQ_USER_BUFFER_SIZE    ( ( int ) 512 )
#endif

/* Size in bytes of the user buffer used to store the internal response context and the HTTP response header lines.
 * The size presented here accounts for the storage of the internal context, the first request line in the HTTP
 * formatted header and extra headers. The minimum can be found in responseUserBufferMinimumSize.
 * Keep in mind that if the headers from the response do not all fit into this buffer, then the rest of the headers
 * will be discarded. The minimum size can be found in extern const uint32_t responseUserBufferMinimumSize. */
#ifndef IOT_DEMO_HTTPS_RESP_USER_BUFFER_SIZE
    #define IOT_DEMO_HTTPS_RESP_USER_BUFFER_SIZE    ( ( int ) 1024 )
#endif

/* Size in bytes of the buffer used to store the response body (parts of it). This should be less than or equal to
 * the size of the file we want to download. */
#ifndef IOT_DEMO_HTTPS_RESP_BODY_BUFFER_SIZE
    #define IOT_DEMO_HTTPS_RESP_BODY_BUFFER_SIZE    ( ( int ) 512 )
#endif

/* Time to wait in milliseconds before retrying the HTTPS Connection. A connection is only attempted again if
 * IOT_HTTPS_CONNECTION_ERROR is returned from IotHttpsClient_Connect(). This indicates an error in the network
 * layer. To view logging for network errors update IOT_LOG_LEVEL_NETWORK to IOT_LOG_ERROR in iot_config.h */
#ifndef IOT_DEMO_HTTPS_CONNECTION_RETRY_WAIT_MS
    #define IOT_DEMO_HTTPS_CONNECTION_RETRY_WAIT_MS    ( ( uint32_t ) 3000 )
#endif

/* Number of times to retry the HTTPS connection. A connection is only attempted again if
 * IOT_HTTPS_CONNECTION_ERROR is returned from IotHttpsClient_Connect(). This indicates an error in the network
 * layer. To view logging for network errors update IOT_LOG_LEVEL_NETWORK to IOT_LOG_ERROR in iot_config.h */
#ifndef IOT_DEMO_HTTPS_CONNECTION_NUM_RETRY
    #define IOT_DEMO_HTTPS_CONNECTION_NUM_RETRY    ( ( uint32_t ) 3 )
#endif

/* The timeout in milliseconds to wait on IotHttpsClient_SendSync(). This timeout is how long the routine will block
 * for waiting for an HTTP response. It is possible that the server could close the connection after receiving a
 * request, so never sending the response. */
#ifndef IOT_DEMO_HTTPS_SYNC_TIMEOUT_MS
    #define IOT_DEMO_HTTPS_SYNC_TIMEOUT_MS    ( ( uint32_t ) 60000 )
#endif

/** @endcond */

/*-----------------------------------------------------------*/

/**
 * @brief Buffer used to store the internal connection context.
 */
static uint8_t _pConnUserBuffer[ IOT_DEMO_HTTPS_CONN_BUFFER_SIZE ] = { 0 };

/**
 * @brief Buffer used to store the request context and the HTTP request header lines.
 */
static uint8_t _pReqUserBuffer[ IOT_DEMO_HTTPS_REQ_USER_BUFFER_SIZE ] = { 0 };

/**
 * @brief Buffer used to store the response context and the HTTP response header lines.
 */
static uint8_t _pRespUserBuffer[ IOT_DEMO_HTTPS_RESP_USER_BUFFER_SIZE ] = { 0 };

/**
 * @brief Buffer used to store parts of the response body.
 */
static uint8_t _pRespBodyBuffer[ IOT_DEMO_HTTPS_RESP_BODY_BUFFER_SIZE ] = { 0 };

/*-----------------------------------------------------------*/

/* Declaration of demo function. */
int RunHttpsSyncDownloadDemo( bool awsIotMqttMode,
                              const char * pIdentifier,
                              void * pNetworkServerInfo,
                              void * pNetworkCredentialInfo,
                              const IotNetworkInterface_t * pNetworkInterface );

/*-----------------------------------------------------------*/

/**
 * @brief The function that runs the HTTPS Synchronous Download demo.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT MQTT server. This is ignored in this demo.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier. This is ignored in this demo.
 * @param[in] pNetworkServerInfo Contains network information specific for the MQTT demo. This is ignored in this demo.
 * @param[in] pNetworkCredentialInfo Contains credential Info for a TLS connection.
 * @param[in] pNetworkInterface Network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunHttpsSyncDownloadDemo( bool awsIotMqttMode,
                              const char * pIdentifier,
                              void * pNetworkServerInfo,
                              void * pNetworkCredentialInfo,
                              const IotNetworkInterface_t * pNetworkInterface )
{
    IOT_FUNCTION_ENTRY( int, EXIT_SUCCESS );

    /* Unused parameters. */
    ( void ) awsIotMqttMode;
    ( void ) pIdentifier;
    ( void ) pNetworkServerInfo;

    /* HTTPS Client library return status. */
    IotHttpsReturnCode_t httpsClientStatus = IOT_HTTPS_OK;

    /* Configurations for the HTTPS connection. */
    IotHttpsConnectionInfo_t connConfig = { 0 };
    /* Handle identifying the HTTPS connection. */
    IotHttpsConnectionHandle_t connHandle = IOT_HTTPS_CONNECTION_HANDLE_INITIALIZER;
    /* Configurations for the HTTPS request. */
    IotHttpsRequestInfo_t reqConfig = { 0 };
    /* Configurations for the HTTPS response. */
    IotHttpsResponseInfo_t respConfig = { 0 };

    /* Handle identifying the HTTP request. This is valid after the request has been initialized with
     * IotHttpsClient_InitializeRequest(). */
    IotHttpsRequestHandle_t reqHandle = IOT_HTTPS_REQUEST_HANDLE_INITIALIZER;

    /* Handle identifying the HTTP response. This is valid after the response has been received with
     * IotHttpsClient_SendSync(). */
    IotHttpsResponseHandle_t respHandle = IOT_HTTPS_RESPONSE_HANDLE_INITIALIZER;
    /* Synchronous request specific configurations. */
    IotHttpsSyncInfo_t reqSyncInfo = { 0 };
    /* Synchronous response specific configurations. */
    IotHttpsSyncInfo_t respSyncInfo = { 0 };

    /* The location of the path within string IOT_DEMO_HTTPS_PRESIGNED_GET_URL. */
    const char * pPath = NULL;
    /* The length of the path within string IOT_DEMO_HTTPS_PRESIGNED_GET_URL. */
    size_t pathLen = 0;
    /* The location of the address within string IOT_DEMO_HTTPS_PRESIGNED_GET_URL. */
    const char * pAddress = NULL;
    /* The length of the address within string IOT_DEMO_HTTPS_PRESIGNED_GET_URL. */
    size_t addressLen = 0;

    /* The status of HTTP responses for each request. */
    uint16_t respStatus = IOT_HTTPS_STATUS_OK;
    /* The content length of HTTP responses for each request. */
    uint32_t contentLength = 0;

    /* The size of the file we are trying to download in S3. */
    uint32_t fileSize = 0;
    /* The number of bytes we want to request with in each range of the file bytes. */
    uint32_t numReqBytes = 0;
    /* curByte indicates which starting byte we want to download next. */
    uint32_t curByte = 0;
    /* Buffer to write the Range: header value string. */
    char pRangeValueStr[ RANGE_VALUE_MAX_LENGTH ] = { 0 };
    /* The current attempt in the number of connection tries. */
    uint32_t connAttempt = 0;

    /* Buffer to read the Connection header value into.  The possible values are "close" and "keep-alive". This is the
     * length of the longest string, "keep-alive" plus a NULL terminator. */
    char connectionValueStr[ CONNECTION_KEEP_ALIVE_HEADER_VALUE_LENGTH + 1 ] = { 0 };

    IotLogInfo( "HTTPS Client Synchronous S3 download demo using pre-signed URL: %s", IOT_DEMO_HTTPS_PRESIGNED_GET_URL );

    /* Retrieve the path location from IOT_DEMO_HTTPS_PRESIGNED_GET_URL. This function returns the length of the path
     * without the query into pathLen. */
    httpsClientStatus = IotHttpsClient_GetUrlPath( IOT_DEMO_HTTPS_PRESIGNED_GET_URL,
                                                   strlen( IOT_DEMO_HTTPS_PRESIGNED_GET_URL ),
                                                   &pPath,
                                                   &pathLen );

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "An error occurred in IotHttpsClient_GetUrlPath() on URL %s. Error code: %d",
                     IOT_DEMO_HTTPS_PRESIGNED_GET_URL,
                     httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Retrieve the address location and length from the IOT_DEMO_HTTPS_PRESIGNED_GET_URL. */
    httpsClientStatus = IotHttpsClient_GetUrlAddress( IOT_DEMO_HTTPS_PRESIGNED_GET_URL,
                                                      strlen( IOT_DEMO_HTTPS_PRESIGNED_GET_URL ),
                                                      &pAddress,
                                                      &addressLen );

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "An error occurred in IotHttpsClient_GetUrlAddress() on URL %s\r\n. Error code %d",
                     IOT_DEMO_HTTPS_PRESIGNED_GET_URL,
                     httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Set the connection configurations. */
    connConfig.pAddress = pAddress;
    connConfig.addressLen = addressLen;
    connConfig.port = IOT_DEMO_HTTPS_PORT;
    connConfig.pCaCert = NULL;
    connConfig.caCertLen = 0;
    connConfig.userBuffer.pBuffer = _pConnUserBuffer;
    connConfig.userBuffer.bufferLen = sizeof( _pConnUserBuffer );
    connConfig.pClientCert = NULL;
    connConfig.clientCertLen =0;
    connConfig.pPrivateKey = NULL;
    connConfig.privateKeyLen = 0;
    connConfig.pNetworkInterface = pNetworkInterface;

    /* Set the configurations needed for a synchronous request. */
    reqSyncInfo.pBody = NULL; /* This is a GET request so there is no data in the body. */
    reqSyncInfo.bodyLen = 0;  /* Since there is not data in the body the length is 0. */

    /* Set the configurations needed for a synchronous response. */
    respSyncInfo.pBody = _pRespBodyBuffer;             /* This is a GET request so should configure a place to retreive the
                                                        * response body. */
    respSyncInfo.bodyLen = sizeof( _pRespBodyBuffer ); /* The length of the GET request's response body. This should be
                                                        * greater than or equal to the size of the file requested, for the
                                                        * best performance. */

    /* Set the request configurations. */
    reqConfig.pPath = pPath;

    /* The path is everything that is not the address. It also includes the query. So we get the strlen( pPath ) to
     * acquire everything following in IOT_DEMO_HTTPS_PRESIGNED_GET_URL. */
    reqConfig.pathLen = strlen( pPath );
    reqConfig.pHost = pAddress;
    reqConfig.hostLen = addressLen;
    reqConfig.method = IOT_HTTPS_METHOD_GET;
    reqConfig.isNonPersistent = false;
    reqConfig.userBuffer.pBuffer = _pReqUserBuffer;
    reqConfig.userBuffer.bufferLen = sizeof( _pReqUserBuffer );
    reqConfig.isAsync = false;
    reqConfig.u.pSyncInfo = &reqSyncInfo;

    /* Set the response configurations. */
    respConfig.userBuffer.pBuffer = _pRespUserBuffer;
    respConfig.userBuffer.bufferLen = sizeof( _pRespUserBuffer );
    respConfig.pSyncInfo = &respSyncInfo;

    /* Initialize the HTTPS library. */
    httpsClientStatus = IotHttpsClient_Init();

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "An error occurred initializing the HTTPS library. Error code: %d", httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Connect to S3. */
    for( connAttempt = 1; connAttempt <= IOT_DEMO_HTTPS_CONNECTION_NUM_RETRY; connAttempt++ )
    {
        httpsClientStatus = IotHttpsClient_Connect( &connHandle, &connConfig );

        if( ( httpsClientStatus == IOT_HTTPS_CONNECTION_ERROR ) &&
            ( connAttempt < IOT_DEMO_HTTPS_CONNECTION_NUM_RETRY ) )
        {
            IotLogError( "Failed to connect to the S3 server, retrying after %d ms.",
                         IOT_DEMO_HTTPS_CONNECTION_RETRY_WAIT_MS );
            IotClock_SleepMs( IOT_DEMO_HTTPS_CONNECTION_RETRY_WAIT_MS );
            continue;
        }
        else
        {
            break;
        }
    }

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "Failed to connect to the server. Error code: %d.", httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    fileSize = 400;

    /* The number of bytes we want to request each time is the size of the buffer or the file size if it is smaller than
     * the buffer size, then the size of the file. */
    numReqBytes = IOT_DEMO_HTTPS_RESP_BODY_BUFFER_SIZE;

    if( fileSize < IOT_DEMO_HTTPS_RESP_BODY_BUFFER_SIZE )
    {
        numReqBytes = fileSize;
    }

    /* Re-initialize the request to reuse the request. If we do not reinitialize then data from the last response
     * associated with this request will linger. We reuse reqHandle because we are sending a new sequential
     * synchronous request. IotHttpsClient_InitializeRequest will create a new request from the reqConfig and return
     * a reqHandle that is ready to use as a NEW request. */
    httpsClientStatus = IotHttpsClient_InitializeRequest( &reqHandle, &reqConfig );

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "An error occurred in IotHttpsClient_InitializeRequest() with error code: %d", httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Send the request and receive the response synchronously. */
    IotLogInfo( "Now requesting Range: %s.", pRangeValueStr );

    /* A new response handle is returned from IotHttpsClient_SendSync(). We reuse the respHandle variable because
     * the last response was already processed fully.  */

    httpsClientStatus = IotHttpsClient_SendSync( connHandle, reqHandle, &respHandle, &respConfig, 0 );

    /* If there was network error try again one more time. */
    if( httpsClientStatus == IOT_HTTPS_NETWORK_ERROR )
    {
        /* Maybe the network error was because the server disconnected us. */
        httpsClientStatus = IotHttpsClient_Connect( &connHandle, &connConfig );

        if( httpsClientStatus != IOT_HTTPS_OK )
        {
            IotLogError( "Failed to reconnect to the S3 server after a network error on IotHttpsClient_SendSync(). Error code %d.", httpsClientStatus );
            IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
        }

        httpsClientStatus = IotHttpsClient_SendSync( connHandle, reqHandle, &respHandle, &respConfig, IOT_DEMO_HTTPS_SYNC_TIMEOUT_MS );

        if( httpsClientStatus != IOT_HTTPS_OK )
        {
            IotLogError( "Failed receiving the response on a second try after a network error. The error code is: %d", httpsClientStatus );
            IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
        }
    }
    else if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "There has been an error receiving the response. The error code is: %d", httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    httpsClientStatus = IotHttpsClient_ReadResponseStatus( respHandle, &respStatus );

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "Error in retreiving the response status. Error code %d", httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Get the content length of the body for printing without a NULL terminator. */
    httpsClientStatus = IotHttpsClient_ReadContentLength( respHandle, &contentLength );

    if( httpsClientStatus != IOT_HTTPS_OK )
    {
        IotLogError( "Failed to read the Content-Length from the response. Error code %d", httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* The response has been fully received. */
    IotLogInfo( "Response return code: %d", respStatus );

    /* The logging buffer may not fit all of the response body received and the output on the console will be
     * truncated to the first configLOGGING_MAX_MESSAGE_LENGTH number of characters. */
    IotLogInfo( "Response Body: \r\n%.*s", contentLength, _pRespBodyBuffer );

    /* We increment by the contentLength because the server may not have sent us the range we request. */
    curByte += contentLength;

    IotLogInfo( "Downloaded %d/%d", curByte, fileSize );

    /* If amount of file remaining to request is less than the current amount of bytes to request next time, then
     * update the amount of bytes to request, on the next iteration, to be the amount remaining. */
    if( curByte > fileSize )
    {
        IotLogError( "Received more data than the size of the file specified." );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    if( ( fileSize - curByte ) < numReqBytes )
    {
        numReqBytes = fileSize - curByte;
    }

    /* S3 will close the connection after 100 requests, so check the "Connection" header for a response with a
     * "close" value. */
    memset( connectionValueStr, 0, sizeof( connectionValueStr ) );
    httpsClientStatus = IotHttpsClient_ReadHeader( respHandle,
                                                   CONNECTION_HEADER_FIELD,
                                                   CONNECTION_HEADER_FILED_LENGTH,
                                                   connectionValueStr,
                                                   sizeof( connectionValueStr ) );

    /* If there is any other error besides not found, then that is a fatal error. */
    if( ( httpsClientStatus != IOT_HTTPS_OK ) && ( httpsClientStatus != IOT_HTTPS_NOT_FOUND ) )
    {
        IotLogError( "Failed to read header %s. Error code: %d.", CONNECTION_HEADER_FIELD, httpsClientStatus );
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    if( strncmp( CONNECTION_CLOSE_HEADER_VALUE, connectionValueStr, CONNECTION_CLOSE_HEADER_VALUE_LENGTH ) == 0 )
    {
        /* Reconnect. */
        httpsClientStatus = IotHttpsClient_Connect( &connHandle, &connConfig );

        if( httpsClientStatus != IOT_HTTPS_OK )
        {
            IotLogError( "Failed to reconnect to the server. Error code: %d.", httpsClientStatus );
            IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
        }
    }

    IOT_FUNCTION_CLEANUP_BEGIN();

    /* Disconnect from the server even if the server may have already disconnected us. */
    if( connHandle != NULL )
    {
        IotHttpsClient_Disconnect( connHandle );
    }

    /* Clean up the library because we are done using it. */
    IotHttpsClient_Cleanup();

    IOT_FUNCTION_CLEANUP_END();
}
