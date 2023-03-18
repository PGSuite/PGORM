#include "globals.h"
#include "util/utils.h"

#define SQL_CONNECTIONS_CLOSED \
			"select connection_id\n"  \
			"  from unnest(string_to_array($1, ',')) connection_id\n" \
			"  where substring(connection_id,15) not in (select " PG_SQL_SESSION_ID_COLUMN " from pg_stat_activity)"

void* monitor_thread(void *args) {

	thread_begin(args);
	sleep(1);

	stream connection_id_list;
	if (stream_init(&connection_id_list)) {
		thread_end(args);
		return 1;
	}

	for(;1;sleep(60*1000)) {

		if (pg_connection_id_list(&connection_id_list))
			continue;

		PGconn *pg_conn;
		if (pg_connect(&pg_conn, db_orm_uri))
			continue;

		PGresult *rs_connections_closed;
		if (pg_select(&rs_connections_closed, pg_conn, 1, SQL_CONNECTIONS_CLOSED, 1, connection_id_list.data)) {
			pg_disconnect(&pg_conn);
			continue;
		}

		for(int i=0; i<PQntuples(rs_connections_closed); i++) {
			char *connection_id = PQgetvalue(rs_connections_closed, i, 0);
			log_info("found closed connection, id: %s", connection_id);
			pg_connection *pg_connection;
			if (pg_connection_lock(&pg_connection, connection_id)) continue;
			pg_connection_free(pg_connection);
		}
		PQclear(rs_connections_closed);

		pg_disconnect(&pg_conn);

	}

	thread_end(args);
	return 0;

}
