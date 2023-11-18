#include <stdarg.h>

#include "globals.h"
#include "util/utils.h"

int req_pgorm_response_sucess_begin(stream *response) {
	stream_clear(response);
	if (stream_add_str(response, "{\n  \"sucess\": true", NULL)) return 1;
	return 0;
}

int req_pgorm_connect(json *req_params, stream *response) {
	char user[STR_SIZE];
	char password[STR_SIZE];
	char idle_session_timeout[STR_SIZE];
	if (json_get_str(user, sizeof(user), req_params, 1, "user", NULL)) return 1;
	if (!strcasecmp(user,"postgres")) return log_error(16, user);
	if (json_get_str(password, sizeof(password), req_params, 1, "password", NULL)) return 1;
	if (json_get_str(idle_session_timeout, sizeof(idle_session_timeout), req_params, 1, "idleSessionTimeout", NULL)) return 1;
	char uri[STR_SIZE];
	if (pg_uri_build(uri, sizeof(uri), db_host, db_port, db_name, user, password)) return 1;
	pg_connection *pg_connection;
	if (pg_connection_assign(&pg_connection, uri)) return 1;
	if (PQserverVersion(pg_connection->conn)/10000>=14) {
		if (pg_execute(pg_connection->conn, "select set_config('idle_session_timeout', $1, true);", 1, idle_session_timeout)) {
			pg_connection_free(pg_connection);
			return 1;
		}
	}
	if (req_pgorm_response_sucess_begin(response) ||
		stream_add_str(response, ",\n  \"connectionID\": ", NULL) ||
		stream_add_str_escaped(response, pg_connection->id) ||
		stream_add_str(response, "\n}", NULL))
	{
		pg_connection_free(pg_connection);
		return 1;
	}
	return 0;
}

int req_pgorm_connection_lock(pg_connection **pg_connection, json *req_params) {
	char connection_id[64];
	if (json_get_str(connection_id, sizeof(connection_id), req_params, 1, "connectionID", NULL)) return 1;
	return pg_connection_lock(pg_connection, connection_id);
}

int req_pgorm_disconnect(json *req_params, stream *response) {
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) return 1;
	pg_connection_free(pg_connection);
	if (req_pgorm_response_sucess_begin(response) || stream_add_str(response, "\n}", NULL)) return 1;
	return 0;
}

int req_pgorm_send_last_error(tcp_socket socket_connection) {
	int error_code;
	char error_text[LOG_ERROR_TEXT_SIZE];
	if (thread_get_last_error(&error_code, error_text, sizeof(error_text))) return 1;
	if (str_escaped(error_text, sizeof(error_text))) return 1;
	char response[STR_SIZE];
	if (str_format(response, sizeof(response), "{\n  \"sucess\": false,\n  \"errorCode\": %d,\n  \"errorText\": %s\n}", error_code, error_text)) return 1;
	if (http_send_response(socket_connection, HTTP_STATUS_OK, HTTP_CONTENT_TYPE_JSON, strlen(response), response)) return 1;
	return 0;
}

int req_pgorm_sql_params_init(stream_list *sql_params, json *json, ...) {
	stream_list_init(sql_params);
	va_list args;
	va_start(args, &json);
	int res = 0;
    while(!res)  {
    	char *json_key = va_arg(args, char *);
    	if (json_key==NULL) break;
    	json_entry *json_entry;
    	if (json_get_array_entry(&json_entry, json, 1, json_key, NULL)>0) { res=1; break; }
    	for(int i=0; i<json_entry->array_size; i++) {
    		if (stream_list_add_str(sql_params, "", "")) { res=1; break; }
    		if (json_get_array_stream(&sql_params->streams[sql_params->len-1], json, json_entry, i)) { res=1; break; }
    	}
    }
	va_end(args);
	if (res)
		stream_list_free(sql_params);
	return res;
}

int req_pgorm_execute(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	stream sql_query;
	if (stream_init(&sql_query)) return 1;
	if (json_get_stream(&sql_query, req_params, 1, "query", NULL)) {
		stream_free(&sql_query);
		return 1;
	}
	stream_list sql_params;
	if (req_pgorm_sql_params_init(&sql_params, req_params, "params", NULL)) {
		stream_free(&sql_query);
		return 1;
	}
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) {
		stream_free(&sql_query);
		stream_list_free(&sql_params);
		return 1;
	}
	PGresult *pg_result = PQexecParams(pg_connection->conn, sql_query.data, sql_params.len, NULL, sql_params.data, NULL, NULL, 0);
	stream_list_free(&sql_params);
	if (pg_check_result(pg_result, sql_query.data, 0)) {
		stream_free(&sql_query);
		pg_connection_unlock(pg_connection);
		return 1;
	}
	stream_free(&sql_query);
	int res = 0;
	if(PQresultStatus(pg_result)==PGRES_TUPLES_OK) {
		res = req_pgorm_response_add_pgresult(response, pg_result, "data", 1, 0);
	}
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	if (!res) res = stream_add_str(response, "\n}", NULL);
	return res;
}

extern req_pgorm_row_save();
extern req_pgorm_row_delete();
extern req_pgorm_row_load_where();
extern req_pgorm_row_array_load();
extern req_pgorm_row_array_delete();

int req_pgorm(tcp_socket socket_connection, http_request *request) {

	if(!strcmp(request->path, "/pgorm/status")) {
		char status_info[STR_SIZE];
		if(admin_get_status_info(status_info, sizeof(status_info))) return req_pgorm_send_last_error(socket_connection);
		char response[STR_SIZE] = "<html><head><title>PGORM | status</title></head><body><b>PGORM available</b>";
		if(str_add(response, sizeof(response), "</body></html>", NULL)) return req_pgorm_send_last_error(socket_connection);
		return http_send_response(socket_connection, HTTP_STATUS_OK, HTTP_CONTENT_TYPE_TEXT_PLAIN, strlen(status_info), status_info);
	}

	int (*req_pgorm_handler)(json *req_params, stream *response);
	if      (!strcmp(request->path, "/pgorm/connect"))          req_pgorm_handler = req_pgorm_connect;
	else if (!strcmp(request->path, "/pgorm/disconnect"))       req_pgorm_handler = req_pgorm_disconnect;
	else if (!strcmp(request->path, "/pgorm/execute"))          req_pgorm_handler = req_pgorm_execute;
	else if (!strcmp(request->path, "/pgorm/row-save"))         req_pgorm_handler = req_pgorm_row_save;
	else if (!strcmp(request->path, "/pgorm/row-delete"))       req_pgorm_handler = req_pgorm_row_delete;
	else if (!strcmp(request->path, "/pgorm/row-load-where"))   req_pgorm_handler = req_pgorm_row_load_where;
	else if (!strcmp(request->path, "/pgorm/row-array-load"))   req_pgorm_handler = req_pgorm_row_array_load;
	else if (!strcmp(request->path, "/pgorm/row-array-delete")) req_pgorm_handler = req_pgorm_row_array_delete;
	else {
		log_error(67, request->path);
		return req_pgorm_send_last_error(socket_connection);
	}

	stream response;
	if (stream_init(&response)) return 1;

	json req_params;
	int res = json_init(&req_params, request->content.data);
	if (res) {
		stream_free(&response);
		return req_pgorm_send_last_error(socket_connection);
	}

	res = req_pgorm_handler(&req_params, &response);
	json_free(&req_params);

	if (res) {
		stream_free(&response);
		return req_pgorm_send_last_error(socket_connection);
	}

	res = http_send_response(socket_connection, HTTP_STATUS_OK, HTTP_CONTENT_TYPE_JSON, response.len, response.data);
	stream_free(&response);
	return res;

}
