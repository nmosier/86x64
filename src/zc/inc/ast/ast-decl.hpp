#ifndef __AST_DECL_HPP
#define __AST_DECL_HPP

#include <ostream>

#include "cgen-fwd.hpp"

namespace zc {

   class Declaration;

   class ExternalDecl: public ASTNode {
   public:
      Declaration *decl() const { return decl_; }
      Symbol *sym() const;
      
      template <typename... Args>
      static ExternalDecl *Create(Args... args) {
         return new ExternalDecl(args...);
      }
      
      virtual void DumpNode(std::ostream& os) const override { os << "ExternalDecl"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;
      
      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env);
      ASTType *Type() const;
      virtual void Enscope(SemantEnv& env) const;
      virtual void Descope(SemantEnv& env) const;
      
   protected:
      Declaration *decl_;

      template <typename... Args>
      ExternalDecl(Declaration *decl, Args... args): ASTNode(args...), decl_(decl) {}
   };

   
   
   //template <> const char *ExternalDecls::name() const;

   class FunctionDef: public ExternalDecl {
   public:
      CompoundStat *comp_stat() const { return comp_stat_; }

      template <typename... Args>
      static FunctionDef *Create(Args... args) {
         return new FunctionDef(args...);
      }

      virtual void DumpNode(std::ostream& os) const override;
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;
      virtual void Enscope(SemantEnv& env) const override;
      virtual void Descope(SemantEnv& env) const override;

   protected:
      CompoundStat *comp_stat_;

      template <typename... Args>
      FunctionDef(CompoundStat *comp_stat, Args... args):
         ExternalDecl(args...), comp_stat_(comp_stat) {}
      
   };

   class TypeSpec: public ASTNode {
   public:
      virtual void AddTo(DeclSpecs *decl_specs) = 0;
      
   protected:
      template <typename... Args>
      TypeSpec(Args... args): ASTNode(args...) {}
   };

   class BasicTypeSpec: public TypeSpec {
   public:
      enum class Kind {TS_VOID, TS_CHAR, TS_SHORT, TS_INT, TS_LONG, TS_UNSIGNED, TS_SIGNED};
      Kind kind() const { return kind_; }
      
      virtual void AddTo(DeclSpecs *decl_specs) override;

      template <typename... Args>
      static BasicTypeSpec *Create(Args... args) { return new BasicTypeSpec(args...); }
      
   private:
      Kind kind_;

      template <typename... Args>
      BasicTypeSpec(Kind kind, Args... args): TypeSpec(args...), kind_(kind) {}
   };

   class ComplexTypeSpec: public TypeSpec {
   public:
      ASTType *type() const { return type_; }

      virtual void AddTo(DeclSpecs *decl_specs) override;

      template <typename... Args>
      static ComplexTypeSpec *Create(Args... args) { return new ComplexTypeSpec(args...); }
      
   private:
      ASTType *type_;

      template <typename... Args>
      ComplexTypeSpec(ASTType *type, Args... args): TypeSpec(args...), type_(type) {}
   };

   class TypenameSpec: public TypeSpec {
   public:
      Symbol *sym() const;
      Identifier *id() const { return id_; }

      virtual void AddTo(DeclSpecs *decl_specs) override;

      template <typename... Args>
      static TypenameSpec *Create(Args... args) { return new TypenameSpec(args...); }

   private:
      Identifier *id_;

      template <typename... Args>
      TypenameSpec(Identifier *id, Args... args): TypeSpec(args...), id_(id) {}
   };

   class StorageClassSpec: public ASTNode {
   public:
      enum class Kind {SC_TYPEDEF, SC_AUTO, SC_REGISTER, SC_STATIC, SC_EXTERN};
      Kind kind() const { return kind_; }
      
      void AddTo(DeclSpecs *decl_specs);
      
      template <typename... Args>
      static StorageClassSpec *Create(Args... args) { return new StorageClassSpec(args...); }
      
   private:
      Kind kind_;

      template <typename... Args>
      StorageClassSpec(Kind kind, Args... args): ASTNode(args...), kind_(kind) {}
   };
      
   
   class DeclSpecs: public ASTNode {
   public:
      typedef std::vector<BasicTypeSpec *> BasicTypeSpecs;
      typedef std::vector<ComplexTypeSpec *> ComplexTypeSpecs;
      typedef std::vector<StorageClassSpec *> StorageClassSpecs;
      typedef std::vector<TypenameSpec *> TypenameSpecs;

      BasicTypeSpecs basic_type_specs;
      ComplexTypeSpecs complex_type_specs;
      TypenameSpecs typename_specs;
      StorageClassSpecs storage_class_specs;

      ASTType *Type(SemantError& err);

      Declaration *GetDecl(SemantError& err, ASTDeclarator *declarator);

      template <typename... Args>
      static DeclSpecs *Create(Args... args) { return new DeclSpecs(args...); }
      
   protected:


      template <typename... Args>
      DeclSpecs(Args... args): ASTNode(args...) {}
   };

   /**
    * NOTE: Abstract.
    */
   class Declaration: public ASTNode {
   public:
      ASTType *type() const { return type_; }

      virtual void Declare(SemantEnv& env) = 0;

      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      void TypeResolve(SemantEnv& env);

   protected:
      ASTType *type_;

      template <typename... Args>
      Declaration(ASTType *type, Args... args):
         ASTNode(args...), type_(type) {}
   };

 
   class VarDeclaration: public Declaration {
   public:
      Symbol *sym() const { return sym_; }            
      bool is_const() const { return is_const_; }
      bool is_valid() const { return sym() != nullptr; }

      virtual void Declare(SemantEnv& env) override;
      void TypeCheck(SemantEnv& env);

      virtual void DumpNode(std::ostream& os) const override;

      int bytes() const;

      template <typename... Args>
      static VarDeclaration *Create(Args... args) { return new VarDeclaration(args...); }
      
   private: 
      Symbol *sym_;     
      bool is_const_;

      template <typename... Args>
      VarDeclaration(Symbol *sym, bool is_const, Args... args):
         Declaration(args...), sym_(sym), is_const_(is_const) {}
   };

   class TypeDeclaration: public Declaration {
   public:
      virtual void Declare(SemantEnv& env) override;

      virtual void DumpNode(std::ostream& os) const override;

      template <typename... Args>
      static TypeDeclaration *Create(Args... args) { return new TypeDeclaration(args...); }
      
   private:
      template <typename... Args>
      TypeDeclaration(Args... args): Declaration(args...) {}
      TypeDeclaration(DeclarableType *type);
   };

   class TypenameDeclaration: public Declaration {
   public:
      Symbol *sym() const { return sym_; }
      
      virtual void Declare(SemantEnv& env) override;

      template <typename... Args>
      static TypenameDeclaration *Create(Args... args) { return new TypenameDeclaration(args...); }
      
   private:
      Symbol *sym_;

      template <typename... Args>
      TypenameDeclaration(Symbol *sym, Args... args): Declaration(args...), sym_(sym) {}
   };
   
   class ASTDeclarator: public ASTNode {
   public:
      /** The fundamental kind of declarator. */
      enum class Kind {DECLARATOR_POINTER,
                       DECLARATOR_BASIC, 
                       DECLARATOR_FUNCTION,
                       DECLARATOR_ARRAY};
      virtual Identifier *id() const = 0;
      Symbol *sym() const;
      virtual Kind kind() const = 0;

      virtual void get_declarables(Declarations* output) const {}

      virtual ASTType *Type(ASTType *init_type) const = 0;

      /**
       * Perform semantic analysis on declarator.
       * @param env semantic environment
       * @param level abstaction level. @see Decl::TypeCheck(Semantic&, int)
       */
      // virtual void TypeCheck(SemantEnv& env) = 0;

      /** Unwrap a type during Decl -> Type conversion. */
      virtual void JoinPointers() = 0;
      
   protected:
      
      ASTDeclarator(const SourceLoc& loc): ASTNode(loc) {}
   };

   
   class BasicDeclarator: public ASTDeclarator {
   public:
      virtual Identifier *id() const override { return id_; }
      virtual Kind kind() const override { return Kind::DECLARATOR_BASIC; }

      static BasicDeclarator *Create(Identifier *id, const SourceLoc& loc) {
         return new BasicDeclarator(id, loc);
      }

      virtual void DumpNode(std::ostream& os) const override { os << "BasicDeclarator"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      // virtual void TypeCheck(SemantEnv& env) override {}

      virtual ASTType *Type(ASTType *init_type) const override;

      virtual void JoinPointers() override { /* base case */ }
      
   protected:
      Identifier *id_;

      
      BasicDeclarator(Identifier *id, const SourceLoc& loc):
         ASTDeclarator(loc), id_(id) {}
   };


   class PointerDeclarator: public ASTDeclarator {
   public:
      int depth() const { return depth_; }
      ASTDeclarator *declarator() const { return declarator_; }
      virtual Identifier *id() const override { return declarator_->id(); }
      virtual Kind kind() const override { return Kind::DECLARATOR_POINTER; }
      
      static PointerDeclarator *Create(int depth, ASTDeclarator *declarator, const SourceLoc& loc)
      { return new PointerDeclarator(depth, declarator, loc); }

      virtual void DumpNode(std::ostream& os) const override;
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {
         declarator_->Dump(os, level, with_types);
      }

      /* virtual void TypeCheck(SemantEnv& env) override {
         declarator()->TypeCheck(env);
         } */
      
      virtual ASTType *Type(ASTType *init_type) const override;
      
      virtual void JoinPointers() override;

   protected:
      int depth_;
      ASTDeclarator *declarator_;

      PointerDeclarator(int depth, ASTDeclarator *declarator, const SourceLoc& loc):
         ASTDeclarator(loc), depth_(depth), declarator_(declarator) {}
   };

   class FunctionDeclarator: public ASTDeclarator {
   public:
      ASTDeclarator *declarator() const { return declarator_; }
      VarDeclarations *params() const { return params_; }
      virtual Identifier *id() const override { return declarator_->id(); }
      virtual Kind kind() const override { return Kind::DECLARATOR_FUNCTION; }

      template <typename... Args>
      static FunctionDeclarator *Create(Args... args) {
         return new FunctionDeclarator(args...);
      }

      virtual void get_declarables(Declarations* output) const override;
      
      virtual void DumpNode(std::ostream& os) const override { os << "FunctionDeclarator"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      virtual ASTType *Type(ASTType *init_type) const override;
      
      virtual void JoinPointers() override;
      
   protected:
      ASTDeclarator *declarator_;
      VarDeclarations *params_;

      FunctionDeclarator(ASTDeclarator *declarator, VarDeclarations *params, const SourceLoc& loc):
         ASTDeclarator(loc), declarator_(declarator), params_(params) {}
   };

   class ArrayDeclarator: public ASTDeclarator {
   public:
      ASTDeclarator *declarator() const { return declarator_; }
      ASTExpr *count_expr() const { return count_expr_; }
      virtual Identifier *id() const override { return declarator_->id(); }
      virtual Kind kind() const override { return Kind::DECLARATOR_ARRAY; }

      template <typename... Args>
      static ArrayDeclarator *Create(Args... args) { return new ArrayDeclarator(args...); }

      virtual void DumpNode(std::ostream& os) const override { os << "ArrayDeclarator"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;
      virtual ASTType *Type(ASTType *init_type) const override;
      virtual void JoinPointers() override { declarator()->JoinPointers(); }

      
   protected:
      ASTDeclarator *declarator_;
      ASTExpr *count_expr_;

      template <typename... Args>
      ArrayDeclarator(ASTDeclarator *declarator, ASTExpr *count_expr, Args... args):
         ASTDeclarator(args...), declarator_(declarator), count_expr_(count_expr) {}
   };


   class Identifier: public ASTNode {
   public:
      Symbol *id() const { return id_; }
      
      static Identifier *Create(const std::string& id, SourceLoc& loc) { return new Identifier(id, loc); }

      virtual void DumpNode(std::ostream& os) const override;
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {
         /* no children */
      }

   protected:
      Symbol *id_;
      
      Identifier(const std::string& id, SourceLoc& loc): ASTNode(loc), id_(nullptr) {
         if (g_id_tab.find(id) == g_id_tab.end()) {
            g_id_tab[id] = new std::string(id);
         }
         id_ = g_id_tab[id];
      }
   };




}

#endif
