#ifndef _STANDARD_H_
#define _STANDARD_H_
#include "emp_server.h"

int get_input(connection_t *connection, char *output) ;
int _read(connection_t *connection, message_t *message);
int _write(connection_t *connection, message_t *message);
enum try_read_result try_read_udp(connection_t *connection);
enum try_read_result try_read_network(connection_t *connection, message_t *message);

#endif
