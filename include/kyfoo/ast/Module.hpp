#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <kyfoo/Slice.hpp>
#include <kyfoo/ast/Node.hpp>

namespace kyfoo {

    class Diagnostics;

    namespace lexer {
        class Scanner;
    }

    namespace ast {

class Declaration;
class DeclarationScope;
class Module;
class AxiomsModule;

class ModuleSet
{
public:
    ModuleSet();
    ~ModuleSet();

public:
    Module* create(std::string const& name);
    Module* create(std::experimental::filesystem::path const& path);

    Module* createImplied(std::string const& name);

    Module* find(std::string const& name);
    Module* find(std::experimental::filesystem::path const& path);

    AxiomsModule* axioms();
    AxiomsModule const* axioms() const;

private:
    std::unique_ptr<AxiomsModule> createAxiomsModule();

private:
    std::unique_ptr<AxiomsModule> myAxioms;
    std::vector<std::unique_ptr<Module>> myModules;
    std::vector<Module*> myImpliedImports;
};

class Module : public INode
{
public:
    Module(ModuleSet* moduleSet,
           std::string const& name);
    Module(ModuleSet* moduleSet,
           std::experimental::filesystem::path const& path);
    ~Module();

    // IIO
public:
    void io(IStream& stream) const override;

public:
    std::string const& name() const;
    std::experimental::filesystem::path const& path() const;

public:
    void parse(Diagnostics& dgn);
    void parse(Diagnostics& dgn, std::istream& stream);

    void resolveImports(Diagnostics& dgn);
    void semantics(Diagnostics& dgn);

    Module const* import(Module* module);
    Module const* import(Diagnostics& dgn, lexer::Token const& token);

    void appendTemplateInstance(Declaration const* instance);

public:
    AxiomsModule* axioms();
    AxiomsModule const* axioms() const;

    Slice<Module*> imports();
    Slice<Module*> imports() const;
    DeclarationScope const* scope() const;

    bool imports(Module* module) const;
    bool parsed() const;

    Slice<Declaration const*> templateInstantiations() const;

private:
    ModuleSet* myModuleSet = nullptr;
    std::experimental::filesystem::path myPath;
    std::string myName;
    std::unique_ptr<DeclarationScope> myScope;
    std::vector<Declaration const*> myTemplateInstantiations;

    std::vector<Module*> myImports;
};

    } // namespace ast
} // namespace kyfoo
