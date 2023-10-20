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
    char* send_ptr;

    int Send(int sock, char* buf, size_t size, int flags) {
        // printf("response size: %d sock: %d!\n", size, sock);
        ssize_t nr = 0;
        while (size>0) {
            nr = send(sock, buf, size, 0);
            if (nr==-1) {
                //log
                printf("%s!\n", strerror(errno));
                if (errno==EAGAIN) {
                    return nr;
                }
                return -1;
            }
            buf += nr;
            size -= nr;
        }
        // return 0;
        return nr;
    }

  public:
    void set_req_file_size(int size) {
        req_file_size = size;
        http_response.set_req_file_size(size);
    }

    /**
     * @brief 接收 HTTP Request并生成相应的 HTTP Response
     * 
     * @param sock 
     * @return int  失败返回-1,成功返回0。
     */
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
        send_ptr = http_response.send_buf;
        // printf("! wait to send response!\n");
        return 0;
    }

    /**
     * @brief 将 HTTP Response和请求文件 写入socket发送缓冲区
     * 
     * @param sock 
     * @return int 若发送失败返回-1; 若不完全发送返回0；若完全发送返回1。
     */
    int write(int sock) {
        while (1) {
            int to_send_nr;
            if (send_ptr>=req_file_addr && send_ptr<req_file_addr+req_file_size) {
                /* 阶段2:发送请求文件 */
                to_send_nr = req_file_addr+req_file_size-send_ptr;
            } else if (send_ptr>=http_response.send_buf && send_ptr<http_response.send_buf+strlen(http_response.send_buf)) {
                /* 阶段1:发送HTTP Response */
                to_send_nr = http_response.send_buf+strlen(http_response.send_buf)-send_ptr;
            } else {
                /* 错误 */
                return -1;
            }

            int sent_nr = Send(sock, send_ptr, to_send_nr, 0);
            if (sent_nr<=-1) {
                /* 发送失败 */
                return -1;
            } else if (sent_nr<to_send_nr) {
                /* 不完全发送（发送缓冲区空间不足）*/
                send_ptr += sent_nr;
                return 0;
            } else if (sent_nr==to_send_nr && send_ptr>=req_file_addr) {
                /* 阶段2发送完毕 */
                if (req_file_addr!=NULL) {
                    if (-1==munmap(req_file_addr, req_file_size)) {
                        printf("fail to munmap request file!\n");
                    }
                }
                memset(http_response.send_buf, 0, sizeof(http_response.send_buf));
                return 1; 
            } else if (sent_nr==to_send_nr) {
                /* 阶段1发送完毕 */
                //转入发送阶段2:发送请求文件
                send_ptr = req_file_addr;
                //继续循环
            }
        }
        return 0;
    }

/*
    int write(int sock) {
        while (1) {
            //阶段2:发送请求文件
            if (send_ptr>=req_file_addr) {
                int to_send_nr = req_file_addr+req_file_size-send_ptr;
                int sent_nr = Send(sock, send_ptr, to_send_nr, 0);
                if (sent_nr<=-1) {//发送失败
                    return -1;
                } else if (sent_nr<to_send_nr) {//不完全发送（发送缓冲区空间不足）
                    send_ptr += sent_nr;
                    return 0;
                } else if (sent_nr==to_send_nr) {//发送完毕
                    if (req_file_addr==NULL) {
                        if (-1==munmap(req_file_addr, req_file_size)) {
                            printf("fail to munmap request file!\n");
                        }
                    }
                    memset(http_response.send_buf, 0, sizeof(http_response.send_buf));
                    return 1; 
                }
            } else {
            //阶段1:发送HTTP Response
                int to_send_nr = http_response.send_buf+strlen(http_response.send_buf)-send_ptr;
                int sent_nr = Send(sock, send_ptr, to_send_nr, 0);
                if (sent_nr<=-1) {
                    return -1;
                } else if (sent_nr<to_send_nr) {
                    send_ptr += sent_nr;
                    return 0;
                } else if (sent_nr==to_send_nr) {
                    send_ptr = req_file_addr;   
                }
            }
        }
        return 0;
    }
*/
    /*
    int write(int sock) {
        if (send_ptr>=req_file_addr) {
            int to_send_nr = req_file_addr+req_file_size-send_ptr;
            int sent_nr = Send(sock, send_ptr, to_send_nr, 0);
            if (sent_nr<=-1) {//发送失败
                return -1;
            } else if (sent_nr<to_send_nr) {//不完全发送
                send_ptr += sent_nr;
                return 0;
            } else if (sent_nr==to_send_nr) {//发送完毕
                if (req_file_addr==NULL) {
                    if (-1==munmap(req_file_addr, req_file_size)) {
                        printf("fail to munmap request file!\n");
                    }
                }
                memset(http_response.send_buf, 0, sizeof(http_response.send_buf));
                return 1; 
            }
        } else {
            int to_send_nr = http_response.send_buf+strlen(http_response.send_buf)-send_ptr;
            int sent_nr = Send(sock, send_ptr, to_send_nr, 0);
            if (sent_nr<=-1) {
                return -1;
            } else if (sent_nr<to_send_nr) {
                send_ptr += sent_nr;
                return 0;
            } else if (sent_nr==to_send_nr) {
                send_ptr = req_file_addr;
                to_send_nr = req_file_size;
                sent_nr = Send(sock, send_ptr, to_send_nr, 0);
                if (sent_nr<=-1) {
                    return -1;
                } else if (sent_nr<to_send_nr) {
                    send_ptr += sent_nr;
                    return 0;
                } else if (sent_nr==to_send_nr) {
                    if (req_file_addr==NULL) {
                        if (-1==munmap(req_file_addr, req_file_size)) {
                            printf("fail to munmap request file!\n");
                        }
                    }
                    memset(http_response.send_buf, 0, sizeof(http_response.send_buf));
                    return 1; 
                } 
                return 1;
            }
        }
    }
    */

    // void write(int sock) {
    //     // printf("write sock:%d %s__%s\n", sock, http_response.send_buf, req_file_addr);
    //     // printf("testbuf: %s\n", test_buf);
    //     if (-1==Send(sock, http_response.send_buf, strlen(http_response.send_buf), 0)) {
    //         printf("fail to send http response!\n");
    //         return;
    //     }

    //     if (-1==Send(sock, req_file_addr, req_file_size, 0)) {
    //         printf("fail to send request file!\n");
    //         return;
    //     }

    //     if (req_file_addr==NULL) {
    //         if (-1==munmap(req_file_addr, req_file_size)) {
    //             printf("fail to munmap request file!\n");
    //         }
    //     }
    //     memset(http_response.send_buf, 0, sizeof(http_response.send_buf));
    //     printf("write successfully!\n");
    // }
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
        int wr = endpoint.write(msock);
        if (wr==-1) {
            if (-1==Epoll_del_fd(msock)) {
                printf("fail to del events OUT!\n");
            }
            close(msock);
        } else if (wr==0) {
            if (-1==Epoll_mod_out(msock)) {
                printf("fail to add client_fd OUT: %s!\n", strerror(errno));
            }
        } else if (wr==1) {
            if (-1==Epoll_del_fd(msock)) {
                printf("fail to del events OUT!\n");
            }
            close(msock);
        }

        // if (-1==Epoll_mod_out(msock)) {
        //     printf("fail to add client_fd OUT: %s!\n", strerror(errno));
        // }
        return ret;
    }

    int write() {
        return endpoint.write(msock);
    }

    // void write() {
    //     endpoint.write(msock);
    // }

};