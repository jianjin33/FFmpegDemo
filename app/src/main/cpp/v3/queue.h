//
// Created by 简祖明 on 19/12/1.
//

#ifndef FFMPEGDEMO_QUEUE_H
#define FFMPEGDEMO_QUEUE_H

struct Queue;

//释放队列中元素所占用的内存
typedef void *(*QueueFreeFunc)(void *elem);


class BufferQueue {

public:
    /**
     * 初始化队列
     */
    Queue *QueueInit(int size);

    /**
     * 销毁队列
     */
    void QueueFree(Queue *queue);

    /**
     * 获取下一个索引位置
     */
    int QueueGetNext(Queue *queue, int current);

    /**
     * 队列压人元素
     */
    void *QueuePush(Queue *queue);

    /**
     * 弹出元素
     */
    void *QueuePop(Queue *queue);

};

#endif //FFMPEGDEMO_QUEUE_H
