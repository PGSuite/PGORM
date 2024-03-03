-- Создаем таблицу
create table invoice(
  id serial primary key,
  date date not null default current_date,
  number varchar(10) not null,
  amount numeric(20,2) not null  
);

-- Включаем ORM для таблицы, JavaScript-модули создадутся автоматически    
call pgorm.orm_table_enable('invoice');

-- Создаем пользователя и выдаем ему права на таблицу  
create user user_1 login password 'user_1';
grant all on invoice,invoice_id_seq to user_1;

