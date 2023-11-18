#include "util/utils.h"
#include "metadata.h"

#define SQL_RELATION \
	"select type,class_name,(schema||'.'||name)::regclass::oid oid from pgorm.orm_relation where schema=$1 and name=$2"

#define SQL_RELATION_COLUMNS \
	"select a.attnum, a.attname, t.oid, t.typcategory='A', te.typcategory, te.typname, attnotnull, otc.class_field_name\n" \
	"  from pg_attribute a\n" \
	"  join pg_type t on t.oid=a.atttypid\n" \
	"  join pg_type te on te.oid = case when t.typcategory='A' then t.typelem else t.oid end\n" \
	"  join pgorm.orm_relation_column otc on (otc.relation_schema,otc.relation_name) = (select relnamespace::regnamespace::text,relname from pg_class where oid=$1) and otc.column_name=a.attname\n" \
	"  left join pg_attrdef ad on ad.adrelid=a.attrelid and ad.adnum=a.attnum\n" \
	"  where attrelid=$1 and attnum>0\n" \
	"  order by attnum"

#define SQL_TABLE_COLUMNS_PKEY \
	"select a.attname\n" \
	"  from pg_constraint pk\n" \
	"  join unnest(pk.conkey) with ordinality as c(num, pos) on true\n" \
	"  join pg_attribute a on a.attrelid=pk.conrelid and a.attnum=c.num\n" \
	"  where pk.contype='p' and pk.conrelid=($1||'.'||$2)::regclass::oid\n" \
	"  order by c.pos"

#define SQL_VIEW_COLUMNS_PKEY \
	"select c.name" \
	"  from pgorm.orm_view_pkey pk" \
	"  join unnest(pkey_columns) with ordinality as c(name, pos) on true" \
	"  where view_schema=$1 and view_name=$2" \
	"  order by c.pos"

#define SQL_INDEXES \
	"select c.relname,i.indisunique,\n" \
	"       (select array_agg(attname order by attnum) from pg_attribute where attrelid=c.oid)\n" \
	"  from pg_index i\n" \
	"  join pg_class c on c.oid = i.indexrelid\n" \
	"  where indrelid = $1\n" \
	"  order by i.indexrelid"

#define SQL_TABLE_FKEYS \
	"select fk.class_field_parent_name,c.parent_table_schema,c.parent_table_name,c.parent_class_name,\n" \
	"       (select array_agg(otn.class_field_name order by pos)\n" \
	"         from unnest(c.parent_columns) with ordinality as columns(name, pos)\n" \
	"         left join pgorm.orm_relation_column otn on otn.relation_schema=c.parent_table_schema and otn.relation_name=c.parent_table_name and otn.column_name=columns.name\n" \
	"       ) parent_fields,\n" \
	"       fk.class_field_child_name,c.child_class_name,c.child_columns,cotc_sp.class_field_name child_field_sort_pos,\n" \
	"       (select array_agg(otn.class_field_name order by pos)\n" \
	"         from unnest(c.child_columns) with ordinality as columns(name, pos)\n" \
	"         left join pgorm.orm_relation_column otn on otn.relation_schema=c.child_table_schema and otn.relation_name=c.child_table_name and otn.column_name=columns.name\n" \
	"       ) child_fields\n" \
	"  from (\n" \
	"    select cn.nspname child_table_schema,cc.relname child_table_name,cot.class_name child_class_name,c.conkey child_columns_pos,\n" \
	"           (select array_agg(attname order by pos)\n" \
	"             from unnest(c.conkey) with ordinality as conkey(attnum, pos)\n" \
	"             join pg_attribute a on a.attrelid=c.conrelid and a.attnum=conkey.attnum\n" \
	"           ) child_columns,\n" \
	"           pn.nspname parent_table_schema,pc.relname parent_table_name,pot.class_name parent_class_name,\n" \
	"           (select array_agg(attname order by pos)\n" \
	"             from unnest(c.confkey) with ordinality as confkey(attnum, pos)\n" \
	"             join pg_attribute a on a.attrelid=c.confrelid and a.attnum=confkey.attnum\n" \
	"           ) parent_columns\n" \
	"      from pg_constraint c\n" \
	"      join pg_class cc on cc.oid=c.conrelid\n" \
	"      join pg_namespace cn on cn.oid=cc.relnamespace\n" \
	"      join pgorm.orm_relation cot on cot.schema=cn.nspname and cot.name=cc.relname and cot.type='table'\n" \
	"      join pg_class pc on pc.oid=c.confrelid\n" \
	"      join pg_namespace pn on pn.oid=pc.relnamespace\n" \
	"      join pg_constraint pk on pk.conrelid=c.confrelid and pk.contype='p' and pk.conkey=c.confkey\n" \
	"      join pgorm.orm_relation pot on pot.schema=pn.nspname and pot.name=pc.relname and pot.type='table'\n" \
	"      where c.contype='f' and (case when $1='p' then c.conrelid else c.confrelid end)=$2\n" \
	"  ) c\n" \
	"  join pgorm.orm_table_fkey fk on fk.parent_table_schema=c.parent_table_schema and fk.parent_table_name=c.parent_table_name and fk.child_table_schema=c.child_table_schema and fk.child_table_name=c.child_table_name and fk.child_columns=c.child_columns\n" \
	"  left join pgorm.orm_relation_column cotc_sp on cotc_sp.relation_schema=fk.child_table_schema and cotc_sp.relation_name=fk.child_table_name and cotc_sp.column_name=fk.child_column_sort_pos\n" \
	"  order by child_columns_pos[1],child_columns_pos[2],child_columns_pos[3],child_columns_pos[4],child_columns_pos[5]"

int metadata_column_ind(metadata_relation *relation, char *column_name) {
	for(int i=0; i<relation->columns_len; i++)
		if (!strcmp(relation->columns[i].name, column_name)) return i;
	return -1;
}

int metadata_relation_load(metadata_relation *relation, PGconn *pg_conn, char *schema, char *name) {
	if (str_copy(relation->schema, sizeof(relation->schema), schema)) return 1;
	if (str_copy(relation->name, sizeof(relation->name), name)) return 1;
	if (str_copy(relation->attnum_list, sizeof(relation->attnum_list), "{")) return 1;
	PGresult *pg_result;
	if (pg_select(&pg_result, pg_conn, 1, SQL_RELATION,  2, schema, name)) return 1;
	if (str_copy(relation->type, sizeof(relation->type), PQgetvalue(pg_result, 0, 0))) { PQclear(pg_result); return 1;}
	if (str_copy(relation->class_name, sizeof(relation->class_name), PQgetvalue(pg_result, 0, 1))) { PQclear(pg_result); return 1;}
	if (str_copy(relation->oid, sizeof(relation->oid), PQgetvalue(pg_result, 0, 2))) { PQclear(pg_result); return 1;}
	PQclear(pg_result);
	if (pg_select(&pg_result, pg_conn, 1, SQL_RELATION_COLUMNS, 1, relation->oid)) return 1;
	relation->columns_len = PQntuples(pg_result);
	if (relation->columns_len>sizeof(relation->columns)/sizeof(relation->columns[0])) {
		PQclear(pg_result);
		return log_error(68, relation->columns_len);
	}
	int i=0;
	for(; i<relation->columns_len; i++) {
		metadata_column *column = &relation->columns[i];
		if (str_add(relation->attnum_list, sizeof(relation->attnum_list), i>0 ? "," : "", PQgetvalue(pg_result, i, 0), NULL)) break;
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
	if (i!=relation->columns_len) return 1;
	if (str_add(relation->attnum_list, sizeof(relation->attnum_list), "}", NULL)) return 1;
	if (pg_select(&pg_result, pg_conn, 1, !strcmp(relation->type, "table") ? SQL_TABLE_COLUMNS_PKEY : SQL_VIEW_COLUMNS_PKEY, 2, relation->schema, relation->name)) return 1;
	relation->columns_pkey_len = PQntuples(pg_result);
	if (relation->columns_pkey_len>sizeof(relation->columns_pkey)/sizeof(relation->columns_pkey[0])) {
		PQclear(pg_result);
		return log_error(70, relation->columns_pkey_len);
	}
	for(int i=0; i<relation->columns_pkey_len; i++) {
		relation->columns_pkey[i] = metadata_column_ind(relation, PQgetvalue(pg_result, i, 0));
		if (relation->columns_pkey[i]==-1) {
			relation->columns_pkey_len = 0;
			break;
		}
	}
	PQclear(pg_result);
	if (pg_select(&pg_result, pg_conn, 1, SQL_INDEXES, 1, relation->oid)) return 1;
	int index_count = PQntuples(pg_result);
	if (index_count>sizeof(relation->indexes)/sizeof(relation->indexes[0])) {
		PQclear(pg_result);
		return log_error(69, index_count);
	}
	relation->indexes_len = 0;
	for(int i=0; i<index_count; i++) {
		metadata_index *index = &relation->indexes[relation->indexes_len];
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
			int column_ind = metadata_column_ind(relation, columns.values[index->columns_len]);
			if (column_ind==-1) break;
			index->columns[index->columns_len] = column_ind;
			if (index->columns[index->columns_len]==-1) break;
			if (str_add(index->id, sizeof(index->id), "$", relation->columns[column_ind].field_name, NULL)) {
				PQclear(pg_result);
				return 1;
			}
		}
		if (index->columns_len!=columns.len) continue;
		relation->indexes_len++;
	}
	PQclear(pg_result);
	//
	for(int fk_type=1; fk_type<=2; fk_type++) {
		if (pg_select(&pg_result, pg_conn, 1, SQL_TABLE_FKEYS, 2, fk_type==1 ? "p" : "c", relation->oid)) return 1;
		int len = PQntuples(pg_result);
		if (fk_type==1) relation->parents_len=len; else relation->children_len=len;
		if (len>METADATA_TABLE_RELATIONSHIPS_SIZE) {
			PQclear(pg_result);
			return log_error(33, len);
		}
		for(int i=0; i<len; i++) {
			metadata_relationship *relationship = fk_type==1 ? &relation->parents[i] : &relation->children[i];
			if (str_copy(relationship->field_parent_name, sizeof(relationship->field_parent_name), PQgetvalue(pg_result, i, 0))) { PQclear(pg_result); return 1; }
			if (str_copy(relationship->parent_table_schema, sizeof(relationship->parent_table_schema), PQgetvalue(pg_result, i, 1))) { PQclear(pg_result); return 1; }
			if (str_copy(relationship->parent_table_name, sizeof(relationship->parent_table_name), PQgetvalue(pg_result, i, 2))) { PQclear(pg_result); return 1; }
			if (str_copy(relationship->parent_class_name, sizeof(relationship->parent_class_name), PQgetvalue(pg_result, i, 3))) { PQclear(pg_result); return 1; }
			if (str_list_split(&relationship->parent_fields, PQgetvalue(pg_result, i, 4), 1, PQgetlength(pg_result, i, 4)-2, ','))  { PQclear(pg_result); return 1; }
			if (str_copy(relationship->field_child_name, sizeof(relationship->field_child_name), PQgetvalue(pg_result, i, 5))) { PQclear(pg_result); return 1; }
			if (str_copy(relationship->child_class_name, sizeof(relationship->child_class_name), PQgetvalue(pg_result, i, 6))) { PQclear(pg_result); return 1; }
			if (str_list_split(&relationship->child_columns, PQgetvalue(pg_result, i, 7), 1, PQgetlength(pg_result, i, 7)-2, ','))  { PQclear(pg_result); return 1; }
			if (str_copy(relationship->child_field_sort_pos, sizeof(relationship->child_field_sort_pos), PQgetvalue(pg_result, i, 8))) { PQclear(pg_result); return 1; }
			if (str_list_split(&relationship->child_fields, PQgetvalue(pg_result, i, 9), 1, PQgetlength(pg_result, i, 9)-2, ','))  { PQclear(pg_result); return 1; }
		}
		PQclear(pg_result);
	}
	relation->parents_field_parent_name_len_max    = 0;
	relation->parents_parent_table_schema_len_max  = 0;
	relation->parents_parent_table_name_len_max    = 0;
	relation->parents_parent_class_name_len_max    = 0;
	relation->parents_child_field_sort_pos_len_max = 4;

	for(int i=0; i<relation->parents_len; i++) {
		metadata_relationship *parent = &relation->parents[i];
		str_len_max(&relation->parents_field_parent_name_len_max,    parent->field_parent_name);
		str_len_max(&relation->parents_parent_table_schema_len_max,  parent->parent_table_schema);
		str_len_max(&relation->parents_parent_table_name_len_max,    parent->parent_table_name);
		str_len_max(&relation->parents_parent_class_name_len_max,    parent->parent_class_name);
		str_len_max(&relation->parents_child_field_sort_pos_len_max, parent->child_field_sort_pos);
	}
	relation->children_field_parent_name_len_max = 0;
	relation->children_field_child_name_len_max  = 0;
	relation->children_child_class_name_len_max  = 0;
	for(int i=0; i<relation->children_len; i++) {
		metadata_relationship *child = &relation->children[i];
		str_len_max(&relation->children_field_parent_name_len_max,    child->field_parent_name);
		str_len_max(&relation->children_field_child_name_len_max,     child->field_child_name);
		str_len_max(&relation->children_child_class_name_len_max,     child->child_class_name);

	}
	relation->relationships_class_name_len_max = UTILS_MAX(relation->parents_parent_class_name_len_max,relation->children_child_class_name_len_max);
	relation->field_relation_name_len_max = UTILS_MAX(relation->parents_field_parent_name_len_max,relation->children_field_child_name_len_max);
	//
	relation->field_name_len_max             = 0;
	relation->field_type_primitive_len_max   = 0;
	relation->field_type_object_len_max      = 0;
	relation->col_name_len_max               = 0;
	relation->col_type_len_max               = 0;
	relation->js_value_func_len_max          = 0;
	relation->pg_value_func_len_max          = 0;
	relation->validate_value_func_len_max    = 0;
	relation->sort_field_name_len_max        = -1;
	relation->sort_field_type_object_len_max = -1;
	for(int i=0; i<relation->columns_len; i++) {
		metadata_column *column = &relation->columns[i];
		str_len_max(&relation->field_name_len_max,           column->field_name);
		str_len_max(&relation->field_type_primitive_len_max, column->field_type_primitive!=NULL ? column->field_type_primitive : "null");
		str_len_max(&relation->field_type_object_len_max,    column->field_type_object);
		str_len_max(&relation->col_name_len_max,             column->name);
		str_len_max(&relation->col_type_len_max,             column->type);
		str_len_max(&relation->js_value_func_len_max,        column->js_value_func);
		str_len_max(&relation->pg_value_func_len_max,        column->pg_value_func);
		str_len_max(&relation->validate_value_func_len_max,  column->validate_value_func);
		if (column->sortable) {
			str_len_max(&relation->sort_field_name_len_max,        column->field_name);
			str_len_max(&relation->sort_field_type_object_len_max, column->field_type_object);
		}
	}
	relation->columns_len_digits = 0;
	for(int i=relation->columns_len-1; i; i /= 10) relation->columns_len_digits++;
	return 0;
}
