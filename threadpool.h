#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include <errno.h>
#include <pthread.h>
#include <queue>
#include <string.h>
#include <stdio.h>
// #include "LockGuard.hpp"

#define THREAD_NUM 32

template<typename T>
class ThreadPool{
    private:
        int num;
        pthread_t threads[THREAD_NUM];
        pthread_cond_t cond;
        pthread_mutex_t mut;
        std::queue<T*> task_queue;

        void Lock() {
            if (0!=pthread_mutex_lock(&mut)) {
                printf("fail to Lock!\n");
            }
            // printf("locked!\n");
        }

        void Unlock() {
            if (0!=pthread_mutex_unlock(&mut)) {
                printf("fail to Unlock!\n");
            }
            // printf("unlocked!\n");
        }

        void Wait() {
            pthread_cond_wait(&cond, &mut);
        }

        void Signal() {
            // printf("signal...\n");
            pthread_cond_signal(&cond);
        }

        void Broadcast() {
            pthread_cond_broadcast(&cond);
        }

        void stop() {
            for (int k=0; k<num; k++) {
                int ret = pthread_join(threads[k], NULL);
                if (ret!=0) {
                    printf("pthread_join error: %s\n", strerror(ret));
                }
            }    
        }

    public:
        ThreadPool(int n=THREAD_NUM):num(n) {
            pthread_cond_init(&cond, NULL);
            // pthread_mutexattr_init(&mutattr);
            // pthread_mutexattr_settype(&mutattr, PTHREAD_MUTEX_ERRORCHECK);
            pthread_mutex_init(&mut, NULL);
            // mutex = PTHREAD_MUTEX_INITIALIZER;
        }
        
        ~ThreadPool() {
            stop();
            pthread_cond_destroy(&cond);
            pthread_mutex_destroy(&mut);
        }

        void addTask(T* task) {
            // printf("add Task...\n");
            Lock();
            // LockGuard lg(&mutex);
            // printf("push ");
            task_queue.push(task);
            // printf("Task..\n");
            Signal();
            Unlock();
        }

        void Init() {
            for (int k=0; k<num; k++) {
                int ret = pthread_create(&threads[k], NULL, run, this);
                if (0!=ret) {
                    printf("fail to pthread_create: %s\n", strerror(ret));
                }
            }
        }

        static void* run(void* arg) {
            ThreadPool *p_threadpool = (ThreadPool*)arg;
            while (1) {
                p_threadpool->Lock();
                while (p_threadpool->task_queue.empty())
                    p_threadpool->Wait();
                T* task = p_threadpool->task_queue.front();
                p_threadpool->task_queue.pop();
                p_threadpool->Unlock();
                printf("peek one Task...\n");
                (*task)();
                // T* task = nullptr;
                // {
                //     LockGuard lg(&(p_threadpool->mutex));
                //     while (p_threadpool->task_queue.empty()) {
                //         p_threadpool->Wait();
                //     }
                //     printf("pppp\n");
                //     task = &(p_threadpool->task_queue.front());
                //     p_threadpool->task_queue.pop();
                //     printf("peek one Task...\n");
                // }
                // (*task)();
            }
        }
};


#endif