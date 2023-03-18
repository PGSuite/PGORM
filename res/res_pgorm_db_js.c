#include "util/utils.h"

#define DATA \
    "export class TableColumn {\n" \ 
    "\n" \ 
    "	#name;\n" \ 
    "	#type;\n" \ 
    "	#typeArray;\n" \ 
    "	#fieldName;\n" \ 
    "	#typePrimitive;\n" \ 
    "	#typeObject;\n" \ 
    "	#notNull;\n" \ 
    "	#tableRowClassFunc;\n" \ 
    "	#validateValueFunc;\n" \ 
    "	#jsValueFunc;\n" \ 
    "	#pgValueFunc;\n" \ 
    "	#comment;\n" \ 
    "\n" \ 
    "	constructor(name, type, typeArray, fieldName, typePrimitive, typeObject, notNull, tableRowClassFunc, validateValueFunc, jsValueFunc, pgValueFunc, comment) {\n" \ 
    "		this.#name              = name;\n" \ 
    "		this.#type              = type;\n" \ 
    "		this.#typeArray         = typeArray;\n" \ 
    "		this.#fieldName         = fieldName;\n" \ 
    "		this.#typePrimitive     = typePrimitive;\n" \ 
    "		this.#typeObject        = typeObject;\n" \ 
    "		this.#notNull           = notNull;\n" \ 
    "		this.#tableRowClassFunc = tableRowClassFunc;\n" \ 
    "		this.#validateValueFunc = validateValueFunc;\n" \ 
    "		this.#jsValueFunc       = jsValueFunc;\n" \ 
    "		this.#pgValueFunc       = pgValueFunc;\n" \ 
    "		this.#comment           = comment;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	getName()          { return this.#name;                }\n" \ 
    "	getType()          { return this.#type;                }\n" \ 
    "	isTypeArray()      { return this.#typeArray;           }\n" \ 
    "	getFieldName()     { return this.#fieldName;           }\n" \ 
    "	getTypePrimitive() { return this.#typePrimitive;       }\n" \ 
    "	getTypeObject()    { return this.#typeObject;          }\n" \ 
    "	isNotNull()        { return this.#notNull;             }\n" \ 
    "	getTableRowClass() { return this.#tableRowClassFunc(); }\n" \ 
    "	getComment()       { return this.#comment;             }\n" \ 
    "\n" \ 
    "	validateValue(value) { this.#validateValueFunc(value); }\n" \ 
    "\n" \ 
    "	jsValue(pgValue) { return this.#jsValueFunc(pgValue); }\n" \ 
    "	pgValue(jsValue) { return this.#pgValueFunc(jsValue); }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class TableMetadata {\n" \ 
    "\n" \ 
    "	#schema;\n" \ 
    "	#tableName;\n" \ 
    "	#columns;\n" \ 
    "	#columnsPrimaryKey;\n" \ 
    "	#columnNames;\n" \ 
    "	#columnNamesPrimaryKey;\n" \ 
    "	#tableRowClassFunc;\n" \ 
    "	#tableRowArrayClassFunc;\n" \ 
    "	#tableComment;\n" \ 
    "\n" \ 
    "	constructor(schema, tableName, columns, columnsPrimaryKey, tableRowClassFunc, tableRowArrayClassFunc, tableComment) {\n" \ 
    "		this.#schema                 = schema;\n" \ 
    "		this.#tableName              = tableName;\n" \ 
    "		this.#columns                = columns;\n" \ 
    "		this.#columnsPrimaryKey      = columnsPrimaryKey;\n" \ 
    "		this.#columnNames            = columns.map( property => property.getName() );\n" \ 
    "		this.#columnNamesPrimaryKey  = columnsPrimaryKey.map( property => property.getName() );\n" \ 
    "		this.#tableRowClassFunc      = tableRowClassFunc;\n" \ 
    "		this.#tableRowArrayClassFunc = tableRowArrayClassFunc;\n" \ 
    "		this.#tableComment           = tableComment;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	getSchema()                { return this.#schema;                   }\n" \ 
    "	getTableName()             { return this.#tableName;                }\n" \ 
    "	getTableComment()          { return this.#tableComment;             }\n" \ 
    "	getColumns()               { return this.#columns;                  }\n" \ 
    "	getColumnsPrimaryKey()     { return this.#columnsPrimaryKey;        }\n" \ 
    "	getColumnNames()           { return this.#columnNames;              }\n" \ 
    "	getColumnNamesPrimaryKey() { return this.#columnNamesPrimaryKey;    }\n" \ 
    "	getTableRowClass()         { return this.#tableRowClassFunc();      }\n" \ 
    "	getTableRowArrayClass()    { return this.#tableRowArrayClassFunc(); }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class TableRow {\n" \ 
    "\n" \ 
    "	constructor() { if (this.constructor===TableRow) throw new Error(\"TableRow is abstract class\"); }\n" \ 
    "\n" \ 
    "	#rowExists = false;\n" \ 
    "	isRowExists() { return this.#rowExists; }\n" \ 
    "\n" \ 
    "	setFieldValue(column, value) { return this[\"set\"+column.getFieldName()](value);   }\n" \ 
    "	getFieldValue(column, value) { return this[\"get\"+column.getFieldName()]();        }\n" \ 
    "	isFieldValueChanged(column)  { return this[\"isChanged\"+column.getFieldName()]();  }\n" \ 
    "\n" \ 
    "	save(connection) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let tableMetadata = this.constructor.getTableMetadata();\n" \ 
    "		let columnsChanged = [];\n" \ 
    "		let values = [];\n" \ 
    "		tableMetadata.getColumns().filter( column => this.isFieldValueChanged(column) ).forEach( column => {\n" \ 
    "			columnsChanged.push(column.getName());\n" \ 
    "			let value = this.getFieldValue(column);\n" \ 
    "			column.validateValue(value);\n" \ 
    "			values.push(column.pgValue(value));\n" \ 
    "		});\n" \ 
    "        let valuesPrimaryKey = tableMetadata.getColumnsPrimaryKey().map( column => column.pgValue(this.getFieldValue(column)) );\n" \ 
    "		let fieldValues = sendRequest(\"/pgorm/table-row-save\", { \n" \ 
    "			\"connectionID\":      connection.getID(), \n" \ 
    "			\"schema\":            tableMetadata.getSchema(),\n" \ 
    "			\"tableName\":         tableMetadata.getTableName(),\n" \ 
    "			\"columns\":           tableMetadata.getColumnNames(),\n" \ 
    "			\"rowExists\":         this.#rowExists,\n" \ 
    "			\"columnsChanged\":    columnsChanged,\n" \ 
    "			\"values\":            values,\n" \ 
    "			\"columnsPrimaryKey\": tableMetadata.getColumnNamesPrimaryKey(),\n" \ 
    "			\"valuesPrimaryKey\":  valuesPrimaryKey\n" \ 
    "		}).row;\n" \ 
    "		this._initialize(fieldValues, true);\n" \ 
    "		return this;\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	delete(connection) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let tableMetadata = this.constructor.getTableMetadata();\n" \ 
    "        let valuesPrimaryKey = tableMetadata.getColumnsPrimaryKey().map( column => column.pgValue(this.getFieldValue(column)) );\n" \ 
    "		let fieldValues = sendRequest(\"/pgorm/table-row-delete\", { \n" \ 
    "			\"connectionID\":      connection.getID(), \n" \ 
    "			\"schema\":            tableMetadata.getSchema(),\n" \ 
    "			\"tableName\":         tableMetadata.getTableName(),\n" \ 
    "			\"columns\":           tableMetadata.getColumnNames(),\n" \ 
    "			\"columnsPrimaryKey\": tableMetadata.getColumnNamesPrimaryKey(),\n" \ 
    "			\"valuesPrimaryKey\":  valuesPrimaryKey\n" \ 
    "		}).row;\n" \ 
    "		this._initialize(fieldValues, false);\n" \ 
    "		return this;\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	static loadByCondition(tableRowClass, connection, condition, params) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let tableMetadata = tableRowClass.getTableMetadata();\n" \ 
    "		let fieldValues = sendRequest(\"/pgorm/table-row-load-by-condition\", { \n" \ 
    "			\"connectionID\": connection.getID(), \n" \ 
    "			\"schema\":       tableMetadata.getSchema(),\n" \ 
    "			\"tableName\":    tableMetadata.getTableName(),\n" \ 
    "			\"columns\":      tableMetadata.getColumnNames(),\n" \ 
    "			\"condition\":    condition,\n" \ 
    "			\"params\":       params!==null && params!==undefined ? params instanceof Array ? params : [params] : []\n" \ 
    "		}).row;\n" \ 
    "		if (fieldValues===null) return null;\n" \ 
    "		return new tableRowClass()._initialize(fieldValues, true);\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "    _initialize(rowExists) { this.#rowExists = rowExists; return this; }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class TableRowArray extends Array {\n" \ 
    "\n" \ 
    "	constructor() { super(); if (this.constructor===TableRowArray) throw new Error(\"TableRowArray is abstract class\"); }\n" \ 
    "\n" \ 
    "	static load(tableRowArrayClass, connection, condition, params) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let tableMetadata = tableRowArrayClass.getTableMetadata();\n" \ 
    "		let rows = sendRequest(\"/pgorm/table-row-array-load\", { \n" \ 
    "			\"connectionID\": connection.getID(), \n" \ 
    "			\"schema\":       tableMetadata.getSchema(),\n" \ 
    "			\"tableName\":    tableMetadata.getTableName(),\n" \ 
    "			\"columns\":      tableMetadata.getColumnNames(),\n" \ 
    "			\"condition\":    condition!==null && condition!==undefined ? condition : \"\",\n" \ 
    "			\"params\":       params!==null && params!==undefined ? params instanceof Array ? params : [params] : []\n" \ 
    "		}).rows;\n" \ 
    "		let tableRowArray = new tableRowArrayClass(); \n" \ 
    "		rows.forEach( fieldValues => {\n" \ 
    "			tableRowArray.push(new (tableMetadata.getTableRowClass())()._initialize(fieldValues, true));\n" \ 
    "    	});\n" \ 
    "		return tableRowArray;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class ORMConnection {\n" \ 
    "\n" \ 
    "	static #LOCAL_STORAGE_KEY = \"ORMConnection.ID\";\n" \ 
    "\n" \ 
    "	#id;\n" \ 
    "\n" \ 
    "	getID() { return this.#id; }\n" \ 
    "\n" \ 
    "	connect(user, password, idleSessionTimeout = '1h') {\n" \ 
    "	    this.#id = sendRequest(\"/pgorm/connect\", { \"user\": user, \"password\": password, \"idleSessionTimeout\": idleSessionTimeout }).connectionID;\n" \ 
    "		return this;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    disconnect() {\n" \ 
    "    	sendRequest(\"/pgorm/disconnect\", { \"connectionID\": this.#id });\n" \ 
    "		if (this.#id==localStorage.getItem(ORMConnection.#LOCAL_STORAGE_KEY))\n" \ 
    "			localStorage.clear(ORMConnection.#LOCAL_STORAGE_KEY); \n" \ 
    "		this.#id = undefined;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    execute(query, params) {\n" \ 
    "		console.log(\"#id=\"+this.#id);\n" \ 
    "		return sendRequest(\"/pgorm/execute\", { \"connectionID\": this.#id, \"query\": query, \"params\": params });\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	setDefault() { \n" \ 
    "		localStorage.setItem(ORMConnection.#LOCAL_STORAGE_KEY, this.#id); \n" \ 
    "		return this;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static getDefault() { 		\n" \ 
    "		let id = localStorage.getItem(ORMConnection.#LOCAL_STORAGE_KEY); \n" \ 
    "		if (id===null)\n" \ 
    "       		return null;      \n" \ 
    "		let connection = new ORMConnection();\n" \ 
    "		connection.#id = id; \n" \ 
    "		return connection;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class ORMError extends Error {\n" \ 
    "\n" \ 
    "	constructor(code, message) {\n" \ 
    "		super( (message.startsWith(\"PGORM-\") ? \"\" : \"PGORM-\"+code.toString().padStart(3, \"0\")+\" \")+message ); \n" \ 
    "	    this.code = code;\n" \ 
    "	    this.name = \"ORMError\";\n" \ 
    "	}\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "\n" \ 
    "export class ORMUtil {\n" \ 
    "\n" \ 
    "    constructor() { throw new Error(\"Final abstract class\"); }\n" \ 
    "\n" \ 
    "	static validateNumber     (value) { if (value!==null && value!==undefined && (typeof value)!==\"number\"  && !(value instanceof Number))  throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not number`);  }\n" \ 
    "	static validateBoolean    (value) { if (value!==null && value!==undefined && (typeof value)!==\"boolean\" && !(value instanceof Boolean)) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not boolean`); }\n" \ 
    "	static validateString     (value) { if (value!==null && value!==undefined && (typeof value)!==\"string\"  && !(value instanceof String))  throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not string`);  }\n" \ 
    "	static validateObject     (value) { if (value!==null && value!==undefined && (typeof value)!==\"object\")       throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not object`);      }\n" \ 
    "	static validateDate       (value) { if (value!==null && value!==undefined && !(value instanceof Date))        throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not Date`);        }\n" \ 
    "	static validateUint8Array (value) { if (value!==null && value!==undefined && !(value instanceof Uint8Array))  throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not Uint8Array`);  }\n" \ 
    "	static validateXMLDocument(value) { if (value!==null && value!==undefined && !(value instanceof XMLDocument)) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not XMLDocument`); }\n" \ 
    "\n" \ 
    "	static validateArray(array, validateElementFunc) { \n" \ 
    "		if (array===null && array===undefined) return;\n" \ 
    "		if (!(array instanceof Array)) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(array)} is not Array`);  \n" \ 
    "		array.forEach( element => element instanceof Array ? validateArray(element, validateElementFunc) : validateElementFunc(element) );\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static validateArrayNumber     (array) { ORMUtil.validateArray(array, ORMUtil.validateNumber);      }\n" \ 
    "	static validateArrayBoolean    (array) { ORMUtil.validateArray(array, ORMUtil.validateBoolean);     }\n" \ 
    "	static validateArrayString     (array) { ORMUtil.validateArray(array, ORMUtil.validateString);      }\n" \ 
    "	static validateArrayObject     (array) { ORMUtil.validateArray(array, ORMUtil.validateObject);      }\n" \ 
    "	static validateArrayDate       (array) { ORMUtil.validateArray(array, ORMUtil.validateDate);        }\n" \ 
    "	static validateArrayUint8Array (array) { ORMUtil.validateArray(array, ORMUtil.validateUint8Array);  }\n" \ 
    "	static validateArrayXMLDocument(array) { ORMUtil.validateArray(array, ORMUtil.validateXMLDocument); }\n" \ 
    "\n" \ 
    "	static _valueToStringShort(value) { return JSON.stringify(value.toString().length<=10 ? value.toString() : value.toString().substr(1,10)+\"...\"); }\n" \ 
    "\n" \ 
    "	static jsPrimitive  (pgValue) { return pgValue; }\n" \ 
    "	static jsDate       (pgValue) { return pgValue!==null ? new Date(pgValue) : null; }\n" \ 
    "	static jsUint8Array (pgValue) { return pgValue!==null ? new Uint8Array([...pgValue].map((c,i) => i && !(i%2) ? parseInt(pgValue.substr(i,2),16) : null).filter(b => b!==null)) : null; }\n" \ 
    "	static jsXMLDocument(pgValue) { return pgValue!==null ? new DOMParser().parseFromString(pgValue, \"text/xml\") : null; }\n" \ 
    "\n" \ 
    "	static pgPrimitive     (jsValue) { return jsValue!==undefined ? jsValue : null; }\n" \ 
    "	static pgDate          (jsValue) { return jsValue!==null && jsValue!==undefined ? jsValue.getFullYear()+\"-\"+(jsValue.getMonth()+1).toString().padStart(2,\"0\")+\"-\"+jsValue.getDate().toString().padStart(2,\"0\") : null; }\n" \ 
    "	static pgTime          (jsValue) { return jsValue!==null && jsValue!==undefined ? jsValue.getHours().toString().padStart(2,\"0\")+\":\"+jsValue.getMinutes().toString().padStart(2,\"0\")+\":\"+jsValue.getSeconds().toString().padStart(2,\"0\")+\".\"+jsValue.getMilliseconds().toString().padStart(3,\"0\") : null; }\n" \ 
    "	static pgTimezoneOffset(jsValue) { return jsValue!==null && jsValue!==undefined ? (jsValue.getTimezoneOffset()>0 ? \"-\" : \"+\")+(Math.abs(jsValue.getTimezoneOffset())/60|0).toString().padStart(2,\"0\")+(Math.abs(jsValue.getTimezoneOffset())%60).toString().padStart(2,\"0\") : null; }\n" \ 
    "	static pgTimetz        (jsValue) { return jsValue!==null && jsValue!==undefined ? ORMUtil.pgTime(jsValue)+ORMUtil.pgTimezoneOffset(jsValue) : null; }\n" \ 
    "	static pgTimestamp     (jsValue) { return jsValue!==null && jsValue!==undefined ? ORMUtil.pgDate(jsValue)+\"T\"+ORMUtil.pgTime(jsValue) : null; }\n" \ 
    "	static pgTimestamptz   (jsValue) { return jsValue!==null && jsValue!==undefined ? ORMUtil.pgDate(jsValue)+\"T\"+ORMUtil.pgTime(jsValue)+ORMUtil.pgTimezoneOffset(jsValue) : null; }\n" \ 
    "	static pgInterval      (jsValue) { return jsValue!==null && jsValue!==undefined ? (jsValue/(24*60*60*1000)|0)+\" day \"+(jsValue%(24*60*60*1000)/(60*60*1000)|0).toString().padStart(2,\"0\")+\":\"+(jsValue%(60*60*1000)/(60*1000)|0).toString().padStart(2,\"0\")+\":\"+(jsValue%(60*1000)/(1000)|0).toString().padStart(2,\"0\")+\".\"+(jsValue%1000).toString().padEnd(3,\"0\") : null; }\n" \ 
    "	static pgBytea         (jsValue) { return jsValue!==null && jsValue!==undefined ? \"\\\\x\"+Array.from(jsValue,  byte => byte.toString(16).padStart(2,'0') ).join(\"\") : null; }\n" \ 
    "	static pgJSON          (jsValue) { return jsValue!==null && jsValue!==undefined ? JSON.stringify(jsValue) : null; }\n" \ 
    "	static pgXML           (jsValue) { return jsValue!==null && jsValue!==undefined ? new XMLSerializer().serializeToString(jsValue) : null; }\n" \ 
    "\n" \ 
    "	static jsArray(pgArray, jsElementFunc) { return pgArray!==null && pgArray!==undefined ? pgArray.map( element => jsElementFunc(element) ) : null; }\n" \ 
    "\n" \ 
    "	static jsArrayPrimitive  (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsPrimitive);   }\n" \ 
    "	static jsArrayDate       (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsDate);        }\n" \ 
    "	static jsArrayUint8Array (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsUint8Array);  }\n" \ 
    "	static jsArrayXMLDocument(pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsXMLDocument); }\n" \ 
    "\n" \ 
    "	static pgArray(jsArray, pgElementFunc) { return jsArray!==null && jsArray!==undefined ? \"{\" + jsArray.map( (element,index) => element===null || element===undefined ? \"null\" : element instanceof Array ? ORMUtil.pgArray(element, pgElementFunc) : JSON.stringify(pgElementFunc(element)) ).join() + \"}\" : null; }\n" \ 
    "\n" \ 
    "	static pgArrayPrimitive     (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgPrimitive);      }\n" \ 
    "	static pgArrayDate          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgDate);           }\n" \ 
    "	static pgArrayTime          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTime);           }\n" \ 
    "	static pgArrayTimezoneOffset(jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimezoneOffset); }\n" \ 
    "	static pgArrayTimetz        (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimetz);         }\n" \ 
    "	static pgArrayTimestamp     (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimestamp);      }\n" \ 
    "	static pgArrayTimestamptz   (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimestamptz);    }\n" \ 
    "	static pgArrayInterval      (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgInterval);       }\n" \ 
    "	static pgArrayBytea         (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgBytea);          }\n" \ 
    "	static pgArrayJSON          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgJSON);           }\n" \ 
    "	static pgArrayXML           (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgXML);            }\n" \ 
    "\n" \ 
    "	static #compareNull  (value1,   value2,   nullsFirst)       { return value1===null ? value2!==null ? (nullsFirst ? -1 : +1) : 0 : (nullsFirst ? +1 : -1); }\n" \ 
    "	static compareNumber (number1,  number2,  desc, nullsFirst) { return number1!==null && number2!==null ? !desc ? number1-number2                : number2-number1                : ORMUtil.#compareNull(number1, number2, nullsFirst); }\n" \ 
    "	static compareString (string1,  string2,  desc, nullsFirst) { return string1!==null && string2!==null ? !desc ? string1.localeCompare(string2) : string2.localeCompare(string1) : ORMUtil.#compareNull(string1, string2, nullsFirst); }\n" \ 
    "	static compareBoolean(boolean1, boolean2, desc, nullsFirst) { return ORMUtil.compareNumber(boolean1!==null ? boolean1|0      : null, boolean2!==null ? boolean2|0      : null, desc, nullsFirst); }\n" \ 
    "	static compareDate   (date1,    date2,    desc, nullsFirst) { return ORMUtil.compareNumber(date1!==null    ? date1.getTime() : null, date2!==null    ? date2.getTime() : null, desc, nullsFirst); }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "function sendRequest(path, params) {\n" \ 
    "	let xhr = new XMLHttpRequest();\n" \ 
    "	try {\n" \ 
    "		xhr.open(\"POST\", path, false);\n" \ 
    "		xhr.setRequestHeader(\"Content-Type\", \"application/json;charset=UTF-8\");	\n" \ 
    "		xhr.send(JSON.stringify(params));\n" \ 
    "	} catch (error) {\n" \ 
    "	    throw new ORMError(305, `Error on send HTTP request (path: ${path}):\\n${error.name} ${error.message}`);\n" \ 
    "	}\n" \ 
    "	if (xhr.readyState!=4) \n" \ 
    "		throw new ORMError(306, `Incorrect HTTP request state (${xhr.readyState})`);\n" \ 
    "	if (xhr.status!=200) \n" \ 
    "		throw new ORMError(307, `Incorrect HTTP request status (${xhr.status})`);\n" \ 
    "	let response = JSON.parse(xhr.responseText);\n" \ 
    "    if (!response.sucess) \n" \ 
    "		throw new ORMError(response.errorCode, response.errorText);\n" \ 
    "    return response; \n" \ 
    "}\n" \ 
    "\n" \ 
    "\n" \ 
    "function validateConnection(connection) {\n" \ 
    "	if (connection===null)\n" \ 
    "		throw new ORMError(308, \"Connection is null\");\n" \ 
    "}\n" \ 
    ""

int res_pgorm_db_js(stream *data) { return stream_add_str(data, DATA, NULL); }