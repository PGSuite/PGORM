#include "util/utils.h"

#define DATA \
    "export class Relation {\n" \
    "\n" \
    "	#schema;\n" \
    "	#name;\n" \
    "	#type;\n" \
    "	#comment;\n" \
    "	#columns;\n" \
    "	#columnsPrimaryKey;\n" \
    "	#columnNames;\n" \
    "	#columnNamesPrimaryKey;\n" \
    "	#rowClassFunc;\n" \
    "	#rowArrayClassFunc;\n" \
    "	#relationshipsParent;\n" \
    "	#relationshipsChildFunc;\n" \
    "\n" \
    "	constructor(schema, name, type, comment, columns, columnsPrimaryKey, rowClassFunc, rowArrayClassFunc, relationshipsParent, relationshipsChildFunc) {\n" \
    "		this.#schema                 = schema;\n" \
    "		this.#name                   = name;\n" \
    "		this.#type                   = type;\n" \
    "		this.#comment                = comment;\n" \
    "		this.#columns                = columns;\n" \
    "		this.#columnsPrimaryKey      = columnsPrimaryKey;\n" \
    "		this.#columnNames            = columns.map( property => property.getName() );\n" \
    "		this.#columnNamesPrimaryKey  = columnsPrimaryKey.map( property => property.getName() );\n" \
    "		this.#rowClassFunc           = rowClassFunc;\n" \
    "		this.#rowArrayClassFunc      = rowArrayClassFunc;\n" \
    "		this.#relationshipsParent    = relationshipsParent;\n" \
    "		this.#relationshipsChildFunc = relationshipsChildFunc;\n" \
    "	}\n" \
    "\n" \
    "	getSchema()                { return this.#schema;                   }\n" \
    "	getName()                  { return this.#name;                     }\n" \
    "	getType()                  { return this.#type;                     }\n" \
    "	getComment()               { return this.#comment;                  }\n" \
    "	getColumns()               { return this.#columns;                  }\n" \
    "	getColumnsPrimaryKey()     { return this.#columnsPrimaryKey;        }\n" \
    "	getColumnNames()           { return this.#columnNames;              }\n" \
    "	getColumnNamesPrimaryKey() { return this.#columnNamesPrimaryKey;    }\n" \
    "	getRowClass()              { return this.#rowClassFunc();           }\n" \
    "	getRowArrayClass()         { return this.#rowArrayClassFunc();      }\n" \
    "	getRelationshipsParent()   { return this.#relationshipsParent;      }\n" \
    "	getRelationshipsChild()    { return this.#relationshipsChildFunc(); }\n" \
    "\n" \
    "}\n" \
    "\n" \
    "export class Column {\n" \
    "\n" \ 
    "	#name;\n" \ 
    "	#type;\n" \ 
    "	#typeArray;\n" \ 
    "	#fieldName;\n" \ 
    "	#typePrimitive;\n" \ 
    "	#typeObject;\n" \ 
    "	#notNull;\n" \ 
    "	#rowClassFunc;\n" \
    "	#validateValueFunc;\n" \ 
    "	#jsValueFunc;\n" \ 
    "	#pgValueFunc;\n" \ 
    "	#comment;\n" \ 
    "\n" \ 
    "	constructor(name, type, typeArray, fieldName, typePrimitive, typeObject, notNull, rowClassFunc, validateValueFunc, jsValueFunc, pgValueFunc, comment) {\n" \
    "		this.#name              = name;\n" \ 
    "		this.#type              = type;\n" \ 
    "		this.#typeArray         = typeArray;\n" \ 
    "		this.#fieldName         = fieldName;\n" \ 
    "		this.#typePrimitive     = typePrimitive;\n" \ 
    "		this.#typeObject        = typeObject;\n" \ 
    "		this.#notNull           = notNull;\n" \ 
    "		this.#rowClassFunc      = rowClassFunc;\n" \
    "		this.#validateValueFunc = validateValueFunc;\n" \ 
    "		this.#jsValueFunc       = jsValueFunc;\n" \ 
    "		this.#pgValueFunc       = pgValueFunc;\n" \ 
    "		this.#comment           = comment;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	getName()          { return this.#name;           }\n" \
    "	getType()          { return this.#type;           }\n" \
    "	isTypeArray()      { return this.#typeArray;      }\n" \
    "	getFieldName()     { return this.#fieldName;      }\n" \
    "	getTypePrimitive() { return this.#typePrimitive;  }\n" \
    "	getTypeObject()    { return this.#typeObject;     }\n" \
    "	isNotNull()        { return this.#notNull;        }\n" \
    "	getRowClass()      { return this.#rowClassFunc(); }\n" \
    "	getComment()       { return this.#comment;        }\n" \
    "\n" \ 
    "	validateValue(value) { this.#validateValueFunc(value); }\n" \ 
    "\n" \ 
    "	jsValue(pgValue) { return this.#jsValueFunc(pgValue); }\n" \ 
    "	pgValue(jsValue) { return this.#pgValueFunc(jsValue); }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class Row {\n" \
    "\n" \ 
    "	constructor() { if (this.constructor===Row) throw new Error(\"Row is abstract class\"); }\n" \
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
    "		let relation = this.constructor.getRelation();\n" \ 
    "		let columnsChanged = [];\n" \ 
    "		let values = [];\n" \ 
    "		relation.getColumns().filter( column => this.isFieldValueChanged(column) ).forEach( column => {\n" \ 
    "			columnsChanged.push(column.getName());\n" \ 
    "			let value = this.getFieldValue(column);\n" \ 
    "			column.validateValue(value);\n" \ 
    "			values.push(column.pgValue(value));\n" \ 
    "		});\n" \ 
    "		let valuesPrimaryKey = relation.getColumnsPrimaryKey().map( column => column.pgValue(this.getFieldValue(column)) );\n" \
    "		let fieldValues = sendRequest(\"/pgorm/row-save\", { \n" \
    "			\"connectionID\":      connection.getID(), \n" \ 
    "			\"relationSchema\":    relation.getSchema(),\n" \
    "			\"relationName\":      relation.getName(),\n" \
    "			\"columns\":           relation.getColumnNames(),\n" \ 
    "			\"rowExists\":         this.#rowExists,\n" \ 
    "			\"columnsChanged\":    columnsChanged,\n" \ 
    "			\"values\":            values,\n" \ 
    "			\"columnsPrimaryKey\": relation.getColumnNamesPrimaryKey(),\n" \ 
    "			\"valuesPrimaryKey\":  valuesPrimaryKey\n" \ 
    "		}).row;\n" \ 
    "		this._initialize(fieldValues, true);\n" \ 
    "		return this;\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	delete(connection) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let relation = this.constructor.getRelation();\n" \ 
    "		let valuesPrimaryKey = relation.getColumnsPrimaryKey().map( column => column.pgValue(this.getFieldValue(column)) );\n" \
    "		let fieldValues = sendRequest(\"/pgorm/row-delete\", { \n" \
    "			\"connectionID\":      connection.getID(), \n" \ 
    "			\"relationSchema\":    relation.getSchema(),\n" \
    "			\"relationName\":      relation.getName(),\n" \
    "			\"columns\":           relation.getColumnNames(),\n" \ 
    "			\"columnsPrimaryKey\": relation.getColumnNamesPrimaryKey(),\n" \ 
    "			\"valuesPrimaryKey\":  valuesPrimaryKey\n" \ 
    "		}).row;\n" \ 
    "		this._initialize(fieldValues, false);\n" \ 
    "		return this;\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	static loadWhere(rowClass, connection, condition, params) {\n" \
    "		validateConnection(connection);\n" \ 
    "		let relation = rowClass.getRelation();\n" \
    "		let fieldValues = sendRequest(\"/pgorm/row-load-where\", { \n" \
    "			\"connectionID\":   connection.getID(), \n" \
    "			\"relationSchema\": relation.getSchema(),\n" \
    "			\"relationName\":   relation.getName(),\n" \
    "			\"columns\":        relation.getColumnNames(),\n" \
    "			\"condition\":      condition,\n" \
    "			\"params\":         params!==null && params!==undefined ? params instanceof Array ? params : [params] : []\n" \
    "		}).row;\n" \ 
    "		if (fieldValues===null) return null;\n" \ 
    "		return new rowClass()._initialize(fieldValues, true);\n" \
    "	}	\n" \ 
    "\n" \ 
    "    _initialize(rowExists) { this.#rowExists = rowExists; return this; }\n" \ 
    "\n" \ 
    "    toJSON() {\n" \ 
    "        let json = {};\n" \ 
    "        this.constructor.getRelation().getColumns().forEach( column => { json[column.getFieldName()] = this.getFieldValue(column); });\n" \ 
    "        return json;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class RowArray extends Array {\n" \
    "\n" \ 
    "	constructor() { super(); if (this.constructor===RowArray) throw new Error(\"RowArray is abstract class\"); }\n" \
    "\n" \ 
    "	static load(rowArrayClass, connection, condition, params) {\n" \
    "		validateConnection(connection);\n" \ 
    "		let relation = rowArrayClass.getRelation();\n" \
    "		let rows = sendRequest(\"/pgorm/row-array-load\", { \n" \
    "			\"connectionID\":   connection.getID(), \n" \
    "			\"relationSchema\": relation.getSchema(),\n" \
    "			\"relationName\":   relation.getName(),\n" \
    "			\"columns\":        relation.getColumnNames(),\n" \
    "			\"condition\":      condition!==null && condition!==undefined ? condition : \"\",\n" \
    "			\"params\":         params!==null && params!==undefined ? params instanceof Array ? params : [params] : []\n" \
    "		}).rows;\n" \ 
    "		let rowArray = new rowArrayClass(); \n" \
    "		rows.forEach( fieldValues => {\n" \ 
    "			rowArray.push(new (relation.getRowClass())()._initialize(fieldValues, true));\n" \
    "    	});\n" \ 
    "		return rowArray;\n" \
    "	}\n" \
    "\n" \
    "	static delete(rowArrayClass, connection, condition, params) {\n" \
    "		validateConnection(connection);\n" \
    "		let relation = rowArrayClass.getRelation();\n" \
    "		return sendRequest(\"/pgorm/row-array-delete\", { \n" \
    "			\"connectionID\": connection.getID(), \n" \
    "			\"relationSchema\": relation.getSchema(),\n" \
    "			\"relationName\":   relation.getName(),\n" \
    "			\"condition\":      condition!==null && condition!==undefined ? condition : \"\",\n" \
    "			\"params\":         params!==null && params!==undefined ? params instanceof Array ? params : [params] : []\n" \
    "		}).rows_deleted;\n" \
    "	}\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \
    "export class Relationship {\n" \
    "\n" \
    "	#parentTableSchema;\n" \
    "	#parentTableName;\n" \
    "	#parentClassFunc;\n" \
    "	#childTableSchema;\n" \
    "	#childTableName;\n" \
    "	#childClassFunc;\n" \
    "	#childColumns;\n" \
    "	#childColumnOrderPos;\n" \
    "\n" \
    "	constructor(parentTableSchema, parentTableName, parentClassFunc, childTableSchema, childTableName, childClassFunc, childColumns, childColumnOrderPos) {\n" \
    "		this.#parentTableSchema   = parentTableSchema;\n" \
    "		this.#parentTableName     = parentTableName;\n" \
    "		this.#parentClassFunc     = parentClassFunc;\n" \
    "		this.#childTableSchema    = childTableSchema;\n" \
    "		this.#childTableName      = childTableName;\n" \
    "		this.#childClassFunc      = childClassFunc;\n" \
    "		this.#childColumns        = childColumns;\n" \
    "		this.#childColumnOrderPos = childColumnOrderPos;\n" \
    "\n" \
    "	}\n" \
    "\n" \
    "	getParentTableSchema()   { return this.#parentTableSchema;   }\n" \
    "	getParentTableName()     { return this.#parentTableName;     }\n" \
    "	getParentClass()         { return this.#parentClassFunc();   }\n" \
    "	getChildTableSchema()    { return this.#childTableSchema;    }\n" \
    "	getChildTableName()      { return this.#childTableName;      }\n" \
    "	getChildClass()          { return this.#childClassFunc();    }\n" \
    "	getChildColumns()        { return this.#childColumns;        }\n" \
    "	getChildColumnOrderPos() { return this.#childColumnOrderPos; }\n" \
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
