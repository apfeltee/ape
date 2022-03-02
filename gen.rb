
names =
{
  "token_type_t" => "ApeTokenType_t",
  "operator_t" => "ApeOperator_t",
  "expression_type_t" => "ApeExpr_type_t",
  "statement_type_t" => "ApeStatementType_t",
  "object_type_t" => "ApeObjectType_t",
  "symbol_type_t" => "ApeSymbolType_t",
  "opcode_val_t" => "ApeOpcodeValue_t",
  "precedence_t" => "ApePrecedence_t",
  "ape_config_t" => "ApeConfig_t",
  "src_pos_t" => "ApePosition_t",
  "ape_timer_t" => "ApeTimer_t",
  "error_t" => "ApeError_t",
  "errors_t" => "ApeErrorList_t",
  "token_t" => "ApeToken_t",
  "compiled_file_t" => "ApeCompiledFile_t",
  "lexer_t" => "ApeLexer_t",
  "code_block_t" => "ApeCodeblock_t",
  "map_literal_t" => "ApeMapLiteral_t",
  "prefix_expression_t" => "ApePrefixExpr_t",
  "infix_expression_t" => "ApeInfixExpr_t",
  "if_case_t" => "ApeIfCase_t",
  "fn_literal_t" => "ApeFnLiteral_t",
  "call_expression_t" => "ApeCallExpr_t",
  "index_expression_t" => "ApeIndexExpr_t",
  "assign_expression_t" => "ApeAssignExpr_t",
  "logical_expression_t" => "ApeLogicalExpr_t",
  "ternary_expression_t" => "ApeTernaryExpr_t",
  "ident_t" => "ApeIdent_t",
  "define_statement_t" => "ApeDefineStmt_t",
  "if_statement_t" => "ApeIfStmt_t",
  "while_loop_statement_t" => "ApeWhileLoopStmt_t",
  "foreach_statement_t" => "ApeForeachStmt_t",
  "for_loop_statement_t" => "ApeForLoopStmt_t",
  "import_statement_t" => "ApeImportStmt_t",
  "recover_statement_t" => "ApeRecoverStmt_t",
  "statement_t" => "ApeStatement_t",
  "parser_t" => "ApeParser_t",
  "function_t" => "ApeFunction_t",
  "native_function_t" => "ApeNativeFunction_t",
  "external_data_t" => "ApeExternalData_t",
  "object_error_t" => "ApeObjectError_t",
  "object_string_t" => "ApeObjectString_t",
  "object_data_t" => "ApeObjectData_t",
  "symbol_t" => "ApeSymbol_t",
  "block_scope_t" => "ApeBlockScope_t",
  "symbol_table_t" => "ApeSymbol_table_t",
  "opcode_definition_t" => "ApeOpcodeDefinition_t",
  "compilation_result_t" => "ApeCompilationResult_t",
  "compilation_scope_t" => "ApeCompilationScope_t",
  "object_data_pool_t" => "ApeObjectDataPool_t",
  "gcmem_t" => "ApeGCMemory_t",
  "traceback_item_t" => "ApeTracebackItem_t",
  "traceback_t" => "ApeTraceback_t",
  "frame_t" => "ApeFrame_t",
  "valdict_t" => "ApeValDictionary_t",
  "dictionary_t" => "ApeDictionary_t",
  "array_t" => "ApeArray_t",
  "ptrdictionary_t" => "ApePtrDictionary_t",
  "ptrarray_t" => "ApePtrArray_t",
  "strbuf_t" => "ApeStringBuffer_t",
  "global_store_t" => "ApeGlobalStore_t",
  "module_t" => "ApeModule_t",
  "file_scope_t" => "ApeFileScope_t",
  "compiler_t" => "ApeCompiler_t",
  "native_fn_wrapper_t" => "ApeNativeFuncWrapper_t",

}


if names.values.length != names.values.map(&:downcase).uniq.length then
  $stderr.printf("potential duplicate names. cannot continue\n")
  exit(1)
end

printf("gsub -r -b ")
Dir.glob("*.{c,h}") do |file|
  printf("-f %p ", file)
end
names.each do |old, new|
  printf("%p=%p ", old, new)
end