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

/* ------------------------------------------------------------------------- */

class FibJob : public AsyncJob
{
    public:
        int n, m;
        int ans;

        FibJob(int n, int m);
        virtual void run();
};

FibJob::FibJob(int n, int m) {
    this->n = n; this->m = m;
}

void FibJob::run(void)
{
    printf("starting, n = %d\n", n);
    int a=0, b=1, c;
    for(int i=0; i<n; i++) {
        c = (a + b) % m;
        a =b;
        b =c;
    }
    ans = a;
    printf("finished, n = %d\n", n);
}

int main(void)
{
    AsyncQueue *q = new AsyncQueue();

    WorkerThread *threads[4];
    for(int i=0; i<4; i++) {
        threads[i] = new WorkerThread(q);
        threads[i]->start();
    }

    Completion c;
    FibJob *jobs[10];

    for(int i=0; i<10; i++) {
        jobs[i] = new FibJob((i+1)*10000000, 43246731);
        jobs[i]->completion = &c;
    }

    for(int i=0; i<10; i++)
        q->queue(jobs[i]);

    c.wait();

    for(int i=0; i<10; i++) {
        printf("answer from %d: %d\n", i, jobs[i]->ans);
        delete jobs[i];
    }

    return 0;
}


