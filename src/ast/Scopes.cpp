#include <kyfoo/ast/Scopes.hpp>

#include <cassert>

#include <kyfoo/Diagnostics.hpp>

#include <kyfoo/ast/Declarations.hpp>
#include <kyfoo/ast/Expressions.hpp>
#include <kyfoo/ast/Module.hpp>
#include <kyfoo/ast/Semantics.hpp>

namespace kyfoo {
    namespace ast {

//
// DeclarationScope

DeclarationScope::DeclarationScope(Module* module)
    : myModule(module)
    , myParent(nullptr)
{
}

DeclarationScope::DeclarationScope(DeclarationScope* parent)
    : myModule(parent->module())
    , myParent(parent)
{
}

DeclarationScope::~DeclarationScope() = default;

void DeclarationScope::io(IStream& stream) const
{
    stream.next("declarations", myDeclarations);
}

void DeclarationScope::resolveImports(Diagnostics& dgn)
{
    for ( auto& e : myDeclarations ) {
        if ( auto d = e->as<ImportDeclaration>() ) {
            module()->import(dgn, d->identifier());
        }
    }
}

void DeclarationScope::resolveSymbols(Diagnostics& dgn)
{
    std::vector<ProcedureDeclaration*> procedures;
    ScopeResolver resolver(this);
    for ( auto const& d : myDeclarations ) {
        d->symbol().resolveSymbols(dgn, resolver);

        auto s = createSymbolSet(d->symbol().name());
        if ( auto other = s->find(d->symbol().parameters()) ) {
            auto& err = dgn.error(module(), d->identifier()) << "symbol is already defined";
            err.see(other);
            continue;
        }

        s->append(d->symbol().parameters(), *d);

        if ( auto p = d->as<ProcedureDeclaration>() )
            procedures.push_back(p);
    }

    for ( auto&& e : procedures )
        e->resolveSymbols(dgn);
}

void DeclarationScope::setDeclaration(Declaration* declaration)
{
    myDeclaration = declaration;
}

void DeclarationScope::append(std::unique_ptr<Declaration> declaration)
{
    myDeclarations.emplace_back(std::move(declaration));
    myDeclarations.back()->setScope(*this);
}

void DeclarationScope::import(Module& module)
{
    append(
        std::make_unique<ImportDeclaration>(
            Symbol(lexer::Token(lexer::TokenKind::Identifier, 0, 0, module.name()))));
}

SymbolSet* DeclarationScope::findSymbol(std::string const& name)
{
    auto l = lower_bound(begin(mySymbols), end(mySymbols), name);
    if ( l->name() == name )
        return &*l;

    return nullptr;
}

SymbolSet* DeclarationScope::createSymbolSet(std::string const& name)
{
    auto l = lower_bound(begin(mySymbols), end(mySymbols), name);
    if ( l != end(mySymbols) && l->name() == name )
        return &*l;

    l = mySymbols.insert(l, SymbolSet(name));
    return &*l;
}

Module* DeclarationScope::module()
{
    return myModule;
}

Declaration* DeclarationScope::declaration()
{
    return myDeclaration;
}

DeclarationScope* DeclarationScope::parent()
{
    return myParent;
}

Declaration const* DeclarationScope::find(std::string const& identifier) const
{
    for ( auto&& d : myDeclarations )
        if ( d->identifier().lexeme() == identifier )
            return d.get();

    return nullptr;
}

//
// TypeScope

TypeScope::TypeScope(DeclarationScope* parent,
                     TypeDeclaration& declaration)
    : DeclarationScope(parent)
    , myTypeDeclaration(&declaration)
{
}

TypeScope::~TypeScope() = default;

void TypeScope::io(IStream& stream) const
{
    DeclarationScope::io(stream);
}

void TypeScope::resolveSymbols(Diagnostics& dgn)
{
    DeclarationScope::resolveSymbols(dgn);
}

Declaration const* TypeScope::find(std::string const& identifier) const
{
    return DeclarationScope::find(identifier);
}

//
// ProcedureScope

ProcedureScope::ProcedureScope(DeclarationScope* parent,
                               ProcedureDeclaration& declaration)
    : DeclarationScope(parent)
    , myDeclaration(&declaration)
{
}

ProcedureScope::~ProcedureScope() = default;

void ProcedureScope::io(IStream& stream) const
{
    DeclarationScope::io(stream);
    stream.next("expressions", myExpressions);
}

void ProcedureScope::resolveSymbols(Diagnostics& dgn)
{
    DeclarationScope::resolveSymbols(dgn);

    ScopeResolver resolver(this);
    for ( auto&& expr : myExpressions )
        expr->resolveSymbols(dgn, resolver);
}

Declaration const* ProcedureScope::find(std::string const& identifier) const
{
    for ( auto&& p : myDeclaration->parameters() )
        if ( p->identifier().lexeme() == identifier )
            return p.get();

    return DeclarationScope::find(identifier);
}

void ProcedureScope::append(std::unique_ptr<Expression> expression)
{
    myExpressions.emplace_back(std::move(expression));
}

    } // namespace ast
} // namespace kyfoo
