#include <kyfoo/ast/Semantics.hpp>

#include <kyfoo/ast/Declarations.hpp>
#include <kyfoo/ast/Module.hpp>
#include <kyfoo/ast/Scopes.hpp>
#include <kyfoo/ast/Symbol.hpp>

namespace kyfoo {
    namespace ast {

//
// ScopeResolver

ScopeResolver::ScopeResolver(DeclarationScope* scope)
    : myScope(scope)
{
    if ( !scope )
        throw std::runtime_error("scope resolver scope cannot be null");
}

Module const* ScopeResolver::module() const
{
    return myScope->module();
}

Declaration const* ScopeResolver::inScope(std::string const& identifier) const
{
    auto decl = myScope->find(identifier);
    if ( decl )
        return decl;

    if ( myScope->declaration() )
        if ( auto symVar = myScope->declaration()->symbol().findVariable(identifier) )
            return symVar;

    for ( auto const& e : mySupplementarySymbols )
        if ( auto symVar = e->findVariable(identifier) )
            return symVar;

    return nullptr;
}

Declaration const* ScopeResolver::lookup(std::string const& identifier) const
{
    if ( auto d = inScope(identifier) )
        return d;

    for ( auto scope = myScope->parent(); scope; scope = scope->parent() ) {
        if ( auto d = scope->find(identifier) )
            return d;
        
        if ( auto decl = scope->declaration() )
            if ( auto s = decl->symbol().findVariable(identifier) )
                return s;
    }

    for ( auto m : module()->imports() )
        if ( auto decl = m->scope()->find(identifier) )
            return decl;

    return nullptr;
}

void ScopeResolver::addSupplementarySymbol(Symbol const& sym)
{
    mySupplementarySymbols.push_back(&sym);
}

//
// SymbolVariableCreatorFailoverResolver

SymbolVariableCreatorFailoverResolver::SymbolVariableCreatorFailoverResolver(IResolver& resolver, Symbol& symbol)
    : myResolver(&resolver)
    , mySymbol(&symbol)
{
}

SymbolVariableCreatorFailoverResolver::~SymbolVariableCreatorFailoverResolver() = default;

Module const* SymbolVariableCreatorFailoverResolver::module() const
{
    return myResolver->module();
}

Declaration const* SymbolVariableCreatorFailoverResolver::inScope(std::string const& identifier) const
{
    return myResolver->inScope(identifier);
}

Declaration const* SymbolVariableCreatorFailoverResolver::lookup(std::string const& identifier) const
{
    if ( auto decl = myResolver->lookup(identifier) )
        return decl;

    return mySymbol->createVariable(identifier);
}

//
// operators

struct SemanticOperator {
    void operator()(PrimaryExpression const& lhs, PrimaryExpression    const& rhs) = delete;
    void operator()(PrimaryExpression const& lhs, TupleExpression      const& rhs) = delete;
    void operator()(PrimaryExpression const& lhs, ConstraintExpression const& rhs) = delete;

    void operator()(TupleExpression const& lhs, PrimaryExpression    const& rhs) = delete;
    void operator()(TupleExpression const& lhs, TupleExpression      const& rhs) = delete;
    void operator()(TupleExpression const& lhs, ConstraintExpression const& rhs) = delete;

    void operator()(ConstraintExpression const& lhs, PrimaryExpression    const& rhs) = delete;
    void operator()(ConstraintExpression const& lhs, TupleExpression      const& rhs) = delete;
    void operator()(ConstraintExpression const& lhs, ConstraintExpression const& rhs) = delete;
};

template <typename O>
auto commute(O& o, Expression const& lhs, Expression const& rhs)
{
    if ( auto l = lhs.as<PrimaryExpression>() ) {
        if ( auto r = rhs.as<PrimaryExpression>() )    return o(*l, *r);
        if ( auto r = rhs.as<TupleExpression>() )      return o(*l, *r);
        if ( auto r = rhs.as<ConstraintExpression>() ) return o(*l, *r);

        goto L_error;
    }
    
    if ( auto l = lhs.as<TupleExpression>() ) {
        if ( auto r = rhs.as<TupleExpression>() )      return o(*l, *r);
        if ( auto r = rhs.as<ConstraintExpression>() ) return o(*l, *r);

        goto L_error;
    }
    
    if ( auto l = lhs.as<ConstraintExpression>() ) {
        if ( auto r = rhs.as<ConstraintExpression>() ) return o(*l, *r);
    }
    
L_error:
    throw std::runtime_error("invalid dispatch");
}

template <typename O>
auto noncommute(O& o, Expression const& lhs, Expression const& rhs)
{
    if ( auto l = lhs.as<PrimaryExpression>() ) {
        if ( auto r = rhs.as<PrimaryExpression>() )    return o(*l, *r);
        if ( auto r = rhs.as<TupleExpression>() )      return o(*l, *r);
        if ( auto r = rhs.as<ConstraintExpression>() ) return o(*l, *r);
        goto L_error;
    }
    if ( auto l = lhs.as<TupleExpression>() ) {
        if ( auto r = rhs.as<PrimaryExpression>() )    return o(*l, *r);
        if ( auto r = rhs.as<TupleExpression>() )      return o(*l, *r);
        if ( auto r = rhs.as<ConstraintExpression>() ) return o(*l, *r);
        goto L_error;
    }
    if ( auto l = lhs.as<ConstraintExpression>() ) {
        if ( auto r = rhs.as<PrimaryExpression>() )    return o(*l, *r);
        if ( auto r = rhs.as<TupleExpression>() )      return o(*l, *r);
        if ( auto r = rhs.as<ConstraintExpression>() ) return o(*l, *r);
    }

L_error:
    throw std::runtime_error("invalid dispatch");
}

bool matchOverload(Expression const& lhs, Expression const& rhs)
{
    if ( auto l = lhs.as<PrimaryExpression>() ) {
        if ( auto r = rhs.as<PrimaryExpression>() ) {
            if ( l->declaration()->kind() == DeclKind::SymbolVariable
                && r->declaration()->kind() == DeclKind::SymbolVariable )
            {
                return true;
            }

            return l->declaration() == r->declaration();
        }

        auto r = rhs.as<ConstraintExpression>();
        if ( !r )
            return false;

        return matchOverload(*l, *r->subject());
    }

    if ( auto l = lhs.as<TupleExpression>() ) {
        auto r = rhs.as<TupleExpression>();
        if ( !r )
            return false;

        auto const size = l->expressions().size();
        if ( size != r->expressions().size() )
            return false;

        for ( std::size_t i = 0; i < size; ++i ) {
            if ( !matchOverload(*l->expressions()[i], *r->expressions()[i]) )
                return false;
        }

        return true;
    }

    auto l = lhs.as<ConstraintExpression>();
    if ( !l )
        throw std::runtime_error("invalid overload matching");

    if ( auto r = rhs.as<ConstraintExpression>() )
        return matchOverload(*l->subject(), *r->subject());

    return matchOverload(*l->subject(), rhs);
}

bool matchPattern(Expression const& lhs, Expression const& rhs)
{
    if ( auto l = lhs.as<PrimaryExpression>() ) {
        if ( l->declaration()->kind() == DeclKind::SymbolVariable )
            return true;

        if ( auto r = rhs.as<PrimaryExpression>() )
            return l->token().lexeme() == r->token().lexeme();

        return false;
    }

    if ( auto l = lhs.as<TupleExpression>() ) {
        auto r = rhs.as<TupleExpression>();
        if ( !r || l->kind() != r->kind() )
            return false;

        auto const size = l->expressions().size();
        if ( size != r->expressions().size() )
            return false;

        for ( std::size_t i = 0; i < size; ++i ) {
            if ( !matchPattern(*l->expressions()[i], *r->expressions()[i]) )
                return false;
        }

        return true;
    }

    return false;
}

    } // namespace ast
} // namespace kyfoo
