#include <dirent.h>

#include "globals.h"
#include "util/utils.h"

#include "metadata.h"

void pgorm_test() {

	PGconn *pg_conn;

	if (pg_connect(&pg_conn, db_orm_uri)) exit(2);

	if (orm_maker_schema_verify(pg_conn));

	// if (orm_maker_object_refresh(pg_conn, "test", "product", "TABLE")) exit(2);

	exit(0);


}
