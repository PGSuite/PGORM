#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>

#include <wchar.h>
#include <locale.h>

#ifndef _WIN32

#include <spawn.h>

#endif

#include "globals.h"
#include "util/utils.h"

extern void* admin_server(void *args);
extern void* http_server(void *args);
extern void* monitor_thread(void *args);
extern void* orm_maker_thread(void *args);

char HELP[] =
	"Usage:\n" \
	"  pgorm start|execute|status|stop [OPTIONS]\n" \
	"\n" \
	"Parameter:\n" \
	"  start      execute pgorm in new process\n" \
	"  execute    execute pgorm in current process\n" \
	"  status     print info about running pgorm\n" \
	"  stop       stop pgorm\n" \
	"\n" \
	"Database options:\n" \
	"  -h HOSTNAME        database server host (default: server ip address)\n" \
	"  -p PORT            database server port (default: " DB_PORT_DEFAULT ")\n" \
	"  -d DBNAME          database name (default: " DB_NAME_DEFAULT ")\n" \
	"  -orm-h HOSTNAME    ORM hostname (default: " DB_ORM_HOST_DEFAULT " or server ip address)\n" \
	"  -orm-u USERNAME    ORM username (default: " DB_ORM_USER_DEFAULT ")\n" \
	"  -orm-w PASSWORD    ORM password (if nessesary)\n" \
	"\n" \
	"HTTP options:\n" \
	"  -hd DIRECTORY       site directory (default: " HTTP_DIRECTORY_DEFAULT ")\n" \
	"  -hp PORT            HTTP port (default: " HTTP_PORT_DEFAULT ")\n" \
	"\n" \
	"Administration options:\n" \
	"  -ap PORT            administration port (default: " ADMIN_PORT_DEFAULT ")\n" \
	"\n" \
	"Logging options:\n" \
	"  -l  FILE           log file\n" \
	"\n" \
	"Info:\n" \
	"  -h, --help          print this help\n"
	"\n" \
	"Examples:\n" \
	"  pgorm execute\n" \
	"  pgorm start -d sitedb -hd /mysite1 -l /tmp/pgorm.log\n";

int main(int argc, char *argv[]) {

	log_set_program_name("PGORM is web server for database PostgreSQL", "PGORM");

	log_check_help(argc, argv, HELP);

	if (tcp_startup()>0)
		log_exit_fatal();

	if (!strcmp(argv[1],"start")) {

		char *args[20];
		int args_count=0;
		int status;
		int error_code;
		unsigned int pid;
		if (sizeof(args)/sizeof(char *)+4<argc) {
			log_error(81, argc);
			exit(3);
		}
		args[args_count++]=argv[0];
		args[args_count++]="execute";
		int log_set = 0;
		for(int i=2; i<argc; i++) {
			if (!strcmp(argv[i],"-l")) log_set = 1;
			args[args_count++]=argv[i];
		}
		if (!log_set) {
			args[args_count++]="-l";
			args[args_count++]=LOG_FILE_DEFAULT;
		}
		args[args_count++]=NULL;
		char command[32*1024] = "";
		for(int i=0; i<args_count-1; i++) {
			if (strlen(command)+strlen(args[i])+2>=sizeof(command)) {
				log_error(80, argc);
				exit(3);
			}
			strcpy(command+strlen(command), args[i]);
		}

		#ifdef _WIN32

			STARTUPINFO cif;
			ZeroMemory(&cif,sizeof(STARTUPINFO));
			PROCESS_INFORMATION pi;
			status = !CreateProcess(argv[0], command, NULL,NULL,FALSE,NULL,NULL,NULL,&cif,&pi);
			error_code =  GetLastError();
			pid = pi.hProcess;

		#else

			status = posix_spawn(&pid, args[0], NULL, NULL, args, NULL);

		#endif

		if (status) {
			log_error(36, error_code, command);
			exit(3);
		}
		return 0;
	}

	if (!strcmp(argv[1],"stop") || !strcmp(argv[1],"status")) {
		admin_client(argc, argv);
		exit(0);
	}

	if (strcmp(argv[1],"execute")) {
		log_error(39, argv[1]);
		exit(3);
	}

	char *log_file   = NULL;
	char *log_prefix = NULL;

	char *db_orm_user_password  = NULL;

	db_port    = DB_PORT_DEFAULT;
	http_port  = atoi(HTTP_PORT_DEFAULT);
	admin_port = atoi(ADMIN_PORT_DEFAULT);

	for(int i=2; i<argc; i++) {
		if (argv[i][0]!='-') {
			log_error(41, argv[i]);
			exit(3);
		}
		if (i==argc-1) {
			log_error(2, argv[i]);
			exit(3);
		}

		if (strcmp(argv[i],"-h")==0) db_host=argv[++i];
		else if (strcmp(argv[i],"-p")==0) db_port=argv[++i];
		else if (strcmp(argv[i],"-d")==0) db_name=argv[++i];
		else if (strcmp(argv[i],"-orm-h")==0) db_orm_host=argv[++i];
		else if (strcmp(argv[i],"-orm-u")==0) db_orm_user=argv[++i];
		else if (strcmp(argv[i],"-orm-w")==0) db_orm_user_password=argv[++i];
		else if (strcmp(argv[i],"-hd")==0) http_directory = argv[++i];
		else if (strcmp(argv[i],"-hp")==0) {
			http_port = atoi(argv[++i]);
			if (http_port<=0) {
				log_error(42, argv[i]);
				exit(3);
			}
		} else if (strcmp(argv[i],"-ap")==0) {
			admin_port = atoi(argv[++i]);
			if (admin_port<=0) {
				log_error(42, argv[i]);
				exit(3);
			}
		} else if (strcmp(argv[i],"-l")==0) log_file = argv[++i];
		else {
			log_error(3, argv[i]);
			exit(3);
		}
	}

    if (db_host==NULL) db_host = tcp_host_addr[0]!=0 ? tcp_host_addr : DB_ORM_HOST_DEFAULT;
    if (db_orm_host==NULL) db_orm_host = !strcmp(db_host,tcp_host_addr) ? DB_ORM_HOST_DEFAULT : db_host;

    if ( pg_uri_build(db_orm_uri, sizeof(db_orm_uri), db_orm_host, db_port, db_name, db_orm_user, db_orm_user_password) )
    	exit(3);

	if (log_file!=NULL) log_set_file(log_file);

	utils_initialize();

	char header[STR_SIZE];
	if (log_get_header(header, sizeof(header)) || globals_add_parameters(header, sizeof(header))) exit(2);
    log_info("%s", header);

    if (!strcmp(db_host, DB_ORM_HOST_DEFAULT)) log_warn(904);

	// pgorm_test();

	if (thread_create(admin_server, "ADMIN", NULL))
		log_exit_fatal();

	if (thread_create(http_server, "HTTP_SERVER", NULL))
		log_exit_fatal();

	if (thread_create(orm_maker_thread, "ORM_MAKER", NULL))
		log_exit_fatal();

	if (thread_create(monitor_thread, "MONITOR", NULL))
		log_exit_fatal();

	while(1) sleep(30*60);

}
