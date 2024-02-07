#include "globals.h"
#include "util/utils.h"

int req_pgorm_record_save(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char relation_schema[STR_SIZE];
	char relation_name[STR_SIZE];
	json_entry *columns_entry;
	int record_exists;
	json_entry *columns_changed_entry;
	json_entry *values_entry;
	json_entry *columns_pk_entry;
	json_entry *values_pk_entry;
	if (json_get_str(relation_schema, sizeof(relation_schema), req_params, 1, "relationSchema", NULL)) return 1;
	if (json_get_str(relation_name, sizeof(relation_name), req_params, 1, "relationName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_bool(&record_exists, req_params, 1, "recordExists", NULL)) return 1;
	if (json_get_array_entry(&columns_changed_entry, req_params, 1, "columnsChanged", NULL)>0) return 1;
	if (json_get_array_entry(&values_entry, req_params, 1, "values", NULL)>0) return 1;
	if (json_get_array_entry(&columns_pk_entry, req_params, 1, "columnsPrimaryKey", NULL)>0) return 1;
	if (json_get_array_entry(&values_pk_entry, req_params, 1, "valuesPrimaryKey", NULL)>0) return 1;
	char sql_query[32*1024] = "";
	if (!record_exists) {
		if (str_add(sql_query, sizeof(sql_query), "insert into ", relation_schema, ".", relation_name, " (", NULL)) return 1;
		for(int i=0; i<columns_changed_entry->array_size; i++) {
			char column[STR_SIZE];
			if (json_get_array_str(column, sizeof(column), req_params, columns_changed_entry, i)) return 1;
			if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
		}
		if (str_add(sql_query, sizeof(sql_query), ") values (", NULL)) return 1;
		for(int i=0; i<values_entry->array_size; i++) {
			char variable[STR_SIZE];
			if(str_format(variable, sizeof(variable), "$%d", i+1)) return 1;
			if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", variable, NULL)) return 1;
		}
		if (str_add(sql_query, sizeof(sql_query), ")", NULL)) return 1;
	} else {
		if (str_add(sql_query, sizeof(sql_query), "update ", relation_schema, ".", relation_name, " set ", NULL)) return 1;
		for(int i=0; i<columns_changed_entry->array_size; i++) {
			char column[STR_SIZE];
			char variable[STR_SIZE];
			if (json_get_array_str(column, sizeof(column), req_params, columns_changed_entry, i)) return 1;
			if(str_format(variable, sizeof(variable), "$%d", i+1)) return 1;
			if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, "=", variable, NULL)) return 1;
		}
		if (str_add(sql_query, sizeof(sql_query), " where ", NULL)) return 1;
		for(int i=0; i<columns_pk_entry->array_size; i++) {
			char column[STR_SIZE];
			char variable[STR_SIZE];
			if (json_get_array_str(column, sizeof(column), req_params, columns_pk_entry, i)) return 1;
			if(str_format(variable, sizeof(variable), "$%d", columns_changed_entry->array_size+i+1)) return 1;
			if (str_add(sql_query, sizeof(sql_query), i>0 ? " and " : "", column, "=", variable, NULL)) return 1;
		}
	}
	if (str_add(sql_query, sizeof(sql_query), " returning ", NULL)) return 1;
	for(int i=0; i<columns_entry->array_size; i++) {
		char column[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_entry, i)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
	}
	stream_list sql_params;
	if (req_pgorm_sql_params_init(&sql_params, req_params, "values", record_exists ? "valuesPrimaryKey" : NULL, NULL))
		return 1;
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) {
		stream_list_free(&sql_params);
		return 1;
	}
	PGresult *pg_result = PQexecParams(pg_connection->conn, sql_query, sql_params.len, NULL, sql_params.data, NULL, NULL, 0);
	stream_list_free(&sql_params);
	if (pg_check_result(pg_result, sql_query, 3)) {
		pg_connection_unlock(pg_connection);
		return 1;
	};
	int res = req_pgorm_response_add_pgresult(response, pg_result, "record", 0, 1) || stream_add_str(response, "\n}", NULL);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	return res;
}

int req_pgorm_record_delete(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char relation_schema[STR_SIZE];
	char relation_name[STR_SIZE];
	json_entry *columns_entry;
	json_entry *columns_pk_entry;
	if (json_get_str(relation_schema, sizeof(relation_schema), req_params, 1, "relationSchema", NULL)) return 1;
	if (json_get_str(relation_name, sizeof(relation_name), req_params, 1, "relationName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_array_entry(&columns_pk_entry, req_params, 1, "columnsPrimaryKey", NULL)>0) return 1;
	char sql_query[32*1024] = "delete from ";
	if (str_add(sql_query, sizeof(sql_query), relation_schema, ".", relation_name, " where ", NULL)) return 1;
	for(int i=0; i<columns_pk_entry->array_size; i++) {
		char column[STR_SIZE];
		char variable[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_pk_entry, i)) return 1;
		if(str_format(variable, sizeof(variable), "$%d", i+1)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? " and " : "", column, "=", variable, NULL)) return 1;
	}
	stream_list sql_params;
	if (req_pgorm_sql_params_init(&sql_params, req_params, "valuesPrimaryKey", NULL)) return 1;
	if (str_add(sql_query, sizeof(sql_query), " returning ", NULL)) return 1;
	for(int i=0; i<columns_entry->array_size; i++) {
		char column[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_entry, i)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
	}
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) {
		stream_list_free(&sql_params);
		return 1;
	}
	PGresult *pg_result = PQexecParams(pg_connection->conn, sql_query, sql_params.len, NULL, sql_params.data, NULL, NULL, 0);
	stream_list_free(&sql_params);
	if (pg_check_result(pg_result, sql_query, 3)) {
		pg_connection_unlock(pg_connection);
		return 1;
	};
	int res = req_pgorm_response_add_pgresult(response, pg_result, "record", 0, 1) || stream_add_str(response, "\n}", NULL);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	return res;
}

int req_pgorm_record_load_where(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char relation_schema[STR_SIZE];
	char relation_name[STR_SIZE];
	char condition[32*1024];
	json_entry *columns_entry;
	if (json_get_str(relation_schema, sizeof(relation_schema), req_params, 1, "relationSchema", NULL)) return 1;
	if (json_get_str(relation_name, sizeof(relation_name), req_params, 1, "relationName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_str(condition, sizeof(condition), req_params, 1, "condition", NULL)) return 1;
	char sql_query[32*1024] = "select ";
	for(int i=0; i<columns_entry->array_size; i++) {
		char column[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_entry, i)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
	}
	if (str_add(sql_query, sizeof(sql_query), " from ", relation_schema, ".", relation_name, " where ", condition, NULL)) return 1;
	stream_list sql_params;
	if (req_pgorm_sql_params_init(&sql_params, req_params, "params", NULL))
		return 1;
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) {
		stream_list_free(&sql_params);
		return 1;
	}
	PGresult *pg_result = PQexecParams(pg_connection->conn, sql_query, sql_params.len, NULL, sql_params.data, NULL, NULL, 0);
	stream_list_free(&sql_params);
	if (pg_check_result(pg_result, sql_query, 0)) {
		pg_connection_unlock(pg_connection);
		return 1;
	};
	int res;
	if (PQntuples(pg_result)==0)
		res = stream_add_str(response, ",\n  \"record\": null\n}", NULL);
	else
		res = req_pgorm_response_add_pgresult(response, pg_result, "record", 0, 1) || stream_add_str(response, "\n}", NULL);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	return res;
}

int req_pgorm_record_array_load(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char relation_schema[STR_SIZE];
	char relation_name[STR_SIZE];
	char condition[32*1024];
	json_entry *columns_entry;
	if (json_get_str(relation_schema, sizeof(relation_schema), req_params, 1, "relationSchema", NULL)) return 1;
	if (json_get_str(relation_name, sizeof(relation_name), req_params, 1, "relationName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_str(condition, sizeof(condition), req_params, 1, "condition", NULL)) return 1;
	char sql_query[32*1024] = "select ";
	for(int i=0; i<columns_entry->array_size; i++) {
		char column[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_entry, i)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
	}
	if (str_add(sql_query, sizeof(sql_query), " from ", relation_schema, ".", relation_name, NULL)) return 1;
	if (strlen(condition)>0)
		if (str_add(sql_query, sizeof(sql_query), " where ", condition, NULL)) return 1;
	stream_list sql_params;
	if (req_pgorm_sql_params_init(&sql_params, req_params, "params", NULL))
		return 1;
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) {
		stream_list_free(&sql_params);
		return 1;
	}
	PGresult *pg_result = PQexecParams(pg_connection->conn, sql_query, sql_params.len, NULL, sql_params.data, NULL, NULL, 0);
	stream_list_free(&sql_params);
	if (pg_check_result(pg_result, sql_query, 1)) {
		pg_connection_unlock(pg_connection);
		return 1;
	};
	int res = req_pgorm_response_add_pgresult(response, pg_result, "records", 0, 0);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	if (res) return 1;
	if (stream_add_str(response, "\n}", NULL)) return 1;
	return 0;
}

int req_pgorm_record_array_delete(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char relation_schema[STR_SIZE];
	char relation_name[STR_SIZE];
	char condition[32*1024];
	if (json_get_str(relation_schema, sizeof(relation_schema), req_params, 1, "relationSchema", NULL)) return 1;
	if (json_get_str(relation_name, sizeof(relation_name), req_params, 1, "relationName", NULL)) return 1;
	if (json_get_str(condition, sizeof(condition), req_params, 1, "condition", NULL)) return 1;
	char sql_query[32*1024] = "with deleted as (delete from ";
	if (str_add(sql_query, sizeof(sql_query), relation_schema, ".", relation_name, NULL)) return 1;
	if (strlen(condition)>0)
		if (str_add(sql_query, sizeof(sql_query), " where ", condition, NULL)) return 1;
	if (str_add(sql_query, sizeof(sql_query), " returning null) select count(1) from deleted", NULL)) return 1;
	stream_list sql_params;
	if (req_pgorm_sql_params_init(&sql_params, req_params, "params", NULL))
		return 1;
	pg_connection *pg_connection;
	if (req_pgorm_connection_lock(&pg_connection, req_params)) {
		stream_list_free(&sql_params);
		return 1;
	}
	PGresult *pg_result = PQexecParams(pg_connection->conn, sql_query, sql_params.len, NULL, sql_params.data, NULL, NULL, 0);
	stream_list_free(&sql_params);
	if (pg_check_result(pg_result, sql_query, 3)) {
		pg_connection_unlock(pg_connection);
		return 1;
	};
	int records_deleted = atoi(PQgetvalue(pg_result,0,0));
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	if (stream_add_str(response, ",\n  \"records_deleted\": ", NULL)) return 1;
	if (stream_add_int(response, records_deleted)) return 1;
	if (stream_add_str(response, "\n}", NULL)) return 1;
	return 0;
}

