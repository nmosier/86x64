#ifndef __AST_FWD
#define __AST_FWD

#include <list>

namespace zc {

   /* ast-base */
   class ASTNode;
   template <class Node> class ASTNodeVec;
   typedef long SourceLoc;

   /* ast-main */
   class TranslationUnit;
   
   /* ast-expr */
   class ASTExpr;
   typedef std::list<ASTExpr *> ASTExprs;
   // typedef ASTNodeVec<ASTExpr> ASTExprs;
   class ASTUnaryExpr;
   class ASTBinaryExpr;
   class AssignmentExpr;
   class UnaryExpr;
   class BinaryExpr;
   class LiteralExpr;
   class StringExpr;
   class IdentifierExpr;
   class NoExpr;
   class CallExpr;
   class CastExpr;
   class MembExpr;
   class SizeofExpr;
   class IndexExpr;

   /* ast-stat */
   class ASTStat;
   typedef std::list<ASTStat *> ASTStats;
   // typedef ASTNodeVec<ASTStat> ASTStats;
   class CompoundStat;
   class ExprStat;
   class JumpStat;
   class ReturnStat;
   class BreakStat;
   class ContinueStat;
   class IfStat;
   class IterationStat;
   class LoopStat;
   class WhileStat;
   class ForStat;
   class GotoStat;
   class LabeledStat;
   class LabelDefStat;
   class NoStat;

   /* ast-decl */
   class ExternalDecl;
   typedef std::list<ExternalDecl *> ExternalDecls;
   //typedef ASTNodeVec<ExternalDecl> ExternalDecls;   
   class FunctionDef;
   class DeclSpecs;
   class TypeSpec;
   class BasicTypeSpec;
   class ComplexTypeSpec;
   class TypenameSpec;
   class StorageClassSpec;
   class DeclSpecs;
   class Declaration;
   typedef std::vector<Declaration *> Declarations;
   class VarDeclaration;
   typedef std::vector<VarDeclaration *> VarDeclarations;
   class TypeDeclaration;
   class TypenameDeclaration;
   class ASTDeclarator;
   typedef ASTNodeVec<ASTDeclarator> ASTDeclarators;
   class BasicDeclarator;
   class PointerDeclarator;
   class FunctionDeclarator;
   class ArrayDeclarator;
   class Identifier;
   
   /* ast-type */
   class ASTType;
   typedef std::vector<ASTType *> Types;
   class PointerType;
   class FunctionType;
   class VoidType;
   class IntegralType;
   class DeclarableType;
   class TaggedType;
   class CompoundType;
   class StructType;
   class UnionType;
   class EnumType;
   class Enumerator;
   class ArrayType;
   class NamedType;
   
}

#endif
