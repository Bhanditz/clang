#pragma once

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/Builtins.h"
#include "eosio/utils.hpp"
#include "eosio/gen.hpp"
#include "eosio/whereami/whereami.hpp"

#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <set>
#include <map>
#include <chrono>
#include <ctime>

#include "abi.hpp"
#include <jsoncons/json.hpp>

#include "llvm/Support/CommandLine.h"

using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;
using namespace eosio;
using namespace eosio::cdt;
using jsoncons::json;
using jsoncons::ojson;

struct abigen_exception : public std::exception {
   virtual const char* what() const throw() {
      return "eosio.abigen fatal error";
   }
} abigen_ex;

static DeclarationMatcher function_decl_matcher = cxxMethodDecl().bind("eosio_abis");
static DeclarationMatcher record_decl_matcher = cxxRecordDecl().bind("eosio_abis");
static DeclarationMatcher typedef_decl_matcher = typedefDecl().bind("eosio_abis");
static auto               class_tmp_matcher    = classTemplateSpecializationDecl().bind("eosio_abis");

class abigen : public generation_utils {
   public:
   abigen() : generation_utils([&](){throw abigen_ex;}) {}
   void add_typedef( const clang::QualType& t ) {
      abi_typedef ret;
      ret.new_type_name = get_base_type_name( t );
      ret.type = get_type_alias( t );
      _abi.typedefs.insert(ret);
   }

   void add_action( const clang::CXXRecordDecl* decl ) {
      abi_action ret;
      auto action_name = decl->getEosioActionAttr()->getName();

      if (action_name.empty()) {
         try {
            validate_name( decl->getName().str(), error_handler );
         } catch (...) {
            std::cout << "Error, name <" <<decl->getName().str() << "> is an invalid EOSIO name.\n";
            throw;
         }
         ret.name = decl->getName().str();
      }
      else {
         try {
            validate_name( action_name.str(), error_handler );
         } catch (...) {
            std::cout << "Error, name <" << action_name.str() << "> is an invalid EOSIO name.\n";
            throw;
         }
         ret.name = action_name.str();
      }
      ret.type = decl->getName().str();
      _abi.actions.insert(ret);
   }

   void add_action( const clang::CXXMethodDecl* decl ) {
      abi_action ret;

      auto action_name = decl->getEosioActionAttr()->getName();
      if (action_name.empty()) {
         try {
            validate_name( decl->getNameAsString(), error_handler );
         } catch (...) {
            std::cout << "Error, name <" <<decl->getNameAsString() << "> is an invalid EOSIO name.\n";
         }
         ret.name = decl->getNameAsString();
      }
      else {
         try {
            validate_name( action_name.str(), error_handler );
         } catch (...) {
            std::cout << "Error, name <" << action_name.str() << "> is an invalid EOSIO name.\n";
         }
         ret.name = action_name.str();
      }
      ret.type = decl->getNameAsString();
      _abi.actions.insert(ret);
   }

   void add_struct( const clang::CXXRecordDecl* decl ) {
      abi_struct ret;
      if ( decl->getNumBases() > 1 ) {
         std::cerr << "Error: abigen can only handle single inheritance <class " << decl->getName().str() << ">\n";
         throw abigen_ex;
      }
      else if ( decl->getNumBases() == 1 ) {
         ret.base = get_type(decl->bases_begin()->getType());
         add_struct(decl->bases_begin()->getType().getTypePtr()->getAsCXXRecordDecl());
      }

      for ( auto field : decl->fields() ) {
         if ( field->getName() == "transaction_extensions") {
            abi_struct ext;
            ext.name = "extension";
            ext.fields.push_back( {"type", "uint16"} );
            ext.fields.push_back( {"data", "bytes"} );
            ret.fields.push_back( {"transaction_extensions", "extension[]"});
            _abi.structs.insert(ext);
         }
         else {
            ret.fields.push_back({field->getName().str(), get_type(field->getType())});
            if ( is_template_specialization(field->getType(), {"vector", "optional"})) {
               if ( get_template_argument(field->getType())->isRecordType() )
                     add_struct(get_template_argument(field->getType()).getTypePtr()->getAsCXXRecordDecl());
            }
         }
      }

      ret.name = decl->getName().str();
      _abi.structs.insert(ret);
   }

   void add_struct( const clang::CXXMethodDecl* decl ) {
      abi_struct new_struct;
      new_struct.name = decl->getNameAsString();
      for (auto param : decl->parameters() ) {
         auto param_type = param->getType().getNonReferenceType().getUnqualifiedType();
         if (is_ignorable(param->getType()))
            param_type = get_ignored_type( param->getType() );

         if (!is_builtin_type( param_type)) {
            if (is_aliasing(param_type))
               add_typedef(param_type);
            if (param_type.getTypePtr()->isRecordType())
               add_struct( param_type.getTypePtr()->getAsCXXRecordDecl() );
            else if (is_template_specialization(param_type, {"vector", "optional"})) {
               if ( get_template_argument(param_type)->isRecordType())
                  add_struct(get_template_argument(param_type).getTypePtr()->getAsCXXRecordDecl());
            }
         }
         new_struct.fields.push_back({param->getNameAsString(), get_type(param_type)});
      }
      _abi.structs.insert(new_struct);
   }

   std::string to_index_type( std::string t ) {
      return "i64";
   }

   void add_table( const clang::CXXRecordDecl* decl ) {
      tables.insert(decl);
      abi_table t;
      t.type = decl->getNameAsString();
      auto table_name = decl->getEosioTableAttr()->getName();
      if (!table_name.empty()) {
         try {
            validate_name( table_name.str(), error_handler );
         } catch (...) {
         }
         t.name = table_name.str();
      }
      else {
         t.name = t.type;
      }
      ctables.insert(t);
   }

   void add_table( uint64_t name, const clang::CXXRecordDecl* decl ) {
      if (!(decl->isEosioTable() && abigen::is_eosio_contract(decl, get_contract_name())))
         return;
      abi_table t;
      t.type = decl->getNameAsString();
      t.name = name_to_string(name);
      _abi.tables.insert(t);
   }

   std::string generate_json_comment() {
      std::stringstream ss;
      ss << "This file was generated with eosio-abigen.";
      ss << " DO NOT EDIT ";
      auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      ss << std::ctime(&t);
      auto output = ss.str();
      return output.substr(0, output.length()-1); // remove the newline character
   }

   ojson struct_to_json( const abi_struct& s ) {
      ojson o;
      o["name"] = s.name;
      o["base"] = s.base;
      o["fields"] = ojson::array();
      for ( auto field : s.fields ) {
         ojson f;
         f["name"] = field.name;
         f["type"] = field.type;
         o["fields"].push_back(f);
      }
      return o;
   }

   ojson typedef_to_json( const abi_typedef& t ) {
      ojson o;
      o["new_type_name"] = t.new_type_name;
      o["type"]          = t.type;
      return o;
   }

   ojson action_to_json( const abi_action& a ) {
      ojson o;
      o["name"] = a.name;
      o["type"] = a.type;
      o["ricardian_contract"] = a.ricardian_contract;
      return o;
   }



   ojson table_to_json( const abi_table& t ) {
      ojson o;
      o["name"] = t.name;
      o["type"] = t.type;
      o["index_type"] = "i64";
      o["key_names"] = ojson::array();
      o["key_types"] = ojson::array();
      return o;
   }

   ojson to_json() {
      ojson o;
      o["____comment"] = generate_json_comment();
      o["version"]     = _abi.version;
      o["structs"]     = ojson::array();
      auto remove_suffix = [&]( std::string name ) {
         int i = name.length()-1;
         for (; i >= 0; i--) 
            if ( name[i] != '[' && name[i] != ']' && name[i] != '?' )
               break;
         return name.substr(0,i+1);
      };

      std::set<abi_table> set_of_tables;
      for ( auto t : ctables ) {
         bool has_multi_index = false;
         for ( auto u : _abi.tables ) {
            if (t.type == u.type) {
               has_multi_index = true;
               break;
            }
            set_of_tables.insert(u);
         }
         if (!has_multi_index)
            set_of_tables.insert(t);
      }
      for ( auto t : _abi.tables ) {
         set_of_tables.insert(t);
      }

      auto validate_struct = [&]( abi_struct as ) {
         if ( is_builtin_type(as.name) )
            return false;
         for ( auto s : _abi.structs ) {
            for ( auto f : s.fields ) {
               if (as.name == remove_suffix(f.type))
                  return true;
            }
            if ( s.base == as.name )
               return true;
         }
         for ( auto a : _abi.actions ) {
            if ( as.name == a.type )
               return true;
         }
         for( auto t : set_of_tables ) {
            if (as.name == t.type)
               return true;
         }
         return false;
      };

      for ( auto s : _abi.structs ) {
         if (validate_struct(s))
            o["structs"].push_back(struct_to_json(s));
      }
      o["types"]       = ojson::array();
      for ( auto t : _abi.typedefs ) {
         o["types"].push_back(typedef_to_json( t ));
      }
      o["actions"]     = ojson::array();
      for ( auto a : _abi.actions ) {
         o["actions"].push_back(action_to_json( a ));
      }
      o["tables"]     = ojson::array();
      for ( auto t : set_of_tables ) {
         o["tables"].push_back(table_to_json( t ));
      }

      o["ricardian_clauses"]  = ojson::array();
      o["abi_extensions"]     = ojson::array();
      return o;
   }
   
   int run( int argc, char** argv ) {
   }

   abi& get_abi_ref() { return _abi; }

   void set_contract_name( const std::string& cn ) { contract_name = cn; }
   std::string get_contract_name()const { return contract_name; }

   private: 
      abi _abi;
      std::set<const clang::CXXRecordDecl*> tables;
      std::set<abi_table>                   ctables;
      std::string contract_name;
};

abigen& get_abigen_ref() {
   static abigen ag;
   return ag;
}

class EosioMethodMatcher : public MatchFinder::MatchCallback {
   public:
      virtual void run( const MatchFinder::MatchResult& res ) {
         if (const clang::CXXMethodDecl* decl = res.Nodes.getNodeAs<clang::CXXMethodDecl>("eosio_abis")->getCanonicalDecl()) {
            abi abi;
            if (decl->isEosioAction() && abigen::is_eosio_contract(decl, get_abigen_ref().get_contract_name())) {
               get_abigen_ref().add_struct(decl);
               get_abigen_ref().add_action(decl);
               auto params = decl->parameters();
               for (auto param : params) {
                  abi_struct abis;
                  if (abigen::is_ignorable(param->getType()))
                     continue;

                  if (param->getType().getTypePtr()->isRecordType() && !get_abigen_ref().is_builtin_type(param->getType())) {
                     get_abigen_ref().add_struct(param->getType().getTypePtr()->getAsCXXRecordDecl());
                  }
                  if ( get_abigen_ref().is_template_specialization(param->getType(), {"vector", "optional"})) {
                     if ( get_abigen_ref().get_template_argument(param->getType())->isRecordType() )
                        get_abigen_ref().add_struct(get_abigen_ref().get_template_argument(param->getType()).getTypePtr()->getAsCXXRecordDecl());
                  }

               }
            }
         }
      }
   
};

class EosioRecordMatcher : public MatchFinder::MatchCallback {
   public:
      virtual void run( const MatchFinder::MatchResult& res ) {
         if (const clang::CXXRecordDecl* decl = res.Nodes.getNodeAs<clang::CXXRecordDecl>("eosio_abis")) {
            if (decl->isEosioAction() && abigen::is_eosio_contract(decl, get_abigen_ref().get_contract_name())) {
               get_abigen_ref().add_struct(decl);
               get_abigen_ref().add_action(decl);
               for (auto field : decl->fields()) {
                  if (!get_abigen_ref().is_builtin_type( field->getType() )) {
                     if (get_abigen_ref().is_aliasing(field->getType()))
                        get_abigen_ref().add_typedef(field->getType());
                     if (field->getType()->isRecordType())
                        get_abigen_ref().add_struct(field->getType()->getAsCXXRecordDecl());
                     if ( get_abigen_ref().is_template_specialization(field->getType(), {"vector", "optional"})) {
                        if ( get_abigen_ref().get_template_argument(field->getType())->isRecordType() )
                           get_abigen_ref().add_struct(get_abigen_ref().get_template_argument(field->getType()).getTypePtr()->getAsCXXRecordDecl());
                     }
                  }
               }
            }
            if (decl->isEosioTable() && abigen::is_eosio_contract(decl, get_abigen_ref().get_contract_name())) {
               get_abigen_ref().add_struct(decl);
               get_abigen_ref().add_table(decl);
               for (auto field : decl->fields())
                  if (!get_abigen_ref().is_builtin_type( field->getType() )) {
                     if (get_abigen_ref().is_aliasing(field->getType()))
                        get_abigen_ref().add_typedef(field->getType());
                     if (field->getType()->isRecordType())
                        get_abigen_ref().add_struct(field->getType()->getAsCXXRecordDecl());
                     if ( get_abigen_ref().is_template_specialization(field->getType(), {"vector", "optional"})) {
                        if ( get_abigen_ref().get_template_argument(field->getType())->isRecordType() )
                           get_abigen_ref().add_struct(get_abigen_ref().get_template_argument(field->getType()).getTypePtr()->getAsCXXRecordDecl());
                     }
                  }
            }
         }
      
         if (const clang::ClassTemplateSpecializationDecl* decl = res.Nodes.getNodeAs<clang::ClassTemplateSpecializationDecl>("eosio_abis")) {
            if ( decl->getName() == "multi_index" ) {
               get_abigen_ref().add_table(decl->getTemplateArgs()[0].getAsIntegral().getExtValue(),
                                         (clang::CXXRecordDecl*)((clang::RecordType*)decl->getTemplateArgs()[1].getAsType().getTypePtr())->getDecl());
            }
         }
      }
};
