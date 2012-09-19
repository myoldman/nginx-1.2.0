#include "emp_server.h"
#include "task_thread.h"
#include "standard.h"
#include "dprint.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof(0[a]))
#define HANDLE_COUNT ARRAY_LEN(built_handle_server)
#define SERVER_ITEM_NUMBER         64
task_threads_manager_t server_threads_manager;
int last_server_thread = -1;
server_asymsg_t *asymsg_list = NULL;
pthread_mutex_t asymsg_list_lock = PTHREAD_MUTEX_INITIALIZER;

connection_t *emp_conns = NULL;
pthread_mutex_t emp_conn_lock = PTHREAD_MUTEX_INITIALIZER;
proxy_config_t proxy_config;
struct event_base *listen_base;
struct event_base *timer_base;
int server_now = -1;
int server_num = 0;



// method definition
static emp_server_message_handler_t *server_find_handle(int type, const char *name);
static void server_push_command(message_t *message) ;
static void server_message_got(int fd, short which, void *arg);
static void server_taskitem_got(int fd, short which, void *arg);
static int emp_server_connect(connection_t *connection);
char *emp_get_header(message_t *message, char *var);


connection_t *connection_new(const int sfd, void (*callback)(int, short, void *),
                                        const int event_flags,
                                        enum network_transport transport,
                                        struct sockaddr_in addr,
                                        struct event_base *base);
void connection_cleanup(connection_t *connection);
void connection_free(connection_t *connection);
void connection_close(connection_t *connection);

int connection_remove_from_list(connection_t *connection);
int connection_add_to_list(connection_t *connection);


void server_dispatch_conn(int sfd, int event_flags,
                       				struct event_base *server_ev_base,
                       				struct sockaddr_in addr, emp_server_t *server);

static void handle_QueryApiOverageRes(connection_t *connection, message_t *message);
static void handle_SendResponseBodyRes(connection_t *connection, message_t *message);


static emp_server_message_handler_t built_handle_server[] = 
{
	{ACTION, 	"QueryApiOverageRes", 			handle_QueryApiOverageRes},
	{ACTION, 	"SendResponseBodyRes", 			handle_SendResponseBodyRes},
};

/** 
* find the event handle with the name and type
* @param   name: the event's name
************************************************************/
static emp_server_message_handler_t *server_find_handle(int type, const char *name)
{
	unsigned int x;
	for (x = 0; x < HANDLE_COUNT; x++) {
		if ((built_handle_server[x].type == type) && !strcasecmp(name, built_handle_server[x].name))
			return (emp_server_message_handler_t*)&built_handle_server[x];
	}

	
	//LM_DBG("can't find the handle, handle is %s\n", name);
	return NULL;
}

static void handle_QueryApiOverageRes(connection_t *connection, message_t *message){
		
}

static void handle_SendResponseBodyRes(connection_t *connection, message_t *message){
		
}

 /***********************************************************/
 /** 
 * Recursive thread safe replacement of inet_ntoa
 * @param	buf: the buffer
 * @param	bufsiz: buffer's size
 * @param	bufsiz: the sturct in_addrs
 ************************************************************/
 const char *ast_inet_ntoa(char *buf, int bufsiz, struct in_addr ia) {
   return inet_ntop(AF_INET, &ia, buf, bufsiz);
 }

 /***********************************************************/
 /** 
 * get the header from messqge
 * @param	m: the meessage 
 * @param	var: the header's name
 ************************************************************/
 char *emp_get_header(message_t *message, char *var) {
   char cmp[80];
   int x;
   snprintf(cmp, sizeof(cmp), "%s: ", var);
   for (x = 0; x < message->hdrcount; x++)
	 if (!strncasecmp(cmp, message->headers[x], strlen(cmp)))
	   return message->headers[x] + strlen(cmp);
   return "";
 }

 /***********************************************************/
 /** 
 * create a new connection
 * @param	afd: the socket fd
 * @param	callback: callback function for connection
 * @param	event_flags: event's flag
 * @param	transport: the the net work type
 * @param	base: base event
 ************************************************************/
 connection_t *connection_new(const int sfd, void (*callback)(int, short, void *),
				 const int event_flags,
				 enum network_transport transport,
				 struct sockaddr_in addr,
				 struct event_base *base) {
 
	 connection_t *connection = NULL;
	 
	 if (!(connection = (connection_t *)calloc(1, sizeof(connection_t)))) {
		 fprintf(stderr, "calloc()\n");
		 return NULL;
	 }
	 
	 pthread_mutex_init(&connection->lock, NULL);
	 pthread_mutex_lock(&connection->lock);
	 
	 connection->transport = transport;
	 //c->protocol = tcp_transport;
	 connection->sin = addr;
 
	 connection->sfd = sfd;
	 connection->item = 0;
	 connection->ev_flags = event_flags;
 
	 event_set(&connection->event, sfd, event_flags, callback, (void *)connection);
	 event_base_set(base, &connection->event);
	
	 if (event_add(&connection->event, 0) == -1) {
	 	LM_ERR("event_add failed\n");
		 pthread_mutex_unlock(&connection->lock);
		 connection_close(connection);
		 perror("event_add");
		 return NULL;
	 }
	 
	 pthread_mutex_unlock(&connection->lock);
 
	 return connection;
 }

  
  
   /***********************************************************/
  /** 
  * cleanup a connection
  * @param	 c: the connection which will be cleanup
  ************************************************************/
  void connection_cleanup(connection_t *c) {
	  assert(c != NULL);
  
  }

  
   /***********************************************************/
  /** 
  * free a connection
  * @param	 c: the connection which will be free
  ************************************************************/
  void connection_free(connection_t *c) {
	  if (c) {
		  free(c);
	  }
  }

  /***********************************************************/
 /** 
 * close a connection
 * @param	c: the connection which will be closed
 ************************************************************/
 void connection_close(connection_t *connection) {
	 assert(connection != NULL);
 
	 char iabuf[INET_ADDRSTRLEN];
	 LM_DBG("*** Disconnect from [%s]\n", ast_inet_ntoa(iabuf, sizeof(iabuf), connection->sin.sin_addr));
 
	 connection_remove_from_list(connection);
 
	 //if (!ret)
	//	 return;
 
	 event_del(&connection->event);
	 close(connection->sfd);
	 pthread_mutex_destroy(&connection->lock);
 /*
	 if (c->pair) {
		 close(c->pair->sfd);
		 pthread_mutex_destroy(&c->pair->lock);
		 free(c->pair);
	 }
 */
	 free(connection);
 
	 return;
 }

 /***********************************************************/
/** 
* add a connection to as list
* @param   c: the connection which will add to the list
************************************************************/
int connection_add_to_list(connection_t *connection)
 {
    int ret = 1;

	connection_t *tmp = emp_conns;

	if( emp_conns == NULL )
	{
		LM_ERR( "emp_conns is NULL now 1\n");
	}
	
	while(tmp) {
		if (!strcmp(tmp->server->emp_host, connection->server->emp_host) 
			&& !strcmp(tmp->server->emp_port, connection->server->emp_port)) {
			LM_ERR("already in the emp server list\n");
			return ret;
		}

		tmp = tmp->next;
	}
	
	pthread_mutex_lock(&emp_conn_lock);

	pthread_mutex_lock(&connection->lock);
	connection->next = emp_conns;
	pthread_mutex_unlock(&connection->lock);

	emp_conns = connection;
	server_num++;
	if( emp_conns == NULL )
	{
		LM_ERR( "emp_conns is NULL now\n");
	}
	else
		LM_DBG("add to emp conns %d\n", connection->sfd);
	pthread_mutex_unlock(&emp_conn_lock);

	return ret;
}

 /***********************************************************/
/** 
* remove the connection from ast list
* @param   c: the connection which will  be removed
************************************************************/
int connection_remove_from_list(connection_t *connection)
{

    int ret;
    connection_t *cur, *prev = NULL;

    pthread_mutex_lock(&emp_conn_lock);
    cur = emp_conns;
    while (cur){
        if (cur == connection)
            break;
        prev = cur;
        cur = cur->next;
    }
    
    if (cur){
        if (prev)
            prev->next = cur->next;
        else
            emp_conns = cur->next;
    
        LM_DBG("remove connection: [%d]\n", connection->sfd);
		ret  = 1;
   } else{
        //LM_ERR("Trying to delete non-existent session?\n");
		ret = 0;
   }
    pthread_mutex_unlock(&emp_conn_lock);
	return ret;
}

/***********************************************************/
/** 
* connect to as server 
* @param   s: the the connection 
************************************************************/
int emp_server_connect(connection_t *connection) {
	int r = 0, res = 0;
	char iabuf[INET_ADDRSTRLEN];
  	const char *ipaddr = (const char*)ast_inet_ntoa(iabuf, sizeof(iabuf), connection->sin.sin_addr);
  	if (ipaddr == NULL) 
   		 ipaddr = "n/a";
	
	for (;;) {
		if (connect(connection->sfd, (struct sockaddr *) &connection->sin, sizeof(connection->sin)) < 0) {
			++r;
			if (errno == EISCONN) {
				pthread_mutex_lock(&connection->lock);
				connection->sfd = socket(AF_INET, SOCK_STREAM, 0);

				// set socket to nonblock
			/*	
				int flags = 1;
				if ((flags = fcntl(s->sfd, F_GETFL, 0)) < 0 || fcntl(s->sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
				                LM_ERR("setting O_NONBLOCK fail \n");
				                close(s->sfd);
				                continue;
				   }
			*/
				struct event_base *base = connection->event.ev_base;
				if (event_del(&connection->event) == -1) {
					pthread_mutex_unlock(&connection->lock);
					LM_ERR("event_del failed \n");
					continue;
				}
				
				event_set(&connection->event, connection->sfd, EV_READ | EV_PERSIST, server_message_got, (void *)connection);
				event_base_set(base, &connection->event);
				
				if (event_add(&connection->event, 0) == -1) {
					pthread_mutex_unlock(&connection->lock);
					LM_ERR("event_del failed \n");
					continue;
				}
				
				pthread_mutex_unlock(&connection->lock);
			}

			LM_DBG("%d> emp_server@%s: Connect failed, Retrying (%04d) %s\n",
				connection->sfd, ipaddr, r, strerror(errno));

			if (proxy_config.maxretries && (r > proxy_config.maxretries)) {
				res = 1;
				break;
			} else
				sleep(proxy_config.retryinterval);
		} else {
			LM_DBG("%d> emp_server@%s connect to emp_server\n", connection->sfd, ipaddr);
			res = 0;
			break;
		}

	}

	return res;
}


/***********************************************************/
/** 
* got command from server, and push them to task queue
* @param   m: the message got 
************************************************************/
static void server_push_command(message_t *message) {
	if( message == NULL ){
		LM_ERR( " server push message is null \n" );
		return;
	}
	connection_t *connection;

	task_thread_t *thread;
	task_queue_item_t *item;
	
	connection = (connection_t*)message->connection;
	
	/*push to the task queue*/
	thread = (task_thread_t *)(connection->thread);
	
	item = task_queue_item_new(thread->ttm,
                            			    SERVER_ITEM_NUMBER);

	if (item == NULL){
		LM_ERR("task_queue_item_new return null\n");
		return ;
	}
	
 	item->task_data.msg = *message;
    task_queue_push(thread->new_task_queue, item);

    if (write(thread->notify_send_fd, "", 1) != 1) {
        LM_DBG("Writing to thread notify pipe\n");
    }

	return ;
	
}


/***********************************************************/
/** 
* the callback function when got a message from server
* @param   fd: the socket which got a read event  
* @param   which: not use
* @param   arg: param send to the callback function
************************************************************/
static void server_message_got(int fd, short which, void *arg) {
	int res;
	message_t message;
	connection_t *connection;

	connection = (connection_t*)arg;
    memset(&message, 0, sizeof(connection_t));	   
	res = try_read_network(connection, &message);
	
	switch (res) {
            case READ_DATA_RECEIVED:
                server_push_command(&message);
                break;	
            case READ_ERROR:
				connection_remove_from_list(connection);
				if (! emp_server_connect(connection)) 
					connection_add_to_list(connection);
		 		break;
			default:
				break;
    }
}

/***********************************************************/
/** 
* the callback function when the task queue got item
* @param   fd: the pipe which got a read event  
* @param   which: not use
* @param   arg: param send to the callback function
************************************************************/
static void server_taskitem_got(int fd, short which, void *arg) {

	task_thread_t *me = (task_thread_t*)arg;
	task_queue_item_t *item;
	char buf[1];
	message_t message;
	memset( &message, 0, sizeof(message_t) );

	message_t message_out;
	connection_t *connection;
	char *action;

	//LM_DBG("handle a new taskitem, [%d]\n", fd);

	if (read(fd, buf, 1) != 1) {}

	item = task_queue_pop(me->new_task_queue);
	message = item->task_data.msg;
	
	if (NULL != item) {
		memset(&message_out, 0, sizeof(message_t));
		message = item->task_data.msg;
		connection = message.connection;
		action = (char*)emp_get_header(&message, "Action");

		task_queue_item_free(&server_threads_manager,item);
		
		//LM_DBG( "ActionID is [%s], Event is [%s]\n", actionid, event);

		emp_server_message_handler_t *handles = NULL;
		
		handles = (emp_server_message_handler_t*)	server_find_handle(ACTION, action);
		if (handles){
			handles->handle_fun(connection, &message);	
			return;
		}
	}
	
}


/***********************************************************/
/** 
* dispatch a thread for a new connection
* @param   sfd: the fd of the connection
* @param   event_flags: event's flag
* @param   server_ev_base: base event
* @param   addr: the ip address of connection
* @param   srv: the struct of ast_server
************************************************************/
void server_dispatch_conn(int sfd, int event_flags,
                  				struct event_base *server_ev_base,
                       				struct sockaddr_in addr, emp_server_t *srv){


	connection_t *connection = (connection_t*)connection_new(sfd, server_message_got, event_flags, tcp_transport,
 	           		     	    addr, server_ev_base);									
	last_server_thread++;
	//LM_DBG ("last_server_thread is %d\n", last_server_thread);
	task_thread_t *thread = server_threads_manager.threads + last_server_thread;
	
	
//	conn *c = (struct conn*)conn_new(sfd, server_message_got, event_flags, tcp_transport,
//	           		     	    addr, thread->base);									

	connection->server = srv;
	connection->thread = thread;

	if (thread == NULL)
		LM_ERR("server_dispatch_conn thread is null\n");

}

/***********************************************************/
/** 
* do the thread init work for server
* @param   server_ev_base: base event 
************************************************************/
int server_threads_init(struct event_base *server_ev_base) {

    	memset(&server_threads_manager, 0x00, sizeof(task_threads_manager_t));
    	set_event_callback(&server_threads_manager, server_taskitem_got);

    	task_thread_init(proxy_config.svr_n, server_ev_base, &server_threads_manager);
			
	return 0;
}

/***********************************************************/
/** 
* do the init work for server
* @param   server_ev_base: base event 
************************************************************/
int server_connect_init(struct event_base *server_ev_base){

	emp_server_t *srv;

	struct sockaddr_in sin;  
	struct hostent *ast_hostent;
//	char iabuf[INET_ADDRSTRLEN];
	int sfd = 0;
	
	srv = proxy_config.serverlist;
	while (srv) {

		LM_DBG("srv->port= %s\n", srv->emp_port);	
		ast_hostent = gethostbyname(srv->emp_host);

		if (!ast_hostent) {
			LM_ERR("Cannot resolve host %s, cannot add!\n", srv->emp_host);
			continue;
		}

		bzero((char *) &sin, sizeof(sin));
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr.s_addr, ast_hostent->h_addr, ast_hostent->h_length);
		sin.sin_port = htons(atoi(srv->emp_port));

		int flag = 1;
		setsockopt( sfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );
		
		sfd = socket(AF_INET, SOCK_STREAM, 0);

/*			
		int flags = 1;

		// set socket to nonblock
	       if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 || fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
	                    LM_ERR("setting O_NONBLOCK fail \n");
	                    close(sfd);
	                    continue;
	       }
*/

		server_dispatch_conn(sfd, EV_READ | EV_PERSIST, server_ev_base,  sin, srv);
		
		srv = srv->next;
	}

	return 0;
}



