-- Create table
create table invoice(
  id serial primary key,
  date date not null default current_date,
  number varchar(10) not null,
  amount numeric(20,2) not null  
);

-- Enable ORM for table, JavaScript modules will be created automatically
call pgorm.orm_table_enable('invoice');

-- Create user and grant privilegies on table
create user user_1 login password 'password_1';
grant all on invoice,invoice_id_seq to user_1;

