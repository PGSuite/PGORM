#include "globals.h"
#include "util/utils.h"

#include "metadata.h"

#define SQL_RELATION_COMMENT \
	"select description from pg_description where objoid=$1 and objsubid=0"

#define SQL_COLUMN_COMMENTS \
	"select description" \
	"  from unnest($1::int[]) attnum" \
	"  left join pg_description on objoid=$2 and objsubid=attnum" \
	"  order by attnum"

#define SQL_MODULE_SOURCE \
	"select module_source from pgorm.orm_relation where schema=$1 and name=$2"

int orm_maker_relation_module_base(PGconn *pg_conn, stream *module_base_src, metadata_relation *relation) {
	PGresult *pg_result;
	char line1[32*1024],line2[32*1024],line3[32*1024];
	int res;
	if (stream_add_str(module_base_src, "// Module made automatically based on relation (table or view definition) and cannot be edited\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "import { Relation,Column,Record,RecordArray,Relationship,ORMConnection,ORMUtil,ORMError } from \"/orm/pgorm-db.js\"\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "import { ", relation->class_name, ",", relation->class_name, "Array } from \"/orm/", relation->schema, "/", relation->class_name, ".js\"\n\n", NULL)) return 1;
	if (relation->parents_len>0 || relation->children_len>0) {
		if (stream_add_str(module_base_src, "// Import classes by relationship\n", NULL)) return 1;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			if (strcmp(parent->parent_class_name,relation->class_name)==0) continue;
			int imported = 0;
			for(int j=0; j<i; j++)
				if (strcmp(relation->parents[j].parent_class_name,parent->parent_class_name)==0) { imported=1; break; }
			if (imported) continue;
			if (stream_add_rpad(module_base_src, "import { ", parent->parent_class_name, relation->relationships_class_name_len_max, ","))  return 1;
			if (stream_add_rpad(module_base_src, "", parent->parent_class_name, relation->relationships_class_name_len_max, "Array"))  return 1;
			if (stream_add_str(module_base_src, " } from \"/orm/", parent->parent_table_schema, "/", parent->parent_class_name, ".js\"\n", NULL)) return 1;
		}
		for(int i=0; i<relation->children_len; i++) {
			metadata_relationship *child = &relation->children[i];
			if (strcmp(child->child_class_name,relation->class_name)==0) continue;
			int imported = 0;
			for(int j=0; j<relation->parents_len; j++)
				if (strcmp(relation->parents[j].parent_class_name,child->child_class_name)==0) { imported=1; break; }
			for(int j=0; j<i; j++)
				if (strcmp(relation->children[j].child_class_name,child->child_class_name)==0) { imported=1; break; }
			if (imported) continue;
			if (stream_add_rpad(module_base_src, "import { ", child->child_class_name, relation->relationships_class_name_len_max, ","))  return 1;
			if (stream_add_rpad(module_base_src, "", child->child_class_name, relation->relationships_class_name_len_max, "Array"))  return 1;
			if (stream_add_str(module_base_src, " } from \"/orm/", child->parent_table_schema, "/", child->child_class_name, ".js\"\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "export class Base", relation->class_name, " extends Record {\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Columns\n", NULL)) return 1;
	if (pg_select(&pg_result, pg_conn, 1, SQL_COLUMN_COMMENTS, 2, relation->attnum_list, relation->oid)) return 1;
	int i;
	for(i=0; i<relation->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    static #column", relation->columns[i].field_name, relation->field_name_len_max, "")) break;
		if (stream_add_rpad(module_base_src, " = new Column(\"", relation->columns[i].name, relation->col_name_len_max, "\", ")) break;
		if (stream_add_rpad(module_base_src, "\"", relation->columns[i].type, relation->col_type_len_max, "\", ")) break;
		if (stream_add_rpad(module_base_src, "", (relation->columns[i].array ? "true" : "false"), 5, ", ")) break;
		if (stream_add_rpad(module_base_src, "\"", relation->columns[i].field_name, relation->field_name_len_max, "\", ")) break;
		if (stream_add_rpad(module_base_src, "", relation->columns[i].field_type, relation->field_type_len_max, ", ")) break;
		if (stream_add_rpad(module_base_src, "", (relation->columns[i].not_null ? "true" : "false"), 5, ", ")) break;
		if (stream_add_str(module_base_src, "function() { return ", relation->class_name, "; }, ", NULL)) break;
		if (stream_add_rpad(module_base_src, "ORMUtil.", relation->columns[i].validate_value_func, relation->validate_value_func_len_max, ", ")) break;
		if (stream_add_rpad(module_base_src, "ORMUtil.", relation->columns[i].js_value_func, relation->js_value_func_len_max, ", ")) break;
		if (stream_add_rpad(module_base_src, "ORMUtil.", relation->columns[i].pg_value_func, relation->pg_value_func_len_max, ", ")) break;
		if (PQgetisnull(pg_result, i, 0)) {
			if (stream_add_str(module_base_src, "null", NULL)) break;
		} else
			if (stream_add_str_escaped(module_base_src, PQgetvalue(pg_result, i, 0)) ) break;
		if (stream_add_str(module_base_src, ");\n", NULL)) break;
	}
	PQclear(pg_result);
	if (i!=relation->columns_len) return 1;
	if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    static getColumn", relation->columns[i].field_name, relation->field_name_len_max, "() "))  return 1;
		if (stream_add_str(module_base_src, "{ return Base", relation->class_name, NULL)) return 1;
		if (stream_add_rpad(module_base_src, ".#column", relation->columns[i].field_name, relation->field_name_len_max, "; "))  return 1;
		if (stream_add_str(module_base_src, "} \n", NULL)) return 1;
	}
	line1[0] = 0; line2[0] = 0;
	if (relation->parents_len>0 || relation->children_len>0) {
		if (stream_add_str(module_base_src, "\n    // Relationships\n", NULL)) return 1;
		int line3_len_max = 0;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			line3[0] = 0;
			for(int j=0; j<parent->child_fields.len; j++)
				if (str_add(line3, sizeof(line3), j>0 ? "," : "", "Base", relation->class_name, ".#column", parent->child_fields.values[j], NULL)) return 1;
			str_len_max(&line3_len_max, line3);
		}
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			line3[0] = 0;
			for(int j=0; j<parent->child_fields.len; j++)
				if (str_add(line3, sizeof(line3), j>0 ? "," : "", "Base", relation->class_name, ".#column", parent->child_fields.values[j], NULL)) return 1;
			if (stream_add_rpad(module_base_src, "    static #relationshipParent", parent->field_parent_name, relation->parents_field_parent_name_len_max, ""))  return 1;
			if (stream_add_rpad(module_base_src, " = new Relationship(\"", parent->parent_table_schema, relation->parents_parent_table_schema_len_max, "\","))  return 1;
			if (stream_add_rpad(module_base_src, " \"", parent->parent_table_name, relation->parents_parent_table_name_len_max, "\","))  return 1;
			if (stream_add_rpad(module_base_src, " function() { return ", parent->parent_class_name, relation->parents_parent_class_name_len_max, ";"))  return 1;
			if (stream_add_str(module_base_src, " }, \"", relation->schema, "\", \"", relation->name, "\", function() { return ", relation->class_name, "; }", NULL)) return 1;
			if (stream_add_rpad(module_base_src, ", [", line3, line3_len_max, "], "))  return 1;
			if (parent->child_field_sort_pos[0]==0) {
				if (stream_add_rpad(module_base_src, "", "null", 4+strlen(relation->class_name)+8+relation->parents_child_field_sort_pos_len_max, ""))  return 1;
			} else {
				if (stream_add_str(module_base_src, "Base", relation->class_name, ".#column", NULL)) return 1;
				if (stream_add_rpad(module_base_src, "", parent->child_field_sort_pos, relation->parents_child_field_sort_pos_len_max, ""))  return 1;
			}
			if (stream_add_str(module_base_src, ");\n", NULL)) return 1;
		}
		if (relation->parents_len>0)
			if (stream_add_str(module_base_src, "\n", NULL)) return 1;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			if (stream_add_rpad(module_base_src, "    static getRelationshipParent", parent->field_parent_name, relation->parents_field_parent_name_len_max, "()"))  return 1;
			if (stream_add_str(module_base_src, " { return Base", relation->class_name, NULL)) return 1;
			if (stream_add_rpad(module_base_src, ".#relationshipParent", parent->field_parent_name, relation->parents_field_parent_name_len_max, ";"))  return 1;
			if (stream_add_str(module_base_src, " }\n", NULL)) return 1;
			if (str_add(line1, sizeof(line1), i>0 ? "," : "", "Base", relation->class_name, ".#relationshipParent", parent->field_parent_name, NULL)) return 1;
		}
		if (relation->parents_len>0 && relation->children_len>0)
			if (stream_add_str(module_base_src, "\n", NULL)) return 1;
		for(int i=0; i<relation->children_len; i++) {
			metadata_relationship *child = &relation->children[i];
			if (stream_add_rpad(module_base_src, "    static getRelationshipChild", child->field_child_name, relation->field_relation_name_len_max, "()"))  return 1;
			if (stream_add_str(module_base_src, " { return ", child->child_class_name, NULL)) return 1;
			if (stream_add_rpad(module_base_src, ".getRelationshipParent", child->field_parent_name, relation->children_field_parent_name_len_max, "();"))  return 1;
			if (stream_add_str(module_base_src, " }\n", NULL)) return 1;
			if (str_add(line2, sizeof(line2), i>0 ? "," : "", "Base", relation->class_name, ".getRelationshipChild", child->field_child_name, "()", NULL)) return 1;
		}
	}
	if (stream_add_str(module_base_src, "\n    // Relation (table definition)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static #relation = new Relation(\"",relation->schema,"\", \"",relation->name,"\", \"",relation->type,"\", ", NULL)) return 1;
	if (pg_select(&pg_result, pg_conn, 1, SQL_RELATION_COMMENT, 1, relation->oid)) return 1;
	res = PQntuples(pg_result)>0 ? stream_add_str_escaped(module_base_src, PQgetvalue(pg_result, 0, 0)) : stream_add_str(module_base_src, "null", NULL);
	PQclear(pg_result);
	if (res) return 1;
	if (stream_add_str(module_base_src, ", [", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++)
		if (stream_add_str(module_base_src, i>0 ? "," : "", "Base", relation->class_name, ".#column", relation->columns[i].field_name, NULL)) return 1;
	if (stream_add_str(module_base_src, "], [", NULL)) return 1;
	for(int i=0; i<relation->columns_pkey_len; i++)
		if (stream_add_str(module_base_src, i>0 ? "," : "", "Base", relation->class_name, ".#column", relation->columns[relation->columns_pkey[i]].field_name, NULL)) return 1;
	if (stream_add_str(module_base_src, "], function() { return ", relation->class_name, "; }, function() { return ", relation->class_name, "Array; }, ", NULL)) return 1;
	if (stream_add_str(module_base_src, "[", line1, "], function() { return [", line2, "]; });\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static getRelation() { return Base", relation->class_name, ".#relation; }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Record fields\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    #", relation->columns[i].field_name, relation->field_name_len_max, ""))  return 1;
		if (stream_add_rpad(module_base_src, " = null; #", relation->columns[i].field_name, relation->field_name_len_max, "$changed"))  return 1;
		if (stream_add_str(module_base_src, " = false; \n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Set values\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		metadata_column *column = &relation->columns[i];
		if (stream_add_rpad(module_base_src, "    set", column->field_name, relation->field_name_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, "(value) { ORMUtil.", column->validate_value_func, relation->validate_value_func_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, "(value); this.#", column->field_name, relation->field_name_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, " = value; this.#", column->field_name, relation->field_name_len_max, "$changed")) return 1;
		if (stream_add_str(module_base_src, " = true;", NULL)) return 1;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			for(int j=0; j<parent->child_fields.len; j++)
				if (strcmp(column->field_name, parent->child_fields.values[j])==0) {
					if (stream_add_str(module_base_src, " this.#", parent->field_parent_name, " = null;", " this.#", parent->field_parent_name, "$assigned = false;", NULL)) return 1;
				}
		}
		if (stream_add_str(module_base_src, " return this; }\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n    // Get values\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    get", relation->columns[i].field_name, relation->field_name_len_max, "()")) return 1;
		if (stream_add_rpad(module_base_src, " { return this.#", relation->columns[i].field_name, relation->field_name_len_max, ";")) return 1;
		if (stream_add_str(module_base_src, " } \n", NULL)) return 1;
	}
	/*
	if (stream_add_str(module_base_src, "\n    // Get values for change\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    change", relation->columns[i].field_name, relation->field_name_len_max, "()")) return 1;
		if (stream_add_rpad(module_base_src, " { this.#", relation->columns[i].field_name, relation->field_name_len_max, "$changed")) return 1;
		if (stream_add_rpad(module_base_src, " = true; return this.#", relation->columns[i].field_name, relation->field_name_len_max, ";")) return 1;
		if (stream_add_str(module_base_src, " }\n", NULL)) return 1;
	}
	*/
	if (stream_add_str(module_base_src, "\n    // Get change marks \n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		if (stream_add_rpad(module_base_src, "    isChanged", relation->columns[i].field_name, relation->field_name_len_max, "()"))  return 1;
		if (stream_add_rpad(module_base_src, " { return this.#", relation->columns[i].field_name, relation->field_name_len_max, "$changed; "))  return 1;
		if (stream_add_str(module_base_src, "} \n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	if (str_copy(line1, sizeof(line1), "throw new ORMError(310, `Relation \"", relation->schema, ".", relation->name, "\" does not have primary key`);", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Save record and get values (SQL statement: insert/update returning)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    save(connection = ORMConnection.getDefault()) { ", relation->columns_pkey_len>0 ? "return super.save(connection);" : line1, " }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Delete record and get values (SQL statement: delete returning)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    delete(connection = ORMConnection.getDefault()) { ", relation->columns_pkey_len>0 ? "super.delete(connection);" : line1, " }\n\n", NULL)) return 1;
	if (relation->columns_pkey_len>0) {
		line1[0] = 0; line2[0] = 0;
		for(int i=0; i<relation->columns_pkey_len; i++) {
			char param[STR_SIZE];
			if (str_format(param, sizeof(param), "$%d", i+1)) return 1;
			if (str_add(line1, sizeof(line1), i>0 ? " and " : "", relation->columns[relation->columns_pkey[i]].name, "=", param, NULL)) return 1;
			if (str_add(line2, sizeof(line2), i>0 ? ", " : "", relation->columns[relation->columns_pkey[i]].field_name, NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "    // Get record by primary key\n", NULL)) return 1;
		if (stream_add_str(module_base_src, "    static find(", line2, ", connection = ORMConnection.getDefault()) { return ", relation->class_name, ".findWhere(\"", line1, "\", [", line2, "], connection); }\n\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "    // Get one record by condition (SQL statement: select where [condition])\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static findWhere(condition, params = null, connection = ORMConnection.getDefault()) { return Record.loadWhere(", relation->class_name, ", connection, condition, params); }\n\n", NULL)) return 1;
	for(int i=0; i<relation->indexes_len; i++) {
		if (!relation->indexes[i].unuque) continue;
		line1[0] = 0; line2[0] = 0;
		for(int j=0; j<relation->indexes[i].columns_len; j++) {
			char variable[256];
			if (str_format(variable, sizeof(variable), "$%d", j+1)) return 1;
			if (str_add(line1, sizeof(line1), j>0 ? ", " : "", relation->columns[relation->indexes[i].columns[j]].field_name, NULL)) return 1;
			if (str_add(line2, sizeof(line2), j>0 ? " and " : "", relation->columns[relation->indexes[i].columns[j]].name, "=", variable, NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "    // Get record by unique index \"",relation->indexes[i].name,"\"\n", NULL)) return 1;
		if (stream_add_str(module_base_src, "    static findBy",relation->indexes[i].id,"(",line1,", connection = ORMConnection.getDefault()) { return ", relation->class_name, ".findWhere(\"",line2,"\", [",line1,"], connection); }\n\n", NULL)) return 1;
	}
	if (relation->parents_len>0) {
		if (stream_add_str(module_base_src, "    // Parents\n", NULL)) return 1;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			if (stream_add_rpad(module_base_src, "    #", parent->field_parent_name, relation->parents_field_parent_name_len_max, ""))  return 1;
			if (stream_add_str(module_base_src, " = null;", NULL)) return 1;
			if (stream_add_rpad(module_base_src, " #", parent->field_parent_name, relation->parents_field_parent_name_len_max, "$assigned"))  return 1;
			if (stream_add_str(module_base_src, " = false;\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n    // Get parents\n", NULL)) return 1;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			if (stream_add_str(module_base_src, "    get", parent->field_parent_name, "(connection = ORMConnection.getDefault()) {\n", NULL)) return 1;
			for(int j=0; j<parent->child_fields.len; j++)
				if (stream_add_str(module_base_src, "        if (this.#", parent->child_fields.values[j], "===null) return null;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (this.#", parent->field_parent_name, "$assigned) return this.#", parent->field_parent_name, ";\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", parent->field_parent_name, " = ", parent->parent_class_name, ".find(", NULL)) return 1;
			for(int j=0; j<parent->child_fields.len; j++)
				if (stream_add_str(module_base_src, j>0 ? ", " : "", "this.#", parent->child_fields.values[j], NULL)) return 1;
			if (stream_add_str(module_base_src, ", connection);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", parent->field_parent_name, "$assigned = true;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        return this.#", parent->field_parent_name, ";\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "    }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n    // Set parents\n", NULL)) return 1;
		for(int i=0; i<relation->parents_len; i++) {
			metadata_relationship *parent = &relation->parents[i];
			if (stream_add_str(module_base_src, "    set", parent->field_parent_name, "(value) {\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (!(value instanceof ", parent->parent_class_name, ")) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not ", parent->parent_class_name, "`);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (value!==null && !value.isRecordExists()) throw new ORMError(302, `Object \"", parent->parent_class_name, "\" not saved in database`);\n", NULL)) return 1;
			for(int j=0; j<parent->child_fields.len; j++) {
				if (stream_add_str(module_base_src, "        this.set", parent->child_fields.values[j], "(value!==null ? ", NULL)) return 1;
			    if (stream_add_str(module_base_src, "value.get", parent->parent_fields.values[j], "() : null);\n", NULL)) return 1;
			}
			if (stream_add_str(module_base_src, "        this.#", parent->field_parent_name, " = value;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", parent->field_parent_name, "$assigned = true;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        return this;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "    }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	}
	if (relation->children_len>0) {
		if (stream_add_str(module_base_src, "    // Children\n", NULL)) return 1;
		for(int i=0; i<relation->children_len; i++) {
			metadata_relationship *child = &relation->children[i];
			if (stream_add_rpad(module_base_src, "    #", child->field_child_name, relation->children_field_child_name_len_max, "Array"))  return 1;
			if (stream_add_str(module_base_src, " = null;", NULL)) return 1;
			if (stream_add_rpad(module_base_src, " #", child->field_child_name, relation->children_field_child_name_len_max, "Array$assigned"))  return 1;
			if (stream_add_str(module_base_src, " = false;\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n    // Get children\n", NULL)) return 1;
		for(int i=0; i<relation->children_len; i++) {
			metadata_relationship *child = &relation->children[i];
			if (stream_add_str(module_base_src, "    get", child->field_child_name, "Array(connection = ORMConnection.getDefault()) {\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (!this.isRecordExists()) throw new ORMError(302, \"Object not saved in database\");\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (this.#", child->field_child_name, "Array$assigned) return this.#", child->field_child_name, "Array;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", child->field_child_name, "Array = ", child->child_class_name, "Array.findWhere(\"", NULL)) return 1;
			for(int j=0; j<child->child_columns.len; j++) {
				if (stream_add_str(module_base_src, j==0 ? "" : " and ", child->child_columns.values[j], "=$", NULL)) return 1;
			    if (stream_add_int(module_base_src, j+1)) return 1;
			}
			if (stream_add_str(module_base_src, "\", [", NULL)) return 1;
			for(int j=0; j<child->parent_fields.len; j++) {
				if (stream_add_str(module_base_src, j==0 ? "" : ",", "this.#", child->parent_fields.values[j], NULL)) return 1;
			}
			if (stream_add_str(module_base_src, "], connection)", NULL)) return 1;
			if (child->child_field_sort_pos[0]!=0)
				if (stream_add_str(module_base_src, ".sortBy", child->child_field_sort_pos, "()", NULL)) return 1;
			if (stream_add_str(module_base_src, ";\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", child->field_child_name, "Array$assigned = true;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        return this.#", child->field_child_name, "Array;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "    }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n    // Add child\n", NULL)) return 1;
		for(int i=0; i<relation->children_len; i++) {
			metadata_relationship *child = &relation->children[i];
			if (stream_add_str(module_base_src, "    add", child->field_child_name, "(child, connection = ORMConnection.getDefault()) {\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (child===null) throw new ORMError(309, \"Child is null\");\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (!(child instanceof ", child->child_class_name, ")) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not ", child->child_class_name, "`);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.get", child->field_child_name, "Array(connection);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        if (!child.isRecordExists()) {\n", NULL)) return 1;
			for(int j=0; j<child->child_columns.len; j++)
				if (stream_add_str(module_base_src, "            if (!child.isChanged", child->child_fields.values[j], "()) child.set", child->child_fields.values[j], "(this.#", child->parent_fields.values[j], ");\n", NULL)) return 1;
			if (child->child_field_sort_pos[0]!=0)
				if (stream_add_str(module_base_src, "            if (!child.isChanged", child->child_field_sort_pos, "()) child.set", child->child_field_sort_pos, "(this.#", child->field_child_name, "Array.length>0 ? this.#", child->field_child_name, "Array[this.#", child->field_child_name, "Array.length-1].get", child->child_field_sort_pos, "()+1 : 1);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "            child.save(connection);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        }\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", child->field_child_name, "Array.push(child);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        return this;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "    }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n    // Delete child\n", NULL)) return 1;
		for(int i=0; i<relation->children_len; i++) {
			metadata_relationship *child = &relation->children[i];
			if (stream_add_str(module_base_src, "    delete", child->field_child_name, "(index, connection = ORMConnection.getDefault()) {\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.get", child->field_child_name, "Array(connection)[index].delete(connection);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        this.#", child->field_child_name, "Array.splice(index, 1);\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "        return this;\n", NULL)) return 1;
			if (stream_add_str(module_base_src, "    }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "    // Create from postgres record\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static fromRecord(pgRecord) { return super.fromRecord(", relation->class_name, ", pgRecord); }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Create from JSON\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static fromJSON(json) { return super.fromJSON(", relation->class_name, ", json); }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Initialize field values\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    _initialize(pgValues, valuesChanged, recordExists) {\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		sprintf(line1, "%*d", relation->columns_len_digits, i);
		if (stream_add_rpad(module_base_src, "        this.#", relation->columns[i].field_name, relation->field_name_len_max, "")) return 1;
		if (stream_add_rpad(module_base_src, " = ORMUtil.", relation->columns[i].js_value_func, relation->js_value_func_len_max, "")) return 1;
		if (stream_add_str(module_base_src, "(pgValues[", line1, "]);\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "        if (valuesChanged!==null) {\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		sprintf(line1, "%*d", relation->columns_len_digits, i);
		if (stream_add_rpad(module_base_src, "            this.#", relation->columns[i].field_name, relation->field_name_len_max, "$changed")) return 1;
		if (stream_add_str(module_base_src, " = valuesChanged[", line1, "];\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "        } else {\n", NULL)) return 1;
	for(int i=0; i<relation->columns_len; i++) {
		sprintf(line1, "%*d", relation->columns_len_digits, i);
		if (stream_add_rpad(module_base_src, "            this.#", relation->columns[i].field_name, relation->field_name_len_max, "$changed")) return 1;
		if (stream_add_str(module_base_src, " = false;\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "        }\n", NULL)) return 1;
	for(int i=0; i<relation->parents_len; i++) {
		metadata_relationship *parent = &relation->parents[i];
		if (stream_add_rpad(module_base_src, "        this.#", parent->field_parent_name, relation->parents_field_parent_name_len_max, ""))  return 1;
		if (stream_add_str(module_base_src, " = null;", NULL)) return 1;
		if (stream_add_rpad(module_base_src, " this.#", parent->field_parent_name, relation->parents_field_parent_name_len_max, "$assigned"))  return 1;
		if (stream_add_str(module_base_src, " = false;\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "        return super._initialize(recordExists);\n    }\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "\n}\n\n", NULL)) return 1;
	//
	if (stream_add_str(module_base_src, "export class Base", relation->class_name, "Array extends RecordArray {\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Get relation\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static getRelation() { return Base", relation->class_name, ".getRelation(); }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Get any records by condition (SQL statement: select where [condition])\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static findWhere(condition = null, params = null, connection = ORMConnection.getDefault()) { return RecordArray.findWhere(", relation->class_name, "Array, connection, condition, params); }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Get all records\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static findAll(connection = ORMConnection.getDefault()) { return ", relation->class_name, "Array.findWhere(null, null, connection); }\n\n", NULL)) return 1;
	for(int i=0; i<relation->indexes_len; i++) {
		if (relation->indexes[i].unuque) continue;
		line1[0] = 0;
		line2[0] = 0;
		for(int j=0; j<relation->indexes[i].columns_len; j++) {
			char variable[256];
			if (str_format(variable, sizeof(variable), "$%d", j+1)) return 1;
			if (str_add(line1, sizeof(line1), j>0 ? ", " : "", relation->columns[relation->indexes[i].columns[j]].field_name, NULL)) return 1;
			if (str_add(line2, sizeof(line2), j>0 ? " and " : "", relation->columns[relation->indexes[i].columns[j]].name, "=", variable, NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "    // Get records by index \"",relation->indexes[i].name,"\"\n", NULL)) return 1;
		if (stream_add_str(module_base_src, "    static findBy",relation->indexes[i].id,"(",line1,", connection = ORMConnection.getDefault()) { return ", relation->class_name, "Array.findWhere(\"",line2,"\", [",line1,"], connection); }\n\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "    // Delete any records by condition (SQL statement: delete where [condition])\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static deleteWhere(condition, params = null, connection = ORMConnection.getDefault()) {\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "        if (condition===null || condition===undefined || condition==='') throw new ORMError(314, \"Parameter \\\"condition\\\" for deleteWhere method is null or empty\");\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "        return RecordArray.deleteWhere(", relation->class_name, "Array, connection, condition, params);\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    }\n\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    // Delete all records by condition (SQL statement: delete)\n", NULL)) return 1;
	if (stream_add_str(module_base_src, "    static deleteAll(connection = ORMConnection.getDefault()) { return RecordArray.deleteWhere(", relation->class_name, "Array, connection, null, null); }\n\n", NULL)) return 1;
	if (relation->sort_field_name_len_max!=-1) {
		if (stream_add_str(module_base_src, "    // Sort by fields\n", NULL)) return 1;
		for(i=0; i<relation->columns_len; i++) {
			metadata_column *column = &relation->columns[i];
			if (!column->sortable) continue;
			if (stream_add_rpad(module_base_src, "    sortBy", column->field_name, relation->sort_field_name_len_max, "")) return 1;
			if (stream_add_rpad(module_base_src, "(desc = false, nullsFirst = false) { this.sort( (record1,record2) => ORMUtil.compare", column->field_type, relation->sort_field_type_len_max, "")) return 1;
			if (stream_add_rpad(module_base_src, "(record1.get", column->field_name, relation->sort_field_name_len_max, "(), ")) return 1;
			if (stream_add_rpad(module_base_src, "record2.get", column->field_name, relation->sort_field_name_len_max, "(), ")) return 1;
			if (stream_add_str(module_base_src,  "desc, nullsFirst) ); return this; }\n", NULL)) return 1;
		}
		if (stream_add_str(module_base_src, "\n", NULL)) return 1;
	}
	if (stream_add_str(module_base_src, "}\n", NULL)) return 1;
	return 0;
}

int orm_maker_relation_refresh(PGconn *pg_conn, metadata_relation *relation) {
	log_info("making class %s for table %s.%s", relation->class_name, relation->schema, relation->name);
	char module_name[STR_SIZE], module_path[STR_SIZE];
	char module_base_name[STR_SIZE], module_base_path[STR_SIZE];
	if (orm_maker_module_name(module_name, sizeof(module_name), module_base_name, sizeof(module_base_name), relation->schema, relation->class_name)) return 1;
	if (orm_maker_module_path(module_path, sizeof(module_path), module_name)) return 1;
	if (orm_maker_module_path(module_base_path, sizeof(module_base_path), module_base_name)) return 1;
	stream module_src;
	if (stream_init(&module_src)) return 1;
	PGresult *pg_result;
	if (pg_select(&pg_result, pg_conn, 3, SQL_MODULE_SOURCE, 2, relation->schema, relation->name)) {
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
	if (orm_maker_relation_module_base(pg_conn, &module_base_src, relation)) {
		stream_free(&module_base_src);
		return 1;
	}
	res = file_write_if_changed(module_base_path, &module_base_src)>0;
	stream_free(&module_base_src);
	if (res) return 1;
	log_info("modules \"%s\",\"%s\" refreshed", module_name, module_base_name);
	return 0;
}

int orm_maker_relation_remove(char *object_schema, char *class_name) {
	char module_name[STR_SIZE], module_path[STR_SIZE];
	char module_base_name[STR_SIZE], module_base_path[STR_SIZE];
	if (orm_maker_module_name(module_name, sizeof(module_name), module_base_name, sizeof(module_base_name), object_schema, class_name)) return 1;
	if (orm_maker_module_path(module_path, sizeof(module_path), module_name)) return 1;
	if (orm_maker_module_path(module_base_path, sizeof(module_base_path), module_base_name)) return 1;
	if (file_remove(module_base_path, 1)) return 1;
	if (file_remove(module_path, 1)) return 1;
	log_info("modules \"%s\",\"%s\" removed", module_name, module_base_name);
	return 0;
}
