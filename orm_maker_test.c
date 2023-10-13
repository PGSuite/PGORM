#include "globals.h"
#include "util/utils.h"

#include "metadata.h"

void class_gen_test() {

	http_directory = "R:\\PGSuite\\pgorm\\test\\site";
	// http_directory = "/tmp/site";

	PGconn *pg_conn;
	if (pg_connect(&pg_conn, "postgresql:///pgsuite?connect_timeout=10&user=postgres")) return;

	metadata_table table;
	if (metadata_table_load(&table, pg_conn, "test","type_sample")) return;
	orm_maker_table_refresh(pg_conn, &table);

}
