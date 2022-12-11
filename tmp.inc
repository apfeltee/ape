



struct ApeAstExpression_t
{
    ApeContext_t* context;
    ApeAstExprType_t extype;
    bool          stringwasallocd;
    ApeSize_t     stringlitlength;
    union
    {
        ApeAstIdentExpr_t*        exident;
        ApeFloat_t         exliteralnumber;
        bool               exliteralbool;
        char*              exliteralstring;
        ApePtrArray_t*     exarray;
        ApeAstMapLiteralExpr_t    exmap;
        ApeAstPrefixExpr_t    exprefix;
        ApeAstInfixExpr_t     exinfix;
        ApeAstFnLiteralExpr_t     exliteralfunc;
        ApeAstCallExpr_t      excall;
        ApeAstIndexExpr_t     exindex;
        ApeAstAssignExpr_t    exassign;
        ApeAstLogicalExpr_t   exlogical;
        ApeAstTernaryExpr_t   externary;
        ApeAstDefineExpr_t    exdefine;
        ApeAstIfExpr_t        exifstmt;
        ApeAstExpression_t*   exreturn;
        ApeAstExpression_t*   exexpression;
        ApeAstWhileLoopExpr_t exwhilestmt;
        ApeAstForeachExpr_t   exforeachstmt;
        ApeAstForLoopExpr_t   exforstmt;
        ApeAstCodeblockExpr_t*    exblock;
        ApeAstIncludeExpr_t   exincludestmt;
        ApeAstRecoverExpr_t   exrecoverstmt;
    };
    ApePosition_t pos;
};

    typsz(ApeAstAssignExpr_t);
    typsz(ApeAstCallExpr_t);
    typsz(ApeAstCodeblockExpr_t);
    typsz(ApeAstDefineExpr_t);
    typsz(ApeAstFnLiteralExpr_t);
    typsz(ApeAstForLoopExpr_t);
    typsz(ApeAstForeachExpr_t);
    typsz(ApeAstIfCaseExpr_t);
    typsz(ApeAstIfExpr_t);
    typsz(ApeAstIncludeExpr_t);
    typsz(ApeAstIndexExpr_t);
    typsz(ApeAstInfixExpr_t);
    typsz(ApeAstLogicalExpr_t);
    typsz(ApeAstMapLiteralExpr_t);
    typsz(ApeAstPrefixExpr_t);
    typsz(ApeAstTernaryExpr_t);
    typsz(ApeAstWhileLoopExpr_t);
