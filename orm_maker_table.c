#include "globals.h"
#include "util/utils.h"

#include "metadata.h"

#define SQL_TABLE_COMMENT \
	"select description from pg_description where objoid=$1 and objsubid=0"

#define SQL_COLUMN_COMMENTS \
	"select description" \
	"  from unnest($1::int[]) attnum" \
	"  left join pg_description on objoid=$2 and objsubid=attnum" \
	"  order by attnum"

#define SQL_MODULE_SOURCE \
	"select module_source from pgorm.orm_table where schema=$1 and table_name=$2"

int orm_maker_table_module_base(PGconn *pg_conn, stream *module_base_src, metadata_table *table) {
	PGresult *pg_result;
	char line1[32*1024],line2[32*1024];
	int res;
	if (stream_add_str(module_base_src, "// Module made automatically based on metadata and cannot be edited\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "import { TableRow,TableRowArray,TableColumn,TableMetadata,ORMConnection,ORMUtil,ORMError } from \"/orm/pgorm-db.js\"\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "import { ", table->class_name, ",", table->class_name, "Array } from \"/orm/", table->schema, "/", table->class_name, ".js\"\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "export class Base", table->class_name, " extends TableRow {\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // TableRow and TableRowList classes\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static getTableRowClass()      { return ", table->class_name, ";     }\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static getTableRowArrayClass() { return ", table->class_name, "Array; }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Table columns\n", NULL)) return 1;
	if (pg_select(&pg_result, pg_conn, 1, SQL_COLUMN_COMMENTS, 2, table->attnum_list, table->oid)) return 1;
	int i;
	for(i=0; i<table->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    static #column", table->columns[i].field_name, table->field_name_len_max, "")) break;
		if (stream_add_rpad(module_base_src, " = new TableColumn(\"", table->columns[i].name, table->col_name_len_max, "\", ")) break;
		if (stream_add_rpad(module_base_src, "\"", table->columns[i].type, table->col_type_len_max, "\", ")) break;
		if (stream_add_rpad(module_base_src, "", (table->columns[i].array ? "true" : "false"), 5, ", ")) break;
		if (stream_add_rpad(module_base_src, "\"", table->columns[i].field_name, table->field_name_len_max, "\", ")) break;
		if (table->columns[i].field_type_primitive!=NULL) {
			if (stream_add_rpad(module_base_src, "\"", table->columns[i].field_type_primitive, table->field_type_primitive_len_max, "\", ")) break;
		} else
			if (stream_add_rpad(module_base_src, "", "null", table->field_type_primitive_len_max, ",   ")) break;
		if (stream_add_rpad(module_base_src, "", table->columns[i].field_type_object, table->field_type_object_len_max, ", ")) break;
		if (stream_add_rpad(module_base_src, "", (table->columns[i].not_null ? "true" : "false"), 5, ", ")) break;
		if (stream_add_str(module_base_src, "Base", table->class_name, ".getTableRowClass, ", NULL)) break;
		if (stream_add_rpad(module_base_src, "ORMUtil.", table->columns[i].validate_value_func, table->validate_value_func_len_max, ", ")) break;
		if (stream_add_rpad(module_base_src, "ORMUtil.", table->columns[i].js_value_func, table->js_value_func_len_max, ", ")) break;
		if (stream_add_rpad(module_base_src, "ORMUtil.", table->columns[i].pg_value_func, table->pg_value_func_len_max, ", ")) break;
		if (PQgetisnull(pg_result, i, 0)) {
			if (stream_add_str(module_base_src, "null", NULL)) break;
		} else
			if (stream_add_str_escaped(module_base_src, PQgetvalue(pg_result, i, 0)) ) break;
		if (stream_add_str(module_base_src, ");\n", NULL)) break;
	}
	PQclear(pg_result);
	if (i!=table->columns_len) return 1;
	if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    static getColumn", table->columns[i].field_name, table->field_name_len_max, "() "))  return 1;
		if (stream_add_str(module_base_src, "{ return Base", table->class_name, NULL)) return 1;
		if (stream_add_rpad(module_base_src, ".#column", table->columns[i].field_name, table->field_name_len_max, "; "))  return 1;
		if (stream_add_str(module_base_src, "} \n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Table metadata\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static #tableMetadata = new TableMetadata(\"",table->schema,"\", \"",table->name,"\", [", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++)
		if (stream_add_str(module_base_src, i>0 ? "," : "", "Base", table->class_name, ".#column", table->columns[i].field_name, NULL)) return 1;
	if (stream_add_str(module_base_src, "], [", NULL)) return 1;
	for(int i=0; i<table->columns_pk_len; i++)
		if (stream_add_str(module_base_src, i>0 ? "," : "", "Base", table->class_name, ".#column", table->columns[table->columns_pk[i]].field_name, NULL)) return 1;
	if (stream_add_str(module_base_src, "], Base", table->class_name, ".getTableRowClass, Base", table->class_name, ".getTableRowArrayClass, ", NULL)) return 1;
	if (pg_select(&pg_result, pg_conn, 1, SQL_TABLE_COMMENT, 1, table->oid)) return 1;
	res = PQntuples(pg_result)>0 ? stream_add_str_escaped(module_base_src, PQgetvalue(pg_result, 0, 0)) : stream_add_str(module_base_src, "null", NULL);
	PQclear(pg_result);
	if (res) return 1;
	if (stream_add_str(module_base_src, ");\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static getTableMetadata() { return Base", table->class_name, ".#tableMetadata; }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Row fields and change marks\n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    #", table->columns[i].field_name, table->field_name_len_max, ""))  return 1;
		if (stream_add_rpad(module_base_src, " = null; #", table->columns[i].field_name, table->field_name_len_max, "$changed"))  return 1;
		if (stream_add_str(module_base_src, " = false; \n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Set values\n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		metadata_column *column = &table->columns[i];
		if (stream_add_rpad(module_base_src, "    set", column->field_name, table->field_name_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, "(value) { ORMUtil.", column->validate_value_func, table->validate_value_func_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, "(value); this.#", column->field_name, table->field_name_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, " = value; this.#", column->field_name, table->field_name_len_max, "$changed")) return 1;
		if (stream_add_str(module_base_src, " = true; return this; }\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Get values\n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    get", table->columns[i].field_name, table->field_name_len_max, "()")) return 1;
		if (stream_add_rpad(module_base_src, " { return this.#", table->columns[i].field_name, table->field_name_len_max, ";")) return 1;
		if (stream_add_str(module_base_src, " } \n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Get values for change\n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    change", table->columns[i].field_name, table->field_name_len_max, "()")) return 1;
		if (stream_add_rpad(module_base_src, " { this.#", table->columns[i].field_name, table->field_name_len_max, "$changed")) return 1;
		if (stream_add_rpad(module_base_src, " = true; return this.#", table->columns[i].field_name, table->field_name_len_max, ";")) return 1;
		if (stream_add_str(module_base_src, " }\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Get change marks \n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    isChanged", table->columns[i].field_name, table->field_name_len_max, "()"))  return 1;
		if (stream_add_rpad(module_base_src, " { return this.#", table->columns[i].field_name, table->field_name_len_max, "$changed; "))  return 1;
		if (stream_add_str(module_base_src, "} \n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Save row and get values (SQL statement: insert or update with returning)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    save(connection = ORMConnection.getDefault()) { return super.save(connection); }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Delete row and get values (SQL statement: delete with returning)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    delete(connection = ORMConnection.getDefault()) { super.delete(connection);  }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Load one row by condition (SQL statement: select)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static loadByCondition(condition, params = null, connection = ORMConnection.getDefault()) { return TableRow.loadByCondition(", table->class_name, ", connection, condition, params); }\n\n", NULL)) return 1;
	line1[0] = 0; line2[0] = 0;
	for(int i=0; i<table->columns_pk_len; i++) {
		char param[STR_SIZE];
		if (str_format(param, sizeof(param), "$%d", i+1)) return 1;
		if (str_add(line1, sizeof(line1), i>0 ? " and " : "", table->columns[table->columns_pk[i]].name, "=", param, NULL)) return 1;
		if (str_add(line2, sizeof(line2), i>0 ? ", " : "", table->columns[table->columns_pk[i]].field_name, NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "    // Load row by primary key\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static load(", line2, ", connection = ORMConnection.getDefault()) { return ", table->class_name, ".loadByCondition(\"", line1, "\", [", line2, "], connection); }\n\n", NULL)) return 1;
	for(int i=0; i<table->indexes_len; i++) {
		if (!table->indexes[i].unuque) continue;
		line1[0] = 0; line2[0] = 0;
		for(int j=0; j<table->indexes[i].columns_len; j++) {
			char variable[256];
			if (str_format(variable, sizeof(variable), "$%d", j+1)) return 1;
			if (str_add(line1, sizeof(line1), j>0 ? ", " : "", table->columns[table->indexes[i].columns[j]].field_name, NULL)) return 1;
			if (str_add(line2, sizeof(line2), j>0 ? " and " : "", table->columns[table->indexes[i].columns[j]].name, "=", variable, NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "    // Load row by unique index \"",table->indexes[i].name,"\"\n", NULL)) return 1;
		if (stream_add_str(module_base_src, "    static loadByUnique",table->indexes[i].id,"(",line1,", connection = ORMConnection.getDefault()) { return ", table->class_name, ".loadByCondition(\"",line2,"\", [",line1,"], connection); }\n\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "    // Initialize field values fetched from database\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    _initialize(pgValues, rowExists) {\n", NULL)) return 1;
	for(int i=0; i<table->columns_len; i++) {
		sprintf(line1, "%*d", table->columns_len_digits, i);
		if (stream_add_rpad(module_base_src, "        this.#", table->columns[i].field_name, table->field_name_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, " = ORMUtil.", table->columns[i].js_value_func, table->js_value_func_len_max, "")) return 1;
		if (stream_add_str(module_base_src, "(pgValues[", line1, "]); ", NULL)) return 1;
		if (stream_add_rpad(module_base_src, "this.#", table->columns[i].field_name, table->field_name_len_max, "$changed")) return 1;
		if (stream_add_str(module_base_src, " = false;\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "        return super._initialize(rowExists);\n    }\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "\n}\n\n", NULL)) return 1;
	//
	if (stream_add_str(module_base_src, "export class Base", table->class_name, "Array extends TableRowArray {\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Get table matadata\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static getTableMetadata() { return Base", table->class_name, ".getTableMetadata(); }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Load any row by condition (SQL statement: select)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // If there is no condition, all row load\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static load(condition = null, params = null, connection = ORMConnection.getDefault()) { return TableRowArray.load(", table->class_name, "Array, connection, condition, params); }\n\n", NULL)) return 1;
	for(int i=0; i<table->indexes_len; i++) {
		if (table->indexes[i].unuque) continue;
		line1[0] = 0;
		line2[0] = 0;
		for(int j=0; j<table->indexes[i].columns_len; j++) {
			char variable[256];
			if (str_format(variable, sizeof(variable), "$%d", j+1)) return 1;
			if (str_add(line1, sizeof(line1), j>0 ? ", " : "", table->columns[table->indexes[i].columns[j]].field_name, NULL)) return 1;
			if (str_add(line2, sizeof(line2), j>0 ? " and " : "", table->columns[table->indexes[i].columns[j]].name, "=", variable, NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "    // Load rows by index \"",table->indexes[i].name,"\"\n", NULL)) return 1;
		if (stream_add_str(module_base_src, "    static loadByIndex",table->indexes[i].id,"(",line1,", connection = ORMConnection.getDefault()) { return ", table->class_name, "List.load(\"",line2,"\", [",line1,"], connection); }\n\n", NULL)) return 1;
	}
	if (table->sort_field_name_len_max!=-1) {
		if (stream_add_str(module_base_src, "    // Sort by fields\n", NULL)) return 1;
		for(i=0; i<table->columns_len; i++) {
			metadata_column *column = &table->columns[i];
			if (!column->sortable) continue;
			if (stream_add_rpad(module_base_src, "    sortBy", column->field_name, table->sort_field_name_len_max, "")) return 1;
			if (stream_add_rpad(module_base_src, "(desc = false, nullsFirst = false) { this.sort( (row1,row2) => ORMUtil.compare", column->field_type_object, table->sort_field_type_object_len_max, "")) return 1;
			if (stream_add_rpad(module_base_src, "(row1.get", column->field_name, table->sort_field_name_len_max, "(), ")) return 1;
			if (stream_add_rpad(module_base_src, "row2.get", column->field_name, table->sort_field_name_len_max, "(), ")) return 1;
			if (stream_add_str(module_base_src,  "desc, nullsFirst) ); return this; }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "}\n", NULL)) return 1;
	return 0;
}

int orm_maker_table_refresh(PGconn *pg_conn, metadata_table *table) {
	log_info("making class %s for table %s.%s", table->class_name, table->schema, table->name);
	char module_name[STR_SIZE], module_path[STR_SIZE];
	char module_base_name[STR_SIZE], module_base_path[STR_SIZE];
	if (orm_maker_module_name(module_name, sizeof(module_name), module_base_name, sizeof(module_base_name), table->schema, table->class_name)) return 1;
	if (orm_maker_module_path(module_path, sizeof(module_path), module_name)) return 1;
	if (orm_maker_module_path(module_base_path, sizeof(module_base_path), module_base_name)) return 1;
	stream module_src;
	if (stream_init(&module_src)) return 1;
	PGresult *pg_result;
	if (pg_select(&pg_result, pg_conn, 3, SQL_MODULE_SOURCE, 2, table->schema, table->name)) {
		stream_free(&module_src);
		return 1;
	}
	int res = stream_add_str(&module_src, PQgetvalue(pg_result, 0, 0), NULL);
	PQclear(pg_result);
	if (res) {
		stream_free(&module_src);
		return 1;
	};
	res = file_write_if_changed(module_path, &module_src)>0;
	stream_free(&module_src);
	if (res) return 1;
	stream module_base_src;
	if (stream_init(&module_base_src)) return 1;
	if (orm_maker_table_module_base(pg_conn, &module_base_src, table)) {
		stream_free(&module_base_src);
		return 1;
	}
	res = file_write_if_changed(module_base_path, &module_base_src)>0;
	stream_free(&module_base_src);
	if (res) return 1;
	log_info("modules \"%s\",\"%s\" refreshed", module_name, module_base_name);
	return 0;
}

int orm_maker_table_remove(char *schema, char *class_name) {
	char module_name[STR_SIZE], module_path[STR_SIZE];
	char module_base_name[STR_SIZE], module_base_path[STR_SIZE];
	if (orm_maker_module_name(module_name, sizeof(module_name), module_base_name, sizeof(module_base_name), schema, class_name)) return 1;
	if (orm_maker_module_path(module_path, sizeof(module_path), module_name)) return 1;
	if (orm_maker_module_path(module_base_path, sizeof(module_base_path), module_base_name)) return 1;
	if (file_remove(module_base_path, 1)) return 1;
	if (file_remove(module_path, 1)) return 1;
	log_info("modules \"%s\",\"%s\" removed", module_name, module_base_name);
	return 0;
}
