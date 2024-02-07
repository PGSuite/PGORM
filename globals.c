#include "globals.h"
#include "util/utils.h"

char *http_directory = HTTP_DIRECTORY_DEFAULT;
int  http_port;

char *db_host     = NULL;
char *db_port     = NULL;
char *db_name     = DB_NAME_DEFAULT;
char *db_orm_host = NULL;
char *db_orm_user = DB_ORM_USER_DEFAULT;
char db_orm_uri[256];

int admin_port;

int globals_add_parameters(char *text, int text_size) {
	if (str_add(text, text_size, "Parameters\n", NULL)) return 1;
	if (str_add(text, text_size, "  HTTP\n", NULL)) return 1;
	if (str_add(text, text_size, "    directory:      ", http_directory, "\n", NULL)) return 1;
	if (str_add_format(text, text_size, "    port:           %d\n", http_port)) return 1;
	if (str_add(text, text_size, "  database\n", NULL)) return 1;
	if (str_add(text, text_size, "    host:           ", db_host,     "\n", NULL)) return 1;
	if (str_add(text, text_size, "    port:           ", db_port,     "\n", NULL)) return 1;
	if (str_add(text, text_size, "    database:       ", db_name,     "\n", NULL)) return 1;
	if (str_add(text, text_size, "    orm host:       ", db_orm_host, "\n", NULL)) return 1;
	if (str_add(text, text_size, "    orm user:       ", db_orm_user, "\n", NULL)) return 1;
	if (str_add(text, text_size, "  administration\n", NULL)) return 1;
	if (str_add_format(text, text_size, "    port:           %d\n", admin_port)) return 1;
	return 0;
}
