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

/* Demo include. */
#include "aws_demo_config.h"

/* Temporary include for demo config TODO: Remove */
#include "mqtt_demo_mutual_auth_config.h"

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "platform/iot_network.h"
#include "platform/iot_network_freertos.h"
#include "platform/iot_clock.h"

/* Include AWS IoT metrics macros header. */
#include "aws_iot_metrics.h"


/* Common HTTP demo utilities. */
#include "http_demo_utils.h"

/* Transport interface implementation include header for TLS. */
#include "transport_secure_sockets.h"

/* Greengrass includes. */
#include "aws_ggd_config.h"
#include "aws_ggd_config_defaults.h"
#include "aws_greengrass_discovery.h"

/* JSON includes. */
#include "core_json.h"

/* Includes for initialization. */
#include "core_mqtt.h"
#include "iot_mqtt.h"

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
    #define democonfigMQTT_BROKER_PORT         clientcredentialMQTT_BROKER_PORT
#endif

#define mqttexampleCONNACK_RECV_TIMEOUT_MS     ( 1000U )
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

#define ggdDEMOHTTP_PATH                         "/greengrass/discover/thing/" clientcredentialIOT_THING_NAME

#define mqttexampleKEEP_ALIVE_TIMEOUT_SECONDS    ( 60U )


/**
 * @brief Contains the user data for callback processing.
 */
typedef struct
{
    const char * pcExpectedString;      /**< Informs the MQTT callback of the next expected string. */
    BaseType_t xCallbackStatus;         /**< Used to communicate the success or failure of the callback function.
                                         * xCallbackStatus is set to pdFALSE before the callback is executed, and is
                                         * set to pdPASS inside the callback only if the callback receives the expected
                                         * data. */
    SemaphoreHandle_t xWakeUpSemaphore; /**< Handle of semaphore to wake up the task. */
    char * pcTopic;                     /**< Topic to subscribe and publish to. */
} GGDUserData_t;

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

/* The maximum time to wait for an MQTT operation to complete.  Needs to be
 * long enough for the TLS negotiation to complete. */
static const uint32_t _maxCommandTimeMs = 20000UL;
static const uint32_t _timeBetweenPublishMs = 1500UL;

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
 * @brief Connect to HTTP server with reconnection retries.
 *
 * @param[out] pxNetworkContext The output parameter to return the created network context.
 *
 * @return pdPASS on successful connection, pdFAIL otherwise.
 */
static BaseType_t prvConnectToServer( NetworkContext_t * pxNetworkContext );

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

static IotMqttError_t _mqttConnect( GGD_HostAddressData_t * pxHostAddressData,
                                    const IotNetworkInterface_t * pNetworkInterface,
                                    IotMqttConnection_t * pMqttConnection );
static void _sendMessageToGGC( IotMqttConnection_t mqttConnection );
static int _discoverGreengrassCore( const IotNetworkInterface_t * pNetworkInterface );

/*-----------------------------------------------------------*/
static BaseType_t prvGGDGetCertificate( char * pcJSONFile,
                                        const uint32_t ulJSONFileSize,
                                        const HostParameters_t * pxHostParameters,
                                        const BaseType_t xAutoSelectFlag,
                                        GGD_HostAddressData_t * pxHostAddressData );
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
                                        const uint32_t ulJSONFileSize,
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
    uint32_t ulReadIndex = 1, ulWriteIndex = 0;

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
    BaseType_t xFoundIP = pdFALSE;
    BaseType_t xFoundPort = pdFALSE;
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

        /*pucTargetInterface->pHostName = FreeRTOS_inet_addr( addressbuf ); */
        pucTargetInterface->pHostName = addressbuf;
        pucTargetInterface->hostNameLength = valueLength;
        xStatus = pdPASS;
        xFoundIP = pdTRUE;
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
        xFoundIP = pdTRUE;
    }

    return xStatus;
}

static void _sendMessageToGGC( IotMqttConnection_t mqttConnection )
{
    const char * pcTopic = ggdDEMO_MQTT_MSG_TOPIC;
    uint32_t ulMessageCounter;
    char cBuffer[ ggdDEMO_MAX_MQTT_MSG_SIZE ];
    IotMqttError_t xMqttStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttPublishInfo_t xPublishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;


    /* Publish to the topic to which this task is subscribed in order
     * to receive back the data that was published. */
    xPublishInfo.qos = IOT_MQTT_QOS_0;
    xPublishInfo.pTopicName = pcTopic;
    xPublishInfo.topicNameLength = ( uint16_t ) ( strlen( pcTopic ) );

    for( ulMessageCounter = 0; ulMessageCounter < ( uint32_t ) ggdDEMO_MAX_MQTT_MESSAGES; ulMessageCounter++ )
    {
        /* Set the members of the publish info. */
        xPublishInfo.payloadLength = ( uint32_t ) sprintf( cBuffer, ggdDEMO_MQTT_MSG_DISCOVERY, ( long unsigned int ) ulMessageCounter ); /*lint !e586 sprintf can be used for specific demo. */
        xPublishInfo.pPayload = ( const void * ) cBuffer;

        /* Call the MQTT blocking PUBLISH function. */
        xMqttStatus = IotMqtt_TimedPublish( mqttConnection,
                                            &xPublishInfo,
                                            0,
                                            _maxCommandTimeMs );

        if( xMqttStatus != IOT_MQTT_SUCCESS )
        {
            IotLogError( "mqtt_client - Failure to publish. Error %s.",
                         IotMqtt_strerror( xMqttStatus ) );
        }

        IotClock_SleepMs( _timeBetweenPublishMs );
    }
}

/*-----------------------------------------------------------*/

static IotMqttError_t _mqttConnect( GGD_HostAddressData_t * pxHostAddressData,
                                    const IotNetworkInterface_t * pNetworkInterface,
                                    IotMqttConnection_t * pMqttConnection )
{
    IotMqttError_t xMqttStatus = IOT_MQTT_STATUS_PENDING;
    IotNetworkServerInfo_t xServerInfo = { 0 };
    IotNetworkCredentials_t xCredentials = AWS_IOT_NETWORK_CREDENTIALS_AFR_INITIALIZER;
    IotMqttNetworkInfo_t xNetworkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t xMqttConnectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    uint32_t connectAttempt = 0;

    /* Set the server certificate for a secured connection. Other credentials
     * are set by the initializer. */
    xCredentials.pRootCa = pxHostAddressData->pcCertificate;
    xCredentials.rootCaSize = ( size_t ) pxHostAddressData->ulCertificateSize;
    /* Disable SNI. */
    xCredentials.disableSni = true;
    /* ALPN is not needed. */
    xCredentials.pAlpnProtos = NULL;

    /* Set the server info. */
    xServerInfo.pHostName = pxHostAddressData->pcHostAddress;
    xServerInfo.port = clientcredentialMQTT_BROKER_PORT;

    /* Set the members of the network info. */
    xNetworkInfo.createNetworkConnection = true;
    xNetworkInfo.u.setup.pNetworkServerInfo = &xServerInfo;
    xNetworkInfo.u.setup.pNetworkCredentialInfo = &xCredentials;
    xNetworkInfo.pNetworkInterface = pNetworkInterface;

    /* Connect to the broker. */
    xMqttConnectInfo.awsIotMqttMode = true;
    xMqttConnectInfo.cleanSession = true;
    xMqttConnectInfo.pClientIdentifier = ( const char * ) ( clientcredentialIOT_THING_NAME );
    xMqttConnectInfo.clientIdentifierLength = ( uint16_t ) ( strlen( clientcredentialIOT_THING_NAME ) );
    xMqttConnectInfo.keepAliveSeconds = ggdDEMO_KEEP_ALIVE_INTERVAL_SECONDS;

    /* Call MQTT's CONNECT function. */
    for( connectAttempt = 0; connectAttempt < ggdDEMO_NUM_TRIES; connectAttempt++ )
    {
        if( connectAttempt > 0 )
        {
            IotLogError( "Failed to establish MQTT connection, retrying in %d ms.",
                         ggdDEMO_RETRY_WAIT_MS );
            IotClock_SleepMs( ggdDEMO_RETRY_WAIT_MS );
        }

        IotLogInfo( "Attempting to establish MQTT connection to Greengrass." );
        xMqttStatus = IotMqtt_Connect( &xNetworkInfo,
                                       &xMqttConnectInfo,
                                       _maxCommandTimeMs,
                                       pMqttConnection );

        if( xMqttStatus != IOT_MQTT_NETWORK_ERROR )
        {
            break;
        }
    }

    return xMqttStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvConnectToServer( NetworkContext_t * pxNetworkContext )
{
    ServerInfo_t xServerInfo = { 0 };
    SocketsConfig_t xSocketsConfig = { 0 };
    BaseType_t xStatus = pdPASS;
    TransportSocketStatus_t xNetworkStatus = TRANSPORT_SOCKET_STATUS_SUCCESS;

    /* Initializer server information. */
    xServerInfo.pHostName = clientcredentialMQTT_BROKER_ENDPOINT;
    xServerInfo.hostNameLength = strlen( clientcredentialMQTT_BROKER_ENDPOINT );
    xServerInfo.port = clientcredentialGREENGRASS_DISCOVERY_PORT;

    /* Configure credentials for TLS mutual authenticated session. */
    xSocketsConfig.enableTls = true;
    xSocketsConfig.pAlpnProtos = NULL;
    xSocketsConfig.maxFragmentLength = 0;
    xSocketsConfig.disableSni = false;
    xSocketsConfig.pRootCa = tlsATS1_ROOT_CERTIFICATE_PEM;
    xSocketsConfig.rootCaSize = sizeof( tlsATS1_ROOT_CERTIFICATE_PEM );
    xSocketsConfig.sendTimeoutMs = 1000;
    xSocketsConfig.recvTimeoutMs = 1000;

    /* Establish a TLS session with the HTTP server. This example connects to
     * the HTTP server as specified in democonfigAWS_IOT_ENDPOINT and
     * democonfigAWS_HTTP_PORT in http_demo_mutual_auth_config.h. */
    IotLogInfo( "Establishing a TLS session to %.*s:%d.",
                ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                clientcredentialMQTT_BROKER_ENDPOINT,
                clientcredentialGREENGRASS_DISCOVERY_PORT );

    /* Attempt to create a mutually authenticated TLS connection. */
    xNetworkStatus = SecureSocketsTransport_Connect( pxNetworkContext,
                                                     &xServerInfo,
                                                     &xSocketsConfig );

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

        IotLogInfo( "Sending HTTP %.*s request to %.*s%.*s...",
                    ( int32_t ) xRequestInfo.methodLen, xRequestInfo.pMethod,
                    ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                    clientcredentialMQTT_BROKER_ENDPOINT,
                    ( int32_t ) xRequestInfo.pathLen,
                    xRequestInfo.pPath );
        IotLogDebug( "Request Headers:\n%.*s\n"
                     "Request Body:\n%.*s\n",
                     ( int32_t ) xRequestHeaders.headersLen,
                     ( char * ) xRequestHeaders.pBuffer );

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
        IotLogError( "Failed to initialize HTTP request headers: Error=%s.",
                     HTTPClient_strerror( xHTTPStatus ) );
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
                                                 &xNetworkContext );

    if( xStatus == pdPASS )
    {
        /* Set a flag indicating that a TLS connection exists. */
        xIsConnectionEstablished = pdTRUE;

        /* Define the transport interface. */
        xTransportInterface.pNetworkContext = &xNetworkContext;
        xTransportInterface.send = SecureSocketsTransport_Send;
        xTransportInterface.recv = SecureSocketsTransport_Recv;

        IotLogInfo( "Connection established....\r\n" );
    }
    else
    {
        /* Log error to indicate connection failure after all
         * reconnect attempts are over. */
        IotLogError( "Failed to connect to HTTP server %.*s.",
                     ( int32_t ) strlen( clientcredentialMQTT_BROKER_ENDPOINT ),
                     clientcredentialMQTT_BROKER_ENDPOINT );
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

static BaseType_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pxNetworkContext )
{
    ServerInfo_t xServerInfo = { 0 };

    SocketsConfig_t xSocketsConfig = { 0 };
    BaseType_t xStatus = pdPASS;
    TransportSocketStatus_t xNetworkStatus = TRANSPORT_SOCKET_STATUS_SUCCESS;
    BaseType_t xBackoffStatus = pdFALSE;
    BaseType_t xDemoStatus = pdFALSE;
    char * pcJSONFile = NULL;
    uint32_t * ulJSONFileLength = 0;


    /* Set the credentials for establishing a TLS connection. */
    /* Initializer server information. */
    xServerInfo.pHostName = democonfigMQTT_BROKER_ENDPOINT;
    xServerInfo.hostNameLength = strlen( democonfigMQTT_BROKER_ENDPOINT );
    xServerInfo.port = democonfigMQTT_BROKER_PORT;

    /* Configure credentials for TLS mutual authenticated session. */
    xSocketsConfig.enableTls = true;
    xSocketsConfig.pAlpnProtos = NULL;
    xSocketsConfig.maxFragmentLength = 0;
    xSocketsConfig.disableSni = true;
    /* TODO: Set proper timeouts with defines */
    xSocketsConfig.sendTimeoutMs = 60;
    xSocketsConfig.recvTimeoutMs = 60;

    /* Demonstrate automated connection. */
    IotLogInfo( "Attempting automated selection of Greengrass device\r\n" );

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

    /* Establish a TLS session with the MQTT broker. This example connects to
     * the MQTT broker as specified in democonfigMQTT_BROKER_ENDPOINT and
     * democonfigMQTT_BROKER_PORT at the top of this file. */
    LogInfo( ( "Creating a TLS connection to %s:%u.",
               democonfigMQTT_BROKER_ENDPOINT,
               democonfigMQTT_BROKER_PORT ) );
    /* Attempt to create a mutually authenticated TLS connection. */
    xNetworkStatus = SecureSocketsTransport_Connect( pxNetworkContext,
                                                     &xServerInfo,
                                                     &xSocketsConfig );

    if( xNetworkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS )
    {
        LogError( ( "Connection to the broker failed. Attempting connection retry after backoff delay." ) );
        xStatus = pdFAIL;
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

static int _discoverGreengrassCore( const IotNetworkInterface_t * pNetworkInterface )
{
    int status = EXIT_SUCCESS;
    MQTTStatus_t xResult;
    BaseType_t xDemoStatus = pdFAIL;
    IotMqttError_t mqttStatus = IOT_MQTT_SUCCESS;
    GGD_HostAddressData_t xHostAddressData = { 0 };
    HostParameters_t xHostParameters;
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;
    TransportInterface_t xTransportInterface = { 0 };
    NetworkContext_t xNetworkContext = { 0 };
    SecureSocketsTransportParams_t secureSocketsTransportParams = { 0 };
    bool xSessionPresent;

    xNetworkContext.pParams = &secureSocketsTransportParams;
    xTransportInterface.pNetworkContext = &xNetworkContext;
    xTransportInterface.send = SecureSocketsTransport_Send;
    xTransportInterface.recv = SecureSocketsTransport_Recv;

    MQTTContext_t xMQTTContext;
    MQTTConnectInfo_t xConnectInfo;
    TransportInterface_t xTransport;
    BaseType_t xStatus = pdFAIL;

    xTransport.pNetworkContext = &xNetworkContext;
    xTransport.send = SecureSocketsTransport_Send;
    xTransport.recv = SecureSocketsTransport_Recv;

    /* TODO: Make this static buffer like other demos. */
    char * xBuffer = pvPortMalloc( 24000 );

    xDemoStatus = prvConnectToServerWithBackoffRetries( &xNetworkContext );

    xResult = MQTT_Init( &xMQTTContext, &xTransport, NULL, NULL, &xBuffer );
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
    xConnectInfo.keepAliveSeconds = mqttexampleKEEP_ALIVE_TIMEOUT_SECONDS;

    /* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it
     * is passed as NULL. */
    xResult = MQTT_Connect( &xMQTTContext,
                            &xConnectInfo,
                            NULL,
                            mqttexampleCONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );

    if( xResult != MQTTSuccess )
    {
        LogError( ( "Failed to establish MQTT connection: Server=%s, MQTTStatus=%s",
                    democonfigMQTT_BROKER_ENDPOINT, MQTT_Status_strerror( xResult ) ) );
    }
    else
    {
        /* Successfully established and MQTT connection with the broker. */
        LogInfo( ( "An MQTT connection is established with %s.", democonfigMQTT_BROKER_ENDPOINT ) );
        xStatus = pdPASS;
    }

    if( xDemoStatus == pdPASS )
    {
        LogInfo( ( "Greengrass device address is %s:%d.\n",
                   xHostAddressData.pcHostAddress,
                   xHostAddressData.usPort ) );
        mqttStatus = _mqttConnect( &xHostAddressData, pNetworkInterface, &mqttConnection );

        if( mqttStatus == IOT_MQTT_SUCCESS )
        {
            _sendMessageToGGC( mqttConnection );

            IotLogInfo( "Disconnecting from broker." );

            /* Call MQTT v2's DISCONNECT function. */
            IotMqtt_Disconnect( mqttConnection, 0 );
            SecureSocketsTransport_Disconnect( &xNetworkContext );
            mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;
            IotLogInfo( "Disconnected from the broker." );
        }
        else
        {
            IotLogError( "Could not connect to the Broker. Error %s.",
                         IotMqtt_strerror( mqttStatus ) );
            status = EXIT_FAILURE;
        }

        /* Report on space efficiency of this demo task. */
        #if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )
            {
                configPRINTF( ( "Heap low watermark: %u. Stack high watermark: %u.\r\n",
                                xPortGetMinimumEverFreeHeapSize(),
                                uxTaskGetStackHighWaterMark( NULL ) ) );
            }
        #endif
    }
    else
    {
        IotLogError( "Auto-connect: Failed to retrieve Greengrass address and certificate." );
        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

int vStartGreenGrassDiscoveryTask( bool awsIotMqttMode,
                                   const char * pIdentifier,
                                   void * pNetworkServerInfo,
                                   void * pNetworkCredentialInfo,
                                   const IotNetworkInterface_t * pNetworkInterface )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;
    IotMqttError_t mqttInitStatus = IOT_MQTT_SUCCESS;

    /* Unused parameters */
    ( void ) awsIotMqttMode;
    ( void ) pIdentifier;
    ( void ) pNetworkServerInfo;
    ( void ) pNetworkCredentialInfo;

    mqttInitStatus = IotMqtt_Init();

    if( mqttInitStatus == IOT_MQTT_SUCCESS )
    {
        status = _discoverGreengrassCore( pNetworkInterface );
        IotMqtt_Cleanup();
        IotLogInfo( "Cleaned up MQTT library." );
    }
    else
    {
        IotLogError( "Failed to initialize MQTT library." );
        status = EXIT_FAILURE;
    }

    return status;
}
