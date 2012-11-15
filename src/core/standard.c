#include "standard.h"
#include "dprint.h"


/** 
* This routine based on get_input from asterisk manager.c
* Good generic line-based input routine for \r\n\r\n terminated input 
* Used by standard.c and other input handlers
* @param   s: the connection 
* @param   output: the buffer save the message
************************************************************/
int get_input(connection_t *connection, char *output) {
  /* output must have at least sizeof(s->inbuf) space */
  int res;
  int x;
  struct pollfd fds[1];
  //char iabuf[INET_ADDRSTRLEN];
  
  //LM_DBG("s->inbuf = %s\n", s->inbuf);

  /* Look for \r\n from the front, our preferred end of line */
  for (x = 0; x < connection->inlen; x++) {
    int xtra = 0;
    if (connection->inbuf[x] == '\n') {
      if (x  && connection->inbuf[x-1] == '\r') {
        xtra = 1;
      }
      /* Copy output data not including \r\n */
      memcpy(output, connection->inbuf, x - xtra);
      /* Add trailing \0 */
      output[x-xtra] = '\0';
      /* Move remaining data back to the front */
      memmove(connection->inbuf, connection->inbuf + x + 1, connection->inlen - x);
      connection->inlen -= (x + 1);
      return 1;
    }
  }

  if ((unsigned int)connection->inlen >= sizeof(connection->inbuf) - 1) {
	char iabuf[INET_ADDRSTRLEN]; 
	const char *ipaddr = (const char*)ast_inet_ntoa(iabuf, sizeof(iabuf), connection->sin.sin_addr);

	if (ipaddr == NULL) 
		ipaddr = "n/a";
		
 	if (connection->server)	
      		LM_ERR("Warning: got wrong msg from server[%s]\n", ipaddr);
      else
		LM_ERR("Warning: got wrong msg from client[%s]\n", ipaddr);

	connection->inlen = 0;
	// return -1;
  }


  fds[0].fd = connection->sfd;
  fds[0].events = POLLIN;
  res = poll(fds, 1, -1);
  if (res < 0) {
    LM_ERR("Select returned error from fd[%d], error:%s\n", fds[0].fd, strerror(errno));
    return 0; 
  } else if (res > 0) {
    pthread_mutex_lock(&connection->lock);
   
    res = read(connection->sfd, connection->inbuf + connection->inlen, sizeof(connection->inbuf) - 1 - connection->inlen);
    pthread_mutex_unlock(&connection->lock);

    if (res < 1)
      return -1;

  }
  connection->inlen += res;
  connection->inbuf[connection->inlen] = '\0';

  //LM_DBG("s->inbuf = %s\n", s->inbuf);

  return 0;
  /* We have some input, but it's not ready for processing */
}

/*
 * read a UDP request.
 */
enum try_read_result try_read_udp(connection_t *connection) {
   	//int res;

    assert(connection != NULL);

    return READ_NO_DATA_RECEIVED;
}


/*
 * read from network as much as we can, handle buffer overflow and connection
 * close.
 * before reading, move the remaining incomplete fragment of a command
 * (if any) to the beginning of the buffer.
 *
 * To protect us from someone flooding a connection with bogus data causing
 * the connection to eat up all available memory, break out and start looking
 * at the data I've got after a number of reallocs...
 *
 * @return enum try_read_result
 */
enum try_read_result try_read_network(connection_t *connection, message_t *message) {

    enum try_read_result gotdata = READ_NO_DATA_RECEIVED;
    int res;
    //int num_allocs = 0;
    assert(connection != NULL);
	assert(message != NULL);
    
    memset(message, 0, sizeof(message_t));

    res = _read(connection, message);
    message->connection= connection;

    //LM_DBG("read from fd = %d, m->hdrcount = %d \n",c->sfd, m->hdrcount); 
    int i;
    
    if (res > 0) {
		int bShow = 1;
		if (bShow){
			for (i = 0; i < message->hdrcount; i++){
				LM_DBG("rcv gotv fd(%d) <- %s\n", connection->sfd, message->headers[i]);
			}
		}
      
      gotdata = READ_DATA_RECEIVED;
    } else {
      //LM_ERR("READ_ERROR\n");
      return READ_ERROR;
    }

    return gotdata;
}


/* Return a fully formed message block to session_do for processing */
int _read(connection_t *connection, message_t *message) {
  int res;

  for (;;) {
    res = get_input(connection, message->headers[message->hdrcount]);

	/*
    if (strstr(connection->headers[connection->hdrcount], "--END COMMAND--")) {
      connection->in_command = 0;
    }

    if (strstr(connection->headers[connection->hdrcount], "Response: Follows")) {
      printf("Found Response Follows");
      connection->in_command = 1;
    }
    */
	
    if (res > 0) {
      if (!message->in_command && *(message->headers[message->hdrcount]) == '\0') {
        break;
      } else if (message->hdrcount < MAX_HEADERS - 1) {
        message->hdrcount++;
      } else {
        message->in_command = 0; // reset when block full
      }
    } else if (res < 0)
      break;
  }

  return res;
}

int _write(connection_t *connection, message_t *message) {
  int i;
  char msg[1024];
  memset(msg, 0, sizeof(msg));

  struct pollfd fds[1];

  pthread_mutex_lock(&connection->lock);
  for (i = 0; i < message->hdrcount; i++) {
    LM_DBG("write to fd(%d) -> msg = %s\n", connection->sfd, message->headers[i]);
    strncat(msg, message->headers[i], strlen(message->headers[i]));
    strncat(msg, "\r\n", 2);
    //write(s->sfd, m->headers[i], strlen(m->headers[i]));
    //write(s->sfd, "\r\n", 2);
  }

  strncat(msg, "\r\n", 2);
  pthread_mutex_unlock(&connection->lock);


  fds[0].fd = connection->sfd;
  fds[0].events = POLLOUT;
   
  int res = poll(fds, 1, -1);
  if (res < 0) {
    LM_ERR("Select returned error from fd[%d], error:%s\n", fds[0].fd, strerror(errno));
  } else if (res > 0) {

  	pthread_mutex_lock(&connection->lock);
  	res = write(connection->sfd, msg, strlen(msg));
	pthread_mutex_unlock(&connection->lock);

  }

  return res;
}

int _autodisconnect() {
  return 0;
}

