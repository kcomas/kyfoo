#include <kyfoo/ast/Expressions.hpp>

#include <algorithm>
#include <iterator>

#include <kyfoo/Diagnostics.hpp>

#include <kyfoo/ast/Axioms.hpp>
#include <kyfoo/ast/Context.hpp>
#include <kyfoo/ast/Declarations.hpp>
#include <kyfoo/ast/Scopes.hpp>
#include <kyfoo/ast/Semantics.hpp>

namespace kyfoo {
    namespace ast {

//
// Expression

Expression::Expression(Kind kind)
    : myKind(kind)
{
}

Expression::Expression(Kind kind, Declaration const* decl)
    : myKind(kind)
    , myDeclaration(decl)
{
}

Expression::Expression(Expression const& rhs)
    : myKind(rhs.myKind)
    , myConstraints(ast::clone(rhs.myConstraints))
    , myDeclaration(rhs.myDeclaration)
{
}

Expression::~Expression() = default;

void Expression::swap(Expression& rhs)
{
    using std::swap;
    swap(myKind, rhs.myKind);
    swap(myConstraints, rhs.myConstraints);
    swap(myDeclaration, rhs.myDeclaration);
}

void Expression::cloneChildren(Expression& c, clone_map_t& map) const
{
    IMPL_CLONE_CHILD(myConstraints)
}

IMPL_CLONE_REMAP_NOBASE_BEGIN(Expression)
IMPL_CLONE_REMAP(myConstraints)
IMPL_CLONE_REMAP(myDeclaration)
IMPL_CLONE_REMAP_END

void Expression::addConstraint(std::unique_ptr<Expression> expr)
{
    myConstraints.emplace_back(std::move(expr));
}

Expression::Kind Expression::kind() const
{
    return myKind;
}

Declaration const* Expression::declaration() const
{
    return myDeclaration;
}

Slice<Expression*> Expression::constraints()
{
    return myConstraints;
}

const Slice<Expression*> Expression::constraints() const
{
    return myConstraints;
}

//
// PrimaryExpression

PrimaryExpression::PrimaryExpression(lexer::Token const& token)
    : Expression(Expression::Kind::Primary)
    , myToken(token)
{
}

PrimaryExpression::PrimaryExpression(PrimaryExpression const& rhs)
    : Expression(rhs)
    , myToken(rhs.myToken)
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

IMPL_CLONE_BEGIN(PrimaryExpression, Expression, Expression)
IMPL_CLONE_END
IMPL_CLONE_REMAP_BEGIN(PrimaryExpression, Expression)
IMPL_CLONE_REMAP_END

void PrimaryExpression::resolveSymbols(Context& ctx)
{
    if ( myToken.kind() == lexer::TokenKind::FreeVariable ) {
        if ( !myDeclaration ) {
            ctx.error(myToken) << "free variable not expected in this context";
            return;
        }

        return;
    }

    if ( myToken.kind() == lexer::TokenKind::Integer ) {
        myDeclaration = ctx.module()->axioms()->integerType();
        return;
    }

    if ( myToken.kind() != lexer::TokenKind::Identifier )
        return;

    auto hit = ctx.matchValue(Symbol(myToken));
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
    : Expression(Expression::Kind::Tuple)
    , myKind(kind)
    , myExpressions(std::move(expressions))
{
}

TupleExpression::TupleExpression(lexer::Token const& open,
                                 lexer::Token const& close,
                                 std::vector<std::unique_ptr<Expression>>&& expressions)
    : Expression(Expression::Kind::Tuple)
    , myKind(toTupleKind(open.kind(), close.kind()))
    , myExpressions(std::move(expressions))
    , myOpenToken(open)
    , myCloseToken(close)
{
}

TupleExpression::TupleExpression(TupleExpression const& rhs)
    : Expression(rhs)
    , myKind(rhs.myKind)
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

IMPL_CLONE_BEGIN(TupleExpression, Expression, Expression)
IMPL_CLONE_CHILD(myExpressions)
IMPL_CLONE_END
IMPL_CLONE_REMAP_BEGIN(TupleExpression, Expression)
IMPL_CLONE_REMAP(myExpressions)
IMPL_CLONE_REMAP_END

void TupleExpression::resolveSymbols(Context& ctx)
{
    ctx.resolveExpressions(myExpressions);

    if ( myKind == TupleKind::Open ) {
        if ( myExpressions.empty() ) {
            auto p = std::make_unique<PrimaryExpression>(myOpenToken);
            p->myDeclaration = ctx.module()->axioms()->emptyType();
            return ctx.rewrite(std::move(p));
        }
        else if ( myExpressions.size() == 1 ) {
            return ctx.rewrite(std::move(myExpressions[0]));
        }
    }
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
    : Expression(Expression::Kind::Apply)
    , myExpressions(std::move(expressions))
{
}

ApplyExpression::ApplyExpression(ApplyExpression const& rhs)
    : Expression(rhs)
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
}

void ApplyExpression::io(IStream& stream) const
{
    stream.next("expressions", myExpressions);
}

IMPL_CLONE_BEGIN(ApplyExpression, Expression, Expression)
IMPL_CLONE_CHILD(myExpressions)
IMPL_CLONE_END
IMPL_CLONE_REMAP_BEGIN(ApplyExpression, Expression)
IMPL_CLONE_REMAP(myExpressions)
IMPL_CLONE_REMAP_END

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
            for ( auto const& sd : symHit.symSet()->prototypes() )
                err.see(sd.declaration);
        }
        return;
    }

    myDeclaration = procDecl;
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
    return static_cast<ProcedureDeclaration const*>(myDeclaration);
}

//
// SymbolExpression

SymbolExpression::SymbolExpression(lexer::Token const& identifier,
                                   std::vector<std::unique_ptr<Expression>>&& expressions)
    : Expression(Expression::Kind::Symbol)
    , myIdentifier(identifier)
    , myExpressions(std::move(expressions))
{
}

SymbolExpression::SymbolExpression(std::vector<std::unique_ptr<Expression>>&& expressions)
    : Expression(Expression::Kind::Symbol)
    , myExpressions(std::move(expressions))
{
}

SymbolExpression::SymbolExpression(lexer::Token const& open,
                                   lexer::Token const& close,
                                   std::vector<std::unique_ptr<Expression>>&& expressions)
    : Expression(Expression::Kind::Symbol)
    , myExpressions(std::move(expressions))
    , myOpenToken(open)
    , myCloseToken(close)
{
}

SymbolExpression::SymbolExpression(SymbolExpression const& rhs)
    : Expression(rhs)
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
}

void SymbolExpression::io(IStream& stream) const
{
    stream.next("expressions", myExpressions);
}

IMPL_CLONE_BEGIN(SymbolExpression, Expression, Expression)
IMPL_CLONE_CHILD(myExpressions)
IMPL_CLONE_END
IMPL_CLONE_REMAP_BEGIN(SymbolExpression, Expression)
IMPL_CLONE_REMAP(myExpressions)
IMPL_CLONE_REMAP_END

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

            for ( auto const& e : t.expressions()(1, t.expressions().size()) ) {
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

            for ( auto const& e : s.expressions()(1, s.expressions().size()) ) {
                stream << ", ";
                dispatch(*e);
            }

            return stream << '>';
        }

        if ( id.empty() )
            return stream << "<>";

        return stream;
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
};

void enforceResolution(Context& ctx, Expression const& expr)
{
    ShallowApply<EnforceResolution> op(ctx);
    op(expr);
}

    } // namespace ast
} // namespace kyfoo
