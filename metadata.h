typedef struct
{
	char name[128];
	int array;
	char type[32];
	int  not_null;
	//
	char field_name[128];
	char *field_type_primitive;
	char *field_type_object;
	int sortable;
	//
	char *js_value_func;
	char *pg_value_func;
	char validate_value_func[128];
} metadata_column;

typedef struct
{
	char name[128];
	int unuque;
	char id[128];
	int columns[32];
	int columns_len;

} metadata_index;

typedef struct
{
	char oid[32];
	char schema[64];
	char name[256];
	char class_name[256];
	metadata_column columns[256];
	int columns_len;
	char attnum_list[1024];
	//
	int columns_pk[32];
	int columns_pk_len;
	//
	metadata_index indexes[64];
	int indexes_len;
	//
	int field_name_len_max;
	int field_type_primitive_len_max;
	int field_type_object_len_max;
	int col_name_len_max;
	int col_type_len_max;
	int js_value_func_len_max;
	int pg_value_func_len_max;
	int validate_value_func_len_max;
	int sort_field_name_len_max;
	int sort_field_type_object_len_max;
	int columns_len_digits;
} metadata_table;
