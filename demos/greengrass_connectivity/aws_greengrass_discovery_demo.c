/*
 * FreeRTOS V202012.00
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
 * @file aws_greengrass_discovery_demo.c
 * @brief A simple Greengrass discovery example.
 *
 * A simple example that perform discovery of the greengrass core device.
 * The JSON file is retrieved.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Temporary include for demo config TODO: Remove */
#include "mqtt_demo_mutual_auth_config.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Include AWS IoT metrics macros header. */
#include "aws_iot_metrics.h"

/* Common HTTP demo utilities. */
#include "http_demo_utils.h"

/* Transport interface implementation include header for TLS. */
#include "transport_secure_sockets.h"

/* JSON include. */
#include "core_json.h"

/* MQTT include. */
#include "core_mqtt.h"

/* Include header for root CA certificates. */
#include "iot_default_root_certificates.h"

/**
 * @brief The MQTT broker endpoint used for this demo.
 */
#ifndef democonfigMQTT_BROKER_ENDPOINT
    #define democonfigMQTT_BROKER_ENDPOINT    clientcredentialMQTT_BROKER_ENDPOINT
#endif

/**
 * @brief The root CA certificate belonging to the broker.
 */
#ifndef democonfigROOT_CA_PEM
    #define democonfigROOT_CA_PEM    tlsATS1_ROOT_CERTIFICATE_PEM
#endif

#ifndef democonfigCLIENT_IDENTIFIER

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique so edit as required to ensure no two clients connecting to the
 * same broker use the same client identifier.
 */
    #define democonfigCLIENT_IDENTIFIER    clientcredentialIOT_THING_NAME
#endif

#ifndef democonfigMQTT_BROKER_PORT

/**
 * @brief The port to use for the demo.
 */
    #define democonfigMQTT_BROKER_PORT    clientcredentialMQTT_BROKER_PORT
#endif

/* GGD Demo macros. */
#define ggdDEMO_MAX_MQTT_MESSAGES              3
#define ggdDEMO_MAX_MQTT_MSG_SIZE              500
#define ggdDEMO_MQTT_MSG_TOPIC                 "freertos/demos/ggd"
#define ggdDEMO_MQTT_MSG_DISCOVERY             "{\"message\":\"Hello #%lu from FreeRTOS to Greengrass Core.\"}"

#define ggdDEMO_KEEP_ALIVE_INTERVAL_SECONDS    1200
/* Number of times to try MQTT connection. */
#define ggdDEMO_NUM_TRIES                      3
#define ggdDEMO_RETRY_WAIT_MS                  2000

/**
 * @brief The length in bytes of the user buffer.
 */
#define democonfigUSER_BUFFER_LENGTH           ( 2500 )

/**
 * @brief HTTP command to retrieve JSON file from the Cloud.
 */
#define ggdCLOUD_DISCOVERY_ADDRESS      \
    "GET /greengrass/discover/thing/%s" \
    " HTTP/1.1\r\n\r\n"

#define ggdDEMOHTTP_PATH    "/greengrass/discover/thing/" clientcredentialIOT_THING_NAME


/**
 * @brief Milliseconds per second.
 */
#define MILLISECONDS_PER_SECOND              ( 1000U )

/**
 * @brief Greengrass keep alive time out in seconds.
 */
#define ggdmqttKEEP_ALIVE_TIMEOUT_SECONDS    ( 60U )

/**
 * @brief Milliseconds per second.
 */
#define ggdmqttCONNACK_RECV_TIMEOUT_MS       ( 1000U )


/**
 * @brief Milliseconds per FreeRTOS tick.
 */
#define MILLISECONDS_PER_TICK    ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

/**
 * @brief Each compilation unit that consumes the NetworkContext must define it.
 * It should contain a single pointer to the type of your desired transport.
 * When using multiple transports in the same compilation unit, define this pointer as void *.
 *
 * @note Transport stacks are defined in amazon-freertos/libraries/abstractions/transport/secure_sockets/transport_secure_sockets.h.
 */
struct NetworkContext
{
    SecureSocketsTransportParams_t * pParams;
};


/**
 * @brief Global entry time into the application to use as a reference timestamp
 * in the #prvGetTimeMs function. #prvGetTimeMs will always return the difference
 * between the current time and the global entry time. This will reduce the chances
 * of overflow for the 32 bit unsigned integer used for holding the timestamp.
 */
static uint32_t ulGlobalEntryTimeMs;

/**
 * @brief Packet Identifier generated when Publish request was sent to the broker;
 * it is used to match received Publish ACK to the transmitted Publish packet.
 */
static uint16_t usPublishPacketIdentifier;

/**
 * @brief A buffer used in the demo for storing HTTP request headers and
 * HTTP response headers and body.
 *
 * @note This demo shows how the same buffer can be re-used for storing the HTTP
 * response after the HTTP request is sent out. However, the user can also
 * decide to use separate buffers for storing the HTTP request and response.
 */
static uint8_t ucUserBuffer[ democonfigUSER_BUFFER_LENGTH ];

/**
 * @brief Static buffer used to hold MQTT messages being sent and received.
 */
static uint8_t ucSharedBuffer[ democonfigNETWORK_BUFFER_SIZE ];

/** @brief Static buffer used to hold MQTT messages being sent and received. */
static MQTTFixedBuffer_t xBuffer =
{
    ucSharedBuffer,
    democonfigNETWORK_BUFFER_SIZE
};

/**
 * @brief Connect to HTTP server with reconnection retries.
 *
 * @param[out] pxNetworkContext The output parameter to return the created network context.
 *
 * @return pdPASS on successful connection, pdFAIL otherwise.
 */
static BaseType_t prvConnectToServer( NetworkContext_t * pxNetworkContext,
                                      SocketsConfig_t * pxSocketsConfig,
                                      ServerInfo_t * pxServerInfo );

/**
 * @brief Send an HTTP request based on a specified method and path, then
 * print the response received from the server.
 *
 * @param[in] pxTransportInterface The transport interface for making network calls.
 * @param[in] pcMethod The HTTP request method.
 * @param[in] xMethodLen The length of the HTTP request method.
 * @param[in] pcPath The Request-URI to the objects of interest.
 * @param[in] xPathLen The length of the Request-URI.
 * @param[out] ppcJSONFile The pointer to JSON file.
 * @param[out] plJSONFileLength The length of the JSON file.
 *
 * @return pdFAIL on failure; pdPASS on success.
 */
static BaseType_t prvSendHttpRequest( const TransportInterface_t * pxTransportInterface,
                                      const char * pcMethod,
                                      size_t xMethodLen,
                                      const char * pcPath,
                                      size_t xPathLen,
                                      char ** ppcJSONFile,
                                      uint32_t * plJSONFileLength );

/**
 * @brief Get the JSON file containing the connection information to
 * Greengrass core.
 *
 * @param[out] ppcJSONFile The pointer to JSON file.
 * @param[out] plJSONFileLength The length of the JSON file.
 *
 * @return pdFAIL on failure; pdPASS on success.
 */
static BaseType_t prvGetGGCoreJSON( char ** ppcJSONFile,
                                    uint32_t * plJSONFileLength );

/**
 * @brief Send MQTT messages to Greengrass core.
 *
 * @param[in] pxMQTTContext The pointer to coreMQTT context.
 *
 * @return pdFAIL on failure; pdPASS on success.
 */
static BaseType_t _sendMessageToGGC( MQTTContext_t * pxMQTTContext );

/**
 * @brief Logic for Greengrass discovery demo.
 *
 * @return pdFAIL on failure; pdPASS on success.
 */
static int _discoverGreengrassCoreDemo();

static uint32_t prvGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

static void prvEventCallback( MQTTContext_t * pxMQTTContext,
                              MQTTPacketInfo_t * pxPacketInfo,
                              MQTTDeserializedInfo_t * pxDeserializedInfo )
{
    ( void ) pxMQTTContext;
    ( void ) pxPacketInfo;
    ( void ) pxDeserializedInfo;
}

/*-----------------------------------------------------------*/
static uint32_t prvConvertCertificateJSONToString( char * certbuf,
                                                   size_t certlen )
{
    uint32_t ulReadIndex = 1, ulWriteIndex = 0;

    do
    {
        if( ( certbuf[ ulReadIndex - ( uint32_t ) 1 ] == '\\' ) &&
            ( certbuf[ ulReadIndex ] == 'n' ) )
        {
            certbuf[ ulWriteIndex ] = '\n';
            ulReadIndex++;
        }
        else
        {
            certbuf[ ulWriteIndex ] =
                certbuf[ ulReadIndex - ( uint32_t ) 1 ];
        }

        ulReadIndex++;
        ulWriteIndex++;
    }
    while( ulReadIndex < certlen );

    return ulWriteIndex;
}

static BaseType_t prvGGDGetCertificate( char * pcJSONFile,
                                        uint32_t ulJSONFileSize,
                                        char ** pucCert,
                                        size_t * pulCertLen )
{
    BaseType_t xStatus = pdFAIL;

    /* TODO: This is just grabbing the first CA. Need to update to match
     * multi CA case. */
    char query[] = "GGGroups[0].CAs[0]";
    size_t queryLength = sizeof( query ) - 1;
    char * value;
    size_t valueLength;
    JSONStatus_t result;
    char * certbuf = NULL;

    result = JSON_Search( pcJSONFile,
                          ulJSONFileSize,
                          query,
                          queryLength,
                          &value,
                          &valueLength );

    if( result == JSONSuccess )
    {
        certbuf = pvPortMalloc( valueLength + 1 );
        memset( certbuf, 0x00, valueLength + 1 );
        /* strip trailing \n character. */
        memcpy( certbuf, value, valueLength - 2 );
        *pulCertLen = prvConvertCertificateJSONToString( certbuf, valueLength );
        *pucCert = certbuf;

        xStatus = pdPASS;
    }

    return xStatus;
}

static BaseType_t prvGGDGetIPOnInterface( char * pcJSONFile,
                                          const uint32_t ulJSONFileSize,
                                          ServerInfo_t * pucTargetInterface )
{
    BaseType_t xStatus = pdFAIL;
    char hostAddressQuery[] = "GGGroups[0].Cores[0].Connectivity[0].HostAddress";
    char hostPortQuery[] = "GGGroups[0].Cores[0].Connectivity[0].PortNumber";
    size_t hostAddressQueryLength = sizeof( hostAddressQuery ) - 1;
    size_t hostPortQueryLength = sizeof( hostPortQuery ) - 1;
    char * value;
    size_t valueLength;
    JSONStatus_t result;

    result = JSON_Search( pcJSONFile,
                          ulJSONFileSize,
                          hostAddressQuery,
                          hostAddressQueryLength,
                          &value,
                          &valueLength );

    if( result == JSONSuccess )
    {
        char * addressbuf = pvPortMalloc( valueLength + 1 );
        memset( addressbuf, 0x00, valueLength + 1 );
        memcpy( addressbuf, value, valueLength );

        pucTargetInterface->pHostName = addressbuf;
        pucTargetInterface->hostNameLength = valueLength;
        xStatus = pdPASS;
    }

    value = NULL;
    valueLength = 0;

    result = JSON_Search( pcJSONFile,
                          ulJSONFileSize,
                          hostPortQuery,
                          hostPortQueryLength,
                          &value,
                          &valueLength );

    if( result == JSONSuccess )
    {
        /* TODO: Define the 10 in a macro. */
        pucTargetInterface->port = strtoul( value, NULL, 10 );
        xStatus = pdPASS;
    }

    return xStatus;
}

static BaseType_t _sendMessageToGGC( MQTTContext_t * pxMQTTContext )
{
    const char * pcTopic = ggdDEMO_MQTT_MSG_TOPIC;
    uint32_t ulMessageCounter;
    char cBuffer[ ggdDEMO_MAX_MQTT_MSG_SIZE ];
    MQTTStatus_t xResult;
    MQTTPublishInfo_t xMQTTPublishInfo;
    BaseType_t xStatus = pdPASS;

    /* Some fields are not used by this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xMQTTPublishInfo, 0x00, sizeof( xMQTTPublishInfo ) );

    /* This demo uses QoS0. */
    xMQTTPublishInfo.qos = MQTTQoS0;
    xMQTTPublishInfo.retain = false;
    xMQTTPublishInfo.pTopicName = pcTopic;
    xMQTTPublishInfo.topicNameLength = ( uint16_t ) strlen( pcTopic );

    for( ulMessageCounter = 0; ulMessageCounter < ( uint32_t ) ggdDEMO_MAX_MQTT_MESSAGES; ulMessageCounter++ )
    {
        xMQTTPublishInfo.pPayload = ( const void * ) cBuffer;
        xMQTTPublishInfo.payloadLength = ( uint32_t ) sprintf( cBuffer, ggdDEMO_MQTT_MSG_DISCOVERY, ( long unsigned int ) ulMessageCounter );


        /* Get a unique packet id. */
        usPublishPacketIdentifier = MQTT_GetPacketId( pxMQTTContext );

        /* Send PUBLISH packet. Packet ID is not used for a QoS1 publish. */
        xResult = MQTT_Publish( pxMQTTContext, &xMQTTPublishInfo, usPublishPacketIdentifier );

        if( xResult != MQTTSuccess )
        {
            xStatus = pdFAIL;
            LogError( ( "Failed to send PUBLISH message to broker: Topic=%s, Error=%s",
                        pcTopic,
                        MQTT_Status_strerror( xResult ) ) );
        }
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvConnectToServer( NetworkContext_t * pxNetworkContext,
                                      SocketsConfig_t * pxSocketsConfig,
                                      ServerInfo_t * pxServerInfo )
{
    BaseType_t xStatus = pdPASS;
    TransportSocketStatus_t xNetworkStatus = TRANSPORT_SOCKET_STATUS_SUCCESS;



    /* Establish a TLS session with the HTTP server. This example connects to
     * the HTTP server as specified in democonfigAWS_IOT_ENDPOINT and
     * democonfigAWS_HTTP_PORT in http_demo_mutual_auth_config.h. */
    LogInfo( ( "Establishing a TLS session to %.*s:%d.",
               ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
               clientcredentialMQTT_BROKER_ENDPOINT,
               clientcredentialGREENGRASS_DISCOVERY_PORT ) );

    /* Attempt to create a mutually authenticated TLS connection. */
    xNetworkStatus = SecureSocketsTransport_Connect( pxNetworkContext,
                                                     pxServerInfo,
                                                     pxSocketsConfig );

    if( xNetworkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS )
    {
        xStatus = pdFAIL;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvSendHttpRequest( const TransportInterface_t * pxTransportInterface,
                                      const char * pcMethod,
                                      size_t xMethodLen,
                                      const char * pcPath,
                                      size_t xPathLen,
                                      char ** ppcJSONFile,
                                      uint32_t * plJSONFileLength )
{
    /* Return value of this method. */
    BaseType_t xStatus = pdPASS;

    /* Configurations of the initial request headers that are passed to
     * #HTTPClient_InitializeRequestHeaders. */
    HTTPRequestInfo_t xRequestInfo;
    /* Represents a response returned from an HTTP server. */
    HTTPResponse_t xResponse;
    /* Represents header data that will be sent in an HTTP request. */
    HTTPRequestHeaders_t xRequestHeaders;

    /* Return value of all methods from the HTTP Client library API. */
    HTTPStatus_t xHTTPStatus = HTTPSuccess;

    configASSERT( pcMethod != NULL );
    configASSERT( pcPath != NULL );

    /* Initialize all HTTP Client library API structs to 0. */
    ( void ) memset( &xRequestInfo, 0, sizeof( xRequestInfo ) );
    ( void ) memset( &xResponse, 0, sizeof( xResponse ) );
    ( void ) memset( &xRequestHeaders, 0, sizeof( xRequestHeaders ) );

    /* Initialize the request object. */
    xRequestInfo.pHost = clientcredentialMQTT_BROKER_ENDPOINT;
    xRequestInfo.hostLen = strlen( clientcredentialMQTT_BROKER_ENDPOINT );
    xRequestInfo.pMethod = pcMethod;
    xRequestInfo.methodLen = xMethodLen;
    xRequestInfo.pPath = pcPath;
    xRequestInfo.pathLen = xPathLen;

    /* Set "Connection" HTTP header to "keep-alive" so that multiple requests
     * can be sent over the same established TCP connection. */
    xRequestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

    /* Set the buffer used for storing request headers. */
    xRequestHeaders.pBuffer = ucUserBuffer;
    xRequestHeaders.bufferLen = democonfigUSER_BUFFER_LENGTH;

    xHTTPStatus = HTTPClient_InitializeRequestHeaders( &xRequestHeaders,
                                                       &xRequestInfo );

    if( xHTTPStatus == HTTPSuccess )
    {
        /* Initialize the response object. The same buffer used for storing
         * request headers is reused here. */
        xResponse.pBuffer = ucUserBuffer;
        xResponse.bufferLen = democonfigUSER_BUFFER_LENGTH;

        LogInfo( ( "Sending HTTP %.*s request to %.*s%.*s...",
                   ( int32_t ) xRequestInfo.methodLen, xRequestInfo.pMethod,
                   ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                   clientcredentialMQTT_BROKER_ENDPOINT,
                   ( int32_t ) xRequestInfo.pathLen,
                   xRequestInfo.pPath ) );
        LogDebug( ( "Request Headers:\n%.*s\n"
                    "Request Body:\n%.*s\n",
                    ( int32_t ) xRequestHeaders.headersLen,
                    ( char * ) xRequestHeaders.pBuffer ) );

        /* Send the request and receive the response. */
        xHTTPStatus = HTTPClient_Send( pxTransportInterface,
                                       &xRequestHeaders,
                                       NULL,
                                       0,
                                       &xResponse,
                                       0 );
    }
    else
    {
        LogError( ( "Failed to initialize HTTP request headers: Error=%s.",
                    HTTPClient_strerror( xHTTPStatus ) ) );
    }

    if( xHTTPStatus == HTTPSuccess )
    {
        LogInfo( ( "Received HTTP response from %.*s%.*s...\n",
                   ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                   clientcredentialMQTT_BROKER_ENDPOINT,
                   ( int32_t ) xRequestInfo.pathLen,
                   xRequestInfo.pPath ) );
        LogInfo( ( "Response Headers:\n%.*s\n",
                   ( int32_t ) xResponse.headersLen, xResponse.pHeaders ) );
        LogInfo( ( "Status Code:\n%u\n",
                   xResponse.statusCode ) );
        LogDebug( ( "Response Body Length:%d\n",
                    ( int32_t ) xResponse.bodyLen ) );
        LogDebug( ( "Response Body:\n%.*s\n",
                    ( int32_t ) xResponse.bodyLen, xResponse.pBody ) );

        /* Set the output parameters. */
        *ppcJSONFile = xResponse.pBody;
        *plJSONFileLength = xResponse.bodyLen;
    }
    else
    {
        LogError( ( "Failed to send HTTP %.*s request to %.*s%.*s: Error=%s.",
                    ( int32_t ) xRequestInfo.methodLen, xRequestInfo.pMethod,
                    ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                    clientcredentialMQTT_BROKER_ENDPOINT,
                    ( int32_t ) xRequestInfo.pathLen, xRequestInfo.pPath,
                    HTTPClient_strerror( xHTTPStatus ) ) );
    }

    if( xHTTPStatus != HTTPSuccess )
    {
        xStatus = pdFAIL;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvGetGGCoreJSON( char ** ppcJSONFile,
                                    uint32_t * plJSONFileLength )
{
    BaseType_t xStatus = pdFAIL;
    /* The transport layer interface used by the HTTP Client library. */
    TransportInterface_t xTransportInterface = { 0 };

    /* The network context for the transport layer interface. */
    NetworkContext_t xNetworkContext = { 0 };

    /* Configure credentials for TLS mutual authenticated session. */
    SocketsConfig_t xSocketsConfig = { 0 };

    xSocketsConfig.enableTls = true;
    xSocketsConfig.pAlpnProtos = NULL;
    xSocketsConfig.maxFragmentLength = 0;
    xSocketsConfig.disableSni = false;
    xSocketsConfig.pRootCa = tlsATS1_ROOT_CERTIFICATE_PEM;
    xSocketsConfig.rootCaSize = sizeof( tlsATS1_ROOT_CERTIFICATE_PEM );
    xSocketsConfig.sendTimeoutMs = 1000;
    xSocketsConfig.recvTimeoutMs = 1000;

    /* Initializer server information. */
    ServerInfo_t xServerInfo;

    xServerInfo.pHostName = clientcredentialMQTT_BROKER_ENDPOINT;
    xServerInfo.hostNameLength = strlen( clientcredentialMQTT_BROKER_ENDPOINT );
    xServerInfo.port = clientcredentialGREENGRASS_DISCOVERY_PORT;

    BaseType_t xIsConnectionEstablished = pdFALSE;
    SecureSocketsTransportParams_t secureSocketsTransportParams = { 0 };

    xNetworkContext.pParams = &secureSocketsTransportParams;

    /**************************** Connect. ******************************/

    /* Attempt to connect to the HTTP server. If connection fails, retry
     * after a timeout. The timeout value will be exponentially increased
     * until either the maximum number of attempts or the maximum timeout
     * value is reached. The function returns pdFAIL if the TCP connection
     * cannot be established with the broker after the configured number of
     * attempts. */
    xStatus = connectToServerWithBackoffRetries( prvConnectToServer,
                                                 &xNetworkContext,
                                                 &xSocketsConfig,
                                                 &xServerInfo );

    if( xStatus == pdPASS )
    {
        /* Set a flag indicating that a TLS connection exists. */
        xIsConnectionEstablished = pdTRUE;

        /* Define the transport interface. */
        xTransportInterface.pNetworkContext = &xNetworkContext;
        xTransportInterface.send = SecureSocketsTransport_Send;
        xTransportInterface.recv = SecureSocketsTransport_Recv;

        LogInfo( ( "Connection established....\r\n" ) );
    }
    else
    {
        /* Log error to indicate connection failure after all
         * reconnect attempts are over. */
        LogError( ( "Failed to connect to HTTP server %.*s.",
                    ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                    clientcredentialMQTT_BROKER_ENDPOINT ) );
    }

    /* Send HTTP requst to retrieve the JSON.*/
    if( xStatus == pdPASS )
    {
        xStatus = prvSendHttpRequest( &xTransportInterface,
                                      HTTP_METHOD_GET,
                                      strlen( HTTP_METHOD_GET ),
                                      ggdDEMOHTTP_PATH,
                                      strlen( ggdDEMOHTTP_PATH ),
                                      ppcJSONFile,
                                      plJSONFileLength );
    }

    /* Disconnect the connection from the AWS IoT broker endpoint. */
    if( xIsConnectionEstablished == pdTRUE )
    {
        SecureSocketsTransport_Disconnect( &xNetworkContext );
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

static int _discoverGreengrassCoreDemo()
{
    int status = EXIT_SUCCESS;
    MQTTStatus_t xResult;
    BaseType_t xDemoStatus = pdFAIL;
    NetworkContext_t xNetworkContext = { 0 };
    SecureSocketsTransportParams_t secureSocketsTransportParams = { 0 };
    bool xSessionPresent;
    char * pcJSONFile = NULL;
    uint32_t ulJSONFileLength = 0;

    xNetworkContext.pParams = &secureSocketsTransportParams;

    MQTTContext_t xMQTTContext;
    MQTTConnectInfo_t xConnectInfo;
    TransportInterface_t xTransport;
    BaseType_t xStatus = pdFAIL;

    xTransport.pNetworkContext = &xNetworkContext;
    xTransport.send = SecureSocketsTransport_Send;
    xTransport.recv = SecureSocketsTransport_Recv;

    /* Set the credentials for establishing a TLS connection. */
    /* Initializer server information. */
    ServerInfo_t xServerInfo = { 0 };

    xServerInfo.pHostName = democonfigMQTT_BROKER_ENDPOINT;
    xServerInfo.hostNameLength = strlen( democonfigMQTT_BROKER_ENDPOINT );
    xServerInfo.port = democonfigMQTT_BROKER_PORT;

    /* Configure credentials for TLS mutual authenticated session. */
    SocketsConfig_t xSocketsConfig = { 0 };

    xSocketsConfig.enableTls = true;
    xSocketsConfig.pAlpnProtos = NULL;
    xSocketsConfig.maxFragmentLength = 0;
    xSocketsConfig.disableSni = true;
    /* TODO: Set proper timeouts with defines */
    xSocketsConfig.sendTimeoutMs = 60;
    xSocketsConfig.recvTimeoutMs = 60;

    /* Demonstrate automated connection. */
    LogInfo( ( "Attempting automated selection of Greengrass device\r\n" ) );

    /* Retrieve the Greengrass core connection details as a JSON by sending an
     * HTTP GET request. */
    xDemoStatus = prvGetGGCoreJSON( &pcJSONFile, &ulJSONFileLength );

    /* Parse the JSON to obtain the IP address, port and credentials of the
     * Green grass core. */
    if( xDemoStatus == pdPASS )
    {
        if( prvGGDGetCertificate( pcJSONFile,
                                  ulJSONFileLength,
                                  &xSocketsConfig.pRootCa,
                                  &xSocketsConfig.rootCaSize ) == pdFAIL )
        {
            xDemoStatus = pdFAIL;
        }
    }

    /* Parse the JSON to obtain the IP address, port and credentials of the
     * Green grass core. */
    if( xDemoStatus == pdPASS )
    {
        if( prvGGDGetIPOnInterface( pcJSONFile,
                                    ulJSONFileLength,
                                    &xServerInfo ) == pdFAIL )
        {
            xDemoStatus = pdFAIL;
        }
    }

    xStatus = connectToServerWithBackoffRetries( prvConnectToServer,
                                                 &xNetworkContext,
                                                 &xSocketsConfig,
                                                 &xServerInfo );

    xResult = MQTT_Init( &xMQTTContext, &xTransport, prvGetTimeMs, prvEventCallback, &xBuffer );
    configASSERT( xResult == MQTTSuccess );

    /* Some fields are not used in this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = true;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = democonfigCLIENT_IDENTIFIER;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( democonfigCLIENT_IDENTIFIER );

    /* Use the metrics string as username to report the OS and MQTT client version
     * metrics to AWS IoT. */
    xConnectInfo.pUserName = AWS_IOT_METRICS_STRING;
    xConnectInfo.userNameLength = AWS_IOT_METRICS_STRING_LENGTH;

    /* Set MQTT keep-alive period. If the application does not send packets at an interval less than
     * the keep-alive period, the MQTT library will send PINGREQ packets. */
    xConnectInfo.keepAliveSeconds = ggdmqttKEEP_ALIVE_TIMEOUT_SECONDS;

    /* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it
     * is passed as NULL. */
    xResult = MQTT_Connect( &xMQTTContext,
                            &xConnectInfo,
                            NULL,
                            ggdmqttCONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );

    if( xResult != MQTTSuccess )
    {
        LogError( ( "Failed to establish MQTT connection: Server=%s, MQTTStatus=%s",
                    democonfigMQTT_BROKER_ENDPOINT, MQTT_Status_strerror( xResult ) ) );
        xDemoStatus = pdFAIL;
    }

    if( xDemoStatus == pdPASS )
    {
        _sendMessageToGGC( &xMQTTContext );

        LogInfo( ( "Disconnecting from broker." ) );

        xResult = MQTT_Disconnect( &xMQTTContext );
        SecureSocketsTransport_Disconnect( &xNetworkContext );
        LogInfo( ( "Disconnected from the broker." ) );
    }
    else
    {
        LogError( ( "Auto-connect: Failed to retrieve Greengrass address and certificate." ) );
        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

int vStartGreenGrassDiscoveryTask( bool awsIotMqttMode,
                                   const char * pIdentifier,
                                   void * pNetworkServerInfo,
                                   void * pNetworkCredentialInfo,
                                   void * pNetworkInterface )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    /* Unused parameters */
    ( void ) awsIotMqttMode;
    ( void ) pIdentifier;
    ( void ) pNetworkServerInfo;
    ( void ) pNetworkCredentialInfo;
    ( void ) pNetworkInterface;

    status = _discoverGreengrassCoreDemo();

    return status;
}
