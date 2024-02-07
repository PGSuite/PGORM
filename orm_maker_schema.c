#include "util/utils.h"

#define SQL_SCHEMA \
	"create schema if not exists pgorm"

#define SQL_TABLE$ORM_RELATION \
	"create table if not exists pgorm.orm_relation(" \
	"  schema name," \
	"  name name," \
	"  primary key (schema,name)," \
    "  type varchar(16) null check (type in ('table','view'))," \
	"  class_name name not null," \
	"  unique (schema,class_name)," \
	"  module_source text not null" \
	")"

#define SQL_TABLE$ORM_RELATION_COLUMN \
	"create table if not exists pgorm.orm_relation_column(" \
	"  relation_schema name," \
	"  relation_name name," \
	"  foreign key (relation_schema,relation_name) references pgorm.orm_relation on delete cascade," \
	"  column_name name," \
	"  primary key (relation_schema,relation_name,column_name)," \
	"  class_field_name name not null" \
	")"

#define SQL_TABLE$ORM_TABLE_FKEY \
	"create table if not exists pgorm.orm_table_fkey(" \
	"  parent_table_schema name," \
	"  parent_table_name name," \
	"  foreign key (parent_table_schema,parent_table_name) references pgorm.orm_relation on delete cascade," \
	"  child_table_schema name," \
	"  child_table_name name," \
	"  foreign key (child_table_schema,child_table_name) references pgorm.orm_relation on delete cascade," \
	"  child_columns name[]," \
	"  primary key (parent_table_schema,parent_table_name,child_table_schema,child_table_name,child_columns)," \
	"  child_column_sort_pos name," \
	"  foreign key (child_table_schema,child_table_name,child_column_sort_pos) references pgorm.orm_relation_column on delete cascade," \
	"  class_field_parent_name varchar(256) not null," \
	"  unique (child_table_schema,child_table_name,class_field_parent_name)," \
	"  class_field_child_name varchar(256) not null," \
	"  unique (parent_table_schema,parent_table_name,class_field_child_name)" \
	")"

#define SQL_TABLE$ORM_VIEW_PKEY \
	"create table if not exists pgorm.orm_view_pkey(" \
	"  view_schema name," \
	"  view_name name," \
	"  primary key (view_schema,view_name)," \
	"  foreign key (view_schema,view_name) references pgorm.orm_relation on delete cascade," \
	"  pkey_columns name[]," \
	"  pkey_table_schema name," \
	"  pkey_table_name name," \
	"  foreign key (pkey_table_schema,pkey_table_name) references pgorm.orm_relation on delete set null" \
	")"

#define SQL_TABLE$ORM_ACTION \
	"create table if not exists pgorm.orm_action(" \
	"  id bigserial primary key," \
	"  timestamp timestamptz not null default now()," \
	"  object_schema name not null," \
	"  object_name name not null," \
	"  object_type varchar(100) not null check (object_type in ('relation'))," \
	"  action varchar(20) not null check (action in ('enable','disable','update_definition','alter','alter_child','drop','orm_refresh','orm_remove'))," \
	"  drop_class_name name," \
	"  orm_server inet" \
	")"

#define SQL_FUNCTION$ORM_IDENTIFIER_NAME \
	"create or replace function pgorm.orm_identifier_name(object_name name) returns varchar language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  return ( " "\n" \
	"    select string_agg(case when length(word)<=2 then upper(word) else upper(substring(word,1,1)) || lower(substring(word,2)) end, '') from" "\n" \
	"      unnest(string_to_array(object_name, '_')) word" "\n" \
	"  );" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_CHILD_COLUMN_ORDER_POS_DEFAULT \
	"create or replace function pgorm.orm_child_column_sort_pos_default(p_parent_table_schema name, p_parent_table_name name, p_child_table_schema name, p_child_table_name name) returns name language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  if substring(p_child_table_name, 1, length(p_parent_table_name))!=p_parent_table_name then" "\n" \
	"    return null;" "\n" \
	"  end if;" "\n" \
	"  return (" "\n" \
	"    select a.attname" "\n" \
	"      from pg_attribute a" "\n" \
	"      join pg_type t on t.oid=atttypid and t.typcategory='N'" "\n" \
	"      join pgorm.orm_relation_column c on c.relation_schema=p_child_table_schema and c.relation_name=p_child_table_name and c.column_name=a.attname" "\n" \
	"      where attrelid=(p_child_table_schema||'.'||p_child_table_name)::regclass::oid" "\n" \
	"        and attname in ('sort_pos','sort_position','order_pos','order_position','by_order','order_by')" "\n" \
	"      order by 1 limit 1" "\n" \
	"  );" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_CLASS_FIELD_PARENT_NAME_DEFAULT \
	"create or replace function pgorm.orm_class_field_parent_name_default(p_parent_table_schema name, p_parent_table_name name, p_child_table_schema name, p_child_table_name name, p_child_columns name[]) returns varchar language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_parent_oid oid := (p_parent_table_schema||'.'||p_parent_table_name)::regclass;" "\n" \
	"  v_parent_column name;" "\n" \
	"begin" "\n" \
	"  if array_length(p_child_columns, 1)=1 then" "\n" \
	"    v_parent_column := (select attname from pg_attribute where attrelid=v_parent_oid and attnum = (select conkey[1] from pg_constraint where contype='p' and conrelid=v_parent_oid));" "\n" \
	"    if right(p_child_columns[1], length(v_parent_column))=v_parent_column then" "\n" \
	"      return pgorm.orm_identifier_name(left(p_child_columns[1], -length(v_parent_column)));" "\n" \
	"    end if;" "\n" \
	"  end if;" "\n" \
	"  return (" "\n" \
	"    select max(pt.class_name) || '$' || string_agg(ctc.class_field_name, '$' order by pos)" "\n" \
	"      from unnest(p_child_columns) with ordinality c(name,pos)" "\n" \
	"      left join pgorm.orm_relation pt on pt.schema=p_parent_table_schema and pt.name=p_parent_table_name" "\n" \
	"      left join pgorm.orm_relation_column ctc on ctc.relation_schema=p_child_table_schema and ctc.relation_name=p_child_table_name and ctc.column_name=c.name" "\n" \
	"  );" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_CLASS_FIELD_CHILD_NAME_DEFAULT \
	"create or replace function pgorm.orm_class_field_child_name_default(p_parent_table_schema name, p_parent_table_name name, p_child_table_schema name, p_child_table_name name, p_child_columns name[]) returns varchar language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_parent_oid oid := (p_parent_table_schema||'.'||p_parent_table_name)::regclass;" "\n" \
	"  v_parent_column name;" "\n" \
	"  v_child_name name := (select class_name from pgorm.orm_relation where schema=p_child_table_schema and name=p_child_table_name);" "\n" \
	"begin" "\n" \
	"  if array_length(p_child_columns, 1)=1 then" "\n" \
	"    v_parent_column := (select attname from pg_attribute where attrelid=v_parent_oid and attnum = (select conkey[1] from pg_constraint where contype='p' and conrelid=v_parent_oid));" "\n" \
	"    if p_child_columns[1] in (p_parent_table_name||'_'||v_parent_column,p_parent_table_name||v_parent_column) then" "\n" \
	"      return v_child_name;" "\n" \
	"    end if;" "\n" \
	"  end if;" "\n" \
	"  return (" "\n" \
	"    select v_child_name || '$' || string_agg(ctc.class_field_name, '$' order by pos)" "\n" \
	"      from unnest(p_child_columns) with ordinality c(name,pos)" "\n" \
	"      left join pgorm.orm_relation_column ctc on ctc.relation_schema=p_child_table_schema and ctc.relation_name=p_child_table_name and ctc.column_name=c.name" "\n" \
	"  );" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_VIEW_PKEY_DEFAULT \
	"create or replace function pgorm.orm_view_pkey_default(in out p_view_schema name, in out p_view_name name, out p_pkey_columns name[], out p_pkey_table_schema name, out p_pkey_table_name name) returns record language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_view_oid oid := (p_view_schema||'.'||p_view_name)::regclass;" "\n" \
	"begin" "\n" \
	"  select array_agg(pkey_column_name order by pkey_column_pos),pkey_table_schema,pkey_table_name" "\n" \
	"    into p_pkey_columns, p_pkey_table_schema, p_pkey_table_name " "\n" \
	"    from (" "\n" \
	"      select relnamespace::regnamespace::text pkey_table_schema, relname pkey_table_name, array_length(conkey,1) pkey_columns_count, ta.attname pkey_column_name, array_position(conkey, ta.attnum) pkey_column_pos" "\n" \
	"        from pg_rewrite" "\n" \
	"        join pg_depend on pg_depend.objid = pg_rewrite.oid" "\n" \
	"        join pg_class on pg_class.oid = pg_depend.refobjid" "\n" \
	"        join pg_constraint on conrelid=pg_class.oid and contype='p'" "\n" \
	"        join pg_attribute ta on ta.attrelid = pg_depend.refobjid and ta.attnum=pg_depend.refobjsubid and ta.attnum = any(conkey)" "\n" \
	"        join pg_attribute va on va.attrelid = ev_class and va.attname=ta.attname" "\n" \
	"        where ev_class=v_view_oid" "\n" \
	"    ) tc" "\n" \
	"    join pgorm.orm_relation r on r.schema=tc.pkey_table_schema and r.name=tc.pkey_table_name and r.type='table'" "\n" \
	"    group by pkey_table_schema,pkey_table_name,pkey_columns_count" "\n" \
	"    having pkey_columns_count = count(1)" "\n" \
	"    order by 2,3" "\n" \
	"    limit 1;" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_RELATION_COLUMN_REFRESH \
	"create or replace procedure pgorm.orm_relation_column_refresh(relation_schema name, relation_name name) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_relation_schema name := lower(relation_schema);" "\n" \
	"  v_relation_name name := lower(relation_name);" "\n" \
	"  v_relation_oid oid := (v_relation_schema||'.'||v_relation_name)::regclass::oid;" "\n" \
	"begin" "\n" \
	"  delete from pgorm.orm_relation_column rc" "\n" \
	"    where rc.relation_schema=v_relation_schema and rc.relation_name=v_relation_name" "\n" \
	"      and rc.column_name not in (select attname from pg_attribute where attrelid=v_relation_oid and attnum>0);" "\n" \
	"  insert into pgorm.orm_relation_column(relation_schema,relation_name,column_name,class_field_name)" "\n" \
	"    select v_relation_schema,v_relation_name,attname,pgorm.orm_identifier_name(attname)" "\n" \
	"      from pg_attribute" "\n" \
	"      where attrelid=v_relation_oid and attnum>0 and exists (select from pgorm.orm_relation r where r.schema=v_relation_schema and r.name=v_relation_name)" "\n" \
	"      on conflict do nothing;" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_TABLE_FKEY_REFRESH \
	"create or replace procedure pgorm.orm_table_fkey_refresh(table_schema name, table_name name) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_table_schema name := lower(table_schema);" "\n" \
	"  v_table_name name := lower(table_name);" "\n" \
	"  v_table_oid oid := (v_table_schema||'.'||v_table_name)::regclass::oid;" "\n" \
	"  v_sql_parents text :=" "\n" \
	"    'select pn.nspname parent_table_schema,pc.relname parent_table_name,cn.nspname child_table_schema,cc.relname child_table_name," "\n" \
	"             (select array_agg(attname order by pos)" "\n" \
	"               from unnest(c.conkey) with ordinality as conkey(attnum, pos)" "\n" \
	"               join pg_attribute a on a.attrelid=c.conrelid and a.attnum=conkey.attnum" "\n" \
	"             ) child_columns" "\n" \
	"        from pg_constraint c" "\n" \
	"        join pg_class cc on cc.oid=c.conrelid" "\n" \
	"        join pg_namespace cn on cn.oid=cc.relnamespace" "\n" \
	"        join pg_class pc on pc.oid=c.confrelid" "\n" \
	"        join pg_namespace pn on pn.oid=pc.relnamespace" "\n" \
	"        join pg_constraint pk on pk.conrelid=c.confrelid and pk.contype=''p'' and pk.conkey=c.confkey" "\n" \
	"        join pgorm.orm_relation pot on pot.schema=pn.nspname and pot.name=pc.relname and pot.type=''table''" "\n" \
	"        where c.contype=''f'' and c.conrelid=$1';" "\n" \
	"begin" "\n" \
	"  execute" "\n" \
	"    'delete from pgorm.orm_table_fkey" "\n" \
	"       where child_table_schema=$2 and child_table_name=$3" "\n" \
	"         and (parent_table_schema,parent_table_name,child_table_schema,child_table_name,child_columns) not in (' || v_sql_parents|| ');'" "\n" \
	"    using v_table_oid,v_table_schema,v_table_name;" "\n" \
	"  execute" "\n" \
	"    'insert into pgorm.orm_table_fkey(parent_table_schema,parent_table_name,child_table_schema,child_table_name,child_columns,child_column_sort_pos,class_field_parent_name,class_field_child_name)" "\n" \
	"       select p.*," "\n" \
	"              pgorm.orm_child_column_sort_pos_default(parent_table_schema, parent_table_name, child_table_schema, child_table_name)," "\n" \
	"              pgorm.orm_class_field_parent_name_default(parent_table_schema, parent_table_name, child_table_schema, child_table_name, child_columns)," "\n" \
	"              pgorm.orm_class_field_child_name_default(parent_table_schema, parent_table_name, child_table_schema, child_table_name, child_columns)" "\n" \
	"         from (' || v_sql_parents|| ') p" "\n" \
	"         on conflict (parent_table_schema,parent_table_name,child_table_schema,child_table_name,child_columns) do nothing;'" "\n" \
	"    using v_table_oid;" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_RELATION_ENABLE \
	"create or replace procedure pgorm.orm_relation_enable(relation_schema name, relation_name name, class_name name default null) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_relation_schema name := lower(relation_schema);" "\n" \
	"  v_relation_name name := lower(relation_name);" "\n" \
	"  v_relation_oid oid=(v_relation_schema||'.'||v_relation_name)::regclass::oid;" "\n" \
    "  v_relation_type varchar(10) := (select case when relkind='r' then 'table' when relkind='v' then 'view' else null end from pg_class where oid=v_relation_oid);" "\n" \
	"  v_class_name name := coalesce(class_name, pgorm.orm_identifier_name(relation_name));" "\n" \
	"  v_newline char := chr(10);" "\n" \
	"  v_module_source text :=" "\n" \
	"    '// Module can be edited by changing field \"module_source\" of table \"pgorm.orm_relation\"'||v_newline||v_newline||" "\n" \
	"    'import { Base'||v_class_name||',Base'||v_class_name||'Array } from \"/orm/'||v_relation_schema||'/base/Base'||v_class_name||'.js\"'||v_newline||v_newline||" "\n" \
	"    'export class '||v_class_name||' extends Base'||v_class_name||' {'||v_newline||v_newline||'}'||v_newline||v_newline||" "\n" \
	"    'export class '||v_class_name||'Array extends Base'||v_class_name||'Array {'||v_newline||v_newline||'}';" "\n" \
	"begin" "\n" \
	"  if v_relation_type is null then" "\n" \
	"    raise exception 'Relation %.% not exists', v_relation_schema, v_relation_name;" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_relation(schema, name, type, class_name, module_source)" "\n" \
	"    values (v_relation_schema, v_relation_name, v_relation_type, v_class_name, v_module_source)" "\n" \
	"    on conflict do nothing;" "\n" \
	"  call pgorm.orm_relation_column_refresh(v_relation_schema, v_relation_name);" "\n" \
	"  if v_relation_type='table' then" "\n" \
	"    call pgorm.orm_table_fkey_refresh(v_relation_schema, v_relation_name);" "\n" \
	"  else" "\n" \
	"    insert into pgorm.orm_view_pkey(view_schema, view_name, pkey_columns, pkey_table_schema, pkey_table_name)" "\n" \
	"      select (pgorm.orm_view_pkey_default(v_relation_schema, v_relation_name)).*" \
	"      on conflict do nothing;" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_action(object_schema, object_name, object_type, action)" "\n" \
	"    values(v_relation_schema, v_relation_name, 'relation', 'enable');" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_RELATION_ENABLE_SIMPLE \
	"create or replace procedure pgorm.orm_relation_enable(relation_name name) language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  call pgorm.orm_relation_enable(current_schema(), relation_name);" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_RELATION_DISABLE \
	"create or replace procedure pgorm.orm_relation_disable(relation_schema name, relation_name name) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_relation_schema name := lower(relation_schema);" "\n" \
	"  v_relation_name name := lower(relation_name);" "\n" \
	"begin" "\n" \
	"  insert into pgorm.orm_action(object_schema, object_name, object_type, action, drop_class_name)" "\n" \
	"    select r.schema, r.name, 'relation', 'disable', r.class_name from pgorm.orm_relation r" "\n" \
	"      where r.schema=v_relation_schema and r.name=v_relation_name;" "\n" \
	"  delete from pgorm.orm_relation r where r.schema=v_relation_schema and r.name=v_relation_name;" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_RELATION_DISABLE_SIMPLE \
	"create or replace procedure pgorm.orm_relation_disable(relation_name name) language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  call pgorm.orm_relation_disable(current_schema(), relation_name);" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_RECORD_SAVE \
	"create or replace procedure pgorm.orm_record_save(inout orm_record jsonb) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_sql text;" "\n" \
	"  v_orm jsonb;" "\n" \
	"begin" "\n" \
	"  if orm_record is null then return; end if;" "\n" \
	"  if not (orm_record ? '#orm' ) then raise exception 'Record JSON does not contain key ''#orm'''; end if;" "\n" \
	"  v_orm := orm_record->'#orm';" "\n" \
	"  if v_orm->'record_exists' then" "\n" \
	"	 if jsonb_array_length(v_orm->'columns_changed')=0 then return; end if;" "\n" \
	"    v_sql:='update '||(v_orm->>'relation')||' as relation set '||(select string_agg(c||'=record.'||c, ',') from jsonb_array_elements_text(v_orm->'columns_changed') c)||" "\n" \
	"      ' from jsonb_populate_record(null::'||(v_orm->>'relation')||',$1) record where '||" "\n" \
	"      (select string_agg('relation.'||c||'=record.'||c, ' and ') from jsonb_array_elements_text(v_orm->'columns_primary_key') c);" "\n" \
	"  else" "\n" \
	"    v_sql := (select string_agg(c, ',') from jsonb_array_elements_text(v_orm->'columns_changed') c);" "\n" \
	"    v_sql:='insert into '||(v_orm->>'relation')||' as relation ('||v_sql||') select '||v_sql||' from jsonb_populate_record(null::'||(v_orm->>'relation')||',$1)';" "\n" \
	"  end if;" "\n" \
	"  v_sql := v_sql||' returning to_jsonb(relation)';" "\n" \
	"  execute v_sql using orm_record into orm_record;" "\n" \
	"  orm_record := orm_record||jsonb_build_object('#orm', v_orm||jsonb_build_object('record_exists', true, 'columns_changed', array[]::text[]));" "\n" \
	"end; $$;"

#define SQL_PROCEDURE$ORM_RECORD_SET_VALUE \
	"create or replace procedure pgorm.orm_record_set_value(inout orm_record jsonb, column_ name, value anyelement) language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  if orm_record is null then return; end if;" "\n" \
	"  if not (orm_record ? '#orm' ) then raise exception 'Record JSON does not contain key ''#orm'''; end if;" "\n" \
	"  orm_record := jsonb_set(orm_record, array[column_], to_jsonb(value));" "\n" \
	"  if not (orm_record->'#orm'->'columns_changed' ? column_) then" "\n" \
	"    orm_record := jsonb_set(orm_record, array['#orm','columns_changed'], (orm_record->'#orm'->'columns_changed')||to_jsonb(column_));" "\n" \
	"  end if;" "\n" \
	"end; $$;"

#define SQL_FUNCTION$ORM_RELATION_FN_UPDATE \
	"create or replace function pgorm.orm_relation_fn_update() returns trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  if old.schema!=new.schema or old.name!=new.name or old.class_name!=new.class_name then" "\n" \
	"    raise exception 'Not allowed to change schema,name or class_name, use pgorm.orm_relation_disable/enable';" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_action(object_schema,object_name,object_type,action) values(new.schema,new.name,'relation','update_definition');" "\n" \
	"  return null;" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_RELATION_COLUMN_FN_UPDATE \
	"create or replace function pgorm.orm_relation_column_fn_update() returns trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  if old.schema!=new.schema or old.relation_name!=new.relation_name or old.column_name!=new.column_name then" "\n" \
	"    raise exception 'Not allowed to change schema, table_name or column_name';" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_action(object_schema,object_name,object_type,action) values(new.schema,new.relation_name,'relation','update_definition');" "\n" \
	"  return null;" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_TABLE_FKEY_FN_UPDATE \
	"create or replace function pgorm.orm_table_fkey_fn_update() returns trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  if old.parent_table_schema!=new.parent_table_schema or old.parent_table_name!=new.parent_table_name or old.child_table_schema!=new.child_table_schema or old.child_table_name!=new.child_table_name or old.child_columns!=new.child_columns then" "\n" \
	"    raise exception 'Not allowed to change parent_table_schema, parent_table_name, child_table_schema, child_table_name or child_columns';" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_action(object_schema,object_name,object_type,action) values" "\n" \
	"    (new.parent_table_schema,new.parent_table_name,'relation','update_definition')," "\n" \
	"    (new.child_table_schema,new.child_table_name,'relation','update_definition');" "\n" \
	"  return null;" "\n" \
	"end; $$"

#define SQL_FUNCTION$EVENT_FN_ALTER_TABLE \
	"create or replace function pgorm.event_fn_alter_table() returns event_trigger language plpgsql security definer as $$" "\n" \
	"declare" "\n" \
	"  rec record;" "\n" \
	"begin" "\n" \
	"  for rec in (" "\n" \
	"    select t.schema,t.name" "\n" \
	"      from pg_event_trigger_ddl_commands() e" "\n" \
	"      join pg_class c on c.oid=e.objid" "\n" \
	"      join pgorm.orm_relation t on t.schema=e.schema_name and t.name=c.relname and t.type='table'" "\n" \
	"      where e.object_type in ('table','table column')" "\n" \
	"   ) loop" "\n" \
	"     call pgorm.orm_table_column_refresh(rec.schema,rec.table_name);" "\n" \
	"     call pgorm.orm_table_fkey_refresh(rec.schema,rec.table_name);" "\n" \
	"     insert into pgorm.orm_action(object_schema,object_name,object_type,action)" "\n" \
	"       select rec.schema,rec.name,'table','alter'" "\n" \
	"       union all" "\n" \
	"       select distinct parent_table_schema,parent_table_name,'table','alter_child' from pgorm.orm_table_fkey where child_table_schema=rec.schema and child_table_name=rec.table_name;" "\n" \
	"   end loop;" "\n" \
	"end; $$"

#define SQL_FUNCTION$EVENT_FN_DROP_OBJECTS \
	"create or replace function pgorm.event_fn_drop_objects() returns event_trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  insert into pgorm.orm_action(object_schema,object_name,object_type,action,drop_class_name)" "\n" \
	"    select r.schema,r.name,'relation','drop',r.class_name" "\n" \
	"      from pg_event_trigger_dropped_objects() e" "\n" \
	"      join pgorm.orm_relation r on r.schema=e.schema_name and r.name=e.object_name and e.object_type in ('table','view');" "\n" \
	"  delete from pgorm.orm_relation" "\n" \
	"    where (schema,name) in (select schema_name,object_name from pg_event_trigger_dropped_objects() where object_type in ('table','view'));" "\n" \
	"end; $$"

#define SQL_BLOCK$CREATE_TRIGGERS \
	"do $$ begin" "\n" \
	"  if not exists (select from pg_event_trigger where evtname='pgorm_event_tg_alter_table') then" "\n" \
	"    create event trigger pgorm_event_tg_alter_table on ddl_command_end when tag in ('ALTER TABLE','COMMENT') execute procedure pgorm.event_fn_alter_table();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_event_trigger where evtname='pgorm_event_tg_drop_objects') then" "\n" \
	"    create event trigger pgorm_event_tg_drop_objects on sql_drop when tag in ('DROP TABLE','DROP SCHEMA') execute function pgorm.event_fn_drop_objects();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_trigger where tgname='pgorm_relation_tg_update') then" "\n" \
	"    create trigger pgorm_relation_tg_update after update on pgorm.orm_relation for each row execute function pgorm.orm_relation_fn_update();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_trigger where tgname='pgorm_relation_column_tg_update') then" "\n" \
	"    create trigger pgorm_relation_column_tg_update after update on pgorm.orm_relation_column for each row execute function pgorm.orm_relation_column_fn_update();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_trigger where tgname='pgorm_table_fkey_tg_update') then" "\n" \
	"    create trigger pgorm_table_fkey_tg_update after update on pgorm.orm_table_fkey for each row execute function pgorm.orm_table_fkey_fn_update();" "\n" \
	"  end if;" "\n" \
	"end $$"

const char *SQL_LIST[] = {
	SQL_SCHEMA,
	SQL_TABLE$ORM_RELATION,
	SQL_TABLE$ORM_RELATION_COLUMN,
	SQL_TABLE$ORM_TABLE_FKEY,
	SQL_TABLE$ORM_VIEW_PKEY,
	SQL_TABLE$ORM_ACTION,
	SQL_FUNCTION$ORM_IDENTIFIER_NAME,
	SQL_FUNCTION$ORM_CHILD_COLUMN_ORDER_POS_DEFAULT,
	SQL_FUNCTION$ORM_CLASS_FIELD_PARENT_NAME_DEFAULT,
	SQL_FUNCTION$ORM_CLASS_FIELD_CHILD_NAME_DEFAULT,
	SQL_FUNCTION$ORM_VIEW_PKEY_DEFAULT,
	SQL_PROCEDURE$ORM_RELATION_COLUMN_REFRESH,
	SQL_PROCEDURE$ORM_TABLE_FKEY_REFRESH,
	SQL_PROCEDURE$ORM_RELATION_ENABLE,
	SQL_PROCEDURE$ORM_RELATION_ENABLE_SIMPLE,
	SQL_PROCEDURE$ORM_RELATION_DISABLE,
	SQL_PROCEDURE$ORM_RELATION_DISABLE_SIMPLE,
	SQL_PROCEDURE$ORM_RECORD_SAVE,
	SQL_PROCEDURE$ORM_RECORD_SET_VALUE,
	SQL_FUNCTION$ORM_RELATION_FN_UPDATE,
    SQL_FUNCTION$ORM_RELATION_COLUMN_FN_UPDATE,
	SQL_FUNCTION$ORM_TABLE_FKEY_FN_UPDATE,
	SQL_FUNCTION$EVENT_FN_ALTER_TABLE,
	SQL_FUNCTION$EVENT_FN_DROP_OBJECTS,
	SQL_BLOCK$CREATE_TRIGGERS
};

int orm_maker_schema_verify(PGconn *pg_conn) {
	for(int i=0; i<sizeof(SQL_LIST)/sizeof(SQL_LIST[0]); i++)
		if (pg_execute(pg_conn, SQL_LIST[i], 0)) return 1;
	log_info("schema \"pgorm\" verified");
	return 0;
}
