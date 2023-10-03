#include <unistd.h>
#include "sys/socket.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "sys/stat.h"
#include <sys/mman.h>

#define BUFSIZE       4096
#define PATH_SIZE     1024

#define STATUS_CODE_OK         200
#define STATUS_MSG_OK          "OK"

#define STATUS_CODE_MOVED      301
#define STATUS_MSG_MOVED       "Move permanently"

#define STATUS_CODE_BAD_REQ    400
#define STATUS_MSG_BAD_REQ     "Bad request"

#define STATUS_CODE_FORBIDDEN  403
#define STATUS_MSG_FORBIDDEN   "Forbidden"

#define STATUS_CODE_NOT_FOUND  404
#define STATUS_MSG_NOT_FOUND   "Not found"

#define STATUS_CODE_NOT_SUPP   505
#define STATUS_MSG_NOT_SUPP    "Http version not supported"

enum PARSE_STATE {PARSE_REQUESTLINE, PARSE_HEADER, PARSE_CONTENT};

char        recv_buf[BUFSIZE];
char        send_buf[BUFSIZE];
char        req_file_path[PATH_SIZE];

char*       req_file_addr =  NULL;
const char* root_path     =  "/var/www/html";
int         status_code   =  STATUS_CODE_OK;
char*       status_msg    =  STATUS_MSG_OK;
char*       default_html   = "/index.html";
char*       bad_req_html   = "/400.html";
char*       forbid_html    = "/403.html";
char*       not_found_html = "/404.html";
char*       not_supp_html  = "/505.html";
long        req_file_size;

class HttpRequest{
    private:
        char  *recv_ptr, *parse_ptr;
        char  *method, *url, *version;
        int   bufsize_left;
        struct stat req_file_stat;
    //    const char* request_line[3]; // method, url, version
    //    const char *host, *connection, *user_agent, *language, *encoding;
        const char* request_headers[5] = {};
        char* headers[5] = {"Host","Connection","User-agent","Accept-language","Accept-Encoding"};
        PARSE_STATE state;

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
        
        int set_file_path(char* file_name) {
            memset(req_file_path, 0, sizeof(req_file_path));
            if (NULL==strcpy(req_file_path, root_path)) {
                printf("copy %s path error!\n", file_name);
                return -1;
            }
            if (NULL==strcat(req_file_path, file_name)) {
                printf("strcat %s path error!\n", file_name);
                return -1;
            }
            return 0;
        }

    public:
        /* if parse http request successfully, return 0;
         if received data isn't complete, return -1;
         if parse error return -2
        */
        int parse() {
            int ret = 0;
            switch (state) {
                case PARSE_REQUESTLINE:
                {
                    // check if line is complete
                    char* end = strpbrk(parse_ptr,"\r\n");
                    // printf("%c\n", *(end));
                    if (end==NULL) return -1;
                    // parse request method
                    char* pos = strchr(parse_ptr,' ');
                    if (pos==NULL) {
                        status_code = STATUS_CODE_BAD_REQ;
                        status_msg  = STATUS_MSG_BAD_REQ;
                        // set_file_path(bad_req_html);
                        return -2;
                    }
                    method = parse_ptr;
                    *pos   = '\0';
                    printf("METHOD: %s\n", method);
                    if (strcasecmp(method, "GET")!=0&&strcasecmp(method, "POST")!=0) {
                        status_code = STATUS_CODE_BAD_REQ;
                        status_msg = STATUS_MSG_BAD_REQ;
                        // set_file_path(bad_req_html);
                        return -2;
                    }
                       
                    parse_ptr = pos+1;
                    pos = strchr(parse_ptr, ' ');
                    if (pos==NULL) {
                        status_code = STATUS_CODE_BAD_REQ;
                        status_msg = STATUS_MSG_BAD_REQ;
                        return -2;
                    }
                    url  = parse_ptr;
                    *pos = '\0';
                    printf("URL: %s\n", url);
                    if (strcmp(url, "/")==0) {
                        set_file_path(default_html);
                    } else  {
                        set_file_path(url);
                    }
                    printf("PATH: %s\n", req_file_path);
                    if (-1==stat(req_file_path, &req_file_stat)) {
                        printf("stat request file error!\n");
                        printf("%s\n", strerror(errno));
                        status_code = STATUS_CODE_NOT_FOUND;
                        status_msg  = STATUS_MSG_NOT_FOUND;
                        // set_file_path(not_found_html);
                        // if (-1==stat(req_file_path, &req_file_stat)){
                        //     printf("stat 404.html error!\n");
                        //     return -2;
                        // }
                        // req_file_size = req_file_stat.st_size;
                        ret = -2;
                    }else if (!(req_file_stat.st_mode&S_IROTH)) {
                        // set_file_path(forbid_html);
                        status_code = STATUS_CODE_FORBIDDEN;
                        status_msg  = STATUS_MSG_FORBIDDEN;
                        ret = -2;
                    }else if (S_ISDIR(req_file_stat.st_mode)) { 
                        // set_file_path(bad_req_html);
                        status_code = STATUS_CODE_BAD_REQ;
                        status_msg  = STATUS_MSG_BAD_REQ;
                        ret = -2;
                    }
                    req_file_size = req_file_stat.st_size;
                    parse_ptr     = pos+1;
                    version       = parse_ptr;
                    *end          = '\0';
                    printf("VERSION: %s\n", version);
                    if (strcasecmp(version, "HTTP/1.1")!=0) {
                        // set_file_path(not_supp_html);
                        status_code = STATUS_CODE_NOT_SUPP;
                        status_msg = STATUS_MSG_NOT_SUPP;
                        return -2;
                    }
                    parse_ptr = end+2;
                    // if (parse_ptr) printf("%c\n", *parse_ptr);
                    state     = PARSE_HEADER;
                }
                case PARSE_HEADER:
                {
                    while (1) {
                        if (parse_ptr==NULL||(parse_ptr>=recv_ptr)||(*parse_ptr)==0) 
                            break;
                        char* end = strpbrk(parse_ptr, "\r\t");
                        if (end==NULL) return -1;
                        if (end==parse_ptr) {
                            printf("empty line! -> Parse Content\n");
                            state = PARSE_CONTENT;
                            break;
                        } else {
                            char* pos = strchr(parse_ptr, ':');
                            if (pos==NULL) {
                                // set_file_path(bad_req_html);
                                status_code = STATUS_CODE_BAD_REQ;
                                status_msg = STATUS_MSG_BAD_REQ;
                                return -2;
                            }
                            char* key = parse_ptr;
                            *pos = '\0';
                            char* value = pos+1;
                            while (value!=NULL && (char)*value==' ') {
                                value += 1;
                            }
                            if (value==NULL) {
                                // set_file_path(bad_req_html);
                                status_code = STATUS_CODE_BAD_REQ;
                                status_msg  = STATUS_MSG_BAD_REQ;
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
    
      // if receive http request successfully, return 0;
      // else return -1
      int http_recv(int sock) {
            recv_ptr     =  recv_buf;
            parse_ptr    =  recv_buf;
            bufsize_left =  BUFSIZE;
            state        =  PARSE_REQUESTLINE;
            // printf("%p~~~%p\n", recv_buf, recv_buf+BUFSIZE);
            memset(recv_buf, 0, sizeof(recv_buf));
            while (1) {
                int nr = recv(sock, recv_ptr, bufsize_left, 0);
                if (nr==-1) {
                    // close(sock);
                    return -1;
                } else if (nr==0){
                    // close(sock);
                    return -1;
                } else if (nr>0){
                    // printf("%s~\n", recv_ptr);
                    bufsize_left -= nr;
                    recv_ptr += nr;
                    int ret = parse();
                    if (ret==-1) {
                        printf("wait to receive more data!\n");
                        continue;
                    // } else if(ret==-2) {
                    //     printf("parse error!\n");
                    //     return -1;
                    } else if (ret==0){
                        printf("parse successfully!\n");
                        break;
                    }
                    // printf("&&&&&&\n");
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
                    // for (int k=0; k<5; k++) {
                    //     if (request_headers[k]!=NULL) printf("%s | %s\n", headers[k], request_headers[k]);     
                    // }
                    if (-1==stat(req_file_path, &req_file_stat)) {
                        printf("stat file: %s error!\n", req_file_path);
                        printf("%s\n", strerror(errno));
                    }
                    req_file_size = req_file_stat.st_size;
                    printf("&&&&&&\n");
                }                            
                
            }
            return 0;
        }
};

class HttpResponse{
    private:
        char *ptr;
        char *protocol     =  "HTTP/1.1";
        char *empty_space  =  " ";
        char *new_line     =  "\r\n";
        char *connection   =  "Connection: ";
        char *conn_close   =  "close";
        char *conn_alive   =  "keep-alive";
        char *content_len  =  "Content-Length: ";
        char *content_type =  "Content-Type: ";
        char *html         =  "text/html";

        int generate_status_line() {
            if (NULL==strcpy(ptr, protocol)) return -1;
            ptr += strlen(protocol);
            if (NULL==strcpy(ptr, empty_space)) return -1;
            ptr += 1;
            if (NULL==strcpy(ptr, std::to_string(status_code).c_str())) return -1;
            ptr += 3;
            if (NULL==strcpy(ptr, empty_space)) return -1;
            ptr += 1;
            if (NULL==strcpy(ptr, status_msg)) return -1;
            ptr += strlen(status_msg);
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
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

        int generate_header_lines() {
            if (-1==set_connection()) return -1;
            if (-1==set_content_length()) return -1;
            if (-1==set_content_type()) return -1;
            if (NULL==strcpy(ptr, new_line)) return -1;
            ptr += 2;
            return 0;
        }


    public:
        int build_response() {
            ptr = send_buf;
            if (-1==generate_status_line()) return -1;
            if (-1==generate_header_lines()) return -1;
            return 0;
        }
};

class EndPoint{
    HttpRequest  http_request;
    HttpResponse http_response;

  public:
    int process(int sock) {
        // printf("recv&parse http request...\n");
        // if (-1==
        http_request.http_recv(sock);
        // ) {
            // printf("recv/parse http request error!\n");
        // }
        if (-1==http_response.build_response()) {
            printf("generate http response error!\n");
        }
        printf("======\n");
        int req_file_fd = open(req_file_path, O_RDONLY);
        if (req_file_fd<0) {
            printf("fail to open request file!\n");
            return -1;
        }    
        req_file_addr = (char*)mmap(0, req_file_size, PROT_READ, MAP_PRIVATE, req_file_fd, 0);
        close(req_file_fd);
        printf("! wait to send response!\n");
        return 0;
    }
};

class CallBack{
    EndPoint endpoint;
  public:
    int operator()(int sock) {
        return endpoint.process(sock);
    }      
};

extern int epollfd;
int Epoll_mod_out(int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLOUT; // leverl trigger
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

class Task{
    int _sock;
  private:
    CallBack callback;

  public:
    Task(){

    }
    void init(int fd) {
        _sock = fd;
    }    
    int operator()() {
        // _sock = sock;
        printf("()...\n");
        int ret = callback(_sock);
        if (-1==Epoll_mod_out(_sock)) {
            printf("fail to add client_fd OUT!\n");
        }
        return ret;
    }

    int Send(int sock, char* buf, size_t size, int flags) {
        // printf("response size: %d sock: %d!\n", size, sock);
        ssize_t nr = 0;
        while (size>0) {
            nr = send(sock, buf, size, 0);
            if (nr==-1) {
                printf("%s!\n", strerror(errno));
                return -1;
            }
            buf += nr;
            size -= nr;
        }
        return 0;
    }

    void write() {
        printf("write sock:%d\n", _sock);
        if (-1==Send(_sock, send_buf, strlen(send_buf), 0)) {
            printf("fail to send http response!\n");
            return;
        }

        if (-1==Send(_sock, req_file_addr, req_file_size, 0)) {
            printf("fail to send request file!\n");
            return;
        }

        if (req_file_addr==NULL) {
            if (-1==munmap(req_file_addr, req_file_size)) {
                printf("fail to munmap request file!\n");
            }
        }
        printf("write successfully!\n");
    }
};