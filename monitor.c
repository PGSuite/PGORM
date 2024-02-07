#include "globals.h"
#include "util/utils.h"

#define SQL_CONNECTIONS_CLOSED \
		"select * from (\n" \
		"    select (unnest::text[])[1]::int index, (unnest::text[])[2] activity_id from unnest($1::text[])\n" \
		"  ) connection\n" \
		"  where activity_id not in (" PG_SQL_ACTIVITY_ID ")"

void* monitor_thread(void *args) {

	thread_begin(args);
	sleep(1);

	stream connections;
	if (stream_init(&connections)) {
		thread_end(args);
		return 1;
	}

	for(;1;sleep(60*1000)) {

		if (pg_connection_array_sql(&connections))
			continue;

		PGconn *pg_conn;
		if (pg_connect(&pg_conn, db_orm_uri))
			continue;

		PGresult *rs_connections_closed;
		if (pg_select(&rs_connections_closed, pg_conn, 1, SQL_CONNECTIONS_CLOSED, 1, connections.data)) {
			pg_disconnect(&pg_conn);
			continue;
		}

		for(int i=0; i<PQntuples(rs_connections_closed); i++) {
			int index = atoi(PQgetvalue(rs_connections_closed, i, 0));
			char *activity_id = PQgetvalue(rs_connections_closed, i, 1);
			log_info("found closed connection, index: %d, activity_id: %s", index, activity_id);
			pg_connection_try_free(index, activity_id);
		}
		PQclear(rs_connections_closed);

		pg_disconnect(&pg_conn);

	}

	thread_end(args);
	return 0;

}
