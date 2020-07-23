#include <unordered_set>

#include "ast.hpp"
#include "symtab.hpp"
#include "semant.hpp"
#include ZC_PARSER_HEADER
#include "util.hpp"

 extern const char *g_filename;

 namespace zc {

    bool g_semant_debug = false;
    SemantError g_semant_error(std::cerr);
    SemantEnv g_semant_env(g_semant_error);

    void Semant(TranslationUnit *root) {
       //       SemantEnv env(g_semant_error);
       root->Enscope(g_semant_env);
       root->TypeCheck(g_semant_env);
       root->Descope(g_semant_env);
    }

    bool SemantExtEnv::LabelCompare::operator()(const Identifier *lhs, const Identifier *rhs) const {
       return lhs->id() < rhs->id();
    }

    std::ostream& SemantError::operator()(const char *filename, const ASTNode *node) {
       return SemantError::operator()(filename, node->loc());
    }
    
    std::ostream& SemantError::operator()(const char *filename, const SourceLoc& loc) {
       errors_++;
       os_ << filename << ":" << loc << ": ";
       return os_;
    }

    void SemantExtEnv::Enter(Symbol *sym) {
       sym_env_.Enter(sym);
       label_refs_.clear();
       label_defs_.clear();
    }

    void SemantExtEnv::Exit(SemantError& err) {
       std::vector<const Identifier *> undefined_labels;
       std::set_difference(label_refs_.begin(), label_refs_.end(),
                           label_defs_.begin(), label_defs_.end(),
                           std::back_inserter(undefined_labels),
                           LabelCompare());
       for (const Identifier *label : undefined_labels) {
          err(g_filename, label) << "use of undeclared label '" << *label->id() << "'" << std::endl;
       }
    }

    void SemantExtEnv::LabelRef(const Identifier *id) {
      label_refs_.insert(id);
   }

    void SemantExtEnv::LabelDef(SemantError& err, const Identifier *id) {
       if (label_defs_.find(id) != label_defs_.end()) {
          err(g_filename, id) << "redefinition of label '" << *id->id() << "'" << std::endl;
       } else {
          label_defs_.insert(id);
       }
    }

    static IntegralType::IntKind token_to_intspec(int token) {
       using Kind = IntegralType::IntKind;
       std::unordered_map<int,Kind> map
          {{CHAR, Kind::SPEC_CHAR},
           {SHORT, Kind::SPEC_SHORT},
           {INT, Kind::SPEC_INT},
           {LONG, Kind::SPEC_LONG}
          };
       return map[token];
    }



    /*** TYPE CHECK ***/

    void PointerType::TypeCheck(SemantEnv& env, bool allow_void) {
       pointee()->TypeCheck(env, true);
    }

    void FunctionType::TypeCheck(SemantEnv& env, bool allow_void) {
       /* NOTE: function's don't need to have complete return types (i.e. they can be 'void').
        * Therefore, we need not type-check the function's return type.
        */

       /* Functions cannot return functions, only function pointers. Verify this: */
       if (return_type()->kind() == Kind::TYPE_FUNCTION) {
          env.error()(g_filename, this) << "functions cannot return other functions, "
                                        << "only function pointers" << std::endl;
          /* error recovery: transform return type into function */
          return_type_ = PointerType::Create(1, return_type(), return_type()->loc());
       }
       return_type()->TypeCheck(env, true);
       for (auto param : *params()) {
          param->type()->TypeCheck(env);
       }
    }

    void TaggedType::TypeCheck(SemantEnv& env, bool allow_void) {
       TypeCheckMembs(env);
    }
    
    void TaggedType::Declare(SemantEnv& env) {
       AssignUniqueID(env);
       
       if (is_complete()) {
          /* this tagged type is a complete definition */
          DeclareMembs(env);

          /* enscope struct if it isn't anonymous */
          if (tag() != nullptr) {
             EnscopeTag(env);
          }
       } else {
          /* struct cannot be anonymous by C syntax */
          const TaggedType *type = env.tagtab().Lookup(tag());
          if (type == nullptr) {
             /* forward-declare struct */
             EnscopeTag(env);
          } else if (type->tag_kind() != tag_kind()) {
             /* tagged kinds do not match */
             env.error()(g_filename, this) << "use of '" << *tag()
                                           << "' with tag type that does "
                                           << "not match previous declaration" << std::endl;
          } else {
             /* otherwise, define this type with other defined type */
             complete(type);
          }
       }

       /* set unique ID */
       if (unique_id() < 0) {
          unique_id_ = env.tagtab().Lookup(tag())->unique_id();
       }
    }

    void CompoundType::DeclareMembs(SemantEnv& env) {
       if (membs()) {
          for (auto memb : *membs()) {
             auto type_memb = dynamic_cast<TypeDeclaration *>(memb);
             if (type_memb) {
                type_memb->Declare(env);
             }
          }
       }
    }

    void VarDeclaration::Declare(SemantEnv& env) {
       if (sym() == nullptr) {
          env.error()(g_filename, this) << "expected variable declaration" << std::endl;
          return;
       }

       if (env.symtab().Probe(sym()) != nullptr) {
           /* ERROR: symbol already defined in this scope. */
           env.error()(g_filename, this) << "redefinition of '" << *sym() << "'" << std::endl;
           return;          
       }

       /* resolve type */
       type_ = type()->TypeResolve(env);

       /* type check type */
       type()->TypeCheck(env, false);

       /* add symbol to scope */
       env.symtab().AddToScope(sym(), this);
    }

    void TypeDeclaration::Declare(SemantEnv& env) {
        type_ = type()->TypeResolve(env);
       dynamic_cast<DeclarableType *>(type())->Declare(env);
    }

    void TypenameDeclaration::Declare(SemantEnv& env) {
       if (env.symtab().Probe(sym())) {
          env.error()(g_filename, this) << "redefinition of '" << *sym() << "' as different kind of "
                                        << "symbol" << std::endl;
          return;
       }

       type_ = type()->TypeResolve(env);
       
       /* type check type */
       type()->TypeCheck(env, false);

       /* add to scope */
       env.symtab().AddToScope(sym(), this);
    }

    void EnumType::Declare(SemantEnv& env) {
       TaggedType::Declare(env);

    }

    void Enumerator::Declare(SemantEnv& env) {
       TypeCheck(env, enum_type());
    }

    void EnumType::DeclareMembs(SemantEnv& env) {
       if (membs_ != nullptr) {
          for (auto enumerator : *membs_) {
             enumerator->TypeCheck(env, this);
          }
       }
    }


    void Enumerator::TypeCheck(SemantEnv& env, EnumType *enum_type) {
       enum_type_ = enum_type;
       
       if (val() != nullptr) {
          val()->TypeCheck(env);
          if (!enum_type->TypeCoerce(val()->type())) {
             env.error()(g_filename, this) << "value assigned to enumerator '" << *sym()
                                           << "' is not coercible to enum type"
                                           << std::endl;
          }
          if (!val()->is_const()) {
             env.error()(g_filename, this) << "enumerator value for '"
                                           << *sym()
                                           << "' is not constant"
                                           << std::endl;
          }
       }

       /* NOTE: an enumerator can be assigned a previous enumerator, 
        * so enscoping each enumerator after typechecking is necessary. */
       if (env.symtab().Probe(sym()) != nullptr) {
          env.error()(g_filename, this) << "redefinition of '" << *sym()
                                        << "' as different kind of symbol" << std::endl;
       } else {
          env.symtab().AddToScope(sym(),
                                  VarDeclaration::Create(sym(), true, enum_type, loc()));
       }

    }

    void ArrayType::TypeCheck(SemantEnv& env, bool allow_void) {
       /* For now, assert that size parameter for arrays must always be given. */
       elem()->TypeCheck(env, false);

       count()->TypeCheck(env);

    }

    void TaggedType::EnscopeTag(SemantEnv& env) {
       assert(tag() != nullptr); /* ensure tagged type isn't anonymous */

       TaggedType *tagged_type = env.tagtab().Lookup(tag());
       if (tagged_type == nullptr) {
          /* first mention of tag */
          env.tagtab().AddToScope(tag(), this);
          unique_id_ = unique_id_counter_++;
       } else if (kind() != tagged_type->kind()) {
          /* tagged kinds do not match */
          env.error()(g_filename, this) << "use of '" << *tag()
                                        << "' with tag type that does "
                                        << "not match previous declaration" << std::endl;
       } else if (!tagged_type->is_complete()) {
          /* tag in table is incomplete */
          if (is_complete()) {
             /* this tagged type has a full definition, 
              * so add members to existing enscoped struct */
             tagged_type->complete(this);
          }
       } else { // tagged_type is defined
          if (!is_complete()) {
             complete(tagged_type);
          } else {
             if (env.tagtab().Probe(tag()) != nullptr) {
                /* redefined in same scope -- error */
                env.error()(g_filename, this) << "redefinition of "
                                              << tag_kind()
                                              << " '" << *tag()
                                              << "'" << std::endl;
             } else {
                /* redefinition in different scope -- OK */
                env.tagtab().AddToScope(tag(), this);
             }
          }
       }
    }

    void TaggedType::AssignUniqueID(SemantEnv& env) {
       auto lookup_type = env.tagtab().Lookup(tag());
       auto probe_type = env.tagtab().Probe(tag());
       if (lookup_type == nullptr) { /* first declaration of tag */
          unique_id_ = unique_id_counter_++;
       } else if (probe_type == nullptr && is_complete()) { /* scoped redefinition of tag */
          unique_id_ = unique_id_counter_++;
       } else { /* reference to previously declared tag */
          unique_id_ = lookup_type->unique_id();
       }
    }

    void TranslationUnit::TypeCheck(SemantEnv& env) {
       Enscope(env);
       for (auto decl : *decls()) {
          decl->TypeCheck(env);
       }
       Descope(env);
    }

    void ExternalDecl::TypeCheck(SemantEnv& env) {
       decl()->Declare(env);
    }

    void FunctionDef::TypeCheck(SemantEnv& env) {
       Enscope(env);
       comp_stat()->TypeCheck(env, false); /* function body doesn't get new scope */
       Descope(env);
    }

   void CompoundStat::TypeCheck(SemantEnv& env, bool scoped) {
      if (scoped) {
         env.EnterScope();
      }

      for (auto decl : *decls()) {
         decl->Declare(env);
      }
      
      for (auto stat : *stats()) {
         stat->TypeCheck(env);
      }

      if (scoped) {
         env.ExitScope();
      }
   }

    void ReturnStat::TypeCheck(SemantEnv& env) {
       expr()->TypeCheck(env);

       const ASTType *expr_type = expr()->type();
       Symbol *fn_sym = env.ext_env().sym();
       VarDeclaration *var = dynamic_cast<VarDeclaration *>(env.symtab().Lookup(fn_sym));
       assert(var);
       const FunctionType *fn_type = dynamic_cast<const FunctionType *>(var->type());
       assert(fn_type);
       ASTType *ret_type = fn_type->return_type();
       if (!ret_type->TypeCoerce(expr_type)) {
          env.error()(g_filename, this) << "value in return statement has incompatible type"
                                        << std::endl;
       } else if (!ret_type->TypeEq(expr_type)) {
          /* add explicit cast */
       }
    }

    void IfStat::TypeCheck(SemantEnv& env) {
       cond()->TypeCheck(env);
       if_body()->TypeCheck(env);
       if (else_body() != nullptr) {
          else_body()->TypeCheck(env);
       }
    }

    void WhileStat::TypeCheck(SemantEnv& env) {
       env.stat_stack().Push(this);
       pred()->TypeCheck(env);
       body()->TypeCheck(env);
       env.stat_stack().Pop();
    }

    void ForStat::TypeCheck(SemantEnv& env) {
       env.stat_stack().Push(this);
       init()->TypeCheck(env);
       pred()->TypeCheck(env);
       after()->TypeCheck(env);
       body()->TypeCheck(env);
       env.stat_stack().Pop();
    }

    void GotoStat::TypeCheck(SemantEnv& env) {
       env.ext_env().LabelRef(label_id());
    }

    void BreakStat::TypeCheck(SemantEnv& env) {
       if (env.stat_stack().Find([](auto stat){ return stat->can_break(); }) == nullptr) {
          env.error()(g_filename, this) << "'break' statement not in loop or switch statement"
                                        << std::endl;
       }
    }

    void ContinueStat::TypeCheck(SemantEnv& env) {
       if (env.stat_stack().Find([](auto stat){ return stat->can_continue(); }) == nullptr) {
          env.error()(g_filename, this) << "'continue' statement not in loop statement"
                                        << std::endl;
       }
    }

    void LabeledStat::TypeCheck(SemantEnv& env) {
       stat()->TypeCheck(env);
    }
    
    void LabelDefStat::TypeCheck(SemantEnv& env) {
       env.ext_env().LabelDef(env.error(), label_id());
       LabeledStat::TypeCheck(env);
    }

    void ExprStat::TypeCheck(SemantEnv& env) {
       expr()->TypeCheck(env);
    }


   void AssignmentExpr::TypeCheck(SemantEnv& env) {
      lhs_->TypeCheck(env);
      rhs_->TypeCheck(env);

      if (lhs_->expr_kind() != ExprKind::EXPR_LVALUE) {
         env.error()(g_filename, this) << "left-hand side of expression is not an lvalue"
                                       << std::endl;
      }

      if (!lhs_->type()->TypeCoerce(rhs_->type())) {
         env.error()(g_filename, this) << "assignment to incompatible type" << std::endl;
      }
      
      type_ = lhs_->type();

      /* add cast expression to coerce rhs value */
      if (!lhs_->type()->TypeEq(rhs_->type())) {
      }
   }

    void CallExpr::TypeCheck(SemantEnv& env) {
       fn()->TypeCheck(env);
       for (ASTExpr *param : *params()) {
          param->TypeCheck(env);
       }
       // params()->TypeCheck(env);

       /* Verify that called expression is of function or function pointer type. */
       const FunctionType *type;
       if ((type = fn()->type()->get_callable()) == nullptr) {
          env.error()(g_filename, this) << "expression is not callable" << std::endl;
          type_ = default_type;
          return;
       }

       type_ = type->return_type();
       
       /* verify that argument parameters can be coerced to function parameter types */
       if (type->params()->size() != params()->size()) {
          env.error()(g_filename, this) << "incorrect number of arguments given (expected"
                                        << type->params()->size() << " but got "
                                        << params()->size() << ")" << std::endl;
          return;
       }

       /* coerce types one-by-one */
       auto type_it = type->params()->begin();
       auto type_end = type->params()->end();
       auto arg_it = params()->begin();
       auto arg_end = params()->end();
       for (int i = 1; type_it != type_end; ++type_it, ++arg_it, ++i) {
          const ASTType *arg_type = (*arg_it)->type();
          if (!(*type_it)->type()->TypeCoerce(arg_type)) {
             env.error()(g_filename, this) << "cannot coerce type of argument " << i << std::endl;
          } else if (!(*type_it)->type()->TypeEq(arg_type)) {
             /* add explicit cast */
          }
       }

    }

    void CastExpr::TypeCheck(SemantEnv& env) {
       expr_->TypeCheck(env);
    }

    void MembExpr::TypeCheck(SemantEnv& env) {
       expr()->TypeCheck(env);
       if (expr()->type()->kind() != ASTType::Kind::TYPE_TAGGED) {
          env.error()(g_filename, this) << "member access into non-tagged type" << std::endl;
       }
       CompoundType *compound_type = dynamic_cast<CompoundType *>(expr()->type());
       if (compound_type->membs() == nullptr) {
          env.error()(g_filename, this) << "member access into incomplete "
                                        << compound_type->kind() << "'"
                                        << compound_type->tag() << "'" << std::endl;
          type_ = default_type;
       } else {
          auto it = compound_type->membs()->find(memb());
          if (it == compound_type->membs()->end()) {
             env.error()(g_filename, this) << "no member named '" << *memb() << "' in "
                                           << compound_type->kind() << "'"
                                           << compound_type->tag() << "'" << std::endl;
             type_ = default_type;
          } else {
             type_ = (*it)->type();
          }
       }
    }

    void SizeofExpr::TypeCheck(SemantEnv& env) {
       std::visit(overloaded {
             [&](auto val) { val->TypeCheck(env); }
                }, variant_);

       type_ = IntegralType::Create(IntegralType::IntKind::SPEC_LONG_LONG, false, loc());
    }

    void IndexExpr::TypeCheck(SemantEnv& env) {
       using IntKind = IntegralType::IntKind;
       
       base()->TypeCheck(env);
       index()->TypeCheck(env);
       if (index()->type()->kind() != ASTType::Kind::TYPE_INTEGRAL) {
          env.error()(g_filename, this) << "cannot index type with a non-integral value"
                                        << std::endl;
       } else {
          const IntegralType *int_type = dynamic_cast<const IntegralType *>(index()->type());
       }
       type_ = base()->type()->get_containee();
    }

   void UnaryExpr::TypeCheck(SemantEnv& env) {
      expr_->TypeCheck(env);

      ASTType *type = expr_->type();
      switch (kind_) {
      case Kind::UOP_ADDR: /* requires LVALUE */
         if (expr_->expr_kind() != ExprKind::EXPR_LVALUE) {
            env.error()(g_filename, this) << "attempt to take address of non-lvalue expression"
                                          << std::endl;
         }
         type_ = type->Address();
         break;
         
      case Kind::UOP_DEREFERENCE:
         if ((type_ = type->Dereference(&env)) == nullptr) {
            type_ = type;
         }
         break;
         
      case Kind::UOP_POSITIVE:
      case Kind::UOP_NEGATIVE:
      case Kind::UOP_BITWISE_NOT:
         /* requires integral type */         
         if (expr_->type()->kind() != ASTType::Kind::TYPE_INTEGRAL) {
            env.error()(g_filename, this) << "unary operator verboten for non-integral type"
                                          << std::endl;
         }
         type_ = expr_->type();
         break;

      case Kind::UOP_LOGICAL_NOT:
         break;

      case Kind::UOP_INC_PRE:
      case Kind::UOP_INC_POST:
      case Kind::UOP_DEC_PRE:
      case Kind::UOP_DEC_POST:
         /* require integral or pointer type */
         // IntegralType::Create(IntegralType::IntKind::SPEC_INT, 0)->TypeCoerce(expr_->type())
         if (!int_type<IntegralType::IntKind::SPEC_INT,true>->TypeCoerce(expr_->type()) &&
             expr_->type()->kind() != ASTType::Kind::TYPE_POINTER) {
            env.error()(g_filename, this) << "operand to increment/decrement must be "
                                          << "of integral or pointer type"
                                          << std::endl;
         }
         if (expr_->expr_kind() != ExprKind::EXPR_LVALUE) {
            env.error()(g_filename, this) << "operand to increment/decrement is not "
                                          << "assignable"
                                          << std::endl;
         }
         type_ = expr_->type();
         break;

      }
   }

   void BinaryExpr::TypeCheck(SemantEnv& env) {
      lhs_->TypeCheck(env);
      rhs_->TypeCheck(env);

      
      switch (kind()) {
      case Kind::BOP_LOGICAL_AND:
      case Kind::BOP_LOGICAL_OR:
         {
            auto expr_ok = [&](const ASTExpr *expr){
                              if (!expr->type()->has_type(std::array{ASTType::Kind::TYPE_INTEGRAL,
                                                                        ASTType::Kind::TYPE_POINTER})) {
                                 env.error()(g_filename, expr) << "binary logical operators "
                                                                  << "require integral or pointer "
                                                                  << "operands" << std::endl;
                                 }
                           };
            expr_ok(lhs());
            expr_ok(rhs());
         }
         break;

      case Kind::BOP_EQ:
      case Kind::BOP_NEQ:
      case Kind::BOP_LT:
      case Kind::BOP_LEQ:
      case Kind::BOP_GT:
      case Kind::BOP_GEQ:
      case Kind::BOP_BITWISE_AND:
      case Kind::BOP_BITWISE_OR:
      case Kind::BOP_BITWISE_XOR:
      case Kind::BOP_PLUS:
      case Kind::BOP_MINUS:
      case Kind::BOP_TIMES:
      case Kind::BOP_DIVIDE:
      case Kind::BOP_MOD:
         {
            bool lhs_int = lhs_->type()->kind() == ASTType::Kind::TYPE_INTEGRAL;
            bool rhs_int = rhs_->type()->kind() == ASTType::Kind::TYPE_INTEGRAL;

            if (lhs_int && rhs_int) {
               type_ = dynamic_cast<const IntegralType *>
                  (lhs_->type())->Max(dynamic_cast<const IntegralType *>(rhs_->type()));

               /* make implicit casts explicit */
               if (!lhs_->type()->TypeEq(type_)) {
               } else if (!rhs_->type()->TypeEq(type_)) {
               }
            } else {
               if (!lhs_int) {
                  env.error()(g_filename, this) << "left-hand expression in binary operation "
                                                << "is of non-integral type" << std::endl;
               }
               if (!rhs_int) {
                  env.error()(g_filename, this) << "right-hand expression in binary operation "
                                                << "is of non-integral type" << std::endl;
               }
               type_ = lhs_->type();
            }
         }
         break;
         
      case Kind::BOP_COMMA:
         type_ = rhs()->type();
         break;
      }
   }

    IntegralType::IntKind IntegralType::min_type(intmax_t val) {
       if (val >= std::numeric_limits<char>::min() &&
           val <= std::numeric_limits<unsigned char>::max()) {
          return IntKind::SPEC_CHAR;
       } else {
          return IntKind::SPEC_LONG_LONG;
       }
    }
    
   void LiteralExpr::TypeCheck(SemantEnv& env) {
      type_ = IntegralType::Create(val(), loc());
   }

   void StringExpr::TypeCheck(SemantEnv& env) {
      type_ = PointerType::Create(1, char_type, loc());
   }

   void IdentifierExpr::TypeCheck(SemantEnv& env) {
      const Declaration *decl;
      if ((decl = env.symtab().Lookup(id()->id())) == nullptr) {
         env.error()(g_filename, this) << "use of undeclared identifier '" << *id()->id()
                                       << "'" << std::endl;
         type_ = default_type;
      } else {
         auto var = dynamic_cast<const VarDeclaration *>(decl);
         if (var) {
            type_ = var->type();//->Decay();
            is_const_ = var->is_const();
         } else {
            env.error()(g_filename, this) << "identifier '" << *id()->id()
                                          << "' is incorrect kind of symbol"
                                          << std::endl;
            type_ = default_type;
         }
         scope_id_ = env.symtab().Depth();
      }
   }

    void NoExpr::TypeCheck(SemantEnv& env) {
       /* set type */
       type_ = VoidType::Create(loc());
    }

    void VarDeclaration::TypeCheck(SemantEnv& env) {
       type()->TypeCheck(env, false);
    }    

    /*** EXPRESSION KIND (LVALUE or RVALUE) ***/
    ASTExpr::ExprKind AssignmentExpr::expr_kind() const {
       /* assignments always produce rvalues */
       return ExprKind::EXPR_RVALUE;
    }

    ASTExpr::ExprKind UnaryExpr::expr_kind() const {
       switch (kind_) {
       case Kind::UOP_ADDR:
          return ExprKind::EXPR_RVALUE;
       
       case Kind::UOP_DEREFERENCE:
          return ExprKind::EXPR_LVALUE;
       
       case Kind::UOP_POSITIVE:
       case Kind::UOP_NEGATIVE:
       case Kind::UOP_BITWISE_NOT:
       case Kind::UOP_LOGICAL_NOT:
          return ExprKind::EXPR_RVALUE;

       case Kind::UOP_INC_PRE:
       case Kind::UOP_DEC_PRE:
       case Kind::UOP_INC_POST:
       case Kind::UOP_DEC_POST:
          return ExprKind::EXPR_RVALUE;
       }
    }

    ASTExpr::ExprKind BinaryExpr::expr_kind() const {
       switch (kind_) {
       case Kind::BOP_LOGICAL_AND:
       case Kind::BOP_BITWISE_AND:
       case Kind::BOP_LOGICAL_OR:
       case Kind::BOP_BITWISE_OR:
       case Kind::BOP_BITWISE_XOR:
       case Kind::BOP_EQ:
       case Kind::BOP_NEQ:
       case Kind::BOP_LT:
       case Kind::BOP_LEQ:
       case Kind::BOP_GT:
       case Kind::BOP_GEQ:
       case Kind::BOP_PLUS:
       case Kind::BOP_MINUS:
       case Kind::BOP_TIMES:
       case Kind::BOP_DIVIDE:
       case Kind::BOP_MOD:
          return ExprKind::EXPR_RVALUE;

       case Kind::BOP_COMMA:
          return ExprKind::EXPR_LVALUE;
       }
    }

    ASTExpr::ExprKind LiteralExpr::expr_kind() const {
       return ExprKind::EXPR_RVALUE;
    }

    ASTExpr::ExprKind StringExpr::expr_kind() const {
       return ExprKind::EXPR_RVALUE;
    }

    ASTExpr::ExprKind IdentifierExpr::expr_kind() const {
       if (is_const()) {
          return ExprKind::EXPR_RVALUE;
       } else if (type()->kind() == ASTType::Kind::TYPE_ARRAY) {
          return ExprKind::EXPR_RVALUE;
       } else {
          return ExprKind::EXPR_LVALUE;
       }
    }

    /*** TYPE EQUALITY ***/

    /*** TYPE COERCION ***/
    
   /*** JOIN POINTERS ***/

   void PointerDeclarator::JoinPointers() {
      if (declarator()->kind() == Kind::DECLARATOR_POINTER) {
         /* merge consecutive pointers */
         PointerDeclarator *other_ptr = dynamic_cast<PointerDeclarator *>(declarator());
         depth_ += other_ptr->depth_;
         declarator_ = other_ptr->declarator_;
      }

      declarator()->JoinPointers();
   }

   void FunctionDeclarator::JoinPointers() {
      declarator()->JoinPointers();
   }

    ASTType *ExternalDecl::Type() const {
       return decl()->type();
    }
    
   ASTType *BasicDeclarator::Type(ASTType *type) const {
      return type;
   }

   /* This is so confusing. */
   ASTType *PointerDeclarator::Type(ASTType *type) const {
      switch (declarator()->kind()) {
      case Kind::DECLARATOR_BASIC:
         /* basic pointer */
         return PointerType::Create(depth(), declarator()->Type(type), loc());
         
      case Kind::DECLARATOR_POINTER:
         /* this shouldn't happen */
         abort();
         
      case Kind::DECLARATOR_FUNCTION:
         /* tricky: pointer is actualy part of the return value type */
         return declarator()->Type(PointerType::Create(depth(), type, loc()));

      case Kind::DECLARATOR_ARRAY:
         /* pointer is part of the element type. */
         return declarator()->Type(PointerType::Create(depth(), type, loc()));
      }
   }

   ASTType *FunctionDeclarator::Type(ASTType *type) const {
      /* NOTE: _type_ represents the (possibly partial) return value of this function. */
      VarDeclarations *param_types = params();

      /* WHOA -- if this works, it's beautiful. */
      switch (declarator()->kind()) {
      case Kind::DECLARATOR_BASIC:
         return FunctionType::Create(type, param_types, loc());
         
      case Kind::DECLARATOR_POINTER:
         /* Actually a function pointer -- but primarily a pointer. */
         return declarator()->Type(FunctionType::Create(type, param_types, loc()));
         
      case Kind::DECLARATOR_FUNCTION:
         /* This function is actually the RETURN TYPE of another function declared
          * _declarator()_. */
         return declarator()->Type(FunctionType::Create(type, param_types, loc()));

      case Kind::DECLARATOR_ARRAY:
         /* Who in their right mind would try to declare an array of functions?! */
         const char *errmsgs[2] = {"who in their right mind would try to declare" \
                                   " an array of functions?",
                                   "please consider, well, uh, NOT declaring an array of " \
                                   "functions?"};
         g_semant_error(g_filename, this) << errmsgs[(intptr_t) this % 7 % 2] << std::endl;
         return declarator()->Type(PointerType::Create(1, type, loc()));
      }
   }

    ASTType *ArrayDeclarator::Type(ASTType *type) const {
       /* NOTE: _type_ represents the (possibly partial) element type of this array. */
       switch (declarator()->kind()) {
       case Kind::DECLARATOR_BASIC:
          return ArrayType::Create(type, count_expr(), loc());
          
       case Kind::DECLARATOR_POINTER:
          /* Actually a pointer to an array. */
          return declarator()->Type(ArrayType::Create(type, count_expr(), loc()));

       case Kind::DECLARATOR_FUNCTION:
          /* Functions can't return arrays. */
          g_semant_error(g_filename, this) << "functions can't return arrays" << std::endl;
          return declarator()->Type(PointerType::Create(1, type, loc()));

       case Kind::DECLARATOR_ARRAY:
          /* Swap array order. */
          return declarator()->Type(ArrayType::Create(type, count_expr(), loc()));
       }
    }

    /*** ADDRESS OF TYPE ***/
    ASTType *PointerType::Address() {
       return PointerType::Create(depth() + 1, pointee(), loc());
    }
    
    ASTType *FunctionType::Address() {
       return this;
    }

    ASTType *TaggedType::Address() {
       return PointerType::Create(1, this, loc());
    }

    ASTType *ArrayType::Address() {
       return PointerType::Create(1, this, loc());
    }

    /*** DECAY TYPE ***/
    ASTType *ArrayType::Decay() {
       return PointerType::Create(1, get_containee(), loc());
    }
    
    /*** DEREFERENCE TYPE ***/
    ASTType *ASTType::Dereference(SemantEnv *env) {
       if (env) {
          env->error()(g_filename, this) << "attempt to dereference non-pointer type" << std::endl;
       } else {
          abort();
       }

       return nullptr;
    }

    
    ASTType *PointerType::Dereference(SemantEnv *env) {
       if (is_void()) {
          if (env) {
             env->error()(g_filename, this) << "cannot dereference void pointer" << std::endl;
          } else {
             abort();
          }
       }
       
       if (depth() == 1) {
          return pointee();
       } else {
          return PointerType::Create(depth() - 1, pointee(), loc());
       }
    }
    
   ASTType *FunctionType::Dereference(SemantEnv *env) {
      return this; /* function types are infinitely dereferencable */
   }

    ASTType *TaggedType::Dereference(SemantEnv *env) {
       if (env) {
          env->error()(g_filename, this)
             << "attempt to dereference a tagged type" << std::endl;
       }
       return nullptr;
    }

    /*** ENSCOPE ***/
    void TranslationUnit::Enscope(SemantEnv& env) const {
       env.EnterScope();
    }

    void TranslationUnit::Descope(SemantEnv& env) const {
       env.ExitScope();
     }


     void ExternalDecl::Enscope(SemantEnv& env) const {
        decl()->Declare(env);

        auto var_decl = dynamic_cast<VarDeclaration *>(decl());
        if (var_decl) {
           env.ext_env().Enter(var_decl->sym());
        }
     }

     void ExternalDecl::Descope(SemantEnv& env) const {
        env.ext_env().Exit(env.error());
     }

     void FunctionDef::Enscope(SemantEnv& env) const {
        /* enscope function symbol */
        ExternalDecl::Enscope(env);

        /* add new scope */
        env.EnterScope();

       /* enscope parameters */
       FunctionType *type;
       if ((type = dynamic_cast<FunctionType *>(Type())) == nullptr) {
          env.error()(g_filename, this) << "function definition missing parameters" << std::endl;
          return;
       }

       auto params = type->params();
       std::for_each(params->begin(), params->end(),
                     [&](auto param) {
                        param->Declare(env);
                     });
    }

    void FunctionDef::Descope(SemantEnv& env) const {
       ExternalDecl::Descope(env);
       env.ExitScope();
    }


}
