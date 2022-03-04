
#pragma once

#if defined(__linux__)
    #define APE_LINUX
    #define APE_POSIX
#elif defined(_WIN32)
    #define APE_WINDOWS
#elif(defined(__APPLE__) && defined(__MACH__))
    #define APE_APPLE
    #define APE_POSIX
#elif defined(__EMSCRIPTEN__)
    #define APE_EMSCRIPTEN
#endif

#if defined(__unix__)
    #include <unistd.h>
    #if defined(_POSIX_VERSION)
        #define APE_POSIX
    #endif
#endif

#if defined(APE_POSIX)
    #include <sys/time.h>
#elif defined(APE_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(APE_EMSCRIPTEN)
    #include <emscripten/emscripten.h>
#endif

#include "ape.h"


#define APE_IMPL_VERSION_MAJOR 0
#define APE_IMPL_VERSION_MINOR 14
#define APE_IMPL_VERSION_PATCH 0

#if(APE_VERSION_MAJOR != APE_IMPL_VERSION_MAJOR) || (APE_VERSION_MINOR != APE_IMPL_VERSION_MINOR) \
|| (APE_VERSION_PATCH != APE_IMPL_VERSION_PATCH)
    #error "Version mismatch"
#endif


#ifdef COLLECTIONS_DEBUG
    #include <assert.h>
    #define COLLECTIONS_ASSERT(x) assert(x)
#else
    #define COLLECTIONS_ASSERT(x)
#endif

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------



// Private declarations
bool valdict_init(ApeValDictionary_t* dict, ApeAllocator_t* alloc, size_t key_size, size_t val_size, unsigned int initial_capacity);
void valdict_deinit(ApeValDictionary_t* dict);
unsigned int valdict_get_cell_ix(const ApeValDictionary_t* dict, const void* key, unsigned long hash, bool* out_found);
bool valdict_grow_and_rehash(ApeValDictionary_t* dict);
bool valdict_set_key_at(ApeValDictionary_t* dict, unsigned int ix, void* key);
bool valdict_keys_are_equal(const ApeValDictionary_t* dict, const void* a, const void* b);
unsigned long valdict_hash_key(const ApeValDictionary_t* dict, const void* key);


// Private declarations
bool dict_init(ApeDictionary_t* dict, ApeAllocator_t* alloc, unsigned int initial_capacity, ApeDictItemCopyFNCallback_t copy_fn, ApeDictItemDestroyFNCallback_t destroy_fn);
void dict_deinit(ApeDictionary_t* dict, bool free_keys);
unsigned int dict_get_cell_ix(const ApeDictionary_t* dict, const char* key, unsigned long hash, bool* out_found);
unsigned long hash_string(const char* str);
bool dict_grow_and_rehash(ApeDictionary_t* dict);
bool dict_set_internal(ApeDictionary_t* dict, const char* ckey, char* mkey, void* value);
char* collections_strndup(ApeAllocator_t* alloc, const char* string, size_t n);
char* collections_strdup(ApeAllocator_t* alloc, const char* string);
unsigned long collections_hash(const void* ptr, size_t len); /* djb2 */
unsigned int upper_power_of_two(unsigned int v);

bool array_init_with_capacity(ApeArray_t* arr, ApeAllocator_t* alloc, unsigned int capacity, size_t element_size);
void array_deinit(ApeArray_t* arr);
bool strbuf_grow(ApeStringBuffer_t* buf, size_t new_capacity);




ApeExpression_t* expression_make(ApeAllocator_t* alloc, ApeExpr_type_t type);
ApeStatement_t* statement_make(ApeAllocator_t* alloc, ApeStatementType_t type);

ApeBlockScope_t* block_scope_copy(ApeBlockScope_t* scope);
ApeBlockScope_t* block_scope_make(ApeAllocator_t* alloc, int offset);
void block_scope_destroy(ApeBlockScope_t* scope);
bool set_symbol(ApeSymbol_table_t* table, ApeSymbol_t* symbol);
int next_symbol_index(ApeSymbol_table_t* table);
int count_num_definitions(ApeSymbol_table_t* table);


bool compiler_init(ApeCompiler_t* comp,
                          ApeAllocator_t* alloc,
                          const ApeConfig_t* config,
                          ApeGCMemory_t* mem,
                          ApeErrorList_t* errors,
                          ptrarray(ApeCompiledFile_t) * files,
                          ApeGlobalStore_t* global_store);
void compiler_deinit(ApeCompiler_t* comp);

bool compiler_init_shallow_copy(ApeCompiler_t* copy, ApeCompiler_t* src);// used to restore compiler's state if something fails

int emit(ApeCompiler_t* comp, opcode_t op, int operands_count, uint64_t* operands);
ApeCompilationScope_t* get_compilation_scope(ApeCompiler_t* comp);
bool push_compilation_scope(ApeCompiler_t* comp);
void pop_compilation_scope(ApeCompiler_t* comp);
bool push_symbol_table(ApeCompiler_t* comp, int global_offset);
void pop_symbol_table(ApeCompiler_t* comp);
opcode_t get_last_opcode(ApeCompiler_t* comp);
bool compile_code(ApeCompiler_t* comp, const char* code);
bool compile_statements(ApeCompiler_t* comp, ptrarray(ApeStatement_t) * statements);
bool import_module(ApeCompiler_t* comp, const ApeStatement_t* import_stmt);
bool compile_statement(ApeCompiler_t* comp, const ApeStatement_t* stmt);
bool compile_expression(ApeCompiler_t* comp, ApeExpression_t* expr);
bool compile_code_block(ApeCompiler_t* comp, const ApeCodeblock_t* block);
int add_constant(ApeCompiler_t* comp, ApeObject_t obj);
void change_uint16_operand(ApeCompiler_t* comp, int ip, uint16_t operand);
bool last_opcode_is(ApeCompiler_t* comp, opcode_t op);
bool read_symbol(ApeCompiler_t* comp, const ApeSymbol_t* symbol);
bool write_symbol(ApeCompiler_t* comp, const ApeSymbol_t* symbol, bool define);

bool push_break_ip(ApeCompiler_t* comp, int ip);
void pop_break_ip(ApeCompiler_t* comp);
int get_break_ip(ApeCompiler_t* comp);

bool push_continue_ip(ApeCompiler_t* comp, int ip);
void pop_continue_ip(ApeCompiler_t* comp);
int get_continue_ip(ApeCompiler_t* comp);

int get_ip(ApeCompiler_t* comp);

array(ApePosition_t) * get_src_positions(ApeCompiler_t* comp);
array(uint8_t) * get_bytecode(ApeCompiler_t* comp);

ApeFileScope_t* file_scope_make(ApeCompiler_t* comp, ApeCompiledFile_t* file);
void file_scope_destroy(ApeFileScope_t* file_scope);
bool push_file_scope(ApeCompiler_t* comp, const char* filepath);
void pop_file_scope(ApeCompiler_t* comp);

void set_compilation_scope(ApeCompiler_t* comp, ApeCompilationScope_t* scope);

ApeModule_t* module_make(ApeAllocator_t* alloc, const char* name);
void module_destroy(ApeModule_t* module);
ApeModule_t* module_copy(ApeModule_t* module);
bool module_add_symbol(ApeModule_t* module, const ApeSymbol_t* symbol);

const char* get_module_name(const char* path);
const ApeSymbol_t* define_symbol(ApeCompiler_t* comp, ApePosition_t pos, const char* name, bool assignable, bool can_shadow);

ApeObject_t object_deep_copy_internal(ApeGCMemory_t* mem, ApeObject_t obj, valdict(ApeObject_t, ApeObject_t) * copies);
bool object_equals_wrapped(const ApeObject_t* a, const ApeObject_t* b);
unsigned long object_hash(ApeObject_t* obj_ptr);
unsigned long object_hash_string(const char* str);
unsigned long object_hash_double(double val);
array(ApeObject_t) * object_get_allocated_array(ApeObject_t object);
bool object_is_number(ApeObject_t obj);
uint64_t get_type_tag(ApeObjectType_t type);
bool freevals_are_allocated(ApeFunction_t* fun);
char* object_data_get_string(ApeObjectData_t* data);
bool object_data_string_reserve_capacity(ApeObjectData_t* data, int capacity);




void set_sp(ApeVM_t* vm, int new_sp);
void stack_push(ApeVM_t* vm, ApeObject_t obj);
ApeObject_t stack_pop(ApeVM_t* vm);
ApeObject_t stack_get(ApeVM_t* vm, int nth_item);

void this_stack_push(ApeVM_t* vm, ApeObject_t obj);
ApeObject_t this_stack_pop(ApeVM_t* vm);
ApeObject_t this_stack_get(ApeVM_t* vm, int nth_item);

bool push_frame(ApeVM_t* vm, ApeFrame_t frame);
bool pop_frame(ApeVM_t* vm);
void run_gc(ApeVM_t* vm, array(ApeObject_t) * constants);
bool call_object(ApeVM_t* vm, ApeObject_t callee, int num_args);
ApeObject_t call_native_function(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args);
bool check_assign(ApeVM_t* vm, ApeObject_t old_value, ApeObject_t new_value);
bool try_overload_operator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, opcode_t op, bool* out_overload_found);



