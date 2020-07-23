/*
 Copyright (c) 1995,1996 The Regents of the University of California.
 All rights reserved.
 
 Permission to use, copy, modify, and distribute this software for any
 purpose, without fee, and without written agreement is hereby granted,
 provided that the above copyright notice and the following two
 paragraphs appear in all copies of this software.
 
 IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 
 Copyright 2017 Michael Linderman.
 Copyright 2018 Nicholas Mosier.
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#ifndef __SEMANT_HPP
#define __SEMANT_HPP

#include <unordered_set>

#include "ast-fwd.hpp"
#include "semant-fwd.hpp"

#include "symtab.hpp"
#include "scopedtab.hpp"
#include "env.hpp"

namespace zc {

   extern bool g_semant_debug;
   extern SemantEnv g_semant_env;

   void Semant(TranslationUnit *root);

   /**
    * Provide error tracking and structured printing of semantic errors
    */
   class SemantError {
   public:
      SemantError(std::ostream& os) : os_(os), errors_(0) {}
        
      std::size_t errors() const { return errors_; }
        
      std::ostream& operator()(const char *filename, const ASTNode *node);
      std::ostream& operator()(const char *filename, const SourceLoc& loc);      
      std::ostream& operator()() {
         errors_++;
         return os_;
      }
       
   private:
      std::ostream& os_;
      std::size_t errors_;
   };

   extern SemantError g_semant_error;

   
   class SemantExtEnv {
      struct LabelCompare {
         bool operator()(const Identifier *lhs, const Identifier *rhs) const;
      };
      typedef std::set<const Identifier *, LabelCompare> Labels;
   public:
      Symbol *sym() const { return sym_env_.sym(); }

      void Enter(Symbol *sym);
      void Exit(SemantError& err);
      
      void LabelRef(const Identifier *id);
      void LabelDef(SemantError& err, const Identifier *id);

   private:
      SymbolEnv sym_env_;
      Labels label_refs_;
      Labels label_defs_;
   };

   class SemantEnv: public Env<Declaration, TaggedType, ASTStat, SemantExtEnv> {
   public:
      SemantEnv(SemantError& error): Env<Declaration, TaggedType, ASTStat, SemantExtEnv>(),
                                     error_(error) {}
      SemantError& error() { return error_; }

   private:
      SemantError& error_;
   };

   IntegralType *combine_integral_type_specs(std::unordered_multiset<int>& type_specs,
                                             SemantError& error, const SourceLoc& loc);   
   
}

#endif
