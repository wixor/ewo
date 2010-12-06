#include <cstdio>
#include <pthread.h>
#include "workers.h"

Completion::Completion(void)
{
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    counter = 0;
}

Completion::~Completion(void)
{
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

void Completion::completed(void)
{
    pthread_mutex_lock(&mutex);
    if(--counter == 0)
        pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

void Completion::queued(void)
{
    pthread_mutex_lock(&mutex);
    counter++;
    pthread_mutex_unlock(&mutex);
}

void Completion::wait(void)
{
    pthread_mutex_lock(&mutex);
    while(counter)
        pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

/* ----------------------------------------------------------------------- */

AsyncJob::~AsyncJob(void)
{
    /* empty */
}

AsyncQueue::AsyncQueue(void)
{
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    first = NULL;
    last = &first;
}

AsyncQueue::~AsyncQueue(void)
{
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

void AsyncQueue::queue(AsyncJob *job)
{
    pthread_mutex_lock(&mutex);

    job->next = NULL;
    *last = job;
    last = &job->next;

    job->completion->queued();
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);

}

AsyncJob *AsyncQueue::get(void)
{
    pthread_mutex_lock(&mutex);

    while(first == NULL)
        pthread_cond_wait(&cond, &mutex);

    AsyncJob *job = first;
    first = first->next;
    if(first == NULL) last = &first;

    pthread_mutex_unlock(&mutex);

    return job;
}

/* ----------------------------------------------------------------------- */

static void* run_worker_thread(void * arg) {
    ((WorkerThread *)arg)->run();
    return NULL;
}

WorkerThread::WorkerThread(AsyncQueue *queue) {
    this->queue = queue;
}

void WorkerThread::start(void) {
    pthread_create(&thread, NULL, run_worker_thread, this);
}

void WorkerThread::run(void)
{
    for(;;)
    {
        AsyncJob *job = queue->get();
        job->run();
        job->completion->completed();
    }
}

/* ----------------------------------------------------------------------- */

AsyncQueue *aq;

void spawn_worker_threads(int n)
{
    aq = new AsyncQueue();
    while(n--)
        (new WorkerThread(aq))->start();
}
