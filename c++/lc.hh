#pragma once

#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <string>
#include <vector>
#include <mach-o/loader.h>

#include "util.hh"
#include "image.hh"
#include "parse.hh"
#include "build.hh"

namespace MachO {

   template <Bits bits>
   class LoadCommand {
   public:
      virtual uint32_t cmd() const = 0;
      virtual std::size_t size() const = 0;

      static LoadCommand<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      virtual void Parse1(const Image& img, ParseEnv<bits>& env) {}
      virtual void Parse2(ParseEnv<bits>& env) {}
      
      virtual void Build(BuildEnv<bits>& env) = 0;
      virtual ~LoadCommand() {}
      virtual void AssignID(BuildEnv<bits>& env) {} /*!< build pass 1 */
      virtual void Emit(Image& img, std::size_t offset) const = 0;

      virtual LoadCommand<opposite<bits>> *Transform(TransformEnv<bits>& env) const = 0;
      
   protected:
      LoadCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env) {}
      LoadCommand(const LoadCommand<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
   };

   template <Bits bits>
   class LinkeditCommand: public LoadCommand<bits> {
   public:
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) = 0;
      virtual void Build(BuildEnv<bits>& env) override {}
      virtual std::size_t content_size() const = 0;

   protected:
      LinkeditCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env) {}
      LinkeditCommand(const LinkeditCommand<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env): LoadCommand<bits>(other, env) {}
      template <Bits b> friend class LinkeditCommand;
   };

   template <Bits bits>
   class DylinkerCommand: public LoadCommand<bits> {
   public:
      dylinker_command dylinker;
      std::string name;

      virtual uint32_t cmd() const override { return dylinker.cmd; }      
      virtual std::size_t size() const override;

      static DylinkerCommand<bits> *Parse(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env) {
         return new DylinkerCommand(img, offset, env);
      }

      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      virtual DylinkerCommand<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DylinkerCommand<opposite<bits>>(*this, env);
      }

   private:
      DylinkerCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DylinkerCommand(const DylinkerCommand<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), dylinker(other.dylinker), name(other.name) {}

      template <Bits b> friend class DylinkerCommand;
   };

   template <Bits bits>
   class UUID: public LoadCommand<bits> {
   public:
      uuid_command uuid;

      virtual uint32_t cmd() const override { return uuid.cmd; }
      virtual std::size_t size() const override { return sizeof(uuid_command); }

      static UUID<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new UUID(img, offset, env);
      }
      
      virtual void Build(BuildEnv<bits>& env) override {
         uuid.cmdsize = size();
      }
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      virtual UUID<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new UUID<opposite<bits>>(*this, env);
      }

   private:
      UUID(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env), uuid(img.at<uuid_command>(offset)) {}
      UUID(const UUID<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), uuid(other.uuid) {}

      template <Bits b> friend class UUID;
   };

   class BuildToolVersion {
   public:
      build_tool_version tool;

      static std::size_t size() { return sizeof(build_tool_version); }
      void Emit(Image& img, std::size_t offset) const;
      
      BuildToolVersion(const Image& img, std::size_t offset):
         tool(img.at<build_tool_version>(offset)) {}
      BuildToolVersion(const build_tool_version& tool): tool(tool) {}
   };

   template <Bits bits>
   class BuildVersion: public LoadCommand<bits> {
   public:
      using Tools = std::vector<BuildToolVersion>;
      build_version_command build_version;
      Tools tools;

      virtual uint32_t cmd() const override { return build_version.cmd; }      
      virtual std::size_t size() const override;
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;

      static BuildVersion<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new BuildVersion(img, offset, env);
      }
      
      virtual BuildVersion<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new BuildVersion<opposite<bits>>(*this, env);
      }

   private:
      BuildVersion(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      BuildVersion(const BuildVersion<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), build_version(other.build_version), tools(other.tools) {}

      template <Bits b> friend class BuildVersion;
   };

   template <Bits bits>
   class SourceVersion: public LoadCommand<bits> {
   public:
      source_version_command source_version;

      virtual uint32_t cmd() const override { return source_version.cmd; }      
      virtual std::size_t size() const override { return sizeof(source_version); }
      virtual void Build(BuildEnv<bits>& env) override {
         source_version.cmdsize = size();
      }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static SourceVersion<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env)
      { return new SourceVersion(img, offset, env); }
      
      template <typename... Args>
      static SourceVersion<bits> *Parse(Args&&... args) {
         return new SourceVersion(args...);
      }
      
      virtual SourceVersion<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new SourceVersion<opposite<bits>>(*this, env);
      }

   private:
      SourceVersion(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env), source_version(img.at<source_version_command>(offset)) {}
      SourceVersion(const SourceVersion<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), source_version(other.source_version) {}

      template <Bits b> friend class SourceVersion;
   };

   template <Bits bits>
   class EntryPoint: public LoadCommand<bits> {
   public:
      entry_point_command entry_point;
      const Placeholder<bits> *entry = nullptr;

      virtual uint32_t cmd() const override { return entry_point.cmd; }            
      virtual std::size_t size() const override { return sizeof(entry_point); }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;

      static EntryPoint<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new EntryPoint(img, offset, env);
      }
      virtual void Parse1(const Image& img, ParseEnv<bits>& env) override;

      virtual EntryPoint<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new EntryPoint<opposite<bits>>(*this, env);
      }
      
   private:
      EntryPoint(const EntryPoint<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      EntryPoint(const Image& img, std::size_t offset, ParseEnv<bits>& env);

      template <Bits b> friend class EntryPoint;
   };

   template <Bits bits>
   class DylibCommand: public LoadCommand<bits> {
   public:
      dylib_command dylib_cmd;
      std::string name;
      unsigned id; /*!< dylib identifier assigned at build time */

      virtual uint32_t cmd() const override { return dylib_cmd.cmd; }            
      virtual std::size_t size() const override;
      virtual void Build(BuildEnv<bits>& env) override {
         dylib_cmd.cmdsize = size();
         dylib_cmd.dylib.name.offset = sizeof(dylib_cmd);
      }
      virtual void AssignID(BuildEnv<bits>& env) override {
         id = env.dylib_counter();
      }
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      template <typename... Args>
      static DylibCommand<bits> *Parse(Args&&... args) { return new DylibCommand(args...); }
      
      virtual DylibCommand<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DylibCommand<opposite<bits>>(*this, env);
      }

   private:
      DylibCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DylibCommand(const DylibCommand<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), dylib_cmd(other.dylib_cmd), name(other.name), id(0) {}
      template <Bits> friend class DylibCommand;
   };   
   
}
