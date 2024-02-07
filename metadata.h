#include <util/utils.h>

#define METADATA_TABLE_RELATIONSHIPS_SIZE 24

typedef struct {
	char name[128];
	int array;
	char type[32];
	int  not_null;
	//
	char field_name[128];
	char *field_type;
	int sortable;
	//
	char js_value_func[32];
	char pg_value_func[32];
	char validate_value_func[128];
} metadata_column;

typedef struct {
	char name[128];
	int unuque;
	char id[128];
	int columns[32];
	int columns_len;
} metadata_index;

typedef struct {
	char field_parent_name[128];
	char parent_table_schema[64];
	char parent_table_name[128];
	char parent_class_name[128];
	str_list parent_fields;
	char field_child_name[128];
	char child_class_name[128];
	str_list child_columns;
	char child_field_sort_pos[128];
	str_list child_fields;
} metadata_relationship;

typedef struct {
	char oid[32];
	char schema[64];
	char name[128];
	char type[16];
	char class_name[128];
	metadata_column columns[256];
	int columns_len;
	char attnum_list[1024];
	//
	int columns_pkey[32];
	int columns_pkey_len;
	//
	metadata_relationship parents[METADATA_TABLE_RELATIONSHIPS_SIZE];
	int parents_len;
	//
	metadata_relationship children[METADATA_TABLE_RELATIONSHIPS_SIZE];
	int children_len;
	//
	metadata_index indexes[64];
	int indexes_len;
	//
	int field_name_len_max;
	int field_type_len_max;
	int col_name_len_max;
	int col_type_len_max;
	int js_value_func_len_max;
	int pg_value_func_len_max;
	int validate_value_func_len_max;
	int sort_field_name_len_max;
	int sort_field_type_len_max;
	int parents_field_parent_name_len_max;
	int parents_parent_table_schema_len_max;
	int parents_parent_table_name_len_max;
	int parents_parent_class_name_len_max;
	int parents_child_field_sort_pos_len_max;
	int children_field_parent_name_len_max;
	int children_field_child_name_len_max;
	int children_child_class_name_len_max;
	int relationships_class_name_len_max;
	int field_relation_name_len_max;
	int columns_len_digits;
} metadata_relation;
