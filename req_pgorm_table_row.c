#include "globals.h"
#include "util/utils.h"

int req_pgorm_table_row_save(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char schema[STR_SIZE];
	char table_name[STR_SIZE];
	json_entry *columns_entry;
	int row_exists;
	json_entry *columns_changed_entry;
	json_entry *values_entry;
	json_entry *columns_pk_entry;
	json_entry *values_pk_entry;
	if (json_get_str(schema, sizeof(schema), req_params, 1, "schema", NULL)) return 1;
	if (json_get_str(table_name, sizeof(table_name), req_params, 1, "tableName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_bool(&row_exists, req_params, 1, "rowExists", NULL)) return 1;
	if (json_get_array_entry(&columns_changed_entry, req_params, 1, "columnsChanged", NULL)>0) return 1;
	if (json_get_array_entry(&values_entry, req_params, 1, "values", NULL)>0) return 1;
	if (json_get_array_entry(&columns_pk_entry, req_params, 1, "columnsPrimaryKey", NULL)>0) return 1;
	if (json_get_array_entry(&values_pk_entry, req_params, 1, "valuesPrimaryKey", NULL)>0) return 1;
	char sql_query[32*1024] = "";
	if (!row_exists) {
		if (str_add(sql_query, sizeof(sql_query), "insert into ", schema, ".", table_name, " (", NULL)) return 1;
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
		if (str_add(sql_query, sizeof(sql_query), "update ", schema, ".", table_name, " set ", NULL)) return 1;
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
	if (req_pgorm_sql_params_init(&sql_params, req_params, "values", row_exists ? "valuesPrimaryKey" : NULL, NULL))
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
	int res = req_pgorm_response_add_pgresult(response, pg_result, "row", 0, 1) || stream_add_str(response, "\n}", NULL);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	return res;
}

int req_pgorm_table_row_delete(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char schema[STR_SIZE];
	char table_name[STR_SIZE];
	json_entry *columns_entry;
	json_entry *columns_pk_entry;
	if (json_get_str(schema, sizeof(schema), req_params, 1, "schema", NULL)) return 1;
	if (json_get_str(table_name, sizeof(table_name), req_params, 1, "tableName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_array_entry(&columns_pk_entry, req_params, 1, "columnsPrimaryKey", NULL)>0) return 1;
	char sql_query[32*1024] = "delete from ";
	if (str_add(sql_query, sizeof(sql_query), schema, ".", table_name, " where ", NULL)) return 1;
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
	int res = req_pgorm_response_add_pgresult(response, pg_result, "row", 0, 1) || stream_add_str(response, "\n}", NULL);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	return res;
}

int req_pgorm_table_row_load_by_condition(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char schema[STR_SIZE];
	char table_name[STR_SIZE];
	char condition[32*1024];
	json_entry *columns_entry;
	if (json_get_str(schema, sizeof(schema), req_params, 1, "schema", NULL)) return 1;
	if (json_get_str(table_name, sizeof(table_name), req_params, 1, "tableName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_str(condition, sizeof(condition), req_params, 1, "condition", NULL)) return 1;
	char sql_query[32*1024] = "select ";
	for(int i=0; i<columns_entry->array_size; i++) {
		char column[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_entry, i)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
	}
	if (str_add(sql_query, sizeof(sql_query), " from ", schema, ".", table_name, " where ", condition, NULL)) return 1;
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
		res = stream_add_str(response, ",\n  \"row\": null\n}", NULL);
	else
		res = req_pgorm_response_add_pgresult(response, pg_result, "row", 0, 1) || stream_add_str(response, "\n}", NULL);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	return res;
}

int req_pgorm_table_row_array_load(json *req_params, stream *response) {
	if (req_pgorm_response_sucess_begin(response)) return 1;
	char schema[STR_SIZE];
	char table_name[STR_SIZE];
	char condition[32*1024];
	json_entry *columns_entry;
	if (json_get_str(schema, sizeof(schema), req_params, 1, "schema", NULL)) return 1;
	if (json_get_str(table_name, sizeof(table_name), req_params, 1, "tableName", NULL)) return 1;
	if (json_get_array_entry(&columns_entry, req_params, 1, "columns", NULL)>0) return 1;
	if (json_get_str(condition, sizeof(condition), req_params, 1, "condition", NULL)) return 1;
	char sql_query[32*1024] = "select ";
	for(int i=0; i<columns_entry->array_size; i++) {
		char column[STR_SIZE];
		if (json_get_array_str(column, sizeof(column), req_params, columns_entry, i)) return 1;
		if (str_add(sql_query, sizeof(sql_query), i>0 ? "," : "", column, NULL)) return 1;
	}
	if (str_add(sql_query, sizeof(sql_query), " from ", schema, ".", table_name, NULL)) return 1;
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
	int res = req_pgorm_response_add_pgresult(response, pg_result, "rows", 0, 0);
	PQclear(pg_result);
	pg_connection_unlock(pg_connection);
	if (res) return 1;
	if (stream_add_str(response, "\n}", NULL)) return 1;
	return 0;
}
