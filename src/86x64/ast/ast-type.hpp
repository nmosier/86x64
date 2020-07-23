#ifndef __AST_HPP
#error "include \"ast.hpp\", not \"ast-type.hpp\" directly"
#endif

#ifndef __AST_TYPE_HPP
#define __AST_TYPE_HPP

#include <vector>

#include "uniq_vector.hpp"
#include "ast-decl.hpp"
#include "asm-fwd.hpp"

extern const char *g_filename;

namespace zc {

   extern IntegralType *default_type;
   extern IntegralType *char_type;
   extern IntegralType *intptr_type;

   
   bool TypeEq(const Types *lhs, const Types *rhs);   

   int unsigned_bits(int bytes);
   int signed_bits(int bytes);

   class ASTType: public ASTNode {
   public:
      enum class Kind {TYPE_VOID,
                       TYPE_INTEGRAL,
                       TYPE_POINTER,
                       TYPE_FUNCTION,
                       TYPE_TAGGED,
                       TYPE_ARRAY};
      virtual Kind kind() const = 0;

      bool is_callable() const { return get_callable() != nullptr; }
      virtual const FunctionType *get_callable() const { return nullptr; }

      template <typename InputIt>
      bool has_type(InputIt begin, InputIt end) const
      { return std::find(begin, end, kind()) != end; }
      template <typename T>
      bool has_type(T list) const
      { return std::find(list.begin(), list.end(), kind()) != list.end(); }

      virtual const IntegralType *int_type() const { return nullptr; }
      
      /**
       * Add all declarable types to list.
       */
      virtual void get_declarables(Declarations* output) const = 0;

      /**
       * Whether type contains another type (i.e. is a pointer or array).
       */
      bool is_container() const { return get_containee() != nullptr; }
      virtual ASTType *get_containee() const { return nullptr; }
      
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {}
      
      virtual ASTType *TypeResolve(SemantEnv& env) = 0;
      void TypeCheck(SemantEnv& env) { TypeCheck(env, true); }
      virtual void TypeCheck(SemantEnv& env, bool allow_void) = 0;
      virtual bool TypeEq(const ASTType *other) const = 0;
      virtual bool TypeCoerce(const ASTType *from) const = 0;
      void Enscope(SemantEnv& env);

      virtual ASTType *Address() = 0;
      virtual ASTType *Dereference(SemantEnv *env);
      virtual bool decays() const { return false; }
      virtual ASTType *Decay() { return this; }

      virtual int bytes() const = 0;
      virtual int bits() const = 0;
      
      void FrameGen(StackFrame& frame) const;

      static ASTType *Create(ASTType *specs, ASTDeclarator *declarator) {
         return declarator->Type(specs);         
      }
      
   protected:
      template <typename... Args>
      ASTType(Args... args): ASTNode(args...) {}
   };
   std::ostream& operator<<(std::ostream& os, ASTType::Kind kind);

   class PointerType: public ASTType {
   public:
      virtual Kind kind() const override { return Kind::TYPE_POINTER; }
      virtual const FunctionType *get_callable() const override;
      virtual ASTType *get_containee() const override { return pointee(); }
      int depth() const { return depth_; }
      ASTType *pointee() const { return pointee_; }
      bool is_void() const { return pointee()->kind() == Kind::TYPE_VOID; }

      template <typename... Args>
      static PointerType *Create(Args... args) {
         return new PointerType(args...);
      }

      virtual const IntegralType *int_type() const override { return intptr_type; }

      virtual void get_declarables(Declarations* output) const override;

      virtual void DumpNode(std::ostream& os) const override;

      virtual ASTType *TypeResolve(SemantEnv& env) override {
         pointee_ = pointee_->TypeResolve(env);
         return this;
      }      
      virtual bool TypeEq(const ASTType *other) const override;
      virtual bool TypeCoerce(const ASTType *from) const override;
      virtual void TypeCheck(SemantEnv& env, bool allow_void) override;

      virtual ASTType *Address() override;
      virtual ASTType *Dereference(SemantEnv *env = nullptr) override;

      virtual int bytes() const override { return z80::long_size; }
      virtual int bits() const override { return unsigned_bits(bytes()); }
      
   protected:
      int depth_;
      ASTType *pointee_;

      template <typename... Args>
      PointerType(int depth, ASTType *pointee, Args... args):
         ASTType(args...), depth_(depth), pointee_(pointee) {}
   };

   class FunctionType: public ASTType {
   public:
      virtual Kind kind() const override { return Kind::TYPE_FUNCTION; }
      virtual const FunctionType *get_callable() const override { return this; }
      ASTType *return_type() const { return return_type_; }
      VarDeclarations *params() const { return params_; }
      
      template <typename... Args>
      static FunctionType *Create(Args... args) {
         return new FunctionType(args...);
      }

      virtual void get_declarables(Declarations* output) const override;
      
      virtual void DumpNode(std::ostream& os) const override;

      virtual ASTType *TypeResolve(SemantEnv& env) override;
      virtual bool TypeEq(const ASTType *other) const override;
      virtual bool TypeCoerce(const ASTType *from) const override;
      virtual void TypeCheck(SemantEnv& env, bool allow_void) override;

      virtual ASTType *Address() override;
      virtual ASTType *Dereference(SemantEnv *env = nullptr) override;

      virtual int bytes() const override { return z80::long_size; }
      virtual int bits() const override { return unsigned_bits(bytes()); }
      
   protected:
      ASTType *return_type_ = nullptr;
      VarDeclarations *params_ = nullptr;

      template <typename... Args>
      FunctionType(ASTType *return_type, VarDeclarations *params, Args... args):
         ASTType(args...), return_type_(return_type), params_(params) {}
   };

   class VoidType: public ASTType {
   public:
      virtual Kind kind() const override { return Kind::TYPE_VOID; }

      virtual void get_declarables(Declarations* output) const override {}
      
      virtual void DumpNode(std::ostream& os) const override { os << "VoidType VOID"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {}

      virtual ASTType *TypeResolve(SemantEnv& env) override { return this; }
      virtual void TypeCheck(SemantEnv& env, bool allow_void) override;
      virtual bool TypeEq(const ASTType *other) const override;
      virtual bool TypeCoerce(const ASTType *from) const override { return TypeEq(from); }

      virtual ASTType *Address() override {
         throw std::logic_error("attempted to take address of 'void' type");
      }

      virtual ASTType *Dereference(SemantEnv *env) override {
         throw std::logic_error("attempted to dereference 'void' type");
      }

      virtual int bytes() const override { return 0; }
      virtual int bits() const override { return 0; }
      
      template <typename... Args>
      static VoidType *Create(Args... args) { return new VoidType(args...); }
      
   protected:
      template <typename... Args>
      VoidType(Args... args): ASTType(args...) {}
   };

   class IntegralType: public ASTType {
   public:
      enum class IntKind {SPEC_BOOL, SPEC_CHAR, SPEC_SHORT, SPEC_INT, SPEC_LONG, SPEC_LONG_LONG};
      virtual Kind kind() const override { return Kind::TYPE_INTEGRAL; }
      IntKind int_kind() const { return int_kind_; }
      bool is_signed() const { return is_signed_; }

      virtual void get_declarables(Declarations* output) const override {}
      virtual const IntegralType *int_type() const override { return this; }

      virtual void DumpNode(std::ostream& os) const override;
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {}

      virtual ASTType *TypeResolve(SemantEnv& env) override { return this; }      
      virtual void TypeCheck(SemantEnv& env, bool allow_void) override {}
      virtual bool TypeEq(const ASTType *other)  const override;
      virtual bool TypeCoerce(const ASTType *from) const override { return true; } // TODO: huh?
      IntegralType *Max(const IntegralType *other) const;

      virtual ASTType *Address() override;
      virtual ASTType *Dereference(SemantEnv *env) override {
         throw std::logic_error("attempted to dereference integral type");
      }

      virtual int bytes() const override;
      virtual int bits() const override {
         return (is_signed() ? signed_bits : unsigned_bits)(bytes());
      }

      template <typename... Args>
      static IntegralType *Create(Args... args) { return new IntegralType(args...); }

      /**
       * Find smallest type that can fit this value.
       */

   protected:
      IntKind int_kind_;
      bool is_signed_;
      
      static IntKind min_type(intmax_t val);

      template <typename... Args>
      IntegralType(IntKind int_kind, bool is_signed, Args... args):
         ASTType(args...), int_kind_(int_kind), is_signed_(is_signed) {}

      template <typename... Args>
      IntegralType(intmax_t val, Args... args):
         ASTType(args...), int_kind_(min_type(val)), is_signed_(val < 0) {}
   };

   /**
    * A type that can be declared.
    * NOTE: abstract.
    */
   class DeclarableType: public ASTType {
   public:
      virtual void get_declarables(Declarations* output) const override;

      virtual Symbol *get_declarable_sym() const = 0;
      
      virtual void Declare(SemantEnv& env) = 0;
      virtual void Declare(CgenEnv& env) = 0;
      
   protected:
      template <typename... Args>
      DeclarableType(Args... args): ASTType(args...) {}
   };

   class TaggedType: public DeclarableType {
   public:
      /**
       * The (not necessarily unique) tag name for this tagged type.
       */
      Symbol *tag() const { return tag_; }
      virtual Symbol *get_declarable_sym() const override { return tag(); }      

      /**
       * The unique, scope-independent identifier for this tagged type.
       * All instances of this type shall have the same unique_id.
       */
      int unique_id() const { return unique_id_; }
      
      enum class TagKind {TAG_STRUCT, TAG_UNION, TAG_ENUM};
      virtual TagKind tag_kind() const = 0;
      virtual Kind kind() const override { return Kind::TYPE_TAGGED; }

      /**
       * Determine whether tagged type has been defined (not just declared).
       */
      virtual bool is_complete() const = 0;

      /**
       * Complete the definition of this tagged type instance given the definition of another
       * instance.
       * NOTE: Expects that @param other be of same derived type as this.
       */
      virtual void complete(const TaggedType *def) = 0;
      
      virtual void Declare(SemantEnv& env) override;
      virtual void Declare(CgenEnv& env) override {}

      virtual void DumpNode(std::ostream& os) const override;
      
      virtual ASTType *Address() override;
      virtual ASTType *Dereference(SemantEnv *env) override;
      virtual void EnscopeTag(SemantEnv& env);

      virtual void TypeCheck(SemantEnv& env, bool allow_void) override;
      virtual void TypeCheckMembs(SemantEnv& env) = 0;
      virtual bool TypeEq(const ASTType *other) const override;      
      virtual void DeclareMembs(SemantEnv& env) = 0;
      
   protected:
      Symbol *tag_;
      int unique_id_;
      static int unique_id_counter_;

      virtual const char *name() const = 0;
      void AssignUniqueID(SemantEnv& env);

      TaggedType(Symbol *tag, const SourceLoc& loc):
         DeclarableType(loc), tag_(tag), unique_id_(-1) {}
   };

   template <typename Memb>
   class TaggedType_aux: public TaggedType {
      struct GetSym {
         Symbol *operator()(Memb *memb) { return memb->sym(); }
      };
      typedef uniq_vector<Memb *, Symbol *, GetSym> Membs;
   public:
      Membs *membs() const { return membs_; }
      virtual bool is_complete() const override { return membs() != nullptr; }
      virtual void complete(const TaggedType *other) override {
         membs_ = dynamic_cast<const TaggedType_aux<Memb> *>(other)->membs();
      }

      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {
         if (membs() != nullptr) {
            for (auto memb : *membs()) {
               memb->Dump(os, level, with_types);
            }
         }
      }
      
      virtual void DeclareMembs(SemantEnv& env) override {
         if (membs() != nullptr) {
            for (auto memb : *membs()) {
               memb->Declare(env);
            }
         }
      }
      
   protected:
      Membs *membs_;

      template <typename... Args>
      TaggedType_aux(Args... args): TaggedType(args...), membs_(nullptr) {}      

      template <typename InputIt>
      TaggedType_aux(SemantError& err, InputIt membs_begin, InputIt membs_end,
                     Symbol *tag, const SourceLoc& loc):
         TaggedType(tag, loc),
         membs_(new Membs(membs_begin, membs_end,
                [&](Memb *first, Memb *second) {
                   err(g_filename, second) << "duplicate member '" << *second->sym() << "'"
                                               << std::endl;

                })) {}

      template <typename InputIt, typename Func, typename... Args>
      TaggedType_aux(SemantError& err, InputIt begin, InputIt end,
                     Func func, Args... args): TaggedType(args...), membs_(nullptr) {
         /* convert */
         std::vector<Memb *> vec;
         for (; begin != end; ++begin) {
            auto memb = func(err, begin);
            if (memb != nullptr) {
               vec.push_back(memb);
            }
         }

         membs_ = new Membs(vec.begin(), vec.end(),
                            [&](Memb *first, Memb *second) {
                               err(g_filename, second) << "duplicate member '"
                                                       << *second->sym() << "'"
                                                       << std::endl;
                               
                            });
      }
   };

   /**
    * Abstract tagged type that has members (i.e. struct or union).
    */
   class CompoundType: public TaggedType_aux<VarDeclaration> {
   public:
      virtual void get_declarables(Declarations* output) const override;
      
      virtual ASTType *TypeResolve(SemantEnv& env) override;
      virtual bool TypeCoerce(const ASTType *from) const override;

      virtual int bytes() const override = 0;
      virtual int offset(const Symbol *sym) const = 0;

      virtual void TypeCheckMembs(SemantEnv& env) override {
         if (membs()) {
            for (auto decl : *membs()) {
               decl->TypeCheck(env);
            }
         }
      }

      virtual void DeclareMembs(SemantEnv& env) override;
      
   protected:
      template <typename... Args>
      CompoundType(Args... args): TaggedType_aux<VarDeclaration>(args...) {}

      template <typename InputIt, typename... Args>
      CompoundType(SemantError& err, InputIt begin, InputIt end, Args... args):
         TaggedType_aux<VarDeclaration>
         (err, begin, end,
          [&](SemantError& err, InputIt it) -> VarDeclaration * {
             auto var_decl = dynamic_cast<VarDeclaration *>(*it);
             if (var_decl == nullptr) {
                err(g_filename, this) << "declaration not allowed here" << std::endl;
             }
             return var_decl;
          }, args...) {}

         
      
   };
   
   class StructType: public CompoundType {
   public:
      virtual TagKind tag_kind() const override { return TagKind::TAG_STRUCT; }

      template <typename... Args>
      static StructType *Create(Args... args) { return new StructType(args...); }

      virtual int bytes() const override;
      virtual int bits() const override { return unsigned_bits(bytes()); }
      virtual int offset(const Symbol *sym) const override;

   protected:
      virtual const char *name() const override { return "struct"; }

      template <typename... Args>
      StructType(Args... args): CompoundType(args...) {}
   };

   class UnionType: public CompoundType {
   public:
      virtual TagKind tag_kind() const override { return TagKind::TAG_UNION; }      

      template <typename... Args>
      static UnionType *Create(Args... args) { return new UnionType(args...); }

      virtual int bytes() const override;
      virtual int bits() const override { return unsigned_bits(bytes()); }
      virtual int offset(const Symbol *sym) const override { return 0; }
      
   protected:
      virtual const char *name() const override { return "union"; }

      template <typename... Args>
      UnionType(Args... args): CompoundType(args...) {}
   };

   class Enumerator: public ASTNode {
   public:
      Identifier *id() const { return id_; }
      Symbol *sym() const { return id()->id(); }
      ASTExpr *val() const { return val_; }
      EnumType *enum_type() const { return enum_type_; }

      void Declare(SemantEnv& env);
      void Declare(CgenEnv& env);

      intmax_t eval() const {
         if (val() == nullptr) {
            if (prev_ == nullptr) {
               return 0;
            } else {
               return prev_->eval() + 1;
            }
         } else {
            return val()->int_const();
         }
      }

      virtual void DumpNode(std::ostream& os) const override;
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      void TypeCheck(SemantEnv& env, EnumType *enum_type);
      
      template <typename... Args>
      static Enumerator *Create(Args... args) { return new Enumerator(args...); }
      
   private:
      Identifier *id_;
      ASTExpr *val_;
      const Enumerator *prev_; /*!< Previous enumerator. Gend for assigning values. */
      EnumType *enum_type_; /*!< Type, which shall be assigned during semantic analysis. */
      
      template <typename... Args>
      Enumerator(Identifier *id, ASTExpr *val, const Enumerator *prev, Args... args):
         ASTNode(args...), id_(id), val_(val), prev_(prev), enum_type_(nullptr) {}
   };
   
   class EnumType: public TaggedType_aux<Enumerator> {
   public:
      virtual TagKind tag_kind() const override {return TagKind::TAG_ENUM; }
      virtual IntegralType *int_type() const override { return int_type_; }
      
      virtual ASTType *TypeResolve(SemantEnv& env) override { return this; } /* TODO */
      virtual void TypeCheckMembs(SemantEnv& env) override;
      virtual bool TypeCoerce(const ASTType *other) const override;

      // virtual void CodeGen(CgenEnv& env) override;
      virtual void Declare(SemantEnv& env) override;
      virtual void Declare(CgenEnv& env) override;
      
      virtual int bytes() const override { return int_type_->bytes(); }
      virtual int bits() const override { return int_type_->bits(); }

      template <typename... Args>
      static EnumType *Create(Args... args) { return new EnumType(args...); }
      
   protected:
      IntegralType *int_type_;
      
      virtual const char *name() const override { return "enum"; }

      virtual void DeclareMembs(SemantEnv& env) override;

      template <typename InputIt>
      EnumType(SemantError& err, InputIt begin, InputIt end, Symbol *tag, const SourceLoc& loc):
         TaggedType_aux<Enumerator>(err, begin, end, tag, loc) {
         int_type_ = IntegralType::Create(IntegralType::IntKind::SPEC_INT, true, 0);
      }

      EnumType(Symbol *tag, const SourceLoc& loc):
         TaggedType_aux<Enumerator>(tag, loc) {
         int_type_ = IntegralType::Create(IntegralType::IntKind::SPEC_INT, true, 0);
      }
   };


   class ArrayType: public ASTType {
   public:
      virtual Kind kind() const override { return Kind::TYPE_ARRAY; }
      ASTType *elem() const { return elem_; }
      ASTExpr *count() const { return count_; }
      intmax_t int_count() const { return int_count_; }
      virtual ASTType *get_containee() const override { return elem(); }            

      virtual void get_declarables(Declarations* output) const override;
      
      virtual void DumpNode(std::ostream& os) const override;

      virtual ASTType *TypeResolve(SemantEnv& env) override {
         elem_ = elem_->TypeResolve(env);
         return this;
      }
      virtual bool TypeEq(const ASTType *other) const override;
      virtual bool TypeCoerce(const ASTType *from) const override;
      virtual void TypeCheck(SemantEnv& env, bool allow_void) override;

      virtual ASTType *Address() override;
      virtual ASTType *Dereference(SemantEnv *env) override { return elem(); }
      virtual bool decays() const override { return true; }
      virtual ASTType *Decay() override;

      virtual int bytes() const override;
      virtual int bits() const override { return unsigned_bits(bytes()); }

      template <typename... Args>
      static ArrayType *Create(Args... args) { return new ArrayType(args...); }
      
   protected:
      ASTType *elem_;
      ASTExpr *count_; /*!< this expression must be constant */
      intmax_t int_count_; /*!< result after @see count_ is converted to 
                            * integer during semantic analysis */

      template <typename... Args>
      ArrayType(ASTType *elem, ASTExpr *count, Args... args):
         ASTType(args...), elem_(elem), count_(count) {}
   };

   class NamedType: public ASTType {
   public:
      Symbol *sym() const { return sym_; }

      ASTType *Type(SemantEnv& env) const;
      virtual Kind kind() const override { throw std::logic_error("unresolved named type"); }
      virtual void get_declarables(Declarations *output) const override {}

      virtual ASTType *TypeResolve(SemantEnv& env) override;
      virtual void TypeCheck(SemantEnv& env, bool allow_void) override {
         throw std::logic_error("unresolved named type");
      }
      virtual bool TypeEq(const ASTType *other) const override {
         throw std::logic_error("unresolved named type");
      }
      virtual bool TypeCoerce(const ASTType *from) const override {
         throw std::logic_error("unresolved named type");
      }
      virtual ASTType *Address() override { throw std::logic_error("unresolved named type"); }
      virtual ASTType *Dereference(SemantEnv *env) override {
         throw std::logic_error("unresolved named type");
      }
      virtual int bytes() const override { throw std::logic_error("unresolved named type"); }
      virtual int bits() const override { throw std::logic_error("unresolved named type"); }
      
      template <typename... Args>
      static NamedType *Create(Args... args) { return new NamedType(args...); }
      
   private:
      Symbol *sym_;

      template <typename... Args>
      NamedType(Symbol *sym, Args... args): ASTType(args...), sym_(sym) {}
   };

   std::ostream& operator<<(std::ostream& os, IntegralType::IntKind kind);
   std::ostream& operator<<(std::ostream& os, CompoundType::TagKind kind);

   /*** PROTOTYPES ***/
   template <IntegralType::IntKind int_kind, bool is_signed> IntegralType *int_type =
                           IntegralType::Create(int_kind, is_signed, 0);

}
   
#endif
