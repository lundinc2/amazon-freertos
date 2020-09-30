# Amazon FreeRTOS TI vendor ports and projects

FreeRTOS does not include any of the TI SimpleLink SDKs required for development.
Users are required to download the full TI SDK for any evaluation and development.
For compatibility reasons, please be sure to use the TI SDK versions listed
below as they have been verified together with Amazon FreeRTOS:

** TI SimpleLink Wi-Fi CC32xx SDK v2.10.00.04: **
   http://www.ti.com/tool/download/SIMPLELINK-CC3220-SDK/2.10.00.04

** TI SimpleLink CC13x2 and CC26x2 SDK v4.30: **
< add link once live >

# Common-IO

Starting from TI SDK version 4.30.*, a subset of the Amazon FreeRTOS common-io
layer is supported for all SimpleLink device SDKs listed above. While most
of the provided common-io ports are device agnostic, some are device specific 
and only available for a given device family.

Available common-io ports are found under "simplelink_common/ports/common-io/src"
where device agnostic versions are found under the "simplelink" folder. 
