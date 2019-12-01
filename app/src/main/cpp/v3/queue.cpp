//
// Created by 简祖明 on 19/12/1.
//
#include <malloc.h>
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

Queue *BufferQueue::QueueInit(int size) {
    Queue *queue = static_cast<Queue *>(malloc(sizeof(Queue)));
    queue->size = size;
    queue->nextToWrite = 0;
    queue->nextToRead = 0;
    //数组开辟空间
    queue->tab = static_cast<void **>(malloc(sizeof(*queue->tab) * size));
    int i;
    for (i = 0; i < size; i++) {
        queue->tab[i] = malloc(sizeof(*queue->tab));
    }
    return queue;
}

void BufferQueue::QueueFree(Queue *queue) {
    int i;
    for (i = 0; i < queue->size; i++) {
        // 销毁队列的元素，回调函数去自定义释放内存
        QueueFreeFunc((void *) queue->tab[i]);
    }
    free(queue->tab);
    free(queue);
}


int BufferQueue::QueueGetNext(Queue *queue, int current) {
    return (current + 1) % queue->size;
}

void *BufferQueue::QueuePop(Queue *queue) {
    int current = queue->nextToWrite;
    queue->nextToWrite = QueueGetNext(queue, current);
    return queue->tab[current];
}


void *BufferQueue::QueuePush(Queue *queue) {
    int current = queue->nextToRead;
    queue->nextToRead = QueueGetNext(queue, current);
    return queue->tab[current];
}