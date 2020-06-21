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
      virtual void Build(BuildEnv<bits>& env) {} // TODO
      virtual ~LoadCommand() {}
      
   protected:
      LoadCommand() {}

      virtual void Parse2(const Image& img, Archive<bits>&& archive) {}
   };


   template <Bits bits>
   class DylinkerCommand: public LoadCommand<bits> {
   public:
      dylinker_command dylinker;
      std::string name;

      virtual uint32_t cmd() const override { return dylinker.cmd; }      
      virtual std::size_t size() const override;

      template <typename... Args>
      static DylinkerCommand<bits> *Parse(Args&&... args) { return new DylinkerCommand(args...); }

      virtual void Build(BuildEnv<bits>& env) override;
      
   private:
      DylinkerCommand(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class UUID: public LoadCommand<bits> {
   public:
      uuid_command uuid;

      virtual uint32_t cmd() const override { return uuid.cmd; }
      virtual std::size_t size() const override { return sizeof(uuid_command); }

      template <typename... Args>
      static UUID<bits> *Parse(Args&&... args) { return new UUID(args...); }
      virtual void Build(BuildEnv<bits>& env) override {}
      
   private:
      UUID(const Image& img, std::size_t offset): uuid(img.at<uuid_command>(offset)) {}
   };

   template <Bits bits>
   class BuildVersion: public LoadCommand<bits> {
   public:
      build_version_command build_version;
      std::vector<BuildToolVersion<bits> *> tools;

      virtual uint32_t cmd() const override { return build_version.cmd; }      
      virtual std::size_t size() const override;
      virtual void Build(BuildEnv<bits>& env) override;

      template <typename... Args>
      static BuildVersion<bits> *Parse(Args&&... args) { return new BuildVersion(args...); }
      
   private:
      BuildVersion(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class BuildToolVersion {
   public:
      build_tool_version tool;

      static std::size_t size() { return sizeof(build_tool_version); }
      
      template <typename... Args>
      static BuildToolVersion<bits> *Parse(Args&&... args) { return new BuildToolVersion(args...); }
      
   private:
      BuildToolVersion(const Image& img, std::size_t offset):
         tool(img.at<build_tool_version>(offset)) {}
   };

   template <Bits bits>
   class SourceVersion: public LoadCommand<bits> {
   public:
      source_version_command source_version;

      virtual uint32_t cmd() const override { return source_version.cmd; }      
      virtual std::size_t size() const override { return sizeof(source_version); }
      virtual void Build(BuildEnv<bits>& env) override {}
      
      template <typename... Args>
      static SourceVersion<bits> *Parse(Args&&... args) {
         return new SourceVersion(args...);
      }
      
   private:
      SourceVersion(const Image& img, std::size_t offset):
         source_version(img.at<source_version_command>(offset)) {}
   };

   template <Bits bits>
   class EntryPoint: public LoadCommand<bits> {
   public:
      entry_point_command entry_point;
      SectionBlob<bits> *entry;

      virtual uint32_t cmd() const override { return entry_point.cmd; }            
      virtual std::size_t size() const override { return sizeof(entry_point); }
      virtual void Build(BuildEnv<bits>& env) override;
      
      template <typename... Args>
      static EntryPoint<bits> *Parse(Args&&... args) { return new EntryPoint(args...); }
      
   private:
      EntryPoint(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class DylibCommand: public LoadCommand<bits> {
   public:
      dylib_command dylib_cmd;
      std::string name;

      virtual uint32_t cmd() const override { return dylib_cmd.cmd; }            
      virtual std::size_t size() const override;

      template <typename... Args>
      static DylibCommand<bits> *Parse(Args&&... args) { return new DylibCommand(args...); }
      
   private:
      DylibCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };   
   
}
