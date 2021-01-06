/*
 * FreeRTOS Greengrass V2.0.2
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
 * @file aws_greengrass_discovery.c
 * @brief API for Green grass Discovery. Provides API to Fetch and parse the
 * JSON file that contains the greenGrass code info.
 */


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Secure sockets includes. */
#include "iot_secure_sockets.h"

/* Greengrass includes. */
#include "aws_ggd_config.h"
#include "aws_ggd_config_defaults.h"
#include "aws_greengrass_discovery.h"
#include "aws_helper_secure_connect.h"
#include "core_json.h"

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Discovery: Strings for JSON file parsing.
 *
 * Below are JSON file sections used by the parsing tool to recover
 * the certificate and IP address.
 */
/** @{ */
#define ggdJSON_FILE_GROUPID         "GGGroupId"
#define ggdJSON_FILE_THING_ARN       "thingArn"
#define ggdJSON_FILE_HOST_ADDRESS    "HostAddress"
#define ggdJSON_FILE_CERTIFICATE     "CAs"
#define ggdJSON_FILE_PORT_NUMBER     "PortNumber"
/** @} */

/**
 * @brief HTTP command to retrieve JSON file from the Cloud.
 */
#define ggdCLOUD_DISCOVERY_ADDRESS      \
    "GET /greengrass/discover/thing/%s" \
    " HTTP/1.1\r\n\r\n"

#define ggJSON_CONVERTION_RADIX    10

/**
 * @brief HTTP field to get the length of the JSON file.
 *
 * The server respond with the JSON file encapsulated in
 * a HTTP header. The header is parsed bytes per bytes until
 * "content-length:" is found. then the length string is stored
 * into a temporary buffer of the ggJSON_PARSING_TMP_BUFFER_SIZE
 * size. Then a string to int conversion is performed to get the size
 * of the JSON file.
 */
/** @{ */
#define ggdHTTP_CONTENT_LENGTH_STRING     "content-length:"
#define ggJSON_PARSING_TMP_BUFFER_SIZE    10
/** @} */

/**
 * @brief Size of the IP address character string
 *
 * IP address is 15 character + the null termination.
 */
#define ggdJSON_IP_ADDRESS_SIZE    16

/**
 * @brief Loop back IP
 *
 * Discarded when Parsing JSON file as potential IP to connect to.
 */
#define ggdLOOP_BACK_IP            "127.0.0.1"

/**
 * @brief JSON parsing helper functions.
 *
 * The functions declared below are used to make parsing of JSON file a bit easier.
 * The code is broken down into tiny functions to limit code complexity and limit
 * duplicate code between auto connect and custom connect.
 */
/** @{ */
static BaseType_t prvGGDGetCertificate( char * pcJSONFile,
                                        const uint32_t ulJSONFileSize,
                                        const HostParameters_t * pxHostParameters,
                                        const BaseType_t xAutoSelectFlag,
                                        GGD_HostAddressData_t * pxHostAddressData );

static BaseType_t prvGGDGetIPOnInterface( char * pcJSONFile,
                                          const uint32_t ulJSONFileSize,
                                          const uint8_t ucTargetInterface,
                                          GGD_HostAddressData_t * pxHostAddressData );
static BaseType_t prvGGDGetCore( const char * pcJSONFile,
                                 const uint32_t ulJSONFileSize,
                                 const HostParameters_t * pxHostParameters,
                                 const BaseType_t xAutoSelectFlag,
                                 GGD_HostAddressData_t * pxHostAddressData );
static BaseType_t prvIsIPvalid( const char * pcIP,
                                uint32_t ulIPlength );
/** @} */

/**
 * @brief Search for length field in server HTTP response
 *
 * Small helper function that return true when the
 * length field is found in the html header.
 */
static BaseType_t prvCheckForContentLengthString( uint8_t * pucIndex,
                                                  const char cNewChar ); /*lint !e971 can use char without signed/unsigned. */

/*-----------------------------------------------------------*/

BaseType_t GGD_GetGGCIPandCertificate( const char * pcHostAddress,
                                       uint16_t usGGDPort,
                                       const char * pcThingName,
                                       char * pcBuffer, /*lint !e971 can use char without signed/unsigned. */
                                       const uint32_t ulBufferSize,
                                       GGD_HostAddressData_t * pxHostAddressData )
{
    Socket_t xSocket;
    uint32_t ulJSONFileSize = 0;
    BaseType_t xJSONFileRetrieveCompleted = pdFALSE;
    uint32_t ulByteRead = 0;
    BaseType_t xStatus;
    HostParameters_t xHostParameters;


    configASSERT( pxHostAddressData != NULL );

    configASSERT( pcBuffer != NULL );

    xStatus = GGD_JSONRequestStart( pcHostAddress, usGGDPort, pcThingName, &xSocket );

    if( xStatus == pdPASS )
    {
        xStatus = GGD_JSONRequestGetSize( &xSocket, &ulJSONFileSize );
    }

    if( xStatus == pdPASS )
    {
        /* Loop until the full JSON is retrieved. */
        do
        {
            xStatus = GGD_JSONRequestGetFile( &xSocket,
                                              &pcBuffer[ ulByteRead ],
                                              ulBufferSize - ulByteRead,
                                              &ulByteRead,
                                              &xJSONFileRetrieveCompleted,
                                              ulJSONFileSize ); /*lint !e644 ulJSONFileSize has been initialized if code reaches here. */
        }
        while( ( xStatus == pdPASS ) && ( xJSONFileRetrieveCompleted != pdTRUE ) && ( ulBufferSize - ulByteRead ) > 0 );

        /* If the JSON file was not completely received and there
         * is no space left in the buffer, it means that the buffer
         * is not large enough to hold the complete GreenGrass
         * Discovery document. The user should increase the size of
         * the buffer. */
        if( ( xJSONFileRetrieveCompleted == pdFALSE ) && ( ( ulBufferSize - ulByteRead ) == 0 ) )
        {
            ggdconfigPRINT( "[ERROR] The supplied buffer is not large enough to hold the GreenGrass discovery document. \r\n" );
            ggdconfigPRINT( "[ERROR] Consider increasing the size of the supplied buffer. \r\n" );
        }

        if( xSocket != SOCKETS_INVALID_SOCKET ) /* Check connection is closed. */
        {
            GGD_JSONRequestAbort( &xSocket );
            xStatus = pdFAIL;
        }

        if( xJSONFileRetrieveCompleted != pdTRUE )
        {
            xStatus = pdFAIL;
        }
    }

    if( xStatus == pdPASS )
    {
        xStatus = GGD_GetIPandCertificateFromJSON( pcBuffer,
                                                   ulJSONFileSize - 1,
                                                   &xHostParameters,
                                                   pxHostAddressData,
                                                   pdTRUE );
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

BaseType_t GGD_JSONRequestStart( const char * pcHostAddress,
                                 uint16_t usGGDPort,
                                 const char * pcThingName,
                                 Socket_t * pxSocket )
{
    GGD_HostAddressData_t xHostAddressData;
    char * pcHttpGetRequest = NULL;
    uint32_t ulHttpGetLength = 0;
    uint32_t ulCharsWritten = 0;
    BaseType_t xStatus;

    configASSERT( pcHostAddress != NULL );
    configASSERT( pcThingName != NULL );
    configASSERT( pxSocket != NULL );

    xHostAddressData.pcHostAddress = pcHostAddress;              /*lint !e971 can use char without signed/unsigned. */
    xHostAddressData.ulHostAddressLen = strlen( pcHostAddress ); /*lint !e971 can use char without signed/unsigned. */
    xHostAddressData.pcCertificate = NULL;                       /* Use default certificate. */
    xHostAddressData.ulCertificateSize = 0;
    xHostAddressData.usPort = usGGDPort;

    /* Establish secure connection. */
    xStatus = GGD_SecureConnect_Connect( &xHostAddressData,
                                         pxSocket,
                                         ggdconfigTCP_RECEIVE_TIMEOUT_MS,
                                         ggdconfigTCP_SEND_TIMEOUT_MS );

    if( xStatus == pdPASS )
    {
        /* Build the HTTP GET request string that is specific to this host. */
        ulHttpGetLength = 1 + strlen( ggdCLOUD_DISCOVERY_ADDRESS ) +
                          strlen( pcThingName );
        pcHttpGetRequest = pvPortMalloc( ulHttpGetLength );

        if( NULL == pcHttpGetRequest )
        {
            xStatus = pdFAIL;
        }
        else
        {
            ulCharsWritten = snprintf( pcHttpGetRequest,
                                       ulHttpGetLength,
                                       ggdCLOUD_DISCOVERY_ADDRESS,
                                       pcThingName );

            if( ulCharsWritten >= ulHttpGetLength )
            {
                xStatus = pdFAIL;
            }
            else
            {
                pcHttpGetRequest[ ulHttpGetLength - 1 ] = '\0';

                /* Send HTTP request over secure connection (HTTPS) to get the GGC JSON file. */
                xStatus = GGD_SecureConnect_Send( pcHttpGetRequest,
                                                  ulCharsWritten,
                                                  *pxSocket );

                if( xStatus == pdFAIL )
                {
                    /* Don't forget to close the connection. */
                    GGD_SecureConnect_Disconnect( pxSocket );
                    ggdconfigPRINT( "JSON request failed\r\n" );
                }
            }
        }
    }
    else
    {
        ggdconfigPRINT( "JSON request could not connect to end point\r\n" );
    }

    if( NULL != pcHttpGetRequest )
    {
        vPortFree( pcHttpGetRequest );
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

BaseType_t GGD_JSONRequestGetSize( Socket_t * pxSocket,
                                   uint32_t * pulJSONFileSize )
{
    BaseType_t xStatus = pdFAIL;
    BaseType_t xReadStatus = pdFAIL;
    char cBuffer[ ggJSON_PARSING_TMP_BUFFER_SIZE ]; /*lint !e971 can use char without signed/unsigned. */
    char cReadChar;                                 /*lint !e971 can use char without signed/unsigned. */
    uint32_t ulReadSize;
    uint8_t ucContentLengthIndex = 0;
    uint8_t ucLengthStrIndex;


    /****** Parse the header until the begining of the content length string is found. *****/
    configASSERT( pxSocket != NULL );
    configASSERT( pulJSONFileSize != NULL );
    memset( cBuffer, 0, sizeof( cBuffer ) );

    do
    {
        xReadStatus = GGD_SecureConnect_Read( &cReadChar,
                                              ( uint32_t ) 1,
                                              *pxSocket,
                                              &ulReadSize );

        /** Check if we have found the end of header. */
        if( prvCheckForContentLengthString( &ucContentLengthIndex, cReadChar ) == pdTRUE )
        {
            xStatus = pdPASS;
            break;
        }
    }
    while( ( xReadStatus == pdPASS ) && ( ulReadSize == ( uint32_t ) 1 ) );

    if( xStatus == pdPASS )
    {
        /****** Get JSON file Size. *****/
        for( ucLengthStrIndex = ( uint8_t ) 0;
             ucLengthStrIndex < ( uint8_t ) ggJSON_PARSING_TMP_BUFFER_SIZE;
             ucLengthStrIndex++ )
        {
            xReadStatus = GGD_SecureConnect_Read( &cBuffer[ ucLengthStrIndex ],
                                                  ( uint32_t ) 1,
                                                  *pxSocket,
                                                  &ulReadSize );

            if( ( xReadStatus == pdFAIL ) || ( ulReadSize != ( uint32_t ) 1 ) )
            {
                ggdconfigPRINT( "JSON parsing could not get JSON file Size.\r\n" );
                xStatus = pdFAIL;
                break;
            }
        }
    }

    if( xStatus == pdPASS )
    {
        /* Add 1 because at the end of the JSON file the escape character '\0' will be added. */
        *pulJSONFileSize =
            ( uint32_t ) strtoul( cBuffer, NULL, ggJSON_CONVERTION_RADIX )
            + ( uint32_t ) 1; /*lint !e645 if code reaches here, cBuffer has been initalized. */

        /****** Go to the end of the header *****/
        xStatus = pdFAIL;

        do
        {
            xReadStatus = GGD_SecureConnect_Read( &cBuffer[ 3 ],
                                                  ( uint32_t ) 1,
                                                  *pxSocket,
                                                  &ulReadSize );

            /* Check if we have found the end of header. */
            if( ( cBuffer[ 0 ] == '\r' ) &&
                ( cBuffer[ 1 ] == '\n' ) &&
                ( cBuffer[ 2 ] == '\r' ) &&
                ( cBuffer[ 3 ] == '\n' ) )
            {
                xStatus = pdPASS;
                break;
            }

            cBuffer[ 0 ] = cBuffer[ 1 ];
            cBuffer[ 1 ] = cBuffer[ 2 ];
            cBuffer[ 2 ] = cBuffer[ 3 ];
        }
        while( ( xReadStatus == pdPASS ) && ( ulReadSize == ( uint32_t ) 1 ) );
    }

    if( xStatus == pdFAIL )
    {
        /* Don't forget to close the connection. */
        GGD_SecureConnect_Disconnect( pxSocket );
        ggdconfigPRINT( "JSON parsing failed\r\n" );
    }

    return xStatus;
}
/*-----------------------------------------------------------*/
BaseType_t GGD_JSONRequestGetFile( Socket_t * pxSocket,
                                   char * pcBuffer, /*lint !e971 can use char without signed/unsigned. */
                                   const uint32_t ulBufferSize,
                                   uint32_t * pulByteRead,
                                   BaseType_t * pxJSONFileRetrieveCompleted,
                                   const uint32_t ulJSONFileSize )
{
    BaseType_t xStatus;
    uint32_t ulDataSizeRead;

    configASSERT( pxSocket != NULL );
    configASSERT( pulByteRead != NULL );
    configASSERT( pxJSONFileRetrieveCompleted != NULL );
    configASSERT( pcBuffer != NULL );

    *pxJSONFileRetrieveCompleted = pdFALSE;

    xStatus = GGD_SecureConnect_Read( pcBuffer,
                                      ulBufferSize,
                                      *pxSocket,
                                      &ulDataSizeRead );

    if( xStatus == pdPASS )
    {
        *pulByteRead += ulDataSizeRead;

        /* We retrieved more than expected, this is failed. */
        if( *pulByteRead > ( ulJSONFileSize - ( uint32_t ) 1 ) )
        {
            ggdconfigPRINT( "JSON parsing - Received %ld, expected at most %ld \r\n",
                            *pulByteRead,
                            ulJSONFileSize - ( uint32_t ) 1 );
            xStatus = pdFAIL;
        }
    }

    if( xStatus == pdPASS )
    {
        /* We still have more to retrieve. */
        if( *pulByteRead < ( ulJSONFileSize - ( uint32_t ) 1 ) )
        {
            *pxJSONFileRetrieveCompleted = pdFALSE;
            ggdconfigPRINT( "JSON file retrieval incomplete, received %ld out of %ld bytes\r\n",
                            *pulByteRead,
                            ulJSONFileSize - ( uint32_t ) 1 );
        }
        else
        {
            /* Add the escape character. */
            pcBuffer[ ulJSONFileSize - ( uint32_t ) 1 ] = '\0';
            *pxJSONFileRetrieveCompleted = pdTRUE;
            ggdconfigPRINT( "JSON file retrieval completed\r\n" );

            /* Close the connection. */
            GGD_SecureConnect_Disconnect( pxSocket );
        }
    }
    else
    {
        ggdconfigPRINT( "JSON parsing - JSON file retrieval failed\r\n" );
        /* Don't forget to close the connection. */
        GGD_SecureConnect_Disconnect( pxSocket );
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

void GGD_JSONRequestAbort( Socket_t * pxSocket )
{
    configASSERT( pxSocket != NULL );

    if( *pxSocket != SOCKETS_INVALID_SOCKET )
    {
        GGD_SecureConnect_Disconnect( pxSocket );
    }
}

/*-----------------------------------------------------------*/

BaseType_t GGD_GetIPandCertificateFromJSON( char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                            const uint32_t ulJSONFileSize,
                                            const HostParameters_t * pxHostParameters,
                                            GGD_HostAddressData_t * pxHostAddressData,
                                            const BaseType_t xAutoSelectFlag )
{
    Socket_t xSocket;
    BaseType_t xStatus = pdPASS;
    JSONStatus_t result;
    uint32_t ulTokenIndex = 0;
    uint8_t ucCurrentInterface = 0, ucTargetInterface = 1;
    BaseType_t xFoundGGC = pdFALSE;
    BaseType_t xIsIPValid;

    configASSERT( pcJSONFile != NULL );
    configASSERT( pxHostAddressData != NULL );

    if( xAutoSelectFlag == pdFALSE )
    {
        configASSERT( pxHostParameters != NULL );
    }

    result = JSON_Validate( pcJSONFile, ulJSONFileSize );

    /* Manage the case. */
    if( result != JSONSuccess )
    {
        ggdconfigPRINT( "JSON parsing: Failed to parse JSON\r\n" );

        xStatus = pdFAIL;
    }
    else
    {
        /* Look for the green grass group certificate. */
        if( prvGGDGetCertificate( pcJSONFile,
                                  ulJSONFileSize,
                                  pxHostParameters,
                                  xAutoSelectFlag,
                                  pxHostAddressData ) == pdFAIL )
        {
            ggdconfigPRINT( "JSON parsing: Couldn't find certificate\r\n" );

            xStatus = pdFAIL;
        }
    }

    if( xStatus == pdPASS )
    {
        /* If autoSelectFlag is set to true, the code below will try
         * to connect directly to the interface.
         * If autoSelectFlag is set to false, the code below will try
         * to connect to every interfaces. */
        if( xAutoSelectFlag == pdFALSE )
        {
            if( prvGGDGetIPOnInterface( pcJSONFile,
                                        ulJSONFileSize,
                                        pxHostParameters->ucInterface,
                                        pxHostAddressData ) == pdFAIL )
            {
                ggdconfigPRINT( "GGC - Can't find interface\r\n" );
            }
            else
            {
                xFoundGGC = pdTRUE;
            }
        }
        else
        {
            while( prvGGDGetIPOnInterface( pcJSONFile,
                                           ulJSONFileSize,
                                           pxHostParameters->ucInterface,
                                           pxHostAddressData ) == pdPASS )
            {
                xIsIPValid = prvIsIPvalid( ( const char * ) pxHostAddressData->pcHostAddress,
                                           pxHostAddressData->ulHostAddressLen );

                if( xIsIPValid == pdTRUE )
                {
                    if( GGD_SecureConnect_Connect( pxHostAddressData,
                                                   &xSocket,
                                                   ggdconfigTCP_RECEIVE_TIMEOUT_MS,
                                                   ggdconfigTCP_SEND_TIMEOUT_MS )
                        == pdPASS )
                    {
                        xFoundGGC = pdTRUE;
                        /* Interface found, disconnect. */
                        GGD_SecureConnect_Disconnect( &xSocket );
                        break;
                    }
                }

                ucTargetInterface++;
            }
        }

        if( xFoundGGC != pdTRUE )
        {
            ggdconfigPRINT( "GGD - Can't connect to greengrass Core\r\n" );

            xStatus = pdFAIL;
        }
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

static BaseType_t prvGGDGetCore( const char * pcJSONFile,
                                 const uint32_t ulJSONFileSize,
                                 const HostParameters_t * const pxHostParameters,
                                 const BaseType_t xAutoSelectFlag )
{
    BaseType_t xStatus = pdFAIL;
    BaseType_t xMatchGroup = pdFALSE;
    BaseType_t xMatchCore = pdFALSE;
    char query[] = ".GGGroups[0].Cores[0].thingArn";
    size_t queryLength = sizeof( query ) - 1;
    char * value;
    size_t valueLength;
    JSONStatus_t result;

    result = JSON_Search( pcJSONFile,
                          ulJSONFileSize,
                          query,
                          queryLength,
                          &value,
                          &valueLength );

    if( result == JSONSuccess )
    {
        xStatus = pdPASS;
    }

    /* Green grass core not found. */
    return xStatus;
}
/*-----------------------------------------------------------*/

static BaseType_t prvGGDGetCertificate( char * pcJSONFile,
                                        const uint32_t ulJSONFileSize,
                                        const HostParameters_t * pxHostParameters,
                                        const BaseType_t xAutoSelectFlag,
                                        GGD_HostAddressData_t * pxHostAddressData )
{
    BaseType_t xMatchGroup = pdFALSE;
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

        /* strip \\ chars */
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
        while( ulReadIndex < valueLength );

        pxHostAddressData->pcCertificate = certbuf;
        pxHostAddressData->ulCertificateSize = ulWriteIndex;
        xStatus = pdPASS;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvIsIPvalid( const char * pcIP,
                                uint32_t ulIPlength )
{
    BaseType_t xStatus;

    ( void ) ulIPlength;

    if( strcmp( ggdLOOP_BACK_IP, pcIP ) != 0 )
    {
        xStatus = pdTRUE;
    }
    else
    {
        xStatus = pdFALSE;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvGGDGetIPOnInterface( char * pcJSONFile,
                                          const uint32_t ulJSONFileSize,
                                          const uint8_t ucTargetInterface,
                                          GGD_HostAddressData_t * pxHostAddressData )
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
        memset( addressbuf, 0x00, pxHostAddressData->ulHostAddressLen + 1 );
        memcpy( addressbuf, value, valueLength );
        pxHostAddressData->pcHostAddress = addressbuf;
        pxHostAddressData->ulHostAddressLen = valueLength;
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
        pxHostAddressData->usPort = strtoul( value, NULL, ggJSON_CONVERTION_RADIX );
        xStatus = pdPASS;
        xFoundIP = pdTRUE;
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

static BaseType_t prvCheckForContentLengthString( uint8_t * pucIndex,
                                                  const char cNewChar ) /*lint !e971 can use char without signed/unsigned. */
{
    BaseType_t xMatch = pdFALSE;

    if( cNewChar == ggdHTTP_CONTENT_LENGTH_STRING[ *pucIndex ] )
    {
        ( *pucIndex )++;

        if( *pucIndex == ( uint8_t ) ( sizeof( ggdHTTP_CONTENT_LENGTH_STRING ) - ( uint8_t ) 1 ) ) /*lint !e734 string length is not going to increase and is lower than 255 characters. */
        {
            xMatch = pdTRUE;
        }
    }
    else
    {
        *pucIndex = 0;
    }

    return xMatch;
}
/*-----------------------------------------------------------*/
/* Provide access to private members for testing. */
#ifdef FREERTOS_ENABLE_UNIT_TESTS
    #include "aws_greengrass_discovery_test_access_define.h"
#endif
