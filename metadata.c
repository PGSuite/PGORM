#include "util/utils.h"
#include "metadata.h"

#define SQL_TABLE_CLASS_NAME \
	"select class_name from pgorm.orm_table where schema=$1 and table_name=$2"

#define SQL_OID \
	"select oid from pg_class where relnamespace=(select oid from pg_namespace where nspname=$1) and relname=$2"

#define SQL_TABLE_COLUMNS \
	"select a.attnum, a.attname, t.oid, t.typcategory='A', te.typcategory, te.typname, attnotnull, otc.class_field_name\n" \
	"  from pg_attribute a\n" \
	"  join pg_type t on t.oid=a.atttypid\n" \
	"  join pg_type te on te.oid = case when t.typcategory='A' then t.typelem else t.oid end\n" \
	"  join pgorm.orm_table_column otc on (otc.schema,otc.table_name) = (select relnamespace::regnamespace::text,relname from pg_class where oid=$1) and otc.column_name=a.attname\n" \
	"  left join pg_attrdef ad on ad.adrelid=a.attrelid and ad.adnum=a.attnum\n" \
	"  where attrelid=$1 and attnum>0\n" \
	"  order by attnum"

#define SQL_TABLE_COLUMNS_REFRESH \
	"call pgorm.orm_table_columns_refresh($1, $2)"

#define SQL_TABLE_COLUMNS_PK \
	"select ca.attname\n" \
	"  from pg_constraint c\n" \
	"  join pg_class cc on cc.relkind='i' and cc.relnamespace=c.connamespace and cc.relname=c.conname\n" \
	"  join pg_attribute ca on ca.attrelid=cc.oid\n" \
	"  where c.contype='p' and c.conrelid=$1\n" \
	"  order by ca.attnum"

#define SQL_INDEXES \
	"select c.relname,i.indisunique,\n" \
	"       (select array_agg(attname order by attnum) from pg_attribute where attrelid=c.oid)\n" \
	"  from pg_index i\n" \
	"  join pg_class c on c.oid = i.indexrelid\n" \
	"  where indrelid = $1\n" \
	"  order by i.indexrelid"

#define SQL_TABLE_RELATIONS \
	"select r.class_field_parent_name,c.parent_schema,c.parent_table_name,c.parent_class_name,\n" \
	"       (select array_agg(otn.class_field_name order by pos)\n" \
	"         from unnest(c.parent_columns) with ordinality as columns(name, pos)\n" \
	"         left join pgorm.orm_table_column otn on otn.schema=c.parent_schema and otn.table_name=c.parent_table_name and otn.column_name=columns.name\n" \
	"       ) parent_fields,\n" \
	"       r.class_field_child_name,c.child_class_name,c.child_columns,cotc_sp.class_field_name child_field_sort_pos,\n" \
	"       (select array_agg(otn.class_field_name order by pos)\n" \
	"         from unnest(c.child_columns) with ordinality as columns(name, pos)\n" \
	"         left join pgorm.orm_table_column otn on otn.schema=c.child_schema and otn.table_name=c.child_table_name and otn.column_name=columns.name\n" \
	"       ) child_fields\n" \
	"  from (\n" \
	"    select cn.nspname child_schema,cc.relname child_table_name,cot.class_name child_class_name,c.conkey child_columns_pos,\n" \
	"           (select array_agg(attname order by pos)\n" \
	"             from unnest(c.conkey) with ordinality as conkey(attnum, pos)\n" \
	"             join pg_attribute a on a.attrelid=c.conrelid and a.attnum=conkey.attnum\n" \
	"           ) child_columns,\n" \
	"           pn.nspname parent_schema,pc.relname parent_table_name,pot.class_name parent_class_name,\n" \
	"           (select array_agg(attname order by pos)\n" \
	"             from unnest(c.confkey) with ordinality as confkey(attnum, pos)\n" \
	"             join pg_attribute a on a.attrelid=c.confrelid and a.attnum=confkey.attnum\n" \
	"           ) parent_columns\n" \
	"      from pg_constraint c\n" \
	"      join pg_class cc on cc.oid=c.conrelid\n" \
	"      join pg_namespace cn on cn.oid=cc.relnamespace\n" \
	"      join pgorm.orm_table cot on cot.schema=cn.nspname and cot.table_name=cc.relname\n" \
	"      join pg_class pc on pc.oid=c.confrelid\n" \
	"      join pg_namespace pn on pn.oid=pc.relnamespace\n" \
	"      join pg_constraint pk on pk.conrelid=c.confrelid and pk.contype='p' and pk.conkey=c.confkey\n" \
	"      join pgorm.orm_table pot on pot.schema=pn.nspname and pot.table_name=pc.relname\n" \
	"      where c.contype='f' and (case when $1='p' then c.conrelid else c.confrelid end)=$2\n" \
	"  ) c\n" \
	"  join pgorm.orm_table_relation r on r.parent_schema=c.parent_schema and r.parent_table_name=c.parent_table_name and r.child_schema=c.child_schema and r.child_table_name=c.child_table_name and r.child_columns=c.child_columns\n" \
	"  left join pgorm.orm_table_column cotc_sp on cotc_sp.schema=r.child_schema and cotc_sp.table_name=r.child_table_name and cotc_sp.column_name=r.child_column_sort_pos\n" \
	"  order by child_columns_pos[1],child_columns_pos[2],child_columns_pos[3],child_columns_pos[4],child_columns_pos[5]"

int metadata_column_ind(metadata_table *metainfo_table, char *column_name) {
	for(int i=0; i<metainfo_table->columns_len; i++)
		if (!strcmp(metainfo_table->columns[i].name, column_name)) return i;
	return -1;
}

int metadata_table_load(metadata_table *table, PGconn *pg_conn, char *schema, char *table_name) {
	if (pg_execute(pg_conn, SQL_TABLE_COLUMNS_REFRESH, 2, schema, table_name)) return 1;
	if (str_copy(table->schema, sizeof(table->schema), schema)) return 1;
	if (str_copy(table->name, sizeof(table->name), table_name)) return 1;
	if (str_copy(table->attnum_list, sizeof(table->attnum_list), "{")) return 1;
	if (pg_select_str(table->class_name, sizeof(table->class_name), pg_conn, SQL_TABLE_CLASS_NAME, 2, schema, table_name)) return 1;
	if (pg_select_str(table->oid, sizeof(table->oid), pg_conn, SQL_OID, 2, schema, table_name)) return 1;
	PGresult *pg_result;
	if (pg_select(&pg_result, pg_conn, 1, SQL_TABLE_COLUMNS, 1, table->oid)) return 1;
	table->columns_len = PQntuples(pg_result);
	if (table->columns_len>sizeof(table->columns)/sizeof(table->columns[0])) {
		PQclear(pg_result);
		return log_error(68, table->columns_len);
	}
	int i=0;
	for(; i<table->columns_len; i++) {
		metadata_column *column = &table->columns[i];
		if (str_add(table->attnum_list, sizeof(table->attnum_list), i>0 ? "," : "", PQgetvalue(pg_result, i, 0), NULL)) break;
		if (str_copy(column->name, sizeof(column->name), PQgetvalue(pg_result, i, 1))) break;
		int type_oid = atoi(PQgetvalue(pg_result, i, 2));
		column->array = pg_str_to_bool(PQgetvalue(pg_result, i, 3));
		char category = PQgetvalue(pg_result, i, 4)[0];
		if (str_copy(column->type, sizeof(column->type), PQgetvalue(pg_result, i, 5))) break;
		column->not_null = pg_str_to_bool(PQgetvalue(pg_result, i, 6));
		if (str_copy(column->field_name, sizeof(column->field_name), PQgetvalue(pg_result, i, 7))) break;
		void (*anyfunc)(void);
		if (req_pgorm_response_add_value_func(&anyfunc, type_oid, column->name)) break;
		column->js_value_func = column->array ? "jsArrayPrimitive" : "jsPrimitive";
		column->pg_value_func = column->array ? "pgArrayPrimitive" : "pgPrimitive";
		if (category=='N') {
			column->field_type_primitive = "number";
			column->field_type_object    = "Number";
			column->sortable = !column->array;
		} else if (category=='B') {
			column->field_type_primitive = "boolean";
			column->field_type_object    = "Boolean";
			column->sortable = !column->array;
		} else if (category=='S' || !strcmp(column->type,"jsonpath")) {
			column->field_type_primitive = "string";
			column->field_type_object    = "String";
		} else if (category=='D') {
			column->field_type_primitive = NULL;
			column->field_type_object    = "Date";
			column->js_value_func           = column->array ? "jsArrayDate" : "jsDate";
			if      (!strcmp(column->type,"date"))        column->pg_value_func = column->array ? "pgArrayDate"        : "pgDate";
			else if (!strcmp(column->type,"time"))        column->pg_value_func = column->array ? "pgArrayTime"        : "pgTime";
			else if (!strcmp(column->type,"timetz"))      column->pg_value_func = column->array ? "pgArrayTimetz"      : "pgTimetz";
			else if (!strcmp(column->type,"timestamp"))   column->pg_value_func = column->array ? "pgArrayTimestamp"   : "pgTimestamp";
			else if (!strcmp(column->type,"timestamptz")) column->pg_value_func = column->array ? "pgArrayTimestamptz" : "pgTimestamptz";
			else {
				log_error(71, column->type);
				break;
			}
		} else if (!strcmp(column->type,"interval")) {
			column->field_type_primitive = "number";
			column->field_type_object    = "Number";
			column->pg_value_func           = column->array ? "pgArrayInterval" : "pgInterval";
		} else if (!strcmp(column->type,"bytea")) {
			column->field_type_primitive = NULL;
			column->field_type_object    = "Uint8Array";
			column->js_value_func           = column->array ? "jsArrayUint8Array" : "jsUint8Array";
			column->pg_value_func           = column->array ? "pgArrayBytea" : "pgBytea";
		} else if (!strcmp(column->type,"json") || !strcmp(column->type,"jsonb")) {
			column->field_type_primitive = NULL;
			column->field_type_object    = "Object";
			column->js_value_func           = column->array ? "jsArrayPrimitive" : "jsPrimitive";
			column->pg_value_func           = column->array ? "pgArrayJSON"      : "pgJSON";
		} else if (!strcmp(column->type,"xml")) {
			column->field_type_primitive = NULL;
			column->field_type_object    = "XMLDocument";
			column->js_value_func           = column->array ? "jsArrayXML" : "jsXMLDocument";
			column->pg_value_func           = column->array ? "pgArrayXML" : "pgXML";
		} else {
			log_error(71, column->type);
			break;
		}
		column->validate_value_func[0] = 0;
		if (str_add(column->validate_value_func, sizeof(column->validate_value_func), "validate", column->array ? "Array" : "", column->field_type_object, NULL)) break;
		column->sortable = !column->array && (!strcmp(column->field_type_object,"Number") || !strcmp(column->field_type_object,"String") || !strcmp(column->field_type_object,"Boolean") || !strcmp(column->field_type_object,"Date"));
	}
	PQclear(pg_result);
	if (i!=table->columns_len) return 1;
	if (str_add(table->attnum_list, sizeof(table->attnum_list), "}", NULL)) return 1;
	if (pg_select(&pg_result, pg_conn, 1, SQL_TABLE_COLUMNS_PK, 1, table->oid)) return 1;
	table->columns_pk_len = PQntuples(pg_result);
	if (table->columns_pk_len>sizeof(table->columns_pk)/sizeof(table->columns_pk[0])) {
		PQclear(pg_result);
		return log_error(70, table->columns_pk_len);
	}
	for(int i=0; i<table->columns_pk_len; i++) {
		table->columns_pk[i] = metadata_column_ind(table, PQgetvalue(pg_result, i, 0));
		if (table->columns_pk[i]==-1) {
			table->columns_pk_len = 0;
			break;
		}
	}
	PQclear(pg_result);
	if (pg_select(&pg_result, pg_conn, 1, SQL_INDEXES, 1, table->oid)) return 1;
	int index_count = PQntuples(pg_result);
	if (index_count>sizeof(table->indexes)/sizeof(table->indexes[0])) {
		PQclear(pg_result);
		return log_error(69, index_count);
	}
	table->indexes_len = 0;
	for(int i=0; i<index_count; i++) {
		metadata_index *index = &table->indexes[table->indexes_len];
		if (str_copy(index->name, sizeof(index->name), PQgetvalue(pg_result, i, 0))) {
			PQclear(pg_result);
			return 1;
		}
		index->unuque = PQgetvalue(pg_result, i, 1)[0]=='t';
		str_list columns;
		if(str_list_split(&columns, PQgetvalue(pg_result, i, 2), 1, PQgetlength(pg_result, i, 2)-2, ',')) {
			PQclear(pg_result);
			return 1;
		}
		if (columns.len>sizeof(index->columns)/sizeof(index->columns[0])) {
			PQclear(pg_result);
			return log_error(73, columns.len);
		}
		index->columns_len = 0;
		index->id[0] = 0;
		for(; index->columns_len<columns.len; index->columns_len++ ) {
			int column_ind = metadata_column_ind(table, columns.values[index->columns_len]);
			if (column_ind==-1) break;
			index->columns[index->columns_len] = column_ind;
			if (index->columns[index->columns_len]==-1) break;
			if (str_add(index->id, sizeof(index->id), "$", table->columns[column_ind].field_name, NULL)) {
				PQclear(pg_result);
				return 1;
			}
		}
		if (index->columns_len!=columns.len) continue;
		table->indexes_len++;
	}
	PQclear(pg_result);
	//
	for(int rt=1; rt<=2; rt++) {
		if (pg_select(&pg_result, pg_conn, 1, SQL_TABLE_RELATIONS, 2, rt==1 ? "p" : "c", table->oid)) return 1;
		int len = PQntuples(pg_result);
		if (rt==1) table->parents_len=len; else table->children_len=len;
		if (len>METADATA_TABLE_RELATIONS_SIZE) {
			PQclear(pg_result);
			return log_error(33, len);
		}
		for(int i=0; i<len; i++) {
			metadata_relation *relation = rt==1 ? &table->parents[i] : &table->children[i];
			if (str_copy(relation->field_parent_name, sizeof(relation->field_parent_name), PQgetvalue(pg_result, i, 0))) { PQclear(pg_result); return 1; }
			if (str_copy(relation->parent_schema, sizeof(relation->parent_schema), PQgetvalue(pg_result, i, 1))) { PQclear(pg_result); return 1; }
			if (str_copy(relation->parent_table_name, sizeof(relation->parent_table_name), PQgetvalue(pg_result, i, 2))) { PQclear(pg_result); return 1; }
			if (str_copy(relation->parent_class_name, sizeof(relation->parent_class_name), PQgetvalue(pg_result, i, 3))) { PQclear(pg_result); return 1; }
			if (str_list_split(&relation->parent_fields, PQgetvalue(pg_result, i, 4), 1, PQgetlength(pg_result, i, 4)-2, ','))  { PQclear(pg_result); return 1; }
			if (str_copy(relation->field_child_name, sizeof(relation->field_child_name), PQgetvalue(pg_result, i, 5))) { PQclear(pg_result); return 1; }
			if (str_copy(relation->child_class_name, sizeof(relation->child_class_name), PQgetvalue(pg_result, i, 6))) { PQclear(pg_result); return 1; }
			if (str_list_split(&relation->child_columns, PQgetvalue(pg_result, i, 7), 1, PQgetlength(pg_result, i, 7)-2, ','))  { PQclear(pg_result); return 1; }
			if (str_copy(relation->child_field_sort_pos, sizeof(relation->child_field_sort_pos), PQgetvalue(pg_result, i, 8))) { PQclear(pg_result); return 1; }
			if (str_list_split(&relation->child_fields, PQgetvalue(pg_result, i, 9), 1, PQgetlength(pg_result, i, 9)-2, ','))  { PQclear(pg_result); return 1; }
		}
		PQclear(pg_result);
	}
	table->parents_field_parent_name_len_max = 0;
	table->parents_parent_schema_len_max     = 0;
	table->parents_parent_table_name_len_max = 0;
	table->parents_parent_class_name_len_max = 0;
	for(int i=0; i<table->parents_len; i++) {
		metadata_relation *parent = &table->parents[i];
		str_len_max(&table->parents_field_parent_name_len_max, parent->field_parent_name);
		str_len_max(&table->parents_parent_schema_len_max,     parent->parent_schema);
		str_len_max(&table->parents_parent_table_name_len_max, parent->parent_table_name);
		str_len_max(&table->parents_parent_class_name_len_max, parent->parent_class_name);
	}
	table->children_field_parent_name_len_max = 0;
	table->children_field_child_name_len_max  = 0;
	table->children_child_class_name_len_max  = 0;
	for(int i=0; i<table->children_len; i++) {
		metadata_relation *child = &table->children[i];
		str_len_max(&table->children_field_parent_name_len_max, child->field_parent_name);
		str_len_max(&table->children_field_child_name_len_max,  child->field_child_name);
		str_len_max(&table->children_child_class_name_len_max,  child->child_class_name);
	}
	table->relation_class_name_len_max = UTILS_MAX(table->parents_parent_class_name_len_max,table->children_child_class_name_len_max);
	table->field_relation_name_len_max = UTILS_MAX(table->parents_field_parent_name_len_max,table->children_field_child_name_len_max);
	//
	table->field_name_len_max             = 0;
	table->field_type_primitive_len_max   = 0;
	table->field_type_object_len_max      = 0;
	table->col_name_len_max               = 0;
	table->col_type_len_max               = 0;
	table->js_value_func_len_max          = 0;
	table->pg_value_func_len_max          = 0;
	table->validate_value_func_len_max    = 0;
	table->sort_field_name_len_max        = -1;
	table->sort_field_type_object_len_max = -1;
	for(int i=0; i<table->columns_len; i++) {
		metadata_column *column = &table->columns[i];
		str_len_max(&table->field_name_len_max,           column->field_name);
		str_len_max(&table->field_type_primitive_len_max, column->field_type_primitive!=NULL ? column->field_type_primitive : "null");
		str_len_max(&table->field_type_object_len_max,    column->field_type_object);
		str_len_max(&table->col_name_len_max,             column->name);
		str_len_max(&table->col_type_len_max,             column->type);
		str_len_max(&table->js_value_func_len_max,        column->js_value_func);
		str_len_max(&table->pg_value_func_len_max,        column->pg_value_func);
		str_len_max(&table->validate_value_func_len_max,  column->validate_value_func);
		if (column->sortable) {
			str_len_max(&table->sort_field_name_len_max,        column->field_name);
			str_len_max(&table->sort_field_type_object_len_max, column->field_type_object);
		}
	}
	table->columns_len_digits = 0;
	for(int i=table->columns_len-1; i; i /= 10) table->columns_len_digits++;
	return 0;
}
