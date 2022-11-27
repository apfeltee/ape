
#define NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,N,...) N
#define NARGS(...) NARGS_SEQ(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)

//#define make_u64_array(...) make_u64_array_actual(NARGS(__VA_ARGS__), __VA_ARGS__) 
#define make_u64_array(...) ({(ApeTempU64Array_t){ {__VA_ARGS__} }).data


nextcasejumpip = ape_compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, make_u64_array((uint64_t)(0xbeef)));
ip = ape_compiler_emit(comp, OPCODE_FUNCTION, 2, make_u64_array((uint64_t)pos, (uint64_t)ape_ptrarray_count(freesymbols)));


nextcasejumpip = ape_compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, ({(ApeTempU64Array_t){ {(uint64_t)(0xbeef)} }).data);
ip = ape_compiler_emit(comp, OPCODE_FUNCTION, 2, ({(ApeTempU64Array_t){ {(uint64_t)pos, (uint64_t)ape_ptrarray_count(freesymbols)} }).data);

