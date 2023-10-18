#ifndef __HTTP_REQUEST_HPP
#define __HTTP_REQUEST_HPP
#include "sys/stat.h"
#include <stdio.h>
#include "sys/socket.h"
#include <string.h>
#include "http_common.hpp"

#define BUFSIZE       4096
#define PATH_SIZE     1024
#define LINE_SIZE     1024


// 解析状态： 解析请求行、解析请求头、解析请求体
enum PARSE_STATE {PARSE_REQUESTLINE, PARSE_HEADER, PARSE_CONTENT};

// html文件路径
const char*     root_path      =  "/var/www/html";
// 默认html和错误请求html
const char*     default_html   =  "/index.html";
const char*     bad_req_html   =  "/400.html";
const char*     forbid_html    =  "/403.html";
const char*     not_found_html =  "/404.html";
const char*     not_supp_html  =  "/505.html";

class HttpRequest{
    private:
        char  recv_buf[BUFSIZE];
        char  *recv_ptr, *parse_ptr; //接收指针、解析指针
        char  *method, *url, *version;
        int   bufsize_left;
        int   status_code   =  STATUS_CODE_OK;
        // char* status_msg    =  Status_msg_OK;

        //请求的文件大小
        int   req_file_size;
        //请求的文件stat
        struct stat req_file_stat;

    //    const char* request_line[3]; // method, url, version
    //    const char *host, *connection, *user_agent, *language, *encoding;
        //请求头的头部字段
        const char* headers[5] = {"Host","Connection","User-agent","Accept-language","Accept-Encoding"};
        //请求头的头部字段值，与headers一一对应
        const char* request_headers[5] = {};
        //当前的解析状态
        PARSE_STATE state;

    protected:
        //请求的文件路径
        char  req_file_path[PATH_SIZE];
        friend class EndPoint;

    private:
        void set_value(const char* key, const char* value) {
            /* 
            if (strcasecmp(key, "Host")==0) {
                 host = value;
             } else if (strcasecmp(key, "Connection")==0) {
                 connection = value;
             } else if (strcasecmp(key, "User-agent")==0) {
                 user_agent = value;
             } else if (strcasecmp(key, "Accept-language")==0) {
                 language = value;
             } else if (strcasecmp(key, "Accept-Encoding")==0) {
                 encoding = value;
             }
            */
            for (int k=0; k<5; k++) {
                if (strcasecmp(key, headers[k])==0) {
                    request_headers[k] = value;
                    break;
                }
            }
            // printf("%s | %s\n", key, value);
        }
        
        int set_file_path(const char* file_name) {
            memset(req_file_path, 0, sizeof(req_file_path));
            if (NULL==strcpy(req_file_path, root_path)) {
                //printf("copy %s path error!\n", file_name);
                return -1;
            }
            if (NULL==strcat(req_file_path, file_name)) {
                //printf("strcat %s path error!\n", file_name);
                return -1;
            }
            return 0;
        }

    public:

        int get_status_code() const {
            return status_code;
        }
    
        /**
         * @brief 接收数据，并解析HTTP请求
         * 
         */
        // if receive http request successfully, return 0;
        // else return -1
        int http_recv(int sock) {
            recv_ptr     =  recv_buf;
            bufsize_left =  BUFSIZE;
            state        =  PARSE_REQUESTLINE;
            // printf("%p~~~%p %d\n", recv_buf, recv_buf+BUFSIZE, sock);
            memset(recv_buf, 0, sizeof(recv_buf));
            while (1) {
                int nr = recv(sock, recv_ptr, bufsize_left, 0);
                if (nr==-1) {
                    //log
                    return -1;
                } else if (nr==0){
                    //log
                    return -1;
                } else if (nr>0){
                    //接收到nr个字节的数据，buf剩余空间减少nr，接收指针向后移动nr
                    bufsize_left -= nr;
                    recv_ptr += nr;
                    //解析
                    int ret = parse();
                    if (ret==-1) {
                        //log
                        // printf("wait to receive more data!\n");
                        continue;
                    // } else if(ret==-2) {
                    //     printf("parse error!\n");
                    //     return -1;
                    } else if (ret==0){
                        printf("parse successfully!\n");
                        break;
                    }
                    //根据解析出的状态码，设置响应的html
                    switch (status_code) {
                        case STATUS_CODE_OK:
                        {
                            break;
                        }
                        case STATUS_CODE_MOVED: 
                        {
                            break;
                        }
                        case STATUS_CODE_BAD_REQ:
                        {
                            set_file_path(bad_req_html);
                            break;
                        }
                        case STATUS_CODE_NOT_SUPP:
                        {
                            set_file_path(not_supp_html);
                            break;
                        }
                        case STATUS_CODE_NOT_FOUND:
                        {
                            set_file_path(not_found_html);
                            break;
                        }
                        case STATUS_CODE_FORBIDDEN:
                        {
                            set_file_path(forbid_html);
                            break;
                        }
                        default:
                        {

                        }
                    }

                    // if (-1==stat(req_file_path, &req_file_stat)) {
                    //     printf("stat file: %s error!\n", req_file_path);
                    //     printf("%s\n", strerror(errno));
                    // }
                    // req_file_size = req_file_stat.st_size;
                }                            
            }
            return 0;
        }

        /* if parse http request successfully, return 0;
         if received data isn't complete, return -1;
         if parse error return -2
        */
        int parse() {
            int ret = 0;
            //解析指针指向buf的第一个字节
            parse_ptr = recv_buf;
            switch (state) {
                case PARSE_REQUESTLINE:
                {
                    
                    char* end = strpbrk(parse_ptr,"\r\n");
                    //还未接收到完整行
                    if (end==NULL){
                        //log
                        return -1;
                    }
                    //1.解析请求方法
                    char* pos = strpbrk(parse_ptr, " \t");
                    // char* pos;
                    
                    //空格或制表符
                    // "  GET /test.html HTTP/1.1"
                    while(pos==parse_ptr) {
                        parse_ptr += 1;
                        if (parse_ptr==end) {
                            //log
                            status_code = STATUS_CODE_BAD_REQ;
                            return -2;
                        }
                        pos = strpbrk(parse_ptr, " \t");
                    }
                    // if (pos==parse_ptr) {
                    //     //log
                    //     return -1;
                    // }
                    //请求行中没有空格或制表符，错误或恶意请求！
                    if (pos==NULL) {
                        //LOG
                        // printf("POS IS NULL\n");
                        status_code = STATUS_CODE_BAD_REQ;
                        return -2;
                    }
                    method = parse_ptr;
                    *pos   = '\0';
                    parse_ptr = pos+1;
                    // printf("METHOD: (%s)\n", method);
                    if (strcasecmp(method, "GET")!=0&&strcasecmp(method, "POST")!=0) {
                        //log
                        status_code = STATUS_CODE_BAD_REQ;
                        return -2;
                    }
                    
                    //解析请求文件URL
                    // pos = strchr(parse_ptr, ' ');
                    pos = strpbrk(parse_ptr, " \t");
                    //跳过空格和制表符
                    while(pos==parse_ptr) {
                        parse_ptr += 1;
                        if (parse_ptr==end){//到达末尾
                            //log
                            status_code = STATUS_CODE_BAD_REQ;
                            return -2;
                        }
                        pos = strpbrk(parse_ptr, " \t");
                    }
                    //请求行中没有空格或制表符，错误或恶意请求！
                    if (pos==NULL) {
                        //log
                        status_code = STATUS_CODE_BAD_REQ;
                        return -2;
                    }
                    url  = parse_ptr;
                    *pos = '\0';
                    // printf("URL: (%s)\n", url);
                    //根据URL设置请求文件路径
                    if (strcmp(url, "/")==0) {
                        set_file_path(default_html);
                    } else  {
                        set_file_path(url);
                    }
                    if (-1==stat(req_file_path, &req_file_stat)) {
                        //log
                        // printf("line:%d file:%s\n", __LINE__, req_file_path);
                        status_code = STATUS_CODE_NOT_FOUND;
                        // status_msg  = Status_msg_NOT_FOUND;
                        ret = -2;
                    }else if (!(req_file_stat.st_mode&S_IROTH)) {
                        //log
                        status_code = STATUS_CODE_FORBIDDEN;
                        // status_msg  = Status_msg_FORBIDDEN;
                        ret = -2;
                    }else if (S_ISDIR(req_file_stat.st_mode)) { 
                        //log
                        status_code = STATUS_CODE_FORBIDDEN;
                        // status_msg  = Status_msg_FORBIDDEN;
                        ret = -2;
                    }
                    req_file_size = req_file_stat.st_size;
                    //解析HTTP协议版本
                    parse_ptr     = pos+1;
                    while (parse_ptr<end && (char)*parse_ptr==' ') {
                        parse_ptr += 1;
                    }
                    version       = parse_ptr;
                    *end          = '\0';
                    // printf("VERSION: %s\n", version);
                    if (strcasecmp(version, "HTTP/1.1")!=0) {
                        //log
                        // printf("line:%d (%s)\n", __LINE__, version);
                        status_code = STATUS_CODE_NOT_SUPP;
                        // status_msg = Status_msg_NOT_SUPP;
                        return -2;
                    }
                    parse_ptr = end+2;
                    //请求行解析完毕，转为解析请求头
                    state = PARSE_HEADER;
                }
                case PARSE_HEADER:
                {
                    while (1) {
                        if (parse_ptr==NULL||(parse_ptr>=recv_ptr)||(*parse_ptr)==0) 
                            break;
                        char* end = strpbrk(parse_ptr, "\r\t");
                        if (end==NULL) {
                            //数据还未完整接收
                            //log
                            return -1;
                        }
                        if (end==parse_ptr) {
                            //log
                            // printf("empty line! -> Parse Content\n");
                            //碰到空行，请求头解析完毕，转为解析请求体
                            state = PARSE_CONTENT;
                            break;
                        } else {
                            char* pos = strchr(parse_ptr, ':');
                            if (pos==NULL) {
                                // if (end-parse_ptr>LINE_SIZE) {
                                //     printf("line:%d\n", __LINE__);
                                //     status_code = STATUS_CODE_BAD_REQ;
                                //     // status_msg = Status_msg_BAD_REQ;
                                //     return -2;
                                // } else {
                                //     return -1;
                                // }
                                status_code = STATUS_CODE_BAD_REQ;
                                return -2;
                            }
                            const char* key = parse_ptr;
                            *pos = '\0';
                            const char* value = pos+1;
                            //跳过空格
                            while (value!=NULL && ((char)*value==' '||(char)*value=='\t')) {
                                value += 1;
                            }
                            if (value==NULL) {
                                //log
                                status_code = STATUS_CODE_BAD_REQ;
                                // status_msg  = Status_msg_BAD_REQ;
                                return -2;
                            }
                            *end = '\0';
                            set_value(key, value);
                            parse_ptr = end+2;
                        }
                    }
                }
                case PARSE_CONTENT:
                {

                    break;
                }
                default:
                    break;
            }
            return ret;
        }

        int get_req_file_size() const{
            return req_file_size;
        }
};
#endif