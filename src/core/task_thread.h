#ifndef	_TASK_THREADS_H_
#define	_TASK_THREADS_H_

#include "emp_server.h"

/** 
 * @brief	task_queue_item: struct define of task_queue_item link 
 */
typedef struct task_queue_item_s{    
    struct task_queue_item_s  *next;
    connection_t *connection;
    int sfd;	
    struct sockaddr_in addr;
    union u_task_data {
        message_t msg;
    } task_data;
} task_queue_item_t;


/** 
 * @brief	task_queue: struct define of task_queue link 
 */
typedef struct task_queue_s{
    task_queue_item_t *head;
    task_queue_item_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} task_queue_t;

typedef struct task_thread_s task_thread_t;
typedef struct task_threads_manager_s task_threads_manager_t;


/** 
 * @brief	task_threads_manager: struct define of task_threads_manager 
 */
struct task_threads_manager_s{
    pthread_mutex_t             init_lock;
    pthread_cond_t              init_cond;
    int                         init_count;
    pthread_mutex_t             task_freelist_lock;
    task_queue_item_t           *task_freelist;
    void                        (*thread_libevent_process)(int, short, void *);
    task_thread_t                 *threads;
};

/** 
 * @brief	task_thread: struct define of task_thread 
 */
struct task_thread_s{
    task_threads_manager_t *ttm;   /* structure for threads manager */
    pthread_t thread_id;        /* unique ID of this thread */
    struct event_base *base;    /* libevent handle this thread uses */
    struct event notify_event;  /* listen event for notify pipe */
    int notify_receive_fd;      /* receiving end of notify pipe */
    int notify_send_fd;         /* sending end of notify pipe */
    task_queue_t *new_task_queue; /* queue of new connections to handle */
};

/***********************************************************/
/**
 * Init the connect task queue
 * @param tq: task queue to init
 ************************************************************/
extern void task_queue_init(task_queue_t *task_queue);

/***********************************************************/
/** 
 * Looks for an item on a connection queue, but doesn't block if there isn't one
 * @param tq: task queue to pop
 * @return   item, or NULL if no item is available
 ************************************************************/
extern task_queue_item_t *task_queue_pop(task_queue_t *task_queue);

/***********************************************************/
/**
 * Adds an item to a connection queue
 * @param tq: task queue to push
 * @param item: task queue item
 ************************************************************/
extern void task_queue_push(task_queue_t *tq, task_queue_item_t *item);

/***********************************************************/
/**
 * To return a fresh connection queue item
 * @param   t_list
 * @param   t_list_lock 
 * @param   nitems
 * @return  item, or NULL if no item is available
 ************************************************************/
extern task_queue_item_t *task_queue_item_new(task_threads_manager_t *thread_manager, int nitems);

/***********************************************************/
/**
 * Frees a connection queue item (adds it to the freelist.)
 * @param   t_list: task queu list
 * @param   t_list_lock: mutex
 * @param   item: task queue item
 ************************************************************/
extern void task_queue_item_free(task_threads_manager_t *thread_manager, task_queue_item_t *item);

/***********************************************************/
/**
 * event callback function handle for the thread process
 * @param   ttm   
 * @param   callback 
 ************************************************************/
extern void set_event_callback(task_threads_manager_t *ttm, void (*callback)(int, short, void *));

/***********************************************************/
/** 
* inti the task thread and create worker thread
* @param   nthreads: number threads to be init
* @param   base: event_base
* @param   ttm: task thread manager
* @see
************************************************************/
extern void task_thread_init(int nthreads, struct event_base *base, task_threads_manager_t *ttm);

/***********************************************************/
/**
 * create a worker thread for the function
 * @param   func
 * @param   arg
 *
 ***********************************************************/
extern void create_worker_thread(void *(*func)(void *), void *arg); 


#endif

