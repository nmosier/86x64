#include <iterator>
#include <map>
#include <set>

#include "ast.hpp"
#include "util.hpp"

namespace zc {


   // template <> const char *ExternalDecls::name() const { return "ExternalDecls"; }      

   Symbol *ASTDeclarator::sym() const {
      return id() ? id()->id() : nullptr;
   }
   
   void ExternalDecl::DumpChildren(std::ostream& os, int level, bool with_types) const {
      decl()->Dump(os, level, with_types);
   }
   
   void FunctionDef::DumpNode(std::ostream& os) const { os << "FunctionDef"; }
   void FunctionDef::DumpChildren(std::ostream& os, int level, bool with_types) const {
      ExternalDecl::DumpChildren(os, level, with_types);
      comp_stat()->Dump(os, level, with_types);      
   }

   void PointerDeclarator::DumpNode(std::ostream& os) const {
      os << "PointerDeclarator " << depth_;
   }

   void Identifier::DumpNode(std::ostream& os) const {
      os << "Identifier " << "\"" << *id_ << "\"";
   }

   void Declaration::DumpChildren(std::ostream& os, int level, bool with_types) const {
      type()->Dump(os, level, with_types);
   }

   void VarDeclaration::DumpNode(std::ostream& os) const {
      os << "VarDeclaration";
   }

   void TypeDeclaration::DumpNode(std::ostream& os) const {
      os << "TypeDeclaration";
   }

   void BasicDeclarator::DumpChildren(std::ostream& os, int level, bool with_types) const {
      id_->Dump(os, level, with_types);
   }

   void FunctionDeclarator::DumpChildren(std::ostream& os, int level, bool with_types) const  {
      declarator_->Dump(os, level, with_types);
      for (auto param : *params_) {
         param->Dump(os, level, with_types);
      }
   }

   void ArrayDeclarator::DumpChildren(std::ostream& os, int level, bool with_types) const {
      declarator()->Dump(os, level, with_types);
      count_expr()->Dump(os, level, with_types);
   }

   void FunctionDeclarator::get_declarables(Declarations* output) const {
#if 0
      for (auto param : *params()) {
         param->get_declarables(output);
      }
#endif
   }

   TypeDeclaration::TypeDeclaration(DeclarableType *type): Declaration(type, type->loc()) {}

   Symbol *ExternalDecl::sym() const {
      const auto var = dynamic_cast<const VarDeclaration *>(decl());
      return var ? var->sym() : nullptr;
   }

   void BasicTypeSpec::AddTo(DeclSpecs *decl_specs) {
      decl_specs->basic_type_specs.push_back(this);
   }

   void ComplexTypeSpec::AddTo(DeclSpecs *decl_specs) {
      decl_specs->complex_type_specs.push_back(this);
   }

   ASTType *DeclSpecs::Type(SemantError& err) {
      /* check if any typenames are present */
      if (typename_specs.size() > 0) {
         if (typename_specs.size() > 1 || !complex_type_specs.empty() ||
             !basic_type_specs.empty()) {
            err(g_filename, this) << "invalid combination of type specifiers" << std::endl;
         }
         return NamedType::Create(typename_specs.front()->sym(), loc());
      }
      
      /* check if any complex types are present */
      if (complex_type_specs.size() > 0) {
         if (complex_type_specs.size() > 1 || basic_type_specs.size() > 0) {
            err(g_filename, this) << "incompatible type specifiers" << std::endl;
         }
         return complex_type_specs.front()->type();
      }

      /* otherwise, all basic. */
      using Kind = BasicTypeSpec::Kind;
      using IntKind = IntegralType::IntKind;      
      std::multiset<Kind> specs;
      std::transform(basic_type_specs.begin(), basic_type_specs.end(),
                     std::inserter(specs, specs.begin()),
                     [](auto spec) { return spec->kind(); });

      /* if `signed' or `unsigned' keywords are present, remove now and record sign */
      bool is_signed;
      if (specs.count(Kind::TS_CHAR) > 0) {
         is_signed = false; /* chars are unsigned by default */
      } else {
         is_signed = true; /* all other type specs are signed by default */
      }
      
      if (specs.count(Kind::TS_SIGNED) == 1) {
         specs.erase(Kind::TS_SIGNED);
         is_signed = true;
      } else if (specs.count(Kind::TS_UNSIGNED) == 1) {
         specs.erase(Kind::TS_UNSIGNED);
         is_signed = false;
      }
      
      std::map<std::multiset<Kind>,ASTType *> valid_combos
         {{{Kind::TS_VOID}, VoidType::Create(loc())},
          {{Kind::TS_CHAR}, IntegralType::Create(IntKind::SPEC_CHAR, is_signed, loc())},
          {{Kind::TS_SHORT}, IntegralType::Create(IntKind::SPEC_SHORT, is_signed, loc())},
          {{Kind::TS_INT}, IntegralType::Create(IntKind::SPEC_INT, is_signed, loc())},
          {{Kind::TS_LONG}, IntegralType::Create(IntKind::SPEC_LONG, is_signed, loc())}, 
          {{Kind::TS_SHORT, Kind::TS_INT}, IntegralType::Create(IntKind::SPEC_SHORT,
                                                                is_signed, loc())},
          {{Kind::TS_LONG, Kind::TS_INT}, IntegralType::Create(IntKind::SPEC_LONG,
                                                               is_signed, loc())},
          {{Kind::TS_LONG, Kind::TS_LONG}, IntegralType::Create(IntKind::SPEC_LONG_LONG,
                                                                is_signed, loc())},
          {{Kind::TS_LONG, Kind::TS_LONG, Kind::TS_INT},
           IntegralType::Create(IntKind::SPEC_LONG_LONG, is_signed, loc())},
          {{}, IntegralType::Create(IntKind::SPEC_INT, is_signed, loc())}
         };

      auto it = valid_combos.find(specs);
      if (it == valid_combos.end()) {
         err(g_filename, this) << "incompatible type specifiers" << std::endl;
         return default_type;
      } else {
         return it->second;
      }
   }

   void StorageClassSpec::AddTo(DeclSpecs *decl_specs) {
      decl_specs->storage_class_specs.push_back(this);
   }

   Symbol *TypenameSpec::sym() const { return id()->id(); }

   void TypenameSpec::AddTo(DeclSpecs *decl_specs) {
      decl_specs->typename_specs.push_back(this);
   }

   void Declaration::TypeResolve(SemantEnv& env) {
      type_ = type_->TypeResolve(env);
   }

   Declaration *DeclSpecs::GetDecl(SemantError& err, ASTDeclarator *declarator) {
      ASTType *type = zc::ASTType::Create(Type(err), declarator);
      auto sc_size = storage_class_specs.size();

      if (sc_size == 0) {
         return VarDeclaration::Create(declarator->sym(), false, type, loc());
      } else if (sc_size == 1) {
         StorageClassSpec *sc = storage_class_specs.front();
         if (sc->kind() == StorageClassSpec::Kind::SC_TYPEDEF) {
            return TypenameDeclaration::Create(declarator->sym(), type, loc());
         } else {
            throw std::logic_error("unimplemented storage class spec");
         }
      } else {
         err(g_filename, this) << "too many storage class specifiers" << std::endl;
         return VarDeclaration::Create(declarator->sym(), false, type, loc());
      }
   }

}
