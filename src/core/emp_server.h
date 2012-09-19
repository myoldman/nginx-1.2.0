#ifndef _EMP_SERVER_H_
#define _EMP_SERVER_H_

#define MAX_HEADERS     256
#define MAX_LEN         100
#define MAX_TIMER_NUM		1000

/********************/
/* structure define */
/********************/

typedef struct emp_server_s{
  char emp_host[40];
  char emp_port[10];
  int status;
  int type; // 1-active, 0-standby
  struct emp_server_s *next;
} emp_server_t;

typedef struct{
  emp_server_t *serverlist;
  int worker_threads;
} proxy_config_t;

/**
 * The structure representing a connection into cti.
 */
typedef struct {
	int    sfd;

	char inbuf[MAX_LEN];
	int inlen;

	emp_server_t *server;
	struct sockaddr_in sin;
	pthread_t t;
	pthread_mutex_t lock;

	u_int32_t pingseq; // Ping sequence id
	u_int32_t pongseq; // Pong sequence id
	time_t conntime;   // timestamp of socket connection established
	time_t logintime;  // timestamp of send login request
	time_t logofftime; // timestamp of send logoff request

	connection_t *pair; // other of dual sessions

	struct event event;
	short  ev_flags;
	short  which;   /** which events were just triggered */

	void   *item;     /* for commands set/add/replace  */

	/* data for the swallow state */
	int    sbytes;    /* how many bytes to swallow */

	enum protocol protocol;   /* which protocol this connection speaks */
	enum network_transport transport; /* what transport is used by this connection */


	connection_t   *next;     /* Used for generating a list of conn structures */
	void *thread; /* Pointer to the thread object serving this connection */

	int subscribe; /*the subscribe event*/
	time_t subTime; /*the timestamp subscribe*/

	int connecting;
}connection_t;


typedef struct {
  int hdrcount;
  char headers[MAX_HEADERS][MAX_LEN];
  int in_command;
  connection_t *connection;
} message_t;

enum try_read_result {
    READ_DATA_RECEIVED,
    READ_NO_DATA_RECEIVED,
    READ_ERROR,            /** an error occured (on the socket) (or client closed connection) */
    READ_MEMORY_ERROR      /** failed to allocate more memory */
};

enum network_transport {
    local_transport, /* Unix sockets*/
    tcp_transport,
    udp_transport
};

enum protocol {
    ascii_prot = 3, /* arbitrary value. */
    binary_prot,
    negotiating_prot /* Discovering the protocol */
};

/** 
 * @brief server_asymsg:struct define for the server asy message
 */
typedef struct {
	pthread_mutex_t	lock;
	message_t message;
	time_t timestamp;
	char server[20];
	timer_id tid;
	int appoint;
	connection_t *connection;
	struct event timeout_event;
	server_asymsg_t *next;
}server_asymsg_t;

enum{
	ACTION = 0,
	EVENT,
};

typedef struct {
	int  type;
	char *name;
	void (*handle_fun)(connection_t *connection, message_t *message);
} emp_server_message_handler_t;

extern connection_t *emp_conns ;
extern pthread_mutex_t emp_conn_lock;
extern struct event_base *listen_base;
extern struct event_base *timer_base;
extern proxy_config_t proxy_config;


int server_connect_init(struct event_base *server_ev_base);
int server_threads_init(struct event_base *server_ev_base);
#endif

