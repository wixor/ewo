#include <cstdio>
#include <ctime>
#include <pthread.h>

#include "util.h"

void (*progress)(float val) = null_progress;
void null_progress(float val) {
}
void text_progress(float val) {
    debug("progress: %.2f%%", val*100.f);
}

/* ----------------------------------------------------------------------- */

float Timer::gettime(void) {
    struct timespec now;
    clock_gettime(clock, &now);
    if (now.tv_nsec < ts.tv_nsec)
        now.tv_nsec += 1000000000, now.tv_sec -= 1;
    if (now.tv_sec < ts.tv_sec)
        now.tv_sec += 60;
    now.tv_nsec -= ts.tv_nsec;
    now.tv_sec -= ts.tv_sec;
    return (float)now.tv_sec + 0.000000001f*now.tv_nsec;
}

/* public: */
void Timer::start(void) {
    clock_gettime(clock, &ts);
    isrunning = true;
    delay = 0.0f;
}

void Timer::pause(void) {
    if (!isrunning)
        return ;
    isrunning = false;
    delay -= gettime();
}

void Timer::resume(void) {
    if (isrunning)
        return ;
    isrunning = true;
    delay += gettime();
}

float Timer::end(void) {
    float got = gettime();
    if (isrunning)
        return got - delay;
    else
        return - delay;
}

/* ----------------------------------------------------------------------- */

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

WorkerThread::WorkerThread(AsyncQueue *queue) {
    this->queue = queue;
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

static void* run_worker_thread(void * arg) {
    ((WorkerThread *)arg)->run();
    return NULL;
}
void WorkerThread::start(void) {
    pthread_create(&thread, NULL, run_worker_thread, this);
}

AsyncQueue *aq;

void spawn_worker_threads(int n)
{
    aq = new AsyncQueue();
    while(n--)
        (new WorkerThread(aq))->start();
}
