-- Create schema
drop schema if exists example cascade;
create schema example;

-- Create tables with various columns, view, index and foreign keys
create table example.customer(
  id serial primary key,  
  name varchar(100) not null
);

create table example.invoice(
  id serial primary key,  
  number varchar(10) not null,
  customer_id int references example.customer(id),
  amount numeric(20,2) not null,
  image bytea,
  control_dates date[],
  extended_information jsonb
);
create index on example.invoice(number);

create or replace view example.invoice_ui as
  select i.id,i.number,c.name customer_name,i.amount 
    from example.invoice i
    left join example.customer c on c.id=i.customer_id;

create table example.invoice_product(
  id serial primary key,  
  invoice_id int not null references example.invoice(id) on delete cascade,
  sort_pos int not null,
  product_name varchar(100) not null,
  quantity int not null
);

-- Enable ORM for tables and view    
call pgorm.orm_relation_enable('example','customer');
call pgorm.orm_relation_enable('example','invoice');
call pgorm.orm_relation_enable('example','invoice_ui');
call pgorm.orm_relation_enable('example','invoice_product');

-- Review ORM definitions and change them (if necessary)
select * from pgorm.orm_relation;
select * from pgorm.orm_relation_column;
select * from pgorm.orm_table_fkey;
select * from pgorm.orm_view_pkey;

-- Create simple function 
create or replace function example.power_3(x int) returns int language plpgsql as $$
begin
  return x*x*x;	
end; $$;

-- Create function: insert multiple records in one transaction and return record 
-- Variant 1: parameters - postgres records, result - postgres record, must specify list of columns    
create or replace function example.add_invoice_v1(invoice example.invoice, invoice_product_array example.invoice_product[]) returns example.invoice security definer language plpgsql as $$
begin
  insert into example.invoice(number,customer_id,amount)	
    select number,customer_id,amount from (select invoice.*) r
    returning * into invoice;
  insert into example.invoice_product(invoice_id,sort_pos,product_name,quantity)	
    select invoice.id,row_number() over (),product_name,quantity from (select unnest.* from unnest(invoice_product_array)) r;
  return invoice;  
end; $$;
-- Variant 2: parameter - one common jsonb, result - postgres record, used functions jsonb_populate_record and jsonb_populate_recordset
create or replace function example.add_invoice_v2(params jsonb) returns example.invoice security definer language plpgsql as $$
declare
  v_invoice example.invoice := jsonb_populate_record(null::example.invoice, params->'invoice');  
  v_invoice_product_array example.invoice_product[] := (select array_agg(r) from jsonb_populate_recordset(null::example.invoice_product, params->'invoice_product_array') r);
begin
  return example.add_invoice_v1(v_invoice, v_invoice_product_array);
end; $$;
-- Variant 3: parameters - records in jsonb format, result - record as jsonb, used procedures pgorm.orm_record_*      
create or replace function example.add_invoice_v3(invoice jsonb, invoice_product_array jsonb[]) returns jsonb security definer language plpgsql as $$
declare
  v_invoice_product jsonb;
  v_sort_pos int := 1;
begin
  call pgorm.orm_record_save(invoice); 
  foreach v_invoice_product in array invoice_product_array loop
    call pgorm.orm_record_set_value(v_invoice_product, 'invoice_id', invoice->>'id');
    call pgorm.orm_record_set_value(v_invoice_product, 'sort_pos', v_sort_pos); v_sort_pos:=v_sort_pos+1;   
    call pgorm.orm_record_save(v_invoice_product);
  end loop;
  return invoice;  
end; $$;

-- Create user (if not exists), grant privileges
do $$ begin
  create user user_1 login password 'password_1';
exception 
  when duplicate_object then null;
end $$;
grant usage on schema example to user_1;
grant all on all tables in schema example to user_1;
grant all on all sequences in schema example to user_1;
grant execute on all functions in schema example to user_1;
grant execute on all procedures in schema example to user_1;
