#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "util/utils.h"

#ifdef _WIN32

#define HTTP_DIRECTORY_DEFAULT "C:\\Site"
#define LOG_FILE_DEFAULT       "C:\\Site\\log\\pgorm.log"

#else

#define HTTP_DIRECTORY_DEFAULT "/etc/site"
#define LOG_FILE_DEFAULT       "/var/log/pgorm/pgorm.log"

#endif

#define HTTP_PORT_DEFAULT "80"

#define DB_HOST_DEFAULT     "127.0.0.1"
#define DB_PORT_DEFAULT     "5432"
#define DB_NAME_DEFAULT     "site"
#define DB_ORM_USER_DEFAULT "postgres"

#define ADMIN_PORT_DEFAULT "1080"

extern char *db_host;
extern char *db_port;
extern char *db_name;
extern char *db_orm_user;
extern char db_orm_uri[256];

extern char *http_directory;
extern int  http_port;

extern int    admin_port;

#endif /* GLOBALS_H_ */
