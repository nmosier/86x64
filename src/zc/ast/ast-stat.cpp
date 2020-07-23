#include "ast.hpp"

namespace zc {

   void WhileStat::DumpChildren(std::ostream& os, int level, bool with_types) const {
      pred()->Dump(os, level, with_types);
      body()->Dump(os, level, with_types);
   }

   void CompoundStat::DumpChildren(std::ostream& os, int level, bool with_types) const {
      for (auto decl : *decls()) {
         decl->Dump(os, level, with_types);
      }
      for (auto stat : *stats()) {
         stat->Dump(os, level, with_types);
      }
   }

   void GotoStat::DumpNode(std::ostream& os) const {
      os << "GotoStat '" << *label_id()->id() << "'";
   }

   void LabelDefStat::DumpNode(std::ostream& os) const {
      os << "LabelDefStat '" << *label_id()->id() << "'";
   }

   void LabeledStat::DumpChildren(std::ostream& os, int level, bool with_types) const {
      stat()->Dump(os, level, with_types);
   }

}
