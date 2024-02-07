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
    "	#recordClassFunc;\n" \ 
    "	#recordArrayClassFunc;\n" \ 
    "	#relationshipsParent;\n" \ 
    "	#relationshipsChildFunc;\n" \ 
    "\n" \ 
    "	constructor(schema, name, type, comment, columns, columnsPrimaryKey, recordClassFunc, recordArrayClassFunc, relationshipsParent, relationshipsChildFunc) {\n" \ 
    "		this.#schema                 = schema;\n" \ 
    "		this.#name                   = name;\n" \ 
    "		this.#type                   = type;\n" \ 
    "		this.#comment                = comment;\n" \ 
    "		this.#columns                = columns;\n" \ 
    "		this.#columnsPrimaryKey      = columnsPrimaryKey;\n" \ 
    "		this.#columnNames            = columns.map( property => property.getName() );\n" \ 
    "		this.#columnNamesPrimaryKey  = columnsPrimaryKey.map( property => property.getName() );\n" \ 
    "		this.#recordClassFunc        = recordClassFunc;\n" \ 
    "		this.#recordArrayClassFunc   = recordArrayClassFunc;\n" \ 
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
    "	getRecordClass()           { return this.#recordClassFunc();        }\n" \ 
    "	getRecordArrayClass()      { return this.#recordArrayClassFunc();   }\n" \ 
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
    "	#fieldType;\n" \ 
    "	#notNull;\n" \ 
    "	#recordClassFunc;\n" \ 
    "	#validateValueFunc;\n" \ 
    "	#jsValueFunc;\n" \ 
    "	#pgValueFunc;\n" \ 
    "	#comment;\n" \ 
    "\n" \ 
    "	constructor(name, type, typeArray, fieldName, fieldType, notNull, recordClassFunc, validateValueFunc, jsValueFunc, pgValueFunc, comment) {\n" \ 
    "		this.#name              = name;\n" \ 
    "		this.#type              = type;\n" \ 
    "		this.#typeArray         = typeArray;\n" \ 
    "		this.#fieldName         = fieldName;\n" \ 
    "		this.#fieldType         = fieldType;\n" \ 
    "		this.#notNull           = notNull;\n" \ 
    "		this.#recordClassFunc   = recordClassFunc;\n" \ 
    "		this.#validateValueFunc = validateValueFunc;\n" \ 
    "		this.#jsValueFunc       = jsValueFunc;\n" \ 
    "		this.#pgValueFunc       = pgValueFunc;\n" \ 
    "		this.#comment           = comment;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	getName()          { return this.#name;              }\n" \ 
    "	getType()          { return this.#type;              }\n" \ 
    "	isTypeArray()      { return this.#typeArray;         }\n" \ 
    "	getFieldName()     { return this.#fieldName;         }\n" \ 
    "	getFieldType()     { return this.#fieldType;        }\n" \ 
    "	isNotNull()        { return this.#notNull;           }\n" \ 
    "	getRecordClass()   { return this.#recordClassFunc(); }\n" \ 
    "	getComment()       { return this.#comment;           }\n" \ 
    "\n" \ 
    "	validateValue(value) { this.#validateValueFunc(value); }\n" \ 
    "\n" \ 
    "	jsValue(pgValue) { return this.#jsValueFunc(pgValue); }\n" \ 
    "	pgValue(jsValue) { return this.#pgValueFunc(jsValue); }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class Record {\n" \ 
    "\n" \ 
    "	constructor() { if (this.constructor===Record) throw new Error(\"Record is abstract class\"); }\n" \ 
    "\n" \ 
    "	#recordExists = false;\n" \ 
    "	isRecordExists() { return this.#recordExists; }\n" \ 
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
    "		let fieldValues = sendRequest(\"/pgorm/record-save\", { \n" \ 
    "			\"connectionID\":      connection.getID(), \n" \ 
    "			\"relationSchema\":    relation.getSchema(),\n" \ 
    "			\"relationName\":      relation.getName(),\n" \ 
    "			\"columns\":           relation.getColumnNames(),\n" \ 
    "			\"recordExists\":      this.#recordExists,\n" \ 
    "			\"columnsChanged\":    columnsChanged,\n" \ 
    "			\"values\":            values,\n" \ 
    "			\"columnsPrimaryKey\": relation.getColumnNamesPrimaryKey(),\n" \ 
    "			\"valuesPrimaryKey\":  valuesPrimaryKey\n" \ 
    "		}).record;\n" \ 
    "		this._initialize(fieldValues, null, true);\n" \ 
    "		return this;\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	delete(connection) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let relation = this.constructor.getRelation();\n" \ 
    "		let valuesPrimaryKey = relation.getColumnsPrimaryKey().map( column => column.pgValue(this.getFieldValue(column)) );\n" \ 
    "		let fieldValues = sendRequest(\"/pgorm/record-delete\", { \n" \ 
    "			\"connectionID\":      connection.getID(), \n" \ 
    "			\"relationSchema\":    relation.getSchema(),\n" \ 
    "			\"relationName\":      relation.getName(),\n" \ 
    "			\"columns\":           relation.getColumnNames(),\n" \ 
    "			\"columnsPrimaryKey\": relation.getColumnNamesPrimaryKey(),\n" \ 
    "			\"valuesPrimaryKey\":  valuesPrimaryKey\n" \ 
    "		}).record;\n" \ 
    "		this._initialize(fieldValues, null, false);\n" \ 
    "		return this;\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	static loadWhere(recordClass, connection, condition, params) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let relation = recordClass.getRelation();\n" \ 
    "		let fieldValues = sendRequest(\"/pgorm/record-load-where\", { \n" \ 
    "			\"connectionID\":   connection.getID(), \n" \ 
    "			\"relationSchema\": relation.getSchema(),\n" \ 
    "			\"relationName\":   relation.getName(),\n" \ 
    "			\"columns\":        relation.getColumnNames(),\n" \ 
    "			\"condition\":      condition,\n" \ 
    "			\"params\":         ORMUtil.pgArrayParam(params)\n" \ 
    "		}).record;\n" \ 
    "		if (fieldValues===null) return null;\n" \ 
    "		return new recordClass()._initialize(fieldValues, null, true);\n" \ 
    "	}	\n" \ 
    "\n" \ 
    "	toRecord() { \n" \ 
    "		let pgRecord = \"(\";\n" \ 
    "		this.constructor.getRelation().getColumns().forEach( (column,index) => { \n" \ 
    "			if (index>0) pgRecord = pgRecord+\",\";\n" \ 
    "			let value = this.getFieldValue(column); \n" \ 
    "			if (value!==null) pgRecord = pgRecord+JSON.stringify(column.pgValue(value));\n" \ 
    "		});\n" \ 
    "		return pgRecord+\")\";\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    static fromRecord(recordClass, pgRecord) {	\n" \ 
    "		if (pgRecord===null) return null;\n" \ 
    "		let pgRecordLen = pgRecord.length;\n" \ 
    "		if (pgRecordLen<2 || pgRecord[0]!=='(' || pgRecord[pgRecordLen-1]!==')') \n" \ 
    "			throw new ORMError(313, `Incorrect format of postgres record ${ORMUtil._valueToStringShort(pgRecord)}`);\n" \ 
    "        let pgValues = [];\n" \ 
    "		let posBeg = 1;\n" \ 
    "	    while(posBeg<pgRecordLen) {\n" \ 
    "			let quoted = pgRecord[posBeg]==='\"';\n" \ 
    "			let posEnd = posBeg;\n" \ 
    "			if (quoted) {\n" \ 
    "				while(true) {					\n" \ 
    "					posEnd = pgRecord.indexOf('\"',posEnd+1);\n" \ 
    "					if (posEnd<0) \n" \ 
    "						throw new ORMError(313, `Incorrect format of postgres record ${ORMUtil._valueToStringShort(pgRecord)}`);\n" \ 
    "					if (pgRecord[posEnd+1]==='\"') \n" \ 
    "						pgRecord = pgRecord.substring(0,posEnd) + '\\\\' + pgRecord.substring(++posEnd);\n" \ 
    "					else\n" \ 
    "						break;\n" \ 
    "				}\n" \ 
    "				posEnd++;\n" \ 
    "			} else {\n" \ 
    "				posEnd = pgRecord.indexOf(',',posEnd);\n" \ 
    "				if (posEnd<0) posEnd=pgRecordLen-1;\n" \ 
    "			}\n" \ 
    "			let value = pgRecord.substring(posBeg,posEnd);\n" \ 
    "			posBeg = posEnd+1;\n" \ 
    "			if (!quoted && value==='') value = null;\n" \ 
    "			if (quoted && value!==null) value = JSON.parse(value);\n" \ 
    "			pgValues.push(value);\n" \ 
    "		}\n" \ 
    "		if (pgValues.length!==recordClass.getRelation().getColumns().length) \n" \ 
    "			throw new ORMError(313, `Incorrect format of postgres record ${ORMUtil._valueToStringShort(pgRecord)}`);\n" \ 
    "        return new recordClass()._initialize(pgValues, null, true);\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    toJSON() {\n" \ 
    "        let relation = this.constructor.getRelation();\n" \ 
    "        let json = {};\n" \ 
    "        let columnsChanged = [];\n" \ 
    "        relation.getColumns().forEach( column => { \n" \ 
    "			let value = this.getFieldValue(column);\n" \ 
    "			if (value!==null && (column.getFieldType()!==Object || column.isTypeArray())) value = column.pgValue(value);\n" \ 
    "			json[column.getName()] = value;\n" \ 
    "            if (this.isFieldValueChanged(column)) columnsChanged.push(column.getName());\n" \ 
    "		});        \n" \ 
    "		json[\"#orm\"] = { \n" \ 
    "			\"relation\" :           relation.getSchema()+\".\"+relation.getName(),\n" \ 
    "			\"record_exists\":       this.#recordExists,\n" \ 
    "			\"columns_primary_key\": relation.getColumnNamesPrimaryKey(),\n" \ 
    "			\"columns_changed\":     columnsChanged\n" \ 
    "		};\n" \ 
    "        return json;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    static fromJSON(recordClass, json) {	\n" \ 
    "		if (json===null) return null;\n" \ 
    "		if ((typeof json)===\"string\") json = JSON.parse(json);\n" \ 
    "        let relation = recordClass.getRelation();\n" \ 
    "        let pgValues = [];\n" \ 
    "        relation.getColumns().forEach( column => {\n" \ 
    "			let value = json[column.getName()];\n" \ 
    "			if (value===undefined) value = null;\n" \ 
    "			if (value!==null && column.getFieldType()===Object && !column.isTypeArray()) value = JSON.stringify(value);\n" \ 
    "			pgValues.push(value);\n" \ 
    "		});\n" \ 
    "		let valuesChanged;\n" \ 
    "		let recordExists;\n" \ 
    "		if (\"#orm\" in json) {\n" \ 
    "			let orm = json[\"#orm\"];\n" \ 
    "			if (orm[\"relation\"]!=(relation.getSchema()+\".\"+relation.getName()))\n" \ 
    "				throw new ORMError(311, `Relation ${orm[\"relation\"]} does not match class ${recordClass.name}`);\n" \ 
    "        	recordExists = orm[\"record_exists\"];\n" \ 
    "	        let columnNames = relation.getColumnNames();\n" \ 
    "			valuesChanged = Array(columnNames.length).fill(false);\n" \ 
    "    		orm[\"columns_changed\"].forEach(columnChanged => { valuesChanged[columnNames.indexOf(columnChanged)] = true; });\n" \ 
    "		} else {\n" \ 
    "			recordExists = true;\n" \ 
    "			relation.getColumnsPrimaryKey.forEach(column => { if ( !(column.getName() in json) || json[column.getName()]===null) recordExists = false; } );\n" \ 
    "			valuesChanged = null;\n" \ 
    "		}        \n" \ 
    "        return new recordClass()._initialize(pgValues, valuesChanged, recordExists);\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    _initialize(recordExists) { this.#recordExists = recordExists; return this; }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "export class RecordArray extends Array {\n" \ 
    "\n" \ 
    "	constructor() { super(); if (this.constructor===RecordArray) throw new Error(\"RecordArray is abstract class\"); }\n" \ 
    "\n" \ 
    "	static load(recordArrayClass, connection, condition, params) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let relation = recordArrayClass.getRelation();\n" \ 
    "		let records = sendRequest(\"/pgorm/record-array-load\", { \n" \ 
    "			\"connectionID\":   connection.getID(), \n" \ 
    "			\"relationSchema\": relation.getSchema(),\n" \ 
    "			\"relationName\":   relation.getName(),\n" \ 
    "			\"columns\":        relation.getColumnNames(),\n" \ 
    "			\"condition\":      condition!==null && condition!==undefined ? condition : \"\",\n" \ 
    "			\"params\":         ORMUtil.pgArrayParam(params)\n" \ 
    "		}).records;\n" \ 
    "		let recordArray = new recordArrayClass(); \n" \ 
    "		records.forEach( fieldValues => {\n" \ 
    "			recordArray.push(new (relation.getRecordClass())()._initialize(fieldValues, true));\n" \ 
    "    	});\n" \ 
    "		return recordArray;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static delete(recordArrayClass, connection, condition, params) {\n" \ 
    "		validateConnection(connection);\n" \ 
    "		let relation = recordArrayClass.getRelation();\n" \ 
    "		return sendRequest(\"/pgorm/record-array-delete\", { \n" \ 
    "			\"connectionID\": connection.getID(), \n" \ 
    "			\"relationSchema\": relation.getSchema(),\n" \ 
    "			\"relationName\":   relation.getName(),\n" \ 
    "			\"condition\":      condition!==null && condition!==undefined ? condition : \"\",\n" \ 
    "			\"params\":         ORMUtil.pgArrayParam(params)\n" \ 
    "		}).records_deleted;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	toJSON() {\n" \ 
    "		let json = JSON.parse('[]');\n" \ 
    "		this.forEach( record=>json.push(record.toJSON()) );\n" \ 
    "		return json;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	toArrayJSON() {\n" \ 
    "		let arrayJSON = [];\n" \ 
    "		this.forEach( record=>arrayJSON.push(record.toJSON()) );\n" \ 
    "		return arrayJSON;\n" \ 
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
    "	static #LS_CONNECTION_DEFAULT = \"PGORM.ConnectionDefault\";\n" \ 
    "\n" \ 
    "	#index;\n" \ 
    "	#key;\n" \ 
    "\n" \ 
    "	connect(user, password, idleSessionTimeout = '1h') {\n" \ 
    "	    let connection = sendRequest(\"/pgorm/connect\", { \"user\": user, \"password\": password, \"idle_session_timeout\": idleSessionTimeout }).connection;\n" \ 
    "		this.#index = connection.index;\n" \ 
    "		this.#key   = connection.key;\n" \ 
    "		return this;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    disconnect() {\n" \ 
    "    	sendRequest(\"/pgorm/disconnect\", { \"connection\": this });\n" \ 
    "		if (this.#key===localStorage.getItem(ORMConnection.#LS_CONNECTION_DEFAULT).#key)\n" \ 
    "			localStorage.clear(ORMConnection.#LS_CONNECTION_DEFAULT); \n" \ 
    "		this.#index = undefined;\n" \ 
    "		this.#key   = undefined;\n" \ 
    "		return this;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	setDefault() { \n" \ 
    "		localStorage.setItem(ORMConnection.#LS_CONNECTION_DEFAULT, JSON.stringify(this)); \n" \ 
    "		return this;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    execute(query, params) {\n" \ 
    "		return sendRequest(\"/pgorm/execute\", { \"connection\": this, \"query\": query, \"params\": ORMUtil.pgArrayParam(params) });\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    toJSON() {\n" \ 
    "        return { index: this.#index, key: this.#key };\n" \ 
    "	}\n" \ 
    "\n" \ 
    "    static fromJSON(json) {	\n" \ 
    "		if (json===null || json===undefined) return null;\n" \ 
    "		console.log(\"fromJSON(json) \" + JSON.stringify(json));\n" \ 
    "		let connection = new ORMConnection();\n" \ 
    "		connection.#index = json.index; \n" \ 
    "		connection.#key   = json.key; \n" \ 
    "		return connection;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static getDefault() { 	\n" \ 
    "		let connection = localStorage.getItem(ORMConnection.#LS_CONNECTION_DEFAULT);\n" \ 
    "		if (connection===null || connection===undefined) return null;\n" \ 
    "		return ORMConnection.fromJSON(JSON.parse(connection));\n" \ 
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
    "export class ORMUtil {\n" \ 
    "\n" \ 
    "    constructor() { throw new Error(\"Final abstract class\"); }\n" \ 
    "\n" \ 
    "	static validateString     (value) { if (value!==null && value!==undefined && (typeof value)!==\"string\"  && !(value instanceof String))  throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not string`);  }\n" \ 
    "	static validateNumber     (value) { if (value!==null && value!==undefined && (typeof value)!==\"number\"  && !(value instanceof Number))  throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not number`);  }\n" \ 
    "	static validateBoolean    (value) { if (value!==null && value!==undefined && (typeof value)!==\"boolean\" && !(value instanceof Boolean)) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not boolean`); }\n" \ 
    "	static validateObject     (value) { if (value!==null && value!==undefined && (typeof value)!==\"object\")       throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not object`);      }\n" \ 
    "	static validateDate       (value) { if (value!==null && value!==undefined && !(value instanceof Date))        throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not Date`);        }\n" \ 
    "	static validateUint8Array (value) { if (value!==null && value!==undefined && !(value instanceof Uint8Array))  throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not Uint8Array`);  }\n" \ 
    "	static validateXMLDocument(value) { if (value!==null && value!==undefined && !(value instanceof XMLDocument)) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(value)} is not XMLDocument`); }\n" \ 
    "\n" \ 
    "	static validateArray(array, validateElementFunc) { \n" \ 
    "		if (array===null || array===undefined) return;\n" \ 
    "		if (!(array instanceof Array)) throw new ORMError(303, `Value ${ORMUtil._valueToStringShort(array)} is not Array`);  \n" \ 
    "		array.forEach( element => element instanceof Array ? validateArray(element, validateElementFunc) : validateElementFunc(element) );\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static validateArrayString     (array) { ORMUtil.validateArray(array, ORMUtil.validateString);      }\n" \ 
    "	static validateArrayNumber     (array) { ORMUtil.validateArray(array, ORMUtil.validateNumber);      }\n" \ 
    "	static validateArrayBoolean    (array) { ORMUtil.validateArray(array, ORMUtil.validateBoolean);     }\n" \ 
    "	static validateArrayObject     (array) { ORMUtil.validateArray(array, ORMUtil.validateObject);      }\n" \ 
    "	static validateArrayDate       (array) { ORMUtil.validateArray(array, ORMUtil.validateDate);        }\n" \ 
    "	static validateArrayUint8Array (array) { ORMUtil.validateArray(array, ORMUtil.validateUint8Array);  }\n" \ 
    "	static validateArrayXMLDocument(array) { ORMUtil.validateArray(array, ORMUtil.validateXMLDocument); }\n" \ 
    "\n" \ 
    "	static _valueToStringShort(value) { return JSON.stringify(value.toString().length<=15 ? value.toString() : value.toString().substr(1,12)+\"...\"); }\n" \ 
    "\n" \ 
    "	static jsString      (pgValue) { return pgValue; }\n" \ 
    "	static jsNumber      (pgValue) { return pgValue!==null ? +pgValue : null; }\n" \ 
    "	static jsBoolean     (pgValue) { return pgValue!==null ? pgValue==='t' : null; }\n" \ 
    "	static jsObject      (pgValue) { return JSON.parse(pgValue); }\n" \ 
    "	static jsDate        (pgValue) { return pgValue!==null ? new Date( (pgValue.charAt(4)!=='-' ? '1970-01-01 ' : '') + pgValue + (pgValue.length===10 ? ' 00:00:00' : '')) : null; }\n" \ 
    "	static jsDateInterval(pgValue) { return pgValue!==null ? eval( (\"0+\"+pgValue).replaceAll('s','').replaceAll(' ','').replace('year','*365.25*24*60*60+').replace('month','*30.4375*24*60*60+').replace('day','*24*60*60+').replace(':','*60*60+').replace(':','*60+').replaceAll('+0','+') )*1000 : null; }\n" \ 
    "	static jsUint8Array  (pgValue) { return pgValue!==null ? new Uint8Array([...pgValue].map((c,i) => i && !(i%2) ? parseInt(pgValue.substr(i,2),16) : null).filter(b => b!==null)) : null; }\n" \ 
    "	static jsXMLDocument (pgValue) { return pgValue!==null ? new DOMParser().parseFromString(pgValue, \"text/xml\") : null; }\n" \ 
    "\n" \ 
    "	static pgText          (jsValue) { return jsValue!==undefined ? jsValue : null; }\n" \ 
    "	static pgNumeric       (jsValue) { return jsValue!==null && jsValue!==undefined ? jsValue.toString() : null; }\n" \ 
    "	static pgBoolean       (jsValue) { return jsValue!==null && jsValue!==undefined ? jsValue ? 't' : 'f' : null; }\n" \ 
    "	static pgJSON          (jsValue) { return jsValue!==null && jsValue!==undefined ? JSON.stringify(jsValue) : null; }\n" \ 
    "	static pgDate          (jsValue) { return jsValue!==null && jsValue!==undefined ? jsValue.getFullYear()+\"-\"+(jsValue.getMonth()+1).toString().padStart(2,\"0\")+\"-\"+jsValue.getDate().toString().padStart(2,\"0\") : null; }\n" \ 
    "	static pgTime          (jsValue) { return jsValue!==null && jsValue!==undefined ? jsValue.getHours().toString().padStart(2,\"0\")+\":\"+jsValue.getMinutes().toString().padStart(2,\"0\")+\":\"+jsValue.getSeconds().toString().padStart(2,\"0\")+\".\"+jsValue.getMilliseconds().toString().padStart(3,\"0\") : null; }\n" \ 
    "	static pgTimezoneOffset(jsValue) { return jsValue!==null && jsValue!==undefined ? (jsValue.getTimezoneOffset()>0 ? \"-\" : \"+\")+(Math.abs(jsValue.getTimezoneOffset())/60|0).toString().padStart(2,\"0\")+(Math.abs(jsValue.getTimezoneOffset())%60).toString().padStart(2,\"0\") : null; }\n" \ 
    "	static pgTimetz        (jsValue) { return jsValue!==null && jsValue!==undefined ? ORMUtil.pgTime(jsValue)+ORMUtil.pgTimezoneOffset(jsValue) : null; }\n" \ 
    "	static pgTimestamp     (jsValue) { return jsValue!==null && jsValue!==undefined ? ORMUtil.pgDate(jsValue)+\"T\"+ORMUtil.pgTime(jsValue) : null; }\n" \ 
    "	static pgTimestamptz   (jsValue) { return jsValue!==null && jsValue!==undefined ? ORMUtil.pgDate(jsValue)+\"T\"+ORMUtil.pgTime(jsValue)+ORMUtil.pgTimezoneOffset(jsValue) : null; }\n" \ 
    "	static pgInterval      (jsValue) { return jsValue!==null && jsValue!==undefined ? (jsValue/(24*60*60*1000)|0)+\" day \"+(jsValue%(24*60*60*1000)/(60*60*1000)|0).toString().padStart(2,\"0\")+\":\"+(jsValue%(60*60*1000)/(60*1000)|0).toString().padStart(2,\"0\")+\":\"+(jsValue%(60*1000)/(1000)|0).toString().padStart(2,\"0\")+\".\"+(jsValue%1000).toString().padEnd(3,\"0\") : null; }\n" \ 
    "	static pgBytea         (jsValue) { if (jsValue===null || jsValue===undefined) return null; let pgValue=\"\\\\x\"; jsValue.forEach(byte=>{ pgValue += byte.toString(16).padStart(2,'0'); }); return pgValue; }\n" \ 
    "	static pgXML           (jsValue) { return jsValue!==null && jsValue!==undefined ? new XMLSerializer().serializeToString(jsValue) : null; }\n" \ 
    "\n" \ 
    "	static jsArray(pgArray, jsElementFunc) { \n" \ 
    "		if (pgArray===null) return null;\n" \ 
    "		let pgArrayLen = pgArray.length;\n" \ 
    "		if (pgArrayLen<2 || pgArray[0]!=='{' || pgArray[pgArrayLen-1]!=='}') \n" \ 
    "			throw new ORMError(312, `Incorrect format of postgres array ${ORMUtil._valueToStringShort(pgArray)}`);\n" \ 
    "		let array = [];\n" \ 
    "		let posBeg = 1;\n" \ 
    "	    while(posBeg<pgArrayLen) {\n" \ 
    "			if (pgArray[posBeg]==='{') {\n" \ 
    "				let posEnd = posBeg+1;\n" \ 
    "				for(let quoted=false, depth=1; posEnd<pgArrayLen-1; posEnd++)  {\n" \ 
    "					let char = pgArray.charAt(posEnd);\n" \ 
    "					if (quoted) {\n" \ 
    "					 	if (char==='\"' && pgArray.charAt(posEnd-1)!='\\\\') quoted = false;\n" \ 
    "						continue;\n" \ 
    "					} \n" \ 
    "					if (char==='\"') quoted = true;\n" \ 
    "					else if (char==='{') depth++;\n" \ 
    "					else if (char==='}' && --depth==0) break;\n" \ 
    "				}\n" \ 
    "				array.push(ORMUtil.jsArray(pgArray.substring(posBeg,posEnd+1), jsElementFunc));\n" \ 
    "				posBeg = posEnd + 2;			\n" \ 
    "				continue;\n" \ 
    "			}\n" \ 
    "			let quoted = pgArray[posBeg]==='\"';\n" \ 
    "			let posEnd = posBeg;\n" \ 
    "			if (quoted) {\n" \ 
    "				do {\n" \ 
    "					posEnd = pgArray.indexOf('\"',posEnd+1);\n" \ 
    "					if (posEnd<0) \n" \ 
    "						throw new ORMError(312, `Incorrect format of postgres array ${ORMUtil._valueToStringShort(pgArray)}`);\n" \ 
    "				} while(pgArray[posEnd-1]==='\\\\');\n" \ 
    "				posEnd++;\n" \ 
    "			} else {\n" \ 
    "				posEnd = pgArray.indexOf(',',posEnd);\n" \ 
    "				if (posEnd<0) posEnd = pgArrayLen-1;\n" \ 
    "			}\n" \ 
    "			let value = pgArray.substring(posBeg,posEnd);\n" \ 
    "			posBeg = posEnd+1;\n" \ 
    "			if (!quoted && value==='NULL') value = null;\n" \ 
    "			if (quoted && value!==null) value = JSON.parse(value);\n" \ 
    "			array.push(jsElementFunc(value));\n" \ 
    "		}\n" \ 
    "		return array;\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static jsArrayString      (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsString);       }\n" \ 
    "	static jsArrayNumber      (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsNumber);       }\n" \ 
    "	static jsArrayBoolean     (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsBoolean);      }\n" \ 
    "	static jsArrayObject      (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsObject);       }\n" \ 
    "	static jsArrayDate        (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsDate);         }\n" \ 
    "	static jsArrayDateInterval(pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsDateInterval); }\n" \ 
    "	static jsArrayUint8Array  (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsUint8Array);   }\n" \ 
    "	static jsArrayXMLDocument (pgArray) { return ORMUtil.jsArray(pgArray, ORMUtil.jsXMLDocument);  }\n" \ 
    "\n" \ 
    "	static pgArray(jsArray, pgElementFunc) { return jsArray!==null && jsArray!==undefined ? \"{\" + jsArray.map( (element,index) => element===null || element===undefined ? \"NULL\" : element instanceof Array ? ORMUtil.pgArray(element, pgElementFunc) : JSON.stringify(pgElementFunc(element)) ).join() + \"}\" : null; }\n" \ 
    "\n" \ 
    "	static pgArrayText          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgText);      }\n" \ 
    "	static pgArrayNumeric       (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgNumeric);      }\n" \ 
    "	static pgArrayBoolean       (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgBoolean);      }\n" \ 
    "	static pgArrayJSON          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgJSON);           }\n" \ 
    "	static pgArrayDate          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgDate);           }\n" \ 
    "	static pgArrayTime          (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTime);           }\n" \ 
    "	static pgArrayTimezoneOffset(jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimezoneOffset); }\n" \ 
    "	static pgArrayTimetz        (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimetz);         }\n" \ 
    "	static pgArrayTimestamp     (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimestamp);      }\n" \ 
    "	static pgArrayTimestamptz   (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgTimestamptz);    }\n" \ 
    "	static pgArrayInterval      (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgInterval);       }\n" \ 
    "	static pgArrayBytea         (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgBytea);          }\n" \ 
    "	static pgArrayXML           (jsArray) { return ORMUtil.pgArray(jsArray, ORMUtil.pgXML);            }\n" \ 
    "\n" \ 
    "	static #pgParam(jsValue) { \n" \ 
    "        if (jsValue===null || jsValue===undefined) return null;\n" \ 
    "		if ((typeof jsValue)===\"number\" || (typeof jsValue)===\"boolean\") return jsValue;\n" \ 
    "		if (jsValue instanceof Date) return ORMUtil.pgTimestamptz(jsValue);\n" \ 
    "		if (jsValue instanceof Uint8Array) return ORMUtil.pgBytea(jsValue);\n" \ 
    "		if (jsValue instanceof XMLDocument) return ORMUtil.pgXML(jsValue);\n" \ 
    "		if (jsValue instanceof Record) return jsValue.toRecord();\n" \ 
    "		if (jsValue instanceof Array) return ORMUtil.pgArray(jsValue, ORMUtil.#pgParam);\n" \ 
    "		return JSON.stringify(jsValue);\n" \ 
    "	}\n" \ 
    "\n" \ 
    "	static pgArrayParam(jsArray) { return jsArray!==null && jsArray!==undefined ? jsArray instanceof Array ? jsArray.map( element => ORMUtil.#pgParam(element) ) : [ORMUtil.#pgParam(jsArray)] : []; }\n" \ 
    "\n" \ 
    "	static #compareNull  (value1,   value2,   nullsFirst)       { return value1===null ? value2!==null ? (nullsFirst ? -1 : +1) : 0 : (nullsFirst ? +1 : -1); }\n" \ 
    "	static compareNumber (number1,  number2,  desc, nullsFirst) { return number1!==null && number2!==null ? !desc ? number1-number2                : number2-number1                : ORMUtil.#compareNull(number1, number2, nullsFirst); }\n" \ 
    "	static compareString (string1,  string2,  desc, nullsFirst) { return string1!==null && string2!==null ? !desc ? string1.localeCompare(string2) : string2.localeCompare(string1) : ORMUtil.#compareNull(string1, string2, nullsFirst); }\n" \ 
    "	static compareBoolean(boolean1, boolean2, desc, nullsFirst) { return ORMUtil.compareNumber(boolean1!==null ? boolean1|0      : null, boolean2!==null ? boolean2|0      : null, desc, nullsFirst); }\n" \ 
    "	static compareDate   (date1,    date2,    desc, nullsFirst) { return ORMUtil.compareNumber(date1!==null    ? date1.getTime() : null, date2!==null    ? date2.getTime() : null, desc, nullsFirst); }\n" \ 
    "\n" \ 
    "}\n" \ 
    "\n" \ 
    "function sendRequest(path, body) {\n" \ 
    "	let xhr = new XMLHttpRequest();\n" \ 
    "	try {\n" \ 
    "		xhr.open(\"POST\", path, false);\n" \ 
    "		xhr.setRequestHeader(\"Content-Type\", \"application/json;charset=UTF-8\");	\n" \ 
    "		xhr.send(JSON.stringify(body));\n" \ 
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
