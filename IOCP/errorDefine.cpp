#include "errorDefine.h"

std::unordered_map<int, std::string> errorCodeComment = {
	// 시스템 에러
	{	ERRORCODE_WSASTARTUP			,	"WSAStartup Error "},
	{	ERRORCODE_SOCKET				,	"Create socket Error "},
	{	ERRORCODE_BIND					,	"Socket bind Error "},
	{	ERRORCODE_LISTEN				,	"Socket listen Error "},
	{	ERRORCODE_ACCEPT				,	"Accept Error "},
	{	ERRORCODE_CREATEKERNELOBJECT	,	"Create kernel object Error "},
	{	ERRORCODE_CONNECT				,	"Socket Connect Error "},

	// 100번대 - send / recv 에러
	{	ERRORCODE_WSASEND				,	"WSAsend API Error "},
	{	ERRORCODE_WSARECV				,	"WSArecv API Error "},

	// 세션 관련 에러
	{	ERRORCODE_SENDBUFFER			,	"Send buffer Error "},
	{	ERRORCODE_RECVBUFFER			,	"Recv buffer Error "},
	{	ERRORCODE_SESSIONARRAY			,	"Session array Error "},


	// 유틸 관련 에러
	{	ERRORCODE_OBJECTPOOL			,	"ObjectPool Error"},

	//
	{	ERRORCODE_NOTDEFINE				,	""},
};