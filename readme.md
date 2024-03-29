## PGORM | JavaScript ORM for PostgreSQL

### [Download](https://pgorm.org/en/download/) ### 
### [Documentation](https://pgorm.org/en/documentation/) ### 
### [Example](https://pgorm.org/en/#example) ### 
  
### Description ###

PGORM is tool for working with a PostgreSQL database directly from web page,
allows to connect, execute SQL queries, use ORM and receive files.

Implemented as a service that accepts HTTP requests from the browser and returns response depending on access type (see below).
The database is configured with list of tables that will be mapped to JavaScript classes.
For each table, JavaScript module is automatically generated, be used through the construction
*import {[Table],[TableList]} from '/orm/[Schema]/[Table].js';*

### Access types ###

*   **ORM** - working with PostgreSQL table rows as JavaScript objects (object-relational mapping)',

*   **SQL** - execution of SQL queries and commands, the result is returned in JSON format with auto-casting',

*   **[GET] file** - receiving files (.html, .js, etc.), including a JavaScript module for connecting to a database, executing SQL queries, working with tables'


### Important qualities ###

*   **Regular use via SQL**  - ORM classes are created on the basis of existing tables, change the table structure (metadata) is performed via SQL. When performing DDL operations on table (alter table, create index, etc.), trigger fires, which updates the JavaScript module 
*   **Modules and class overriding** - For each table, two modules are created: an editable one with empty inherited classes and an autogenerated one with superclasses, which is recreated when the table structure changes
*   **Auto type conversion</b>** - Implemented automatic conversion of SQL and JavaScript data types, including arrays
*   **Table row and array** - Two classes are created for each table: table row (Record) and array of table rows (RecordArray extends Array)
*   **Relationships** - Separate methods created to work with table rows related by foreign keys
*   **Row array automethods** - In class of table rows, load by index and sort methods are automatically created
*   **Naming** - Сlass name for table, field names for columns and relationship names (parent,children) are formed automatically, but can specify them explicitly
*   **Autocorrect** - When performing DDL operations on table (alter table, create index, etc.), trigger fires, which updates the JavaScript module
*   **Table definition (relation)** - Class has static method that returns table definition (relation): schema, name, fields, primary key, relationships
*   **Simplicity and sufficiency** -  Server sends the modules created by it via HTTP, installation consists in copying the executable file and creation of the service, no third party libraries required (libpq already installed with postgres)

