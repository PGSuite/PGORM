#include "util/utils.h"

#define SQL_SCHEMA \
	"create schema if not exists pgorm"

#define SQL_TABLE$ORM_TABLE \
	"create table if not exists pgorm.orm_table(" \
	"  schema name," \
	"  table_name name," \
	"  primary key (schema,table_name)," \
	"  readonly boolean not null check (not readonly)," \
	"  class_name name not null," \
	"  unique (schema,class_name)," \
	"  module_source text not null" \
	")"

#define SQL_TABLE$ORM_TABLE_COLUMN \
	"create table if not exists pgorm.orm_table_column(" \
	"  schema name," \
	"  table_name name," \
	"  column_name name," \
	"  primary key (schema,table_name,column_name)," \
	"  foreign key (schema,table_name) references pgorm.orm_table(schema,table_name) on delete cascade," \
	"  class_field_name name not null" \
	")"

#define SQL_TABLE$ORM_ACTION \
	"create table if not exists pgorm.orm_action(" \
	"  id bigserial primary key," \
	"  timestamp timestamptz not null default now()," \
	"  schema name not null," \
	"  object_name name not null," \
	"  object_type varchar(100) not null check (object_type in ('TABLE'))," \
	"  class_name name not null," \
	"  action varchar(20) not null check (action in ('ENABLE','DISABLE','UPDATE','ALTER','DROP','ORM_REFRESH','ORM_REMOVE'))," \
	"  orm_server varchar(20)" \
	")"

#define SQL_FUNCTION$ORM_IDENTIFIER_NAME \
	"create or replace function pgorm.orm_identifier_name(object_name name) returns varchar language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  return ( " "\n" \
	"    select string_agg(case when length(word)<=3 then upper(word) else upper(substring(word,1,1)) || lower(substring(word,2)) end, '') from" "\n" \
	"      unnest(string_to_array(object_name, '_')) word" "\n" \
	"  );" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_TABLE_COLUMNS_REFRESH \
	"create or replace procedure pgorm.orm_table_columns_refresh(schema name, table_name name) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_schema name := lower(schema);" "\n" \
	"  v_table_name name := lower(table_name);" "\n" \
	"  v_table_oid oid := (v_schema||'.'||v_table_name)::regclass::oid;" "\n" \
	"begin" "\n" \
	"  delete from pgorm.orm_table_column tc" "\n" \
	"    where (tc.schema,tc.table_name,tc.column_name) not in (select v_schema,v_table_name,attname from pg_attribute where attrelid=v_table_oid and attnum>0);" "\n" \
	"  insert into pgorm.orm_table_column(schema,table_name,column_name,class_field_name)" "\n" \
	"    select v_schema,v_table_name,attname,pgorm.orm_identifier_name(attname)" "\n" \
	"      from pg_attribute" "\n" \
	"      where attrelid=v_table_oid and attnum>0 and exists (select from pgorm.orm_table t where t.schema=v_schema and t.table_name=v_table_name)" "\n" \
	"      on conflict do nothing;" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_TABLE_ENABLE \
	"create or replace procedure pgorm.orm_table_enable(schema name, table_name name, class_name name default null) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_schema name := lower(schema);" "\n" \
	"  v_table_name name := lower(table_name);" "\n" \
	"  v_class_name name := coalesce(class_name, pgorm.orm_identifier_name(table_name));" "\n" \
	"  v_newline char := chr(10);" "\n" \
	"  v_module_source text :=" "\n" \
	"    '// Module can be edited by changing field \"module_source\" of table \"pgorm.orm_table\"'||v_newline||v_newline||" "\n" \
	"    'import { Base'||v_class_name||',Base'||v_class_name||'Array } from \"/orm/'||v_schema||'/base/Base'||v_class_name||'.js\"'||v_newline||v_newline||" "\n" \
	"    'export class '||v_class_name||' extends Base'||v_class_name||' {'||v_newline||v_newline||'}'||v_newline||v_newline||" "\n" \
	"    'export class '||v_class_name||'Array extends Base'||v_class_name||'Array {'||v_newline||v_newline||'}';" "\n" \
	"begin" "\n" \
	"  if not exists (select 1 from pg_tables where schemaname=v_schema and tablename=v_table_name) then" "\n" \
	"    raise exception 'Table %.% not exists', v_schema, v_table_name;" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_table(schema, table_name, readonly, class_name, module_source)" "\n" \
	"    values (v_schema, v_table_name, false, v_class_name, v_module_source)" "\n" \
	"    on conflict do nothing;" "\n" \
	"  call pgorm.orm_table_columns_refresh(v_schema, v_table_name);" "\n" \
	"  insert into pgorm.orm_action(schema, object_name, object_type, class_name, action)" "\n" \
	"    values(v_schema, v_table_name, 'TABLE', v_class_name, 'ENABLE');" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_TABLE_ENABLE_SIMPLE \
	"create or replace procedure pgorm.orm_table_enable(table_name name) language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  call pgorm.orm_table_enable(current_schema(), table_name);" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_TABLE_DISABLE \
	"create or replace procedure pgorm.orm_table_disable(schema name, table_name name) language plpgsql as $$" "\n" \
	"declare" "\n" \
	"  v_schema name := lower(schema);" "\n" \
	"  v_table_name name := lower(table_name);" "\n" \
	"begin" "\n" \
	"  insert into pgorm.orm_action(schema, object_name, object_type, class_name, action)" "\n" \
	"    select t.schema, t.table_name, 'TABLE', t.class_name, 'DISABLE' from pgorm.orm_table t" "\n" \
	"      where t.schema=v_schema and t.table_name=v_table_name;" "\n" \
	"  delete from pgorm.orm_table t where t.schema=v_schema and t.table_name=v_table_name;" "\n" \
	"end; $$"

#define SQL_PROCEDURE$ORM_TABLE_DISABLE_SIMPLE \
	"create or replace procedure pgorm.orm_table_disable(table_name name) language plpgsql as $$" "\n" \
	"begin" "\n" \
	"  call pgorm.orm_table_disable(current_schema(), table_name);" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_TABLE_FN_UPDATE \
	"create or replace function pgorm.orm_table_fn_update() returns trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  if old.schema!=new.schema or old.table_name!=new.table_name or old.class_name!=new.class_name then" "\n" \
	"    raise exception 'Not allowed to change schema,table_name or class_name, use pgorm.orm_table_disable/enable';" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_action(schema,object_name,object_type,class_name,action) values(new.schema,new.table_name,'TABLE',new.class_name,'UPDATE');" "\n" \
	"  return null;" "\n" \
	"end; $$"

#define SQL_FUNCTION$ORM_TABLE_COLUMN_FN_UPDATE \
	"create or replace function pgorm.orm_table_column_fn_update() returns trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  if old.schema!=new.schema or old.table_name!=new.table_name or old.column_name!=new.column_name then" "\n" \
	"    raise exception 'Not allowed to change schema,table_name or column_name';" "\n" \
	"  end if;" "\n" \
	"  insert into pgorm.orm_action(schema,object_name,object_type,class_name,action) values(new.schema,new.table_name,'TABLE',new.class_name,'UPDATE');" "\n" \
	"  return null;" "\n" \
	"end; $$"

#define SQL_FUNCTION$EVENT_FN_ALTER_TABLE \
	"create or replace function pgorm.event_fn_alter_table() returns event_trigger language plpgsql security definer as $$" "\n" \
	"declare" "\n" \
	"  rec record;" "\n" \
	"begin" "\n" \
	"  for rec in (" "\n" \
	"    select t.schema,t.table_name,t.class_name" "\n" \
	"      from pg_event_trigger_ddl_commands() e" "\n" \
	"      join pg_class c on c.oid=e.objid" "\n" \
	"      join pgorm.orm_table t on t.schema=e.schema_name and t.table_name=c.relname" "\n" \
	"      where e.object_type in ('table','table column')" "\n" \
	"   ) loop" "\n" \
	"     call pgorm.orm_table_columns_refresh(rec.schema,rec.table_name);" "\n" \
	"     insert into pgorm.orm_action(schema,object_name,object_type,class_name,action) values (rec.schema,rec.table_name,'TABLE',rec.class_name,'ALTER');" "\n" \
	"   end loop;" "\n" \
	"end; $$"

#define SQL_FUNCTION$EVENT_FN_DROP_OBJECTS \
	"create or replace function pgorm.event_fn_drop_objects() returns event_trigger language plpgsql security definer as $$" "\n" \
	"begin" "\n" \
	"  insert into pgorm.orm_action(schema,object_name,object_type,class_name,action)" "\n" \
	"    select t.schema,t.table_name,'TABLE',t.class_name,'DROP'" "\n" \
	"      from pg_event_trigger_dropped_objects() e" "\n" \
	"      join pgorm.orm_table t on t.schema=e.schema_name and t.table_name=e.object_name and e.object_type='table';" "\n" \
	"  delete from pgorm.orm_table" "\n" \
	"    where (schema,table_name) in (select schema_name,table_name from pg_event_trigger_dropped_objects() where object_type='table');" "\n" \
	"end; $$"

#define SQL_BLOCK$CREATE_TRIGGERS \
	"do $$ begin" "\n" \
	"  if not exists (select from pg_event_trigger where evtname='pgorm_event_tg_alter_table') then" "\n" \
	"    create event trigger pgorm_event_tg_alter_table on ddl_command_end when tag in ('ALTER TABLE','COMMENT') execute procedure pgorm.event_fn_alter_table();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_event_trigger where evtname='pgorm_event_tg_drop_objects') then" "\n" \
	"    create event trigger pgorm_event_tg_drop_objects on sql_drop when tag in ('DROP TABLE','DROP SCHEMA') execute function pgorm.event_fn_drop_objects();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_trigger where tgname='pgorm_table_tg_update') then" "\n" \
	"    create trigger pgorm_table_tg_update after update on pgorm.orm_table for each row execute function pgorm.orm_table_fn_update();" "\n" \
	"  end if;" "\n" \
	"  if not exists (select from pg_trigger where tgname='pgorm_table_column_tg_update') then" "\n" \
	"    create trigger pgorm_table_column_tg_update after update on pgorm.orm_table_column for each row execute function pgorm.orm_table_column_fn_update();" "\n" \
	"  end if;" "\n" \
	"end $$"

const char *SQL_LIST[] = {
	SQL_SCHEMA,
	SQL_TABLE$ORM_TABLE,
	SQL_TABLE$ORM_TABLE_COLUMN,
	SQL_TABLE$ORM_ACTION,
	SQL_FUNCTION$ORM_IDENTIFIER_NAME,
	SQL_PROCEDURE$ORM_TABLE_COLUMNS_REFRESH,
	SQL_PROCEDURE$ORM_TABLE_ENABLE,
	SQL_PROCEDURE$ORM_TABLE_ENABLE_SIMPLE,
	SQL_PROCEDURE$ORM_TABLE_DISABLE,
	SQL_PROCEDURE$ORM_TABLE_DISABLE_SIMPLE,
	SQL_FUNCTION$ORM_TABLE_FN_UPDATE,
    SQL_FUNCTION$ORM_TABLE_COLUMN_FN_UPDATE,
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
