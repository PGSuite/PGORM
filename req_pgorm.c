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
	if (json_get_str(idle_session_timeout, sizeof(idle_session_timeout), req_params, 1, "idle_session_timeout", NULL)) return 1;
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
		stream_add_str(response, ",\n  \"connection\": { \"index\": ", NULL) ||
		stream_add_int(response, pg_connection->index) ||
		stream_add_str(response, ", \"key\": \"", pg_connection->key, "\"}\n}", NULL))
	{
		pg_connection_free(pg_connection);
		return 1;
	}
	return 0;
}

int req_pgorm_connection_lock(pg_connection **pg_connection, json *req_params) {
	int index;
	char key[PG_CONNECTION_KEY_SIZE*2];
	if (json_get_int(&index, req_params, 1, "connection", "index", NULL)) return 1;
	if (json_get_str(key, sizeof(key), req_params, 1, "connection", "key", NULL)) return 1;
	if (pg_connection_check_key(index, key)) return 1;
	return pg_connection_lock(pg_connection, index);
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
	char error_text[LOG_TEXT_SIZE+64];
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
    		stream *stream = &sql_params->streams[sql_params->len-1];
    		if (json_get_array_stream(stream, json, json_entry, i)) { res=1; break; }
    		if (!(stream->last_add_str_unescaped) && stream->len==4 && stream->data[0]=='n' && stream->data[1]=='u' && stream->data[2]=='l' && stream->data[3]=='l')
    			sql_params->data[sql_params->len-1]=NULL;
    	}
    }
	va_end(args);
	if (res)
		stream_list_free(sql_params);
	return res;
}

int req_pgorm_response_add_pgresult(stream *response, PGresult *pg_result, char *name, int columns_add, int single_row) {
	int column_count = PQnfields(pg_result);
	if (columns_add) {
		if (stream_add_str(response, ",\n  \"columns\": [", NULL)) return 1;
		for (int c=0; c<column_count; c++) {
			int type_oid = PQftype(pg_result, c);
			char *name = PQfname(pg_result, c);
			if (c>0 && stream_add_str(response, ",", NULL)) return 1;
			if (stream_add_str(response, "\n    { \"name\": ", NULL)) return 1;
			if (stream_add_str_escaped(response, name)) return 1;
			if (stream_add_str(response, ", \"type\": ", NULL)) return 1;
			if (stream_add_int(response, type_oid)) return 1;
			if (stream_add_str(response, ", \"size\": ", NULL)) return 1;
			if (stream_add_int(response, PQfsize(pg_result, c))) return 1;
			if (stream_add_str(response, " }", NULL)) return 1;
		}
		if (stream_add_str(response, "\n  ]", NULL)) return 1;
	}
	if (stream_add_str(response, ",\n  \"",name,"\": ", single_row ? "" : "[\n", NULL)) return 1;
	int row_count = PQntuples(pg_result);
	if (single_row && row_count>1) row_count=1;
	for(int r=0; r<row_count; r++) {
		if (!single_row && stream_add_str(response, r>0 ?  ",\n" : "", "    ", NULL)) return 1;
		if (stream_add_str(response, "[ ", NULL)) return 1;
		for (int c=0; c<column_count; c++) {
			if (c>0 && stream_add_str(response, ", ", NULL)) return 1;
			if (PQgetisnull(pg_result, r, c)) {
				if (stream_add_str(response, "null", NULL)) return 1;
			} else {
				char *data = PQgetvalue(pg_result, r, c);
				int data_len = PQgetlength(pg_result, r, c);
				if (data[0]=='"') {
					if (stream_add_substr(response, data, 0, data_len-1)) return 1;
				} else {
					if (stream_add_substr_escaped(response, data, 0, data_len-1)) return 1;
				}
			}
		}
		if (stream_add_str(response, " ]", NULL)) return 1;
	}
	if (!single_row && stream_add_str(response, "\n  ]", NULL)) return 1;
	return 0;
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

extern req_pgorm_record_save();
extern req_pgorm_record_delete();
extern req_pgorm_record_load_where();
extern req_pgorm_record_array_load();
extern req_pgorm_record_array_delete();

int req_pgorm(tcp_socket socket_connection, http_request *request) {

	if(!strcmp(request->path, "/pgorm/status")) {
		char status_info[STR_SIZE];
		if(admin_get_status_info(status_info, sizeof(status_info))) return req_pgorm_send_last_error(socket_connection);
		char response[STR_SIZE] = "<html><head><title>PGORM | status</title></head><body><b>PGORM available</b>";
		if(str_add(response, sizeof(response), "</body></html>", NULL)) return req_pgorm_send_last_error(socket_connection);
		return http_send_response(socket_connection, HTTP_STATUS_OK, HTTP_CONTENT_TYPE_TEXT_PLAIN, strlen(status_info), status_info);
	}

	int (*req_pgorm_handler)(json *req_params, stream *response);
	if      (!strcmp(request->path, "/pgorm/connect"))             req_pgorm_handler = req_pgorm_connect;
	else if (!strcmp(request->path, "/pgorm/disconnect"))          req_pgorm_handler = req_pgorm_disconnect;
	else if (!strcmp(request->path, "/pgorm/execute"))             req_pgorm_handler = req_pgorm_execute;
	else if (!strcmp(request->path, "/pgorm/record-save"))         req_pgorm_handler = req_pgorm_record_save;
	else if (!strcmp(request->path, "/pgorm/record-delete"))       req_pgorm_handler = req_pgorm_record_delete;
	else if (!strcmp(request->path, "/pgorm/record-load-where"))   req_pgorm_handler = req_pgorm_record_load_where;
	else if (!strcmp(request->path, "/pgorm/record-array-load"))   req_pgorm_handler = req_pgorm_record_array_load;
	else if (!strcmp(request->path, "/pgorm/record-array-delete")) req_pgorm_handler = req_pgorm_record_array_delete;
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
