#include <unistd.h>
#include "sys/socket.h"
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "sys/stat.h"
#include <sys/mman.h>
#include <errno.h>
#include "my_epoll.hpp"
#include "http_request.hpp"
#include "http_response.hpp"

// class CallBack{
//     EndPoint endpoint;
//   public:
//     int operator()(int sock) {
//         return endpoint.process(sock);
//     }      
// };

/**
 * @brief 表示一个http连接端点
 * 
 */
class EndPoint{
    HttpRequest  http_request;
    HttpResponse http_response;
    int req_file_size;
    char* req_file_addr;
    // char test_buf[1024];

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

  public:
    void set_req_file_size(int size) {
        req_file_size = size;
        http_response.set_req_file_size(size);
    }

    int process(int sock) {
        // printf("recv&parse http request...\n");
        // if (-1==
        http_request.http_recv(sock);
        // ) {
            // printf("recv/parse http request error!\n");
        // }
        set_req_file_size(http_request.get_req_file_size());
        
        if (-1==http_response.build_response(http_request.get_status_code())) {
            printf("generate http response error!\n");
        }
        // printf("======\n");
        int req_file_fd = open(http_request.req_file_path, O_RDONLY);
        if (req_file_fd<0) {
            printf("fail to open request file: %s!\n", http_request.req_file_path);
            return -1;
        }
        // printf("file is: %d %s\n", req_file_size, http_request.req_file_path);
        req_file_addr = (char*)mmap(0, req_file_size, PROT_READ, MAP_PRIVATE, req_file_fd, 0);
        if (req_file_addr==(void*)-1) {
            printf("fail to mmap:%s\n", strerror(errno));
        }
        // printf("file: %s\n", req_file_addr);
        close(req_file_fd);
        // printf("! wait to send response!\n");
        // memset(test_buf, 0, sizeof(test_buf));
        // strcpy(test_buf, "test...");
        // printf("testbuf1: %s\n", test_buf);
        return 0;
    }


    void write(int sock) {
        // printf("write sock:%d %s__%s\n", sock, http_response.send_buf, req_file_addr);
        // printf("testbuf: %s\n", test_buf);
        if (-1==Send(sock, http_response.send_buf, strlen(http_response.send_buf), 0)) {
            printf("fail to send http response!\n");
            return;
        }

        if (-1==Send(sock, req_file_addr, req_file_size, 0)) {
            printf("fail to send request file!\n");
            return;
        }

        if (req_file_addr==NULL) {
            if (-1==munmap(req_file_addr, req_file_size)) {
                printf("fail to munmap request file!\n");
            }
        }
        memset(http_response.send_buf, 0, sizeof(http_response.send_buf));
        printf("write successfully!\n");
    }
};

/**
 * @brief 表示一个http连接
 * 
 */
class Task{
    int msock;
    // CallBack callback;
    EndPoint endpoint;

  public:
    Task(): msock(0){
    }
    
    void init(int fd) {
        msock = fd;
    }    

    int operator()() {
        // printf("()...\n");
        // int ret = callback(msock);
        int ret = endpoint.process(msock);
        if (-1==Epoll_mod_out(msock)) {
            printf("fail to add client_fd OUT: %s!\n", strerror(errno));
        }
        return ret;
    }

    void write() {
        endpoint.write(msock);
    }

};