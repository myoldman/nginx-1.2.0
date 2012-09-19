/** @file
 * source file of the task thread and task queue
 * \author lh
 */
#include "task_thread.h"
#include "dprint.h"
	
/***********************************************************/
/**
 * event callback function handle for the thread process
 * @param   ttm   
 * @param   callback 
 ************************************************************/
void set_event_callback(task_threads_manager_t *ttm, void (*callback)(int, short, void *)) {
    ttm->thread_libevent_process = callback;
}

/***********************************************************/
/**
 * Init the connect task queue
 * @param tq: task queue to init
 ************************************************************/
void task_queue_init(task_queue_t *tq) {
    pthread_mutex_init(&tq->lock, NULL);
    pthread_cond_init(&tq->cond, NULL);
    tq->head = NULL;
    tq->tail = NULL;
}

/***********************************************************/
/** 
 * Looks for an item on a connection queue, but doesn't block if there isn't one
 * @param tq: task queue to pop
 * @return   item, or NULL if no item is available
 ************************************************************/
task_queue_item_t *task_queue_pop(task_queue_t *task_queue) {
    task_queue_item_t *item;

    pthread_mutex_lock(&task_queue->lock);
    item = task_queue->head;
    if (NULL != item) {
        task_queue->head = item->next;
        if (NULL == task_queue->head)
            task_queue->tail = NULL;
    }
    pthread_mutex_unlock(&task_queue->lock);

    return item;
}

/***********************************************************/
/**
 * Adds an item to a connection queue
 * @param tq: task queue to push
 * @param item: task queue item
 ************************************************************/
void task_queue_push(task_queue_t *tq, task_queue_item_t *item) {
    if( tq == NULL || item == NULL ){
	LM_ERR("in tq_push tq or item is null\n");
	return;
    }
    item->next = NULL;

    pthread_mutex_lock(&tq->lock);
    if (NULL == tq->tail)
        tq->head = item;
    else
        tq->tail->next = item;
    tq->tail = item;
    pthread_cond_signal(&tq->cond);
    pthread_mutex_unlock(&tq->lock);
}

/***********************************************************/
/**
 * To return a fresh connection queue item
 * @param   t_list
 * @param   t_list_lock 
 * @param   nitems
 * @return  item, or NULL if no item is available
 ************************************************************/
task_queue_item_t *task_queue_item_new(task_threads_manager_t *thread_manager, int nitems) {

	if( thread_manager == NULL ){
		LM_ERR( " in tq_item_new thead manager is null \n");
		return NULL;
	}

    task_queue_item_t *item = NULL;
    pthread_mutex_t *t_list_lock;
    t_list_lock	= &(thread_manager->task_freelist_lock);
	
    pthread_mutex_lock(t_list_lock);
    if (thread_manager->task_freelist) {
        item = thread_manager->task_freelist;
        thread_manager->task_freelist = item->next;
    }
    pthread_mutex_unlock(t_list_lock);
    

    if (NULL == item) {
		int i;
		LM_DBG("freelist is null ,should molloc\n");
	
        /* Allocate a bunch of items at once to reduce fragmentation */
        item = (task_queue_item_t*)malloc(sizeof(task_queue_item_t) * nitems);
        if (NULL == item){
	     LM_DBG("out of memery\n");
            return NULL;
        }

        /*
         * Link together all the new items except the first one
         * (which we'll return to the caller) for placement on
         * the freelist.
         */
        for (i = 2; i < nitems; i++)
            item[i - 1].next = &item[i];

        pthread_mutex_lock(t_list_lock);
        item[nitems - 1].next = thread_manager->task_freelist;
        thread_manager->task_freelist = &item[1];
        pthread_mutex_unlock(t_list_lock);
    }

    //LM_DBG("freelist = %p\n", thread_manager->task_freelist);
    return item;
}

/***********************************************************/
/**
 * Frees a connection queue item (adds it to the freelist.)
 * @param   t_list: task queu list
 * @param   t_list_lock: mutex
 * @param   item: task queue item
 ************************************************************/
void task_queue_item_free(task_threads_manager_t *thread_manager, task_queue_item_t *item) {

	if( thread_manager == NULL || item == NULL ){
		LM_ERR( " in tq_item_free thead manager or item is null \n");
		return;
	}

    pthread_mutex_t *t_list_lock = &(thread_manager->task_freelist_lock);


    pthread_mutex_lock(t_list_lock);
    item->next = thread_manager->task_freelist;
    thread_manager->task_freelist = item;
    pthread_mutex_unlock(t_list_lock);


}

/***********************************************************/
/**
 * setup task thread
 * @param   me   
 *
 ************************************************************/
static void setup_task_thread(task_thread_t *me) {

	if( me == NULL ){
		LM_ERR( " in setup_task_thread task_thread me is null \n");
		return;
	}
    me->base = event_init();
    if (! me->base) {
        fprintf(stderr, "Can't allocate event base\n");
        exit(1);
    }

    /* Listen for notifications from other threads */
    event_set(&me->notify_event, me->notify_receive_fd,
              EV_READ | EV_PERSIST, me->ttm->thread_libevent_process, me);
    event_base_set(me->base, &me->notify_event);
    
    if (event_add(&me->notify_event, 0) == -1) {
        fprintf(stderr, "Can't monitor libevent notify pipe\n");
        exit(1);
    }
    
    me->new_task_queue = (task_queue_t*)malloc(sizeof(task_queue_t));
    if (me->new_task_queue == NULL) {
        LM_ERR("Failed to allocate memory for connection queue");
        exit(EXIT_FAILURE);
    }
    task_queue_init(me->new_task_queue);
	/*
    if (pthread_mutex_init(&me->stats.mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }
    */
}

/***********************************************************/
/**
 * create a worker thread for the function
 * @param   func
 * @param   arg
 *
 ************************************************************/
void create_worker_thread(void *(*func)(void *), void *arg) {
    pthread_t       thread;
    pthread_attr_t  attr;
    int             ret;

    pthread_attr_init(&attr);

    if ((ret = pthread_create(&thread, &attr, func, arg)) != 0) {
        fprintf(stderr, "Can't create thread: %s\n",
                strerror(ret));
        exit(1);
    }
}

/***********************************************************/
/**
 * worker thread: main event loop
 * @param   arg
 *
 ************************************************************/
static void *worker_thread(void *arg) {

	if( arg == NULL ){
		LM_ERR( " in worker_thread arg is null \n");
		return NULL;
	}

    task_thread_t *me = (task_thread_t*)arg;
    task_threads_manager_t *ttm = me->ttm;
    /* Any per-thread setup can happen here; thread_init() will block until
     * all threads have finished initializing.
     */

    pthread_mutex_lock(&ttm->init_lock);
    ttm->init_count++;
    pthread_cond_signal(&ttm->init_cond);
    pthread_mutex_unlock(&ttm->init_lock);

    LM_DBG("Thread [%lu] start up.\n", pthread_self());

    event_base_loop(me->base, 0);
    return NULL;
}

/***********************************************************/
/** 
* inti the task thread and create worker thread
* @param   nthreads: number threads to be init
* @param   base: event_base
* @param   ttm: task thread manager
* @see
************************************************************/
void task_thread_init(int nthreads, struct event_base *base, task_threads_manager_t *ttm) {

	if( ttm == NULL ){
		LM_ERR( " in task_thread_init ttm is null \n");
		return;
	}

    int         i;
    task_thread_t *threads;

    pthread_mutex_init(&ttm->init_lock, NULL);
    pthread_cond_init(&ttm->init_cond, NULL);

    pthread_mutex_init(&ttm->task_freelist_lock, NULL);
    ttm->task_freelist = NULL;

    ttm->threads = (task_thread_t*)calloc(nthreads, sizeof(task_thread_t));
    threads = ttm->threads;
    if (! threads) {
        LM_ERR("Can't allocate thread descriptors");
        exit(1);
    }

    for (i = 0; i < nthreads; i++) {
        int fds[2];
        if (pipe(fds)) {
            LM_ERR("Can't create notify pipe");
            exit(1);
        }

        threads[i].notify_receive_fd = fds[0];
        threads[i].notify_send_fd = fds[1];

        LM_DBG("Thread pipe fds [%d], [%d]\n", fds[0], fds[1]);

        threads[i].ttm = ttm;

        setup_task_thread(&threads[i]);
    }
    LM_DBG("Pipe of threads create successfully.\n");

    /* Create threads after we've done all the libevent setup. */
    for (i = 0; i < nthreads; i++) {
        create_worker_thread(worker_thread, &threads[i]);
    }

    /* Wait for all the threads to set themselves up before returning. */
    pthread_mutex_lock(&ttm->init_lock);
    while (ttm->init_count < nthreads) {
        pthread_cond_wait(&ttm->init_cond, &ttm->init_lock);
    }
    pthread_mutex_unlock(&ttm->init_lock);

    LM_DBG("Worker thread start up finished.\n");
}


