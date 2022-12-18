
table =
{
  "ape_object_type_t" => "ApeObjType_t",
  "token_type_t" => "ApeAstTokType_t",
  "operator_t" => "ApeOperType_t",
  "expression_type_t" => "ApeAstExprType_t",
  "statement_type_t" => "ApeAstStmtType_t",
  "object_type_t" => "ApeObjType_t",
  "error_type_t" => "ApeErrtype_t",
  "opcode_val_t" => "ApeOpcodeVal_t",
  "symbol_type_t" => "ApeSymType_t",
  "precedence_t" => "ApeAstPrecedence_t",
  "ape_t" => "ApeContext_t",
  "ape_object_t" => "ApeObject_t",
  "ape_program_t" => "ApeProgram_t",
  "ape_traceback_t" => "ApeTraceback_t",
  "compiled_file_t" => "ApeAstCompFile_t",
  "allocator_t" => "ApeAllocator_t",
  "src_pos_t" => "ApePosition_t",
  "ape_config_t" => "ApeConfig_t",
  "ape_timer_t" => "ApeTimer_t",
  "dict_t_" => "ApeStrDict_t",
  "valdict_t_" => "ApeValDict_t",
  "ptrdict_t_" => "ApePtrDict_t",
  "array_t_" => "ApeValArray_t",
  "ptrarray_t_" => "ApePtrArray_t",
  "strbuf_t" => "ApeWriter_t",
  "token_t" => "ApeAstToken_t",
  "code_block_t" => "ApeAstCodeblock_t",
  "expression_t" => "ApeAstExpression_t",
  "statement_t" => "ApeAstStatement_t",
  "map_literal_t" => "ApeAstLiteralMap_t",
  "prefix_expression_t" => "ApeAstPrefixExpr_t",
  "infix_expression_t" => "ApeAstInfixExpr_t",
  "if_case_t" => "ApeAstIfCase_t",
  "fn_literal_t" => "ApeAstLiteralFunc_t",
  "call_expression_t" => "ApeAstCallExpr_t",
  "index_expression_t" => "ApeAstIndexExpr_t",
  "assign_expression_t" => "ApeAstAssignExpr_t",
  "logical_expression_t" => "ApeAstLogicalExpr_t",
  "ternary_expression_t" => "ApeAstTernaryExpr_t",
  "ident_t" => "ApeAstIdent_t",
  "define_statement_t" => "ApeAstDefineExpr_t",
  "if_statement_t" => "ApeAstIfExpr_t",
  "while_loop_statement_t" => "ApeAstWhileExpr_t",
  "foreach_statement_t" => "ApeAstForeachExpr_t",
  "for_loop_statement_t" => "ApeAstForExpr_t",
  "import_statement_t" => "ApeAstImportExpr_t",
  "recover_statement_t" => "ApeAstRecoverExpr_t",
  "compilation_result_t" => "ApeAstCompResult_t",
  "traceback_t" => "ApeTraceback_t",
  "vm_t" => "ApeVM_t",
  "gcmem_t" => "ApeGCMemory_t",
  "object_t" => "ApeObject_t",
  "function_t" => "ApeObjScriptFunc_t",
  "native_function_t" => "ApeObjNativeFunc_t",
  "external_data_t" => "ApeObjExternal_t",
  "object_error_t" => "ApeObjError_t",
  "object_string_t" => "ApeObjString_t",
  "object_data_t" => "ApeGCObjData_t",
  "object_data_pool_t" => "ApeGCDataPool_t",
  "error_t" => "ApeError_t",
  "errors_t" => "ApeErrorList_t",
  "lexer_t" => "ApeAstLexer_t",
  "parser_t" => "ApeAstParser_t",
  "opcode_definition_t" => "ApeOpcodeDef_t",
  "global_store_t" => "ApeGlobalStore_t",
  "symbol_t" => "ApeSymbol_t",
  "block_scope_t" => "ApeBlockScope_t",
  "symbol_table_t" => "ApeSymTable_t",
  "compilation_scope_t" => "ApeAstCompScope_t",
  "compiler_t" => "ApeAstCompiler_t",
  "frame_t" => "ApeFrame_t",
  "traceback_item_t" => "ApeTraceItem_t",
  "module_t" => "ApeModule_t",
  "file_scope_t" => "ApeAstFileScope_t",
  "native_fn_wrapper_t" => "ApeWrappedNativeFunc_t",

}

cmd = ["gsub", "-r", "-b"]
Dir.glob("*.{c,h}") do |f|
  cmd.push("-f", f)
end
table.each do |old, new|
  cmd.push(sprintf("%s=%s", old, new))
end
system(*cmd)




