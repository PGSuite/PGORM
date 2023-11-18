#include <dirent.h>

#include "globals.h"
#include "util/utils.h"

#include "metadata.h"

#define SQL_OBJECTS \
	"select schema,name,'relation',class_name from pgorm.orm_relation order by 1,2"

#define SQL_ACTIONS \
	"select id,object_schema,object_name,object_type,action,drop_class_name " \
	"  from pgorm.orm_action" \
    "  where id>$1" \
	"  order by id"

#define SQL_ACTION_ID_MAX \
	"select coalesce(max(id),0) from pgorm.orm_action"

#define SQL_ACTION_INSERT \
	"insert into pgorm.orm_action(object_schema,object_name,object_type,action,drop_class_name,orm_server) values ($1,$2,$3,$4,$5,inet_client_addr())"

#define SQL_ORM_RELATION_EXISTS \
	"select exists (select from pgorm.orm_relation where schema=$1 and name=$2)"

#define MODULE_NAME$PGORM_DB "/orm/pgorm-db.js"

int orm_maker_module_name(char *module_name, int module_name_size, char *module_base_name, int module_base_name_size, char *schema, char *class_name) {
	module_name[0] = 0;
	module_base_name[0] = 0;
	if (str_add(module_name, module_name_size, "/orm/", schema, "/", class_name, ".js", NULL)) return 1;
	if (str_add(module_base_name, module_base_name_size, "/orm/", schema, "/base/Base", class_name, ".js", NULL)) return 1;
	return 0;
}

int orm_maker_module_path(char *module_path, int module_path_size, char *module_name) {
	module_path[0] = 0;
	if (str_add(module_path, module_path_size, http_directory, module_name, NULL)) return 1;
	#ifdef _WIN32
	str_replace_char(module_path, '/', FILE_SEPARATOR[0]);
	#endif
	if (file_make_dirs(module_path, 1)) return 1;
	return 0;
}

int orm_maker_object_refresh(PGconn *pg_conn, char *object_schema, char *object_name, char *object_type) {
	if (!strcmp(object_type,"relation")) {
		int orm_relation_exists;
		if (pg_select_bool(&orm_relation_exists, pg_conn, SQL_ORM_RELATION_EXISTS, 2, object_schema, object_name)) return 1;
		if (!orm_relation_exists) return 0;
		metadata_relation relation;
		if (metadata_relation_load(&relation, pg_conn, object_schema, object_name)) return 1;
		if (orm_maker_relation_refresh(pg_conn, &relation)) return 1;
	}
	if (pg_execute(pg_conn, SQL_ACTION_INSERT, 5, object_schema, object_name, object_type, "orm_refresh", NULL)) return 1;
	return 0;
}

int orm_maker_object_remove(PGconn *pg_conn, char *object_schema, char *object_name, char *object_type, char *drop_class_name) {
	if (!strcmp(object_type,"relation")) {
		if (orm_maker_relation_remove(object_schema, drop_class_name)) return 1;
	}
	if (pg_execute(pg_conn, SQL_ACTION_INSERT, 5, object_schema, object_name, object_type, "orm_remove", drop_class_name)) return 1;
	return 0;
}

void orm_maker_try_remove_obsolete_files(char *dir_path, stream *module_files) {
    DIR *dir = opendir(dir_path);
    struct dirent *dir_ent;
    while ((dir_ent = readdir(dir)) != NULL) {
    	if (!strcmp(dir_ent->d_name, ".") || !strcmp(dir_ent->d_name, "..")) continue;
		char path_ent[STR_SIZE] = "";
		if (str_add(path_ent, sizeof(path_ent), dir_path, FILE_SEPARATOR, dir_ent->d_name, NULL)) continue;
		int is_dir;
		if (file_is_dir(path_ent, &is_dir) || is_dir==-1) continue;
		if (is_dir) {
			orm_maker_try_remove_obsolete_files(path_ent, module_files);
		} else {
			char path_ent_find[STR_SIZE] = "";
			if (str_add(path_ent_find, sizeof(path_ent_find), ";", path_ent, ";", NULL)) continue;
			if (str_find(module_files->data, 0, path_ent_find, 0)==-1) {
				if(file_remove(path_ent)<=0, 1)
					log_info("obsolete file \"%s\" removed", path_ent);
			}
		}
    }
    closedir(dir);
	if (rmdir(dir_path)) {
		if (errno!=2 && errno!=39 && errno!=41) log_error(79, dir_path, errno);
	} else
		log_info("obsolete directory \"%s\" removed", dir_path);
 }

void* orm_maker_thread(void *args) {

	thread_begin(args);

	for(;1;sleep(60)) {
		char module_path[STR_SIZE];
		if (orm_maker_module_path(module_path, sizeof(module_path), MODULE_NAME$PGORM_DB))
			continue;
		stream module_body;
		if (stream_init(&module_body))
			continue;
		if (res_pgorm_db_js(&module_body) || file_write_if_changed(module_path, &module_body)>0) {
			stream_free(&module_body);
			continue;
		}
		stream_free(&module_body);
		log_info("module \"%s\" verified", MODULE_NAME$PGORM_DB);
		break;
	}

	PGconn *pg_conn;
	PGresult *pg_result;
	char action_id_last[20];

	for(;1;sleep(60)) {
		if (pg_connect(&pg_conn, db_orm_uri))
			continue;
		if (orm_maker_schema_verify(pg_conn)) {
			pg_disconnect(&pg_conn);
			continue;
		}
		break;
	}

	stream module_files;
	if (!stream_init(&module_files)) {
		for(;1;sleep(60)) {
			if (pg_conn==NULL && pg_connect(&pg_conn, db_orm_uri))
				continue;
			if (pg_select(&pg_result, pg_conn, 1, SQL_OBJECTS, 0)) {
				pg_disconnect(&pg_conn);
				continue;
			}
			char module_name[STR_SIZE], module_path[STR_SIZE];
			char module_base_name[STR_SIZE], module_base_path[STR_SIZE];
			int res = orm_maker_module_path(module_path, sizeof(module_path), MODULE_NAME$PGORM_DB) || stream_add_str(&module_files, ";", module_path, ";", NULL);
			if (!res) {
				for(int i=0; i<PQntuples(pg_result); i++) {
					char *schema      = PQgetvalue(pg_result, i, 0);
					char *table_name  = PQgetvalue(pg_result, i, 1);
					char *object_type = PQgetvalue(pg_result, i, 2);
					char *class_name  = PQgetvalue(pg_result, i, 3);
					if (!strcmp(object_type,"relation")) {
						if (orm_maker_module_name(module_name, sizeof(module_name), module_base_name, sizeof(module_base_name), schema, class_name)) { res=1; break; }
						if (orm_maker_module_path(module_path, sizeof(module_path), module_name) || orm_maker_module_path(module_base_path, sizeof(module_base_path), module_base_name)) { res=1; break; }
						if (stream_add_str(&module_files, module_path, ";", module_base_path, ";", NULL)) { res=1; break; }
					}
				}
			}
			PQclear(pg_result);
			if (!res) {
				char http_directory_orm[STR_SIZE] = "";
				if (!str_add(http_directory_orm, sizeof(http_directory_orm), http_directory, FILE_SEPARATOR, "orm", NULL))
					orm_maker_try_remove_obsolete_files(http_directory_orm, &module_files);
			}
			break;
		}
		stream_free(&module_files);
	}

	for(;1;sleep(60)) {
		if (pg_conn==NULL && pg_connect(&pg_conn, db_orm_uri))
			continue;
		if (pg_select_str(action_id_last, sizeof(action_id_last), pg_conn, SQL_ACTION_ID_MAX, 0)) {
			pg_disconnect(&pg_conn);
			continue;
		}
		if (pg_select(&pg_result, pg_conn, 1, SQL_OBJECTS, 0)) {
			pg_disconnect(&pg_conn);
			continue;
		}
		int res = 0;
		for(int i=0; i<PQntuples(pg_result); i++) {
			if (orm_maker_object_refresh(pg_conn, PQgetvalue(pg_result, i, 0), PQgetvalue(pg_result, i, 1), PQgetvalue(pg_result, i, 2))) res = 1;
		}
		PQclear(pg_result);
		if (!res) break;
	}
	log_info("all modules verified");

	for(;1;sleep(10)) {
		if (pg_conn==NULL && pg_connect(&pg_conn, db_orm_uri))
			continue;
		if (pg_select(&pg_result, pg_conn, 1, SQL_ACTIONS, 1, action_id_last)) {
			pg_disconnect(&pg_conn);
			continue;
		}
		int res = 0;
		for(int i=0; i<PQntuples(pg_result); i++) {
			char *action_id        = PQgetvalue(pg_result, i, 0);
			char *object_schema    = PQgetvalue(pg_result, i, 1);
			char *object_name      = PQgetvalue(pg_result, i, 2);
			char *object_type      = PQgetvalue(pg_result, i, 3);
			char *action           = PQgetvalue(pg_result, i, 4);
			char *drop_class_name  = PQgetvalue(pg_result, i, 5);
			if (!strcmp(action,"enable") || !strcmp(action,"update_definition") || !strcmp(action,"alter") || !strcmp(action,"alter_child"))
				if (orm_maker_object_refresh(pg_conn, object_schema, object_name, object_type)) { res=1; break; };
			if (!strcmp(action,"disable") || !strcmp(action,"drop") )
				if (orm_maker_object_remove(pg_conn, object_schema, object_name, object_type, drop_class_name)) { res=1; break; };
			str_copy(action_id_last, sizeof(action_id_last), action_id);
		}
		PQclear(pg_result);
		if (res) continue;
	}
	thread_end(args);
	return 0;

}
