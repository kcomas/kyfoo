#include <kyfoo/ast/Expressions.hpp>

#include <algorithm>
#include <iterator>

#include <kyfoo/Diagnostics.hpp>

#include <kyfoo/ast/Declarations.hpp>
#include <kyfoo/ast/Scopes.hpp>
#include <kyfoo/ast/Semantics.hpp>

namespace kyfoo {
    namespace ast {

template <typename T>
std::vector<std::unique_ptr<T>> clone(std::vector<std::unique_ptr<T>> const& rhs)
{
    std::vector<std::unique_ptr<T>> ret;
    ret.reserve(rhs.size());
    for ( auto const& e : rhs )
        ret.emplace_back(e->clone());

    return ret;
}

//
// Context

Context::Context(Diagnostics& dgn, IResolver& resolver)
    : myDiagnostics(&dgn)
    , myResolver(&resolver)
{
}

Context::~Context() = default;

Error& Context::error(lexer::Token const& token)
{
    return myDiagnostics->error(myResolver->module(), token);
}

Error& Context::error(Expression const& expr)
{
    return myDiagnostics->error(myResolver->module(), expr);
}

std::size_t Context::errorCount() const
{
    return myDiagnostics->errorCount();
}

LookupHit Context::matchEquivalent(SymbolReference const& sym) const
{
    return myResolver->matchEquivalent(sym);
}

LookupHit Context::matchValue(SymbolReference const& sym) const
{
    return myResolver->matchValue(sym);
}

LookupHit Context::matchProcedure(SymbolReference const& sym) const
{
    return myResolver->matchProcedure(sym);
}

void Context::rewrite(std::unique_ptr<Expression> expr)
{
    myRewrite = std::move(expr);
}

void Context::resolveExpression(std::unique_ptr<Expression>& expression)
{
    myRewrite.reset();
    expression->resolveSymbols(*this);
    while ( myRewrite ) {
        expression = std::move(myRewrite);
        expression->resolveSymbols(*this);
    }
}

void Context::resolveExpressions(std::vector<std::unique_ptr<Expression>>& expressions)
{
    myRewrite.reset();

    for ( auto i = begin(expressions); i != end(expressions); ++i ) {
        (*i)->resolveSymbols(*this);
        while ( myRewrite ) {
            *i = std::move(std::move(myRewrite));
            (*i)->resolveSymbols(*this);
        }
    }
}

//
// Expression

Expression::Expression(Kind kind)
    : myKind(kind)
{
}

Expression::Expression(Expression const& rhs)
    : myKind(rhs.myKind)
{
}

Expression::~Expression() = default;

void Expression::swap(Expression& rhs)
{
    using std::swap;
    swap(myKind, rhs.myKind);
}

Expression::Kind Expression::kind() const
{
    return myKind;
}

//
// PrimaryExpression

PrimaryExpression::PrimaryExpression(lexer::Token const& token)
    : base_t(Expression::Kind::Primary)
    , myToken(token)
{
}

PrimaryExpression::PrimaryExpression(PrimaryExpression const& rhs)
    : base_t(rhs)
    , myToken(rhs.myToken)
    , myDeclaration(rhs.myDeclaration)
{
}

PrimaryExpression& PrimaryExpression::operator = (PrimaryExpression const& rhs)
{
    PrimaryExpression(rhs).swap(*this);
    return *this;
}

PrimaryExpression::~PrimaryExpression() = default;

void PrimaryExpression::swap(PrimaryExpression& rhs)
{
    Expression::swap(rhs);
    
    using std::swap;
    swap(myToken, rhs.myToken);
}

void PrimaryExpression::io(IStream& stream) const
{
    stream.next("primary", myToken);
}

void PrimaryExpression::resolveSymbols(Context& ctx)
{
    if ( myToken.kind() == lexer::TokenKind::FreeVariable ) {
        if ( !myDeclaration )
            throw std::runtime_error("unbound free variable");

        return;
    }

    if ( myToken.kind() != lexer::TokenKind::Identifier )
        return;

    auto hit = ctx.matchEquivalent(Symbol(myToken));
    if ( !hit ) {
        if ( !hit.symSet() )
            ctx.error(myToken) << "undeclared identifier";
        return;
    }

    myDeclaration = hit.decl();
}

lexer::Token const& PrimaryExpression::token() const
{
    return myToken;
}

Declaration const* PrimaryExpression::declaration() const
{
    return myDeclaration;
}

void PrimaryExpression::setFreeVariable(Declaration const* decl)
{
    if ( myDeclaration )
        throw std::runtime_error("free variable can only be bound once");

    myDeclaration = decl;
}

//
// TupleExpression

TupleKind toTupleKind(lexer::TokenKind open, lexer::TokenKind close)
{
    if ( open == lexer::TokenKind::OpenParen ) {
        if ( close == lexer::TokenKind::CloseParen )
            return TupleKind::Open;
        else if ( close == lexer::TokenKind::CloseBracket )
            return TupleKind::OpenLeft;
    }
    else if ( open == lexer::TokenKind::OpenBracket ) {
        if ( close == lexer::TokenKind::CloseParen )
            return TupleKind::OpenRight;
        else if ( close == lexer::TokenKind::CloseBracket )
            return TupleKind::Closed;
    }

    throw std::runtime_error("invalid tuple expression syntax");
}

const char* to_string(TupleKind kind)
{
    switch (kind) {
#define X(a) case TupleKind::a: return #a;
        TUPLE_KINDS(X)
#undef X
    }

    throw std::runtime_error("invalid tuple kind");
}

const char* presentTupleOpen(TupleKind kind)
{
    switch (kind) {
    case TupleKind::Open:
    case TupleKind::OpenLeft:
        return "(";

    default:
        return "[";
    }
}

const char* presentTupleClose(TupleKind kind)
{
    switch (kind) {
    case TupleKind::Open:
    case TupleKind::OpenLeft:
        return ")";

    default:
        return "]";
    }
}
const char* presentTupleWeave(TupleKind)
{
    return ", ";
}

TupleExpression::TupleExpression(TupleKind kind,
                                 std::vector<std::unique_ptr<Expression>>&& expressions)
    : base_t(Expression::Kind::Tuple)
    , myKind(kind)
    , myExpressions(std::move(expressions))
{
}

TupleExpression::TupleExpression(lexer::Token const& open,
                                 lexer::Token const& close,
                                 std::vector<std::unique_ptr<Expression>>&& expressions)
    : base_t(Expression::Kind::Tuple)
    , myKind(toTupleKind(open.kind(), close.kind()))
    , myExpressions(std::move(expressions))
    , myOpenToken(open)
    , myCloseToken(close)
{
}

TupleExpression::TupleExpression(TupleExpression const& rhs)
    : base_t(rhs)
    , myKind(rhs.myKind)
    , myExpressions(ast::clone(rhs.myExpressions))
{
}

TupleExpression& TupleExpression::operator = (TupleExpression const& rhs)
{
    TupleExpression(rhs).swap(*this);
    return *this;
}

void TupleExpression::swap(TupleExpression& rhs)
{
    Expression::swap(rhs);

    using std::swap;
    swap(myKind, rhs.myKind);
    swap(myExpressions, rhs.myExpressions);
}

TupleExpression::~TupleExpression() = default;

void TupleExpression::io(IStream& stream) const
{
    stream.openArray(to_string(myKind));
    for ( auto const& e : myExpressions )
        e->io(stream);
    stream.closeArray();
}

void TupleExpression::resolveSymbols(Context& ctx)
{
    ctx.resolveExpressions(myExpressions);
}

TupleKind TupleExpression::kind() const
{
    return myKind;
}

lexer::Token const& TupleExpression::openToken() const
{
    return myOpenToken;
}

lexer::Token const& TupleExpression::closeToken() const
{
    return myCloseToken;
}

Slice<Expression*> TupleExpression::expressions() const
{
    return myExpressions;
}

Slice<Expression*> TupleExpression::expressions()
{
    return myExpressions;
}

/**
 * Flatten open-tuples
 */
void TupleExpression::flattenOpenTuples()
{
    for ( auto i = begin(myExpressions); i != end(myExpressions); ) {
        if ( (*i)->kind() == Expression::Kind::Tuple ) {
            auto tuple = static_cast<TupleExpression*>(i->get());
            if ( tuple->kind() == TupleKind::Open ) {
                move(begin(tuple->myExpressions), end(tuple->myExpressions),
                     std::inserter(myExpressions, i));
                goto L_removeItem;
            }
        }

        ++i;
        continue;

    L_removeItem:
        i = myExpressions.erase(i);
    }
}

//
// ApplyExpression

ApplyExpression::ApplyExpression(std::vector<std::unique_ptr<Expression>>&& expressions)
    : base_t(Expression::Kind::Apply)
    , myExpressions(std::move(expressions))
{
}

ApplyExpression::ApplyExpression(ApplyExpression const& rhs)
    : base_t(rhs)
    , myExpressions(ast::clone(rhs.myExpressions))
    , myDeclaration(rhs.myDeclaration)
{
}

ApplyExpression& ApplyExpression::operator = (ApplyExpression const& rhs)
{
    ApplyExpression(rhs).swap(*this);
    return *this;
}

ApplyExpression::~ApplyExpression() = default;

void ApplyExpression::swap(ApplyExpression& rhs)
{
    Expression::swap(rhs);

    using std::swap;
    swap(myExpressions, rhs.myExpressions);
    swap(myDeclaration, rhs.myDeclaration);
}

void ApplyExpression::io(IStream& stream) const
{
    stream.next("expressions", myExpressions);
}

void ApplyExpression::resolveSymbols(Context& ctx)
{
    ctx.resolveExpressions(myExpressions);

    if ( auto symExpr = myExpressions.front()->as<SymbolExpression>() ) {
        // explicit procedure lookup
        return;
    }

    // implicit procedure lookup
    auto subject = myExpressions.front()->as<PrimaryExpression>();
    if ( !subject || !isIdentifier(subject->token().kind()) ) {
        ctx.error(*this) << "implicit procedure application must begin with an identifier";
        return;
    }

    if ( myExpressions.size() == 1 ) {
        return ctx.rewrite(std::move(myExpressions.front()));
    }

    if ( subject->token().kind() == lexer::TokenKind::FreeVariable ) {
        // defer symbol lookup until concrete expression is instantiated
        return;
    }

    Slice<Expression*> args;
    if ( !myExpressions.empty() )
        args = slice(myExpressions, 1);

    SymbolReference sym(subject->token().lexeme(), args);

    // Look for hit on symbol
    auto symHit = ctx.matchValue(sym);
    if ( symHit ) {
        // Transmute apply-expression into symbol-expression
        auto id = subject->token();
        auto expr = std::move(myExpressions);
        rotate(begin(expr), next(begin(expr)), end(expr));
        expr.pop_back();
        return ctx.rewrite(std::make_unique<SymbolExpression>(id, std::move(expr)));
    }

    // Search procedure overloads by arguments
    auto procHit = ctx.matchProcedure(sym);
    auto procDecl = procHit.as<ProcedureDeclaration>();
    if ( !procDecl ) {
        auto& err = ctx.error(*this) << "does not match any symbol declarations or procedure overloads";
        // todo: references to potential overloads
        // todo: chained symbol sets
        // todo: lookup failures are not returning symbol sets
        if ( symHit.symSet() ) {
            for ( auto const& sd : symHit.symSet()->declarations() )
                err.see(sd.declaration);
        }
        return;
    }
}

/**
 * Flatten apply-expressions that don't start with a procedure identifier
 */
void ApplyExpression::flatten()
{
    for ( auto i = begin(myExpressions); i != end(myExpressions); ) {
        if ( auto apply = (*i)->as<ApplyExpression>() ) {
            if ( auto p = apply->expressions()[0]->as<PrimaryExpression>() ) {
                if ( auto d = p->declaration() )
                    if ( d->as<ProcedureDeclaration>() )
                        goto L_next;
            }

            move(begin(apply->myExpressions), end(apply->myExpressions),
                 std::inserter(myExpressions, i));
            goto L_removeItem;
        }

    L_next:
        ++i;
        continue;

    L_removeItem:
        i = myExpressions.erase(i);
    }
}

Slice<Expression*> ApplyExpression::expressions() const
{
    return myExpressions;
}

ProcedureDeclaration const* ApplyExpression::declaration() const
{
    return myDeclaration;
}

//
// SymbolExpression

SymbolExpression::SymbolExpression(lexer::Token const& identifier,
                                   std::vector<std::unique_ptr<Expression>>&& expressions)
    : base_t(Expression::Kind::Symbol)
    , myIdentifier(identifier)
    , myExpressions(std::move(expressions))
{
}

SymbolExpression::SymbolExpression(std::vector<std::unique_ptr<Expression>>&& expressions)
    : base_t(Expression::Kind::Symbol)
    , myExpressions(std::move(expressions))
{
}

SymbolExpression::SymbolExpression(lexer::Token const& open,
                                   lexer::Token const& close,
                                   std::vector<std::unique_ptr<Expression>>&& expressions)
    : base_t(Expression::Kind::Symbol)
    , myExpressions(std::move(expressions))
    , myOpenToken(open)
    , myCloseToken(close)
{
}

SymbolExpression::SymbolExpression(SymbolExpression const& rhs)
    : base_t(rhs)
    , myExpressions(ast::clone(rhs.myExpressions))
    , myDeclaration(rhs.myDeclaration)
{
}

SymbolExpression& SymbolExpression::operator = (SymbolExpression const& rhs)
{
    SymbolExpression(rhs).swap(*this);
    return *this;
}

SymbolExpression::~SymbolExpression() = default;

void SymbolExpression::swap(SymbolExpression& rhs)
{
    Expression::swap(rhs);

    using std::swap;
    swap(myExpressions, rhs.myExpressions);
    swap(myDeclaration, rhs.myDeclaration);
}

void SymbolExpression::io(IStream& stream) const
{
    stream.next("expressions", myExpressions);
}

void SymbolExpression::resolveSymbols(Context& ctx)
{
    if ( myIdentifier.kind() == lexer::TokenKind::Undefined ) {
        if ( myExpressions.empty() )
            return;

        auto subject = myExpressions.front()->as<PrimaryExpression>();
        if ( !subject ) {
            ctx.error(*this) << "symbol tuples must start with an identifier";
            return;
        }

        myIdentifier = subject->token();
        rotate(begin(myExpressions), next(begin(myExpressions)), end(myExpressions));
        myExpressions.pop_back();
    }

    ctx.resolveExpressions(myExpressions);

    {
        auto const startCount = ctx.errorCount();
        enforceResolution(ctx, *this);

        if ( ctx.errorCount() - startCount )
            return;
    }

    SymbolReference sym(myIdentifier.lexeme(), myExpressions);
    auto hit = ctx.matchValue(sym);
    if ( !hit ) {
        ctx.error(*this) << "undeclared symbol identifier";
        return;
    }

    myDeclaration = hit.decl();
}

lexer::Token const& SymbolExpression::identifier() const
{
    return myIdentifier;
}

Slice<Expression*> SymbolExpression::expressions()
{
    return myExpressions;
}

Slice<Expression*> SymbolExpression::expressions() const
{
    return myExpressions;
}

lexer::Token const& SymbolExpression::openToken() const
{
    return myOpenToken;
}

lexer::Token const& SymbolExpression::closeToken() const
{
    return myCloseToken;
}

std::vector<std::unique_ptr<Expression>>& SymbolExpression::internalExpressions()
{
    return myExpressions;
}

Declaration const* SymbolExpression::declaration() const
{
    return myDeclaration;
}

//
// ConstraintExpression

ConstraintExpression::ConstraintExpression(std::unique_ptr<Expression> subject,
                                           std::unique_ptr<Expression> constraint)
    : base_t(Expression::Kind::Constraint)
    , mySubject(std::move(subject))
    , myConstraint(std::move(constraint))
{
    if ( !mySubject )
        throw std::runtime_error("constraint expression must have a subject");

    if ( !myConstraint )
        throw std::runtime_error("constraint expression must have a constraint");
}

ConstraintExpression::ConstraintExpression(ConstraintExpression const& rhs)
    : base_t(rhs)
    , mySubject(rhs.mySubject->clone())
    , myConstraint(rhs.myConstraint->clone())
{
}

ConstraintExpression& ConstraintExpression::operator = (ConstraintExpression const& rhs)
{
    ConstraintExpression(rhs).swap(*this);
    return *this;
}

void ConstraintExpression::swap(ConstraintExpression& rhs)
{
    Expression::swap(rhs);

    using std::swap;
    swap(mySubject, rhs.mySubject);
    swap(myConstraint, rhs.myConstraint);
}

ConstraintExpression::~ConstraintExpression() = default;

void ConstraintExpression::io(IStream& stream) const
{
    stream.next("subject", mySubject);
    stream.next("constraint", myConstraint);
}

void ConstraintExpression::resolveSymbols(Context& ctx)
{
    ctx.resolveExpression(mySubject);
    ctx.resolveExpression(myConstraint);
}

Expression* ConstraintExpression::subject()
{
    return mySubject.get();
}

Expression const* ConstraintExpression::subject() const
{
    return mySubject.get();
}

Expression* ConstraintExpression::constraint()
{
    return myConstraint.get();
}

Expression const* ConstraintExpression::constraint() const
{
    return myConstraint.get();
}

//
// Utilities

template <typename Dispatcher>
struct FrontExpression
{
    using result_t = lexer::Token const&;

    Dispatcher& dispatch;

    FrontExpression(Dispatcher& dispatch)
        : dispatch(dispatch)
    {
    }

    result_t exprPrimary(PrimaryExpression const& p)
    {
        return p.token();
    }

    result_t exprTuple(TupleExpression const& t)
    {
        if ( t.expressions().empty() )
            return t.openToken();

        return dispatch(*t.expressions()[0]);
    }

    result_t exprApply(ApplyExpression const& a)
    {
        return dispatch(*a.expressions()[0]);
    }

    result_t exprSymbol(SymbolExpression const& s)
    {
        if ( s.identifier().kind() != lexer::TokenKind::Undefined )
            return s.identifier();

        if ( s.expressions().empty() )
            return s.openToken();

        return dispatch(*s.expressions()[0]);
    }

    result_t exprConstraint(ConstraintExpression const& c)
    {
        if ( c.subject() )
            return dispatch(*c.subject());
        else if ( c.constraint() )
            return dispatch(*c.constraint());

        throw std::runtime_error("expression has no traceable tokens");
    }
};

lexer::Token const& front(Expression const& expr)
{
    ShallowApply<FrontExpression> op;
    return op(expr);
}

template <typename Dispatcher>
struct PrintOperator
{
    using result_t = std::ostream&;

    Dispatcher& dispatch;
    result_t stream;

    PrintOperator(Dispatcher& dispatch, result_t stream)
        : dispatch(dispatch)
        , stream(stream)
    {
    }

    std::ostream& exprPrimary(PrimaryExpression const& p)
    {
        return stream << p.token().lexeme();
    }

    std::ostream& exprTuple(TupleExpression const& t)
    {
        stream << presentTupleOpen(t.kind());

        if ( !t.expressions().empty() ) {
            dispatch(*t.expressions()[0]);

            for ( auto const& e : t.expressions()(1, $) ) {
                stream << presentTupleWeave(t.kind());
                dispatch(*e);
            }
        }

        return stream << presentTupleClose(t.kind());
    }

    std::ostream& exprApply(ApplyExpression const& a)
    {
        auto first = true;
        for ( auto const& e : a.expressions() ) {
            if ( !first )
                stream << " ";
            else
                first = false;

            auto const group = e->kind() == Expression::Kind::Apply;
            if ( group )
                stream << "(";

            dispatch(*e);

            if ( group )
                stream << ")";
        }

        return stream;
    }

    std::ostream& exprSymbol(SymbolExpression const& s)
    {
        auto const& id = s.identifier().lexeme();
        if ( !id.empty() )
            stream << id;

        if ( !s.expressions().empty() ) {
            stream << '<';
            dispatch(*s.expressions()[0]);

            for ( auto const& e : s.expressions()(1, $) ) {
                stream << ", ";
                dispatch(*e);
            }

            return stream << '>';
        }

        if ( id.empty() )
            return stream << "<>";

        return stream;
    }
    
    std::ostream& exprConstraint(ConstraintExpression const& c)
    {
        dispatch(*c.subject());
        stream << " : ";
        return dispatch(*c.constraint());
    }
};

std::ostream& print(std::ostream& stream, Expression const& expr)
{
    ShallowApply<PrintOperator> op(stream);
    return op(expr);
}

template <typename Dispatcher>
struct EnforceResolution
{
    using result_t = void;

    Dispatcher& dispatch;
    Context& ctx;

    EnforceResolution(Dispatcher& dispatch, Context& ctx)
        : dispatch(dispatch)
        , ctx(ctx)
    {
    }

    void exprPrimary(PrimaryExpression const& p) const
    {
        if ( p.token().kind() == lexer::TokenKind::Identifier && !p.declaration() )
            ctx.error(p.token()) << "does not identify a declaration";
    }

    void exprTuple(TupleExpression const& t)
    {
        for ( auto const& e : t.expressions() )
            dispatch(*e);
    }

    void exprApply(ApplyExpression const& a)
    {
        for ( auto const& e : a.expressions() )
            dispatch(*e);
    }

    void exprSymbol(SymbolExpression const& s)
    {
        for ( auto const& e : s.expressions() )
            dispatch(*e);
    }

    void exprConstraint(ConstraintExpression const& c)
    {
        if ( c.subject() )
            dispatch(*c.subject());

        if ( c.constraint() )
            dispatch(*c.constraint());
    }
};

void enforceResolution(Context& ctx, Expression const& expr)
{
    ShallowApply<EnforceResolution> op(ctx);
    op(expr);
}

    } // namespace ast
} // namespace kyfoo
