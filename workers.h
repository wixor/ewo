#ifndef __WORKERS_H__
#define __WORKERS_H__

#include <pthread.h>

class Completion
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int counter;

public:
    Completion();
    ~Completion();

    void completed();
    void queued();
    void wait();
};

class AsyncJob
{
    friend class AsyncQueue;
    AsyncJob *next;

public:
    Completion *completion;

    virtual ~AsyncJob();
    virtual void run() = 0;
};

class AsyncQueue
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    AsyncJob *first, **last;

public:
    AsyncQueue();
    ~AsyncQueue();

    void queue(AsyncJob *job);
    AsyncJob *get(void);
};

class WorkerThread
{
    pthread_t thread;
    AsyncQueue *queue;
    
public:
    WorkerThread(AsyncQueue *queue);
    void start();
    void run();
};

extern AsyncQueue *aq;
void spawn_worker_threads(int n);

#endif
