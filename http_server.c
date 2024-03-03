#include <stdio.h>
#include <sys/types.h>

#include "globals.h"
#include "util/utils.h"

int http_server_request_count=0;

void* http_server_request(void *args) {

	thread_begin(args);

	tcp_socket socket_connection = thread_get_socket_connection(args);

	tcp_set_socket_timeout(socket_connection);

	http_request request;

	if (http_recv_request(socket_connection, &request)) {
		tcp_socket_close(socket_connection);
		thread_end(args);
		return NULL;
	}

	int res;
	if (!strncmp(request.path, "/pgorm/", 7)) {

		res = req_pgorm(socket_connection, &request);

	} else {

		res = req_file(socket_connection, &request);

	}

	if (res)
		http_send_error(socket_connection, HTTP_STATUS_INTERNAL_ERROR);

	tcp_socket_close(socket_connection);

	thread_end(args);

}

void* http_server(void *args) {

	thread_begin(args);

	tcp_socket socket_listen;

	if (tcp_socket_create(&socket_listen)) log_exit_fatal();
	log_info("listening socket created");

	if (tcp_bind(socket_listen, NULL, http_port)) log_exit_fatal();
	log_info("listening socket bound to port %d", http_port);

	if (tcp_socket_listen(socket_listen)) log_exit_fatal();
	log_info("incoming connections are listening");

	while (1) {
		tcp_socket socket_connection;

		if (tcp_socket_accept(socket_listen, &socket_connection)) continue;
		int request_id = ++http_server_request_count;
		log_info("connection accepted, request_id: %d", request_id);

		char thread_name[THREAD_NAME_SIZE];
		str_format(thread_name, sizeof(thread_name), "REQ_%07d", request_id % 10000000);
		thread_create(http_server_request, thread_name, socket_connection);
	}

	thread_end(args);
	return 0;

}
