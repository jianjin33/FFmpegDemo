//
// Created by 简祖明 on 19/12/1.
//
#include <malloc.h>
#include <pthread.h>
#include "queue.h"


struct Queue {
    int size;

    /**
     * 任意类型的指针数组，这里每一个元素都是AVPacket指针，总共有size个
     */
    void **tab;

    int nextToWrite;

    int nextToRead;

    int *ready;
};

Queue *BufferQueue::QueueInit(int size, QueueAllocFunc func) {
    Queue *queue = static_cast<Queue *>(malloc(sizeof(Queue)));
    queue->size = size;
    queue->nextToWrite = 0;
    queue->nextToRead = 0;
    //数组开辟空间
    queue->tab = static_cast<void **>(malloc(sizeof(*queue->tab) * size));
    int i;
    for (i = 0; i < size; i++) {
        queue->tab[i] = func();
    }
    return queue;
}

void BufferQueue::QueueFree(Queue *queue, QueueFreeFunc func) {
    int i;
    for (i = 0; i < queue->size; i++) {
        // 销毁队列的元素，回调函数去自定义释放内存
        func(queue->tab[i]);
    }
    free(queue->tab);
    free(queue);
}


int BufferQueue::QueueGetNext(Queue *queue, int current) {
    return (current + 1) % queue->size;
}

void *BufferQueue::QueuePop(Queue *queue, pthread_mutex_t* mutex, pthread_cond_t* cond) {
    int current = queue->nextToWrite;
    int nextToWrite;
    for (;;) {
        nextToWrite = QueueGetNext(queue, current);
        if (nextToWrite != queue->nextToWrite) {
            break;
        }
        pthread_cond_wait(cond, mutex);
    }

    queue->nextToWrite = QueueGetNext(queue, current);

    pthread_cond_broadcast(cond);
    return queue->tab[current];
}


void *BufferQueue::QueuePush(Queue *queue, pthread_mutex_t* mutex, pthread_cond_t* cond) {
    int current = queue->nextToRead;

    int nextToRead;
    for (;;) {
        nextToRead = QueueGetNext(queue, current);
        if (nextToRead != queue->nextToRead) {
            break;
        }
        pthread_cond_wait(cond, mutex);
    }
    queue->nextToRead = QueueGetNext(queue, current);
    return queue->tab[current];
}