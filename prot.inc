/* libobject.c */
ApeObject_t ape_object_make_number(ApeContext_t *ctx, ApeFloat_t val);
ApeObject_t ape_object_make_bool(ApeContext_t *ctx, _Bool val);
ApeObject_t ape_object_make_null(ApeContext_t *ctx);
ApeObject_t ape_object_make_external(ApeContext_t *ctx, void *ptr);
_Bool ape_object_value_ishashable(ApeObject_t obj);
void ape_tostring_object(ApeObject_t obj, ApeWriter_t *buf, _Bool quote_str);
const char *ape_object_value_typename(const ApeObjectType_t type);
void ape_object_data_deinit(ApeObjectData_t *data);
char *ape_object_value_typeunionname(ApeContext_t *ctx, const ApeObjectType_t type);
char *ape_object_value_serialize(ApeContext_t *ctx, ApeObject_t object, ApeSize_t *lendest);
_Bool ape_object_value_setexternaldata(ApeObject_t object, void *ext_data);
_Bool ape_object_value_setexternalcopy(ApeObject_t object, ApeDataCallback_t copy_fn);
ApeObject_t ape_object_getkvpairat(ApeContext_t *ctx, ApeObject_t object, int ix);
ApeObject_t ape_object_value_internalcopydeep(ApeContext_t *ctx, ApeObject_t obj, ApeValDictionary_t *copies);
_Bool ape_object_value_wrapequals(const ApeObject_t *a_ptr, const ApeObject_t *b_ptr);
unsigned long ape_object_value_hash(ApeObject_t *obj_ptr);
ApeObject_t ape_object_value_copydeep(ApeContext_t *ctx, ApeObject_t obj);
ApeObject_t ape_object_value_copyflat(ApeContext_t *ctx, ApeObject_t obj);
ApeFloat_t ape_object_value_asnumerica(ApeObject_t obj, ApeObjectType_t t);
ApeFloat_t ape_object_value_asnumeric(ApeObject_t obj);
ApeFloat_t ape_object_value_compare(ApeObject_t a, ApeObject_t b, _Bool *out_ok);
_Bool ape_object_value_equals(ApeObject_t a, ApeObject_t b);
_Bool ape_object_value_setexternaldestroy(ApeObject_t object, ApeDataCallback_t destroy_fn);
/* error.c */
ApeTraceback_t *ape_make_traceback(ApeContext_t *ctx);
void ape_traceback_destroy(ApeTraceback_t *traceback);
_Bool ape_traceback_append(ApeTraceback_t *traceback, const char *function_name, ApePosition_t pos);
_Bool ape_traceback_appendfromvm(ApeTraceback_t *traceback, ApeVM_t *vm);
const char *ape_traceback_itemgetfilepath(ApeTracebackItem_t *item);
_Bool ape_tostring_traceback(ApeTraceback_t *traceback, ApeWriter_t *buf);
const char *ape_tostring_errortype(ApeErrorType_t type);
ApeErrorType_t ape_error_gettype(ApeError_t *error);
const char *ape_error_gettypestring(ApeError_t *error);
ApeObject_t ape_object_make_error(ApeContext_t *ctx, const char *error);
ApeObject_t ape_object_make_error_nocopy(ApeContext_t *ctx, char *error);
void ape_errorlist_initerrors(ApeErrorList_t *errors);
void ape_errorlist_destroy(ApeErrorList_t *errors);
void ape_errorlist_add(ApeErrorList_t *errors, ApeErrorType_t type, ApePosition_t pos, const char *message);
void ape_errorlist_addformat(ApeErrorList_t *errors, ApeErrorType_t type, ApePosition_t pos, const char *format, ...);
void ape_errorlist_clear(ApeErrorList_t *errors);
ApeSize_t ape_errorlist_count(ApeErrorList_t *errors);
ApeError_t *ape_errorlist_getat(ApeErrorList_t *errors, ApeInt_t ix);
ApeError_t *ape_errorlist_lasterror(ApeErrorList_t *errors);
_Bool ape_errorlist_haserrors(ApeErrorList_t *errors);
const char *ape_error_getmessage(ApeError_t *error);
const char *ape_error_getfile(ApeError_t *error);
const char *ape_error_getsource(ApeError_t *error);
int ape_error_getline(ApeError_t *error);
int ape_error_getcolumn(ApeError_t *error);
ApeTraceback_t *ape_error_gettraceback(ApeError_t *error);
const char *ape_object_value_geterrormessage(ApeObject_t object);
void ape_object_value_seterrortraceback(ApeObject_t object, ApeTraceback_t *traceback);
ApeTraceback_t *ape_object_value_geterrortraceback(ApeObject_t object);
/* vm.c */
ApeCompiledFile_t *ape_make_compiledfile(ApeContext_t *ctx, const char *path);
void *compiled_file_destroy(ApeCompiledFile_t *file);
ApeGlobalStore_t *ape_make_globalstore(ApeContext_t *ctx, ApeGCMemory_t *mem);
void ape_globalstore_destroy(ApeGlobalStore_t *store);
const ApeSymbol_t *ape_globalstore_getsymbol(ApeGlobalStore_t *store, const char *name);
_Bool ape_globalstore_set(ApeGlobalStore_t *store, const char *name, ApeObject_t object);
ApeObject_t ape_globalstore_getat(ApeGlobalStore_t *store, int ix, _Bool *out_ok);
ApeObject_t *ape_globalstore_getobjectdata(ApeGlobalStore_t *store);
ApeSize_t ape_globalstore_getobjectcount(ApeGlobalStore_t *store);
ApeSymbol_t *ape_make_symbol(ApeContext_t *ctx, const char *name, ApeSymbolType_t type, int index, _Bool assignable);
void *ape_symbol_destroy(ApeSymbol_t *symbol);
ApeSymbol_t *ape_symbol_copy(ApeSymbol_t *symbol);
ApeSymbolTable_t *ape_make_symtable(ApeContext_t *ctx, ApeSymbolTable_t *outer, ApeGlobalStore_t *global_store, int module_global_offset);
void ape_symtable_destroy(ApeSymbolTable_t *table);
ApeSymbolTable_t *ape_symtable_copy(ApeSymbolTable_t *table);
_Bool ape_symtable_addmodulesymbol(ApeSymbolTable_t *st, ApeSymbol_t *symbol);
const ApeSymbol_t *ape_symtable_define(ApeSymbolTable_t *table, const char *name, _Bool assignable);
const ApeSymbol_t *ape_symtable_deffree(ApeSymbolTable_t *st, const ApeSymbol_t *original);
const ApeSymbol_t *ape_symtable_definefuncname(ApeSymbolTable_t *st, const char *name, _Bool assignable);
const ApeSymbol_t *ape_symtable_definethis(ApeSymbolTable_t *st);
const ApeSymbol_t *ape_symtable_resolve(ApeSymbolTable_t *table, const char *name);
_Bool ape_symtable_symbol_is_defined(ApeSymbolTable_t *table, const char *name);
_Bool ape_symtable_pushblockscope(ApeSymbolTable_t *table);
void ape_symtable_popblockscope(ApeSymbolTable_t *table);
ApeBlockScope_t *ape_symtable_getblockscope(ApeSymbolTable_t *table);
_Bool ape_symtable_ismoduleglobalscope(ApeSymbolTable_t *table);
_Bool ape_symtable_istopblockscope(ApeSymbolTable_t *table);
_Bool ape_symtable_istopglobalscope(ApeSymbolTable_t *table);
ApeSize_t ape_symtable_getmoduleglobalsymbolcount(const ApeSymbolTable_t *table);
const ApeSymbol_t *ape_symtable_getmoduleglobalsymbolat(const ApeSymbolTable_t *table, int ix);
ApeBlockScope_t *ape_make_blockscope(ApeContext_t *ctx, int offset);
void *ape_blockscope_destroy(ApeBlockScope_t *scope);
ApeBlockScope_t *ape_blockscope_copy(ApeBlockScope_t *scope);
int ape_make_code(ApeOpByte_t op, ApeSize_t operands_count, uint64_t *operands, ApeArray_t *res);
ApeCompilationScope_t *ape_make_compscope(ApeContext_t *ctx, ApeCompilationScope_t *outer);
void ape_compscope_destroy(ApeCompilationScope_t *scope);
ApeCompilationResult_t *ape_compscope_orphanresult(ApeCompilationScope_t *scope);
ApeCompilationResult_t *ape_make_compresult(ApeContext_t *ctx, ApeUShort_t *bytecode, ApePosition_t *src_positions, int count);
void ape_compresult_destroy(ApeCompilationResult_t *res);
ApeExpression_t *ape_optimizer_optexpr(ApeExpression_t *expr);
ApeExpression_t *ape_optimizer_optinfixexpr(ApeExpression_t *expr);
ApeExpression_t *ape_optimizer_optprefixexpr(ApeExpression_t *expr);
ApeModule_t *ape_make_module(ApeContext_t *ctx, const char *name);
void *ape_module_destroy(ApeModule_t *module);
ApeModule_t *ape_module_copy(ApeModule_t *src);
const char *ape_module_getname(const char *path);
_Bool ape_module_addsymbol(ApeModule_t *module, const ApeSymbol_t *symbol);
_Bool ape_frame_init(ApeFrame_t *frame, ApeObject_t function_obj, int base_pointer);
ApeOpcodeValue_t ape_frame_readopcode(ApeFrame_t *frame);
uint64_t ape_frame_readuint64(ApeFrame_t *frame);
uint16_t ape_frame_readuint16(ApeFrame_t *frame);
ApeUShort_t ape_frame_readuint8(ApeFrame_t *frame);
ApePosition_t ape_frame_srcposition(const ApeFrame_t *frame);
ApeObject_t ape_vm_getlastpopped(ApeVM_t *vm);
_Bool ape_vm_haserrors(ApeVM_t *vm);
_Bool ape_vm_setglobal(ApeVM_t *vm, ApeSize_t ix, ApeObject_t val);
ApeObject_t ape_vm_getglobal(ApeVM_t *vm, ApeSize_t ix);
void ape_vm_pushstack(ApeVM_t *vm, ApeObject_t obj);
ApeObject_t ape_vm_popstack(ApeVM_t *vm);
ApeObject_t ape_vm_getstack(ApeVM_t *vm, int nth_item);
_Bool ape_vm_pushframe(ApeVM_t *vm, ApeFrame_t frame);
_Bool ape_vm_popframe(ApeVM_t *vm);
void ape_vm_collectgarbage(ApeVM_t *vm, ApeArray_t *constants);
_Bool ape_vm_callobject(ApeVM_t *vm, ApeObject_t callee, ApeInt_t num_args);
ApeObject_t ape_vm_callnativefunction(ApeVM_t *vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t *args);
_Bool ape_vm_checkassign(ApeVM_t *vm, ApeObject_t oldval, ApeObject_t newval);
ApeVM_t *ape_vm_make(ApeContext_t *ctx, const ApeConfig_t *config, ApeGCMemory_t *mem, ApeErrorList_t *errors, ApeGlobalStore_t *global_store);
void ape_vm_destroy(ApeVM_t *vm);
void ape_vm_reset(ApeVM_t *vm);
_Bool ape_vm_run(ApeVM_t *vm, ApeCompilationResult_t *comp_res, ApeArray_t *constants);
_Bool ape_vm_appendstring(ApeVM_t *vm, ApeObject_t left, ApeObject_t right, ApeObjectType_t lefttype, ApeObjectType_t righttype);
_Bool ape_vm_executefunction(ApeVM_t *vm, ApeObject_t function, ApeArray_t *constants);
/* util.c */
char *ape_util_stringfmt(ApeContext_t *ctx, const char *format, ...);
ApeUInt_t ape_util_floattouint(ApeFloat_t val);
ApeFloat_t ape_util_uinttofloat(ApeUInt_t val);
_Bool ape_util_timersupported(void);
ApeTimer_t ape_util_timerstart(void);
ApeFloat_t ape_util_timergetelapsed(const ApeTimer_t *timer);
char *ape_util_strndup(ApeContext_t *ctx, const char *string, size_t n);
char *ape_util_strdup(ApeContext_t *ctx, const char *string);
unsigned long ape_util_hashstring(const void *ptr, size_t len);
unsigned long ape_util_hashfloat(ApeFloat_t val);
unsigned int ape_util_upperpoweroftwo(unsigned int v);
char *ape_util_default_readfile(void *ptr, const char *filename);
size_t ape_util_default_writefile(void *ctx, const char *path, const char *string, size_t string_size);
size_t ape_util_default_stdoutwrite(void *ctx, const void *data, size_t size);
ApePtrArray_t *ape_util_splitstring(ApeContext_t *ctx, const char *str, const char *delimiter);
char *ape_util_joinarray(ApeContext_t *ctx, ApePtrArray_t *items, const char *with);
char *ape_util_canonicalisepath(ApeContext_t *ctx, const char *path);
_Bool ape_util_isabspath(const char *path);
_Bool ape_util_strequal(const char *a, const char *b);
/* context.c */
ApeContext_t *ape_make_context(void);
ApeContext_t *ape_make_contextex(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void *optr);
void ape_context_destroy(ApeContext_t *ctx);
void ape_context_freeallocated(ApeContext_t *ctx, void *ptr);
void ape_context_setreplmode(ApeContext_t *ctx, _Bool enabled);
_Bool ape_context_settimeout(ApeContext_t *ctx, ApeFloat_t max_execution_time_ms);
void ape_context_setstdoutwrite(ApeContext_t *ctx, ApeIOStdoutWriteFunc_t stdout_write, void *context);
void ape_context_setfilewrite(ApeContext_t *ctx, ApeIOWriteFunc_t file_write, void *context);
void ape_context_setfileread(ApeContext_t *ctx, ApeIOReadFunc_t file_read, void *context);
void ape_context_dumpast(ApeContext_t *ctx, ApePtrArray_t *statements);
void ape_context_dumpbytecode(ApeContext_t *ctx, ApeCompilationResult_t *cres);
ApeObject_t ape_context_executesource(ApeContext_t *ctx, const char *code, _Bool alsoreset);
ApeObject_t ape_context_executefile(ApeContext_t *ctx, const char *path);
_Bool ape_context_haserrors(ApeContext_t *ctx);
ApeSize_t ape_context_errorcount(ApeContext_t *ctx);
void ape_context_clearerrors(ApeContext_t *ctx);
ApeError_t *ape_context_geterror(ApeContext_t *ctx, int index);
_Bool ape_context_setnativefunction(ApeContext_t *ctx, const char *name, ApeWrappedNativeFunc_t fn, void *data);
_Bool ape_context_setglobal(ApeContext_t *ctx, const char *name, ApeObject_t obj);
char *ape_context_errortostring(ApeContext_t *ctx, ApeError_t *err);
void ape_context_deinit(ApeContext_t *ctx);
ApeObject_t ape_context_makenamednative(ApeContext_t *ctx, const char *name, ApeWrappedNativeFunc_t fn, void *data);
void ape_context_resetstate(ApeContext_t *ctx);
void ape_context_setdefaultconfig(ApeContext_t *ctx);
/* libmap.c */
ApeValDictionary_t *ape_make_valdict_actual(ApeContext_t *ctx, ApeSize_t key_size, ApeSize_t val_size);
ApeValDictionary_t *ape_make_valdictcapacity(ApeContext_t *ctx, ApeSize_t min_capacity, ApeSize_t key_size, ApeSize_t val_size);
void ape_valdict_destroy(ApeValDictionary_t *dict);
void ape_valdict_sethashfunction(ApeValDictionary_t *dict, ApeDataHashFunc_t hash_fn);
void ape_valdict_setequalsfunction(ApeValDictionary_t *dict, ApeDataEqualsFunc_t equals_fn);
_Bool ape_valdict_set(ApeValDictionary_t *dict, void *key, void *value);
void *ape_valdict_get(const ApeValDictionary_t *dict, const void *key);
void *ape_valdict_getkeyat(const ApeValDictionary_t *dict, ApeSize_t ix);
void *ape_valdict_getvalueat(const ApeValDictionary_t *dict, ApeSize_t ix);
_Bool ape_valdict_setvalueat(const ApeValDictionary_t *dict, ApeSize_t ix, const void *value);
ApeSize_t ape_valdict_count(const ApeValDictionary_t *dict);
void ape_valdict_clear(ApeValDictionary_t *dict);
_Bool ape_valdict_init(ApeValDictionary_t *dict, ApeSize_t key_size, ApeSize_t val_size, ApeSize_t initial_capacity);
void ape_valdict_deinit(ApeValDictionary_t *dict);
ApeSize_t ape_valdict_getcellindex(const ApeValDictionary_t *dict, const void *key, unsigned long hash, _Bool *out_found);
_Bool ape_valdict_growandrehash(ApeValDictionary_t *dict);
_Bool ape_valdict_setkeyat(ApeValDictionary_t *dict, ApeSize_t ix, void *key);
_Bool ape_valdict_keysareequal(const ApeValDictionary_t *dict, const void *a, const void *b);
unsigned long ape_valdict_hashkey(const ApeValDictionary_t *dict, const void *key);
void ape_valdict_destroywithitems(ApeValDictionary_t *dict);
void ape_valdict_setcopyfunc(ApeValDictionary_t *dict, ApeDataCallback_t fn);
void ape_valdict_setdeletefunc(ApeValDictionary_t *dict, ApeDataCallback_t fn);
ApeValDictionary_t *ape_valdict_copywithitems(ApeValDictionary_t *dict);
ApeStrDictionary_t *ape_make_strdict(ApeContext_t *ctx, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn);
void ape_strdict_destroy(ApeStrDictionary_t *dict);
void ape_strdict_destroywithitems(ApeStrDictionary_t *dict);
ApeStrDictionary_t *ape_strdict_copywithitems(ApeStrDictionary_t *dict);
_Bool ape_strdict_set(ApeStrDictionary_t *dict, const char *key, void *value);
void *ape_strdict_get(const ApeStrDictionary_t *dict, const char *key);
void *ape_strdict_getvalueat(const ApeStrDictionary_t *dict, ApeSize_t ix);
const char *ape_strdict_getkeyat(const ApeStrDictionary_t *dict, ApeSize_t ix);
ApeSize_t ape_strdict_count(const ApeStrDictionary_t *dict);
_Bool ape_strdict_init(ApeStrDictionary_t *dict, ApeContext_t *ctx, ApeSize_t initial_capacity, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn);
void ape_strdict_deinit(ApeStrDictionary_t *dict, _Bool free_keys);
ApeSize_t ape_strdict_getcellindex(const ApeStrDictionary_t *dict, const char *key, unsigned long hash, _Bool *out_found);
_Bool ape_strdict_growandrehash(ApeStrDictionary_t *dict);
_Bool ape_strdict_setinternal(ApeStrDictionary_t *dict, const char *ckey, char *mkey, void *value);
ApeObject_t ape_object_make_map(ApeContext_t *ctx);
ApeObject_t ape_object_make_mapcapacity(ApeContext_t *ctx, unsigned capacity);
ApeSize_t ape_object_map_getlength(ApeObject_t object);
ApeObject_t ape_object_map_getkeyat(ApeObject_t object, ApeSize_t ix);
ApeObject_t ape_object_map_getvalueat(ApeObject_t object, ApeSize_t ix);
_Bool ape_object_map_setvalue(ApeObject_t object, ApeObject_t key, ApeObject_t val);
ApeObject_t ape_object_map_getvalueobject(ApeObject_t object, ApeObject_t key);
_Bool ape_object_map_setnamedvalue(ApeObject_t obj, const char *key, ApeObject_t value);
_Bool ape_object_map_setnamedstring(ApeObject_t obj, const char *key, const char *string);
_Bool ape_object_map_setnamednumber(ApeObject_t obj, const char *key, ApeFloat_t number);
_Bool ape_object_map_setnamedbool(ApeObject_t obj, const char *key, _Bool value);
/* main.c */
int main(int argc, char *argv[]);
/* ccparse.c */
ApeExpression_t *ape_ast_make_identexpr(ApeContext_t *ctx, ApeIdent_t *ident);
ApeExpression_t *ape_ast_make_numberliteralexpr(ApeContext_t *ctx, ApeFloat_t val);
ApeExpression_t *ape_ast_make_boolliteralexpr(ApeContext_t *ctx, _Bool val);
ApeExpression_t *ape_ast_make_stringliteralexpr(ApeContext_t *ctx, char *value);
ApeExpression_t *ape_ast_make_nullliteralexpr(ApeContext_t *ctx);
ApeExpression_t *ape_ast_make_arrayliteralexpr(ApeContext_t *ctx, ApePtrArray_t *values);
ApeExpression_t *ape_ast_make_mapliteralexpr(ApeContext_t *ctx, ApePtrArray_t *keys, ApePtrArray_t *values);
ApeExpression_t *ape_ast_make_prefixexpr(ApeContext_t *ctx, ApeOperator_t op, ApeExpression_t *right);
ApeExpression_t *ape_ast_make_infixexpr(ApeContext_t *ctx, ApeOperator_t op, ApeExpression_t *left, ApeExpression_t *right);
ApeExpression_t *ape_ast_make_fnliteralexpr(ApeContext_t *ctx, ApePtrArray_t *params, ApeCodeblock_t *body);
ApeExpression_t *ape_ast_make_callexpr(ApeContext_t *ctx, ApeExpression_t *function, ApePtrArray_t *args);
ApeExpression_t *ape_ast_make_indexexpr(ApeContext_t *ctx, ApeExpression_t *left, ApeExpression_t *index);
ApeExpression_t *ape_ast_make_assignexpr(ApeContext_t *ctx, ApeExpression_t *dest, ApeExpression_t *source, _Bool ispostfix);
ApeExpression_t *ape_ast_make_logicalexpr(ApeContext_t *ctx, ApeOperator_t op, ApeExpression_t *left, ApeExpression_t *right);
ApeExpression_t *ape_ast_make_ternaryexpr(ApeContext_t *ctx, ApeExpression_t *test, ApeExpression_t *iftrue, ApeExpression_t *iffalse);
void *ape_ast_destroy_expr(ApeExpression_t *expr);
ApeExpression_t *ape_ast_copy_expr(ApeExpression_t *expr);
ApeStatement_t *ape_ast_make_definestmt(ApeContext_t *ctx, ApeIdent_t *name, ApeExpression_t *value, _Bool assignable);
ApeStatement_t *ape_ast_make_ifstmt(ApeContext_t *ctx, ApePtrArray_t *cases, ApeCodeblock_t *alternative);
ApeStatement_t *ape_ast_make_returnstmt(ApeContext_t *ctx, ApeExpression_t *value);
ApeStatement_t *ape_ast_make_expressionstmt(ApeContext_t *ctx, ApeExpression_t *value);
ApeStatement_t *ape_ast_make_whileloopstmt(ApeContext_t *ctx, ApeExpression_t *test, ApeCodeblock_t *body);
ApeStatement_t *ape_ast_make_breakstmt(ApeContext_t *ctx);
ApeStatement_t *ape_ast_make_foreachstmt(ApeContext_t *ctx, ApeIdent_t *iterator, ApeExpression_t *source, ApeCodeblock_t *body);
ApeStatement_t *ape_ast_make_forloopstmt(ApeContext_t *ctx, ApeStatement_t *init, ApeExpression_t *test, ApeExpression_t *update, ApeCodeblock_t *body);
ApeStatement_t *ape_ast_make_continuestmt(ApeContext_t *ctx);
ApeStatement_t *ape_ast_make_blockstmt(ApeContext_t *ctx, ApeCodeblock_t *block);
ApeStatement_t *ape_ast_make_importstmt(ApeContext_t *ctx, char *path);
ApeStatement_t *ape_ast_make_recoverstmt(ApeContext_t *ctx, ApeIdent_t *errorident, ApeCodeblock_t *body);
void *ape_ast_destroy_stmt(ApeStatement_t *stmt);
ApeStatement_t *ape_ast_copy_stmt(const ApeStatement_t *stmt);
ApeCodeblock_t *ape_ast_make_code_block(ApeAllocator_t *alloc, ApePtrArray_t *statements);
void *ape_ast_destroy_codeblock(ApeCodeblock_t *block);
ApeCodeblock_t *ape_ast_copy_codeblock(ApeCodeblock_t *block);
void *ape_ast_destroy_ident(ApeIdent_t *ident);
ApeIfCase_t *ape_ast_make_if_case(ApeAllocator_t *alloc, ApeExpression_t *test, ApeCodeblock_t *consequence);
void *ape_ast_destroy_ifcase(ApeIfCase_t *cond);
ApeIfCase_t *ape_ast_copy_ifcase(ApeIfCase_t *ifcase);
ApeExpression_t *ape_ast_make_expression(ApeContext_t *ctx, ApeExprType_t type);
ApeStatement_t *ape_ast_make_statement(ApeContext_t *ctx, ApeStatementType_t type);
ApeParser_t *ape_ast_make_parser(ApeContext_t *ctx, const ApeConfig_t *config, ApeErrorList_t *errors);
void ape_parser_destroy(ApeParser_t *parser);
ApePtrArray_t *ape_parser_parseall(ApeParser_t *parser, const char *input, ApeCompiledFile_t *file);
ApeStatement_t *ape_parser_parsestmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parsedefinestmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseifstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parsereturnstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseexprstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parsewhileloopstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parsebreakstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parsecontinuestmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseblockstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseimportstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parserecoverstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseforloopstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseforeachstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parseclassicforstmt(ApeParser_t *p);
ApeStatement_t *ape_parser_parsefuncstmt(ApeParser_t *p);
ApeCodeblock_t *ape_parser_parsecodeblock(ApeParser_t *p);
ApeExpression_t *ape_parser_parseexpr(ApeParser_t *p, ApePrecedence_t prec);
ApeExpression_t *ape_parser_parseident(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralnumber(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralbool(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralstring(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteraltplstring(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralnull(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralarray(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralmap(ApeParser_t *p);
ApeExpression_t *ape_parser_parseprefixexpr(ApeParser_t *p);
ApeExpression_t *ape_parser_parseinfixexpr(ApeParser_t *p, ApeExpression_t *left);
ApeExpression_t *ape_parser_parsegroupedexpr(ApeParser_t *p);
ApeExpression_t *ape_parser_parseliteralfunc(ApeParser_t *p);
_Bool ape_parser_parsefuncparams(ApeParser_t *p, ApePtrArray_t *outparams);
ApeExpression_t *ape_parser_parsecallexpr(ApeParser_t *p, ApeExpression_t *left);
ApePtrArray_t *ape_parser_parseexprlist(ApeParser_t *p, ApeTokenType_t starttoken, ApeTokenType_t endtoken, _Bool trailingcommaallowed);
ApeExpression_t *ape_parser_parseindexexpr(ApeParser_t *p, ApeExpression_t *left);
ApeExpression_t *ape_parser_parseassignexpr(ApeParser_t *p, ApeExpression_t *left);
ApeExpression_t *ape_parser_parselogicalexpr(ApeParser_t *p, ApeExpression_t *left);
ApeExpression_t *ape_parser_parseternaryexpr(ApeParser_t *p, ApeExpression_t *left);
ApeExpression_t *ape_parser_parseincdecprefixexpr(ApeParser_t *p);
ApeExpression_t *ape_parser_parseincdecpostfixexpr(ApeParser_t *p, ApeExpression_t *left);
ApeExpression_t *ape_parser_parsedotexpr(ApeParser_t *p, ApeExpression_t *left);
/* libstring.c */
ApeObject_t ape_object_make_stringlen(ApeContext_t *ctx, const char *string, ApeSize_t len);
ApeObject_t ape_object_make_string(ApeContext_t *ctx, const char *string);
ApeObject_t ape_object_make_stringcapacity(ApeContext_t *ctx, ApeSize_t capacity);
const char *ape_object_string_getdata(ApeObject_t object);
char *ape_object_string_getinternalobjdata(ApeObjectData_t *data);
_Bool ape_object_string_reservecapacity(ApeObjectData_t *data, ApeSize_t capacity);
ApeSize_t ape_object_string_getlength(ApeObject_t object);
void ape_object_string_setlength(ApeObject_t object, ApeSize_t len);
char *ape_object_string_getmutable(ApeObject_t object);
_Bool ape_object_string_append(ApeObject_t obj, const char *src, ApeSize_t len);
unsigned long ape_object_string_gethash(ApeObject_t obj);
/* tostring.c */
ApeOpcodeDefinition_t *ape_tostring_opcodefind(ApeOpByte_t op);
const char *ape_tostring_opcodename(ApeOpByte_t op);
_Bool ape_tostring_stmtlist(ApePtrArray_t *statements, ApeWriter_t *buf);
_Bool ape_tostring_statement(const ApeStatement_t *stmt, ApeWriter_t *buf);
_Bool ape_tostring_expression(ApeExpression_t *expr, ApeWriter_t *buf);
_Bool ape_tostring_codeblock(const ApeCodeblock_t *stmt, ApeWriter_t *buf);
const char *ape_tostring_operator(ApeOperator_t op);
const char *ape_tostring_exprtype(ApeExprType_t type);
_Bool ape_tostring_compresult(ApeCompilationResult_t *res, ApeWriter_t *buf);
_Bool ape_tostring_code(ApeUShort_t *code, ApePosition_t *source_positions, size_t code_size, ApeWriter_t *res);
const char *ape_tostring_tokentype(ApeTokenType_t type);
/* ccom.c */
ApeCompiler_t *ape_compiler_make(ApeContext_t *ctx, const ApeConfig_t *config, ApeGCMemory_t *mem, ApeErrorList_t *errors, ApePtrArray_t *files, ApeGlobalStore_t *globalstore);
void ape_compiler_destroy(ApeCompiler_t *comp);
ApeCompilationResult_t *ape_compiler_compilesource(ApeCompiler_t *comp, const char *code);
ApeCompilationResult_t *ape_compiler_compilefile(ApeCompiler_t *comp, const char *path);
ApeSymbolTable_t *ape_compiler_getsymboltable(ApeCompiler_t *comp);
ApeArray_t *ape_compiler_getconstants(const ApeCompiler_t *comp);
_Bool ape_compiler_init(ApeCompiler_t *comp, ApeContext_t *ctx, const ApeConfig_t *config, ApeGCMemory_t *mem, ApeErrorList_t *errors, ApePtrArray_t *files, ApeGlobalStore_t *globalstore);
void ape_compiler_deinit(ApeCompiler_t *comp);
_Bool ape_compiler_initshallowcopy(ApeCompiler_t *copy, ApeCompiler_t *src);
/* mem.c */
void *ape_mem_defaultmalloc(void *opaqptr, size_t size);
void ape_mem_defaultfree(void *opaqptr, void *objptr);
void *ape_allocator_alloc(ApeAllocator_t *alloc, size_t size);
void ape_allocator_free(ApeAllocator_t *alloc, void *ptr);
ApeAllocator_t ape_make_allocator(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void *ctx);
ApeGCMemory_t *ape_make_gcmem(ApeContext_t *ctx);
void ape_gcmem_destroy(ApeGCMemory_t *mem);
ApeObjectData_t *ape_gcmem_allocobjdata(ApeGCMemory_t *mem, ApeObjectType_t type);
_Bool ape_gcmem_canputinpool(ApeGCMemory_t *mem, ApeObjectData_t *data);
ApeObjectDataPool_t *ape_gcmem_getpoolfor(ApeGCMemory_t *mem, ApeObjectType_t type);
ApeObjectData_t *ape_gcmem_getfrompool(ApeGCMemory_t *mem, ApeObjectType_t type);
void ape_gcmem_unmarkall(ApeGCMemory_t *mem);
void ape_gcmem_markobjlist(ApeObject_t *objects, ApeSize_t count);
void ape_gcmem_markobject(ApeObject_t obj);
void ape_gcmem_sweep(ApeGCMemory_t *mem);
int ape_gcmem_shouldsweep(ApeGCMemory_t *mem);
/* cclex.c */
void ape_lexer_token_init(ApeToken_t *tok, ApeTokenType_t type, const char *literal, int len);
char *ape_lexer_tokendupliteral(ApeContext_t *ctx, const ApeToken_t *tok);
_Bool ape_lexer_init(ApeLexer_t *lex, ApeContext_t *ctx, ApeErrorList_t *errs, const char *input, ApeCompiledFile_t *file);
_Bool ape_lexer_failed(ApeLexer_t *lex);
void ape_lexer_continuetemplatestring(ApeLexer_t *lex);
_Bool ape_lexer_currenttokenis(ApeLexer_t *lex, ApeTokenType_t type);
_Bool ape_lexer_peektokenis(ApeLexer_t *lex, ApeTokenType_t type);
_Bool ape_lexer_nexttoken(ApeLexer_t *lex);
_Bool ape_lexer_previous_token(ApeLexer_t *lex);
ApeToken_t ape_lexer_internalnexttoken(ApeLexer_t *lex);
_Bool ape_lexer_expectcurrent(ApeLexer_t *lex, ApeTokenType_t type);
/* libarray.c */
ApeArray_t *ape_make_valarray_actual(ApeContext_t *ctx, ApeSize_t element_size);
ApeArray_t *ape_make_valarraycapacity(ApeContext_t *ctx, ApeSize_t capacity, ApeSize_t element_size);
_Bool ape_valarray_initcapacity(ApeArray_t *arr, ApeContext_t *ctx, ApeSize_t capacity, ApeSize_t element_size);
void ape_valarray_deinit(ApeArray_t *arr);
void ape_valarray_destroy(ApeArray_t *arr);
ApeArray_t *ape_valarray_copy(const ApeArray_t *arr);
_Bool ape_valarray_add(ApeArray_t *arr, const void *value);
_Bool ape_valarray_push(ApeArray_t *arr, const void *value);
_Bool ape_valarray_pop(ApeArray_t *arr, void *out_value);
void *ape_valarray_top(ApeArray_t *arr);
_Bool ape_valarray_set(ApeArray_t *arr, ApeSize_t ix, void *value);
void *ape_valarray_get(ApeArray_t *arr, ApeSize_t ix);
ApeSize_t ape_valarray_count(const ApeArray_t *arr);
_Bool ape_valarray_removeat(ApeArray_t *arr, ApeSize_t ix);
void ape_valarray_clear(ApeArray_t *arr);
void *ape_valarray_data(ApeArray_t *arr);
void ape_valarray_orphandata(ApeArray_t *arr);
ApePtrArray_t *ape_make_ptrarray(ApeContext_t *ctx);
ApePtrArray_t *ape_make_ptrarraycapacity(ApeContext_t *ctx, ApeSize_t capacity);
void ape_ptrarray_destroy(ApePtrArray_t *arr);
void ape_ptrarray_destroywithitems(ApePtrArray_t *arr, ApeDataCallback_t destroy_fn);
ApePtrArray_t *ape_ptrarray_copywithitems(ApePtrArray_t *arr, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn);
_Bool ape_ptrarray_add(ApePtrArray_t *arr, void *ptr);
void *ape_ptrarray_get(ApePtrArray_t *arr, ApeSize_t ix);
_Bool ape_ptrarray_push(ApePtrArray_t *arr, void *ptr);
void *ape_ptrarray_pop(ApePtrArray_t *arr);
void *ape_ptrarray_top(ApePtrArray_t *arr);
ApeSize_t ape_ptrarray_count(const ApePtrArray_t *arr);
_Bool ape_ptrarray_removeat(ApePtrArray_t *arr, ApeSize_t ix);
void ape_ptrarray_clear(ApePtrArray_t *arr);
void ape_ptrarray_clearanddestroyitems(ApePtrArray_t *arr, ApeDataCallback_t destroy_fn);
ApeObject_t ape_object_make_array(ApeContext_t *ctx);
ApeObject_t ape_object_make_arraycapacity(ApeContext_t *ctx, unsigned capacity);
ApeObject_t ape_object_array_getvalue(ApeObject_t object, ApeSize_t ix);
_Bool ape_object_array_setat(ApeObject_t object, ApeInt_t ix, ApeObject_t val);
_Bool ape_object_array_pushvalue(ApeObject_t object, ApeObject_t val);
ApeSize_t ape_object_array_getlength(ApeObject_t object);
_Bool ape_object_array_removeat(ApeObject_t object, ApeInt_t ix);
_Bool ape_object_array_pushstring(ApeObject_t obj, const char *string);
ApeArray_t *ape_object_array_getarray(ApeObject_t object);
/* libfunction.c */
ApeObject_t ape_object_make_function(ApeContext_t *ctx, const char *name, ApeCompilationResult_t *comp_res, _Bool owns_data, ApeInt_t num_locals, ApeInt_t num_args, ApeSize_t free_vals_count);
ApeObject_t ape_object_make_nativefuncmemory(ApeContext_t *ctx, const char *name, ApeNativeFunc_t fn, void *data, ApeSize_t data_len);
const char *ape_object_function_getname(ApeObject_t obj);
ApeObject_t ape_object_function_getfreeval(ApeObject_t obj, ApeInt_t ix);
void ape_object_function_setfreeval(ApeObject_t obj, ApeInt_t ix, ApeObject_t val);
/* writer.c */
ApeWriter_t *ape_make_writer(ApeContext_t *ctx);
ApeWriter_t *ape_make_writerdefault(ApeContext_t *ctx);
ApeWriter_t *ape_make_writercapacity(ApeContext_t *ctx, ApeSize_t capacity);
ApeWriter_t *ape_make_writerio(ApeContext_t *ctx, FILE *hnd, _Bool alsoclose, _Bool alsoflush);
void ape_writer_destroy(ApeWriter_t *buf);
_Bool ape_writer_appendn(ApeWriter_t *buf, const char *str, ApeSize_t str_len);
_Bool ape_writer_append(ApeWriter_t *buf, const char *str);
_Bool ape_writer_appendf(ApeWriter_t *buf, const char *fmt, ...);
const char *ape_writer_getdata(const ApeWriter_t *buf);
ApeSize_t ape_writer_getlength(const ApeWriter_t *buf);
char *ape_writer_getstringanddestroy(ApeWriter_t *buf);
_Bool ape_writer_failed(ApeWriter_t *buf);
_Bool ape_writer_grow(ApeWriter_t *buf, ApeSize_t new_capacity);
/* builtins.c */
void builtins_install(ApeVM_t *vm);
ApeSize_t builtins_count(void);
ApeNativeFunc_t builtins_get_fn(ApeSize_t ix);
const char *builtins_get_name(ApeSize_t ix);
ApeNativeFunc_t builtin_get_object(ApeObjectType_t objt, const char *idxname);