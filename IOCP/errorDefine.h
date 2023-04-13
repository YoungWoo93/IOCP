#pragma once

/*/
Error Code			description
/*/
#include <unordered_map>
#include <string>

#define ERRORCODE_WSASTARTUP				1
#define ERRORCODE_SOCKET					2
#define ERRORCODE_BIND						3
#define ERRORCODE_LISTEN					4
#define ERRORCODE_ACCEPT					5
#define ERRORCODE_CREATEKERNELOBJECT		6
#define ERRORCODE_CONNECT					7

#define ERRORCODE_WSASEND					101
#define ERRORCODE_WSARECV					102

#define ERRORCODE_SENDBUFFER				1000
#define ERRORCODE_RECVBUFFER				1001

#define ERRORCODE_SESSIONARRAY				1010

#define ERRORCODE_SENDBUFFER				1000

#define ERRORCODE_OBJECTPOOL				2000

#define ERRORCODE_NOTDEFINE					~0

extern std::unordered_map<int, std::string> errorCodeComment;



