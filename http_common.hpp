#ifndef __HTTP_COMMON_H
#define __HTTP_COMMON_H

//常用状态码
#define STATUS_CODE_OK         200
#define STATUS_CODE_MOVED      301
#define STATUS_CODE_BAD_REQ    400
#define STATUS_CODE_FORBIDDEN  403
#define STATUS_CODE_NOT_FOUND  404
#define STATUS_CODE_NOT_SUPP   505
//以上状态码对应的状态信息
const char* Status_msg_OK        = "OK";
const char* Status_msg_MOVED     = "Move permanently";
const char* Status_msg_BAD_REQ   = "Bad request";
const char* Status_msg_FORBIDDEN = "Forbidden";
const char* Status_msg_NOT_FOUND = "Not found";
const char* Status_msg_NOT_SUPP  = "Http version not supported";

#endif