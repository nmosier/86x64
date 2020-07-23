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

#pragma once

#include <stdexcept>
#include <list>
#include <unordered_map>

namespace zc {

  // TODO: Refactor ScopedTable to use UniquePtr and the eventual observer_ptr

  /**
   * Scoped table for use as a SymbolTable in AST analysis
   * @tparam Key
   * @tparam Value
   *
   * The table actually stores Value* types, so a table mapping Symbol* to Symbol* would
   * be declared as ScopedTable<Symbol*,Symbol>.
   */
  template<class Key, class Value>
  class ScopedTable {
    typedef std::unordered_map<Key, Value*> ScopeEntry;
    typedef std::list<ScopeEntry> ScopeStack;

   public:

     /**
     * Push new scope onto the stack
     */
     void EnterScope() { scopes_.emplace_front(); }

    /**
     * Pop current scope off the stack
     */
    void ExitScope() {
      if (!scopes_.empty())
        scopes_.pop_front();
    }

    /**
     * Emplace key and value in current scope on top of the stack
     *
     * Will create new scope if one does not currently exist. Will throw std::invalid_argument
     * if key already exists in scope.
     * @param key
     * @param value
     * @return value
     */
    Value* AddToScope(const Key& key, Value* value) {
       if (scopes_.empty()) {
         EnterScope();
      }
      auto r = scopes_.front().emplace(key, value);
      if (!r.second) {
        throw std::invalid_argument("key already exists in scope");
      }
      return r.first->second;
    }

    /**
     * Return value associated key in nearest scope (examining all scopes)
     * @param key
     * @return value or nullptr if key is not present in any scope
     */
    Value* Lookup(const Key& key) const {
      for (auto & scope : scopes_) {
        auto found = scope.find(key);
        if (found != scope.end())
          return found->second;
      }
      return nullptr;
    }

    /**
     * Return value associated key in _current_ scope
     * @param key
     * @return value or nullptr if key is not present in any scope
     */
    Value* Probe(const Key& key) const {
      if (!scopes_.empty()) {
        const ScopeEntry& scope = scopes_.front();
        auto found = scope.find(key);
        if (found != scope.end())
          return found->second;
      }
      return nullptr;
    }

     std::size_t Depth() const { return scopes_.size(); }
     
   private:
    ScopeStack scopes_;
  };
}


