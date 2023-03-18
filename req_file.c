#include "globals.h"
#include "util/utils.h"

int req_file(tcp_socket socket_connection, http_request *request) {

	char path[STR_SIZE] = "";
	if (str_add(path, sizeof(path), http_directory, request->path, NULL)) return 1;

	#ifdef _WIN32
		for(int i=0; path[i]!=0; i++)
			if (path[i]=='/') path[i]=FILE_SEPARATOR[0];
	#endif

	int is_dir;
	if (file_is_dir(path, &is_dir))	return 1;

	if (is_dir==-1) {
		return http_send_error(socket_connection, HTTP_STATUS_NOT_FOUND);
	}

	if (is_dir) {
		if (path[strlen(path)-1]!=FILE_SEPARATOR[0])
			if (str_add(path, sizeof(path), FILE_SEPARATOR, NULL))
				return 1;
		if (str_add(path, sizeof(path), "index.html", NULL))
			return 1;
		if (file_is_dir(path, &is_dir))
			return 1;
		if (is_dir) {
			return http_send_error(socket_connection, HTTP_STATUS_NOT_FOUND);
		}
	}

	char extension[STR_SIZE];
	if (file_extension(extension, sizeof(extension), path)) return 1;
	char *content_type;
	if (!strcmp(extension,"js")) content_type = HTTP_CONTENT_TYPE_JS;
	else content_type = HTTP_CONTENT_TYPE_HTML;

	stream file;
	if(file_read(path, &file)) return 1;

	int error = http_send_response(socket_connection, HTTP_STATUS_OK, content_type, file.len, file.data);
	stream_free(&file);
	return error;

}
