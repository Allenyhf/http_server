#ifndef __HTTP_RESPONSE_HPP
#define __HTTP_RESPONSE_HPP
#include <string.h>
#include "http_common.hpp"

class HttpResponse{
    private:
        char* ptr = NULL;
        const char* protocol     =  "HTTP/1.1";
        const char* empty_space  =  " ";
        const char* new_line     =  "\r\n";
        const char* connection   =  "Connection: ";
        const char* conn_close   =  "close";
        const char* conn_alive   =  "keep-alive";
        const char* content_len  =  "Content-Length: ";
        const char* content_type =  "Content-Type: ";
        const char* html         =  "text/html";
        
        long  req_file_size;
        // char* req_file_addr =  NULL;
        int   status_code;
        char* status_msg   =  const_cast<char*>(Status_msg_OK);

        //状态行
        int generate_status_line() {
            if (NULL==strcpy(ptr, protocol)) return -1;
            ptr += strlen(protocol);
            if (NULL==strcpy(ptr, empty_space)) return -1;
            ptr += 1;
            std::string statuscode_str = std::to_string(status_code);
            if (NULL==strcpy(ptr, statuscode_str.c_str())) return -1;
            ptr += statuscode_str.size();
            if (NULL==strcpy(ptr, empty_space)) return -1;
            ptr += 1;
            if (NULL==strcpy(ptr, status_msg)) return -1;
            ptr += strlen(status_msg);
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
            // printf("status: %s\n", status_msg);
            return 0;
        }

        int set_connection() {
            if (NULL==strcpy(ptr, connection)) return -1;
            ptr += strlen(connection);
            if (NULL==strcpy(ptr, conn_close)) return -1;
            ptr += strlen(conn_close);
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
            return 0;
        }

        int set_content_length() {
            if (NULL==strcpy(ptr, content_len)) return -1;
            ptr += strlen(content_len);
            std::string len_str = std::to_string(req_file_size);
            if (NULL==strcpy(ptr, len_str.c_str())) return -1;
            ptr += len_str.size();
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
            return 0;
        }

        int set_content_type() {
            if (NULL==strcpy(ptr, content_type)) return -1;
            ptr += strlen(content_type);
            if (NULL==strcpy(ptr, html)) return -1;
            ptr += strlen(html);
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
            return 0;
        }
        //生成首部行
        int generate_header_lines() {
            if (-1==set_connection()) return -1;
            if (-1==set_content_length()) return -1;
            if (-1==set_content_type()) return -1;
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
            return 0;
        }

        void set_status(int code) {
            status_code = code;
            switch (code)
            {
            case STATUS_CODE_OK:
                /* code */
                status_msg = const_cast<char*>(Status_msg_OK);
                break;
            case STATUS_CODE_NOT_SUPP:
                status_msg = const_cast<char*>(Status_msg_NOT_SUPP);
                break;
            case STATUS_CODE_FORBIDDEN:
                status_msg = const_cast<char*>(Status_msg_FORBIDDEN);
                break;
            case STATUS_CODE_BAD_REQ:
                status_msg = const_cast<char*>(Status_msg_BAD_REQ);
                break;
            case STATUS_CODE_MOVED:
                status_msg = const_cast<char*>(Status_msg_MOVED);
                break;
            case STATUS_CODE_NOT_FOUND:
                status_msg = const_cast<char*>(Status_msg_NOT_FOUND);
                break;
            default:
                break;
            }
        }
        
    protected:
        char send_buf[BUFSIZE];
        friend class EndPoint;

    public:
        //生成HTTP响应
        int build_response(int status_code) {
            set_status(status_code);
            memset(send_buf, 0, sizeof(send_buf));
            ptr = send_buf;
            // printf("status code: %d", status_code);
            if (-1==generate_status_line()) return -1;
            if (-1==generate_header_lines()) return -1;
            // printf("response: (%s)\n", send_buf);
            return 0;
        }

        void set_req_file_size(int size) {
            req_file_size = size;
        }
};

#endif