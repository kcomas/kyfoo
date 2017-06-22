#pragma once

#include <memory>

#include <kyfoo/lexer/Token.hpp>

#include <kyfoo/ast/Node.hpp>
#include <kyfoo/ast/Symbol.hpp>
#include <kyfoo/ast/Expressions.hpp>

#include <kyfoo/codegen/Codegen.hpp>

namespace kyfoo {

    namespace lexer {
        class Scanner;
    }

    namespace ast {

#define DECLARATION_KINDS(X) \
    X(DataSum       , "data sum"       , DataSumDeclaration) \
    X(DataSumCtor   , "data sum ctor"  , DataSumDeclaration::Constructor) \
    X(DataProduct   , "data product"   , DataProductDeclaration) \
    X(Symbol        , "symbol"         , SymbolDeclaration) \
    X(Procedure     , "procedure"      , ProcedureDeclaration) \
    X(Variable      , "variable"       , VariableDeclaration) \
    X(Import        , "import"         , ImportDeclaration) \
    X(SymbolVariable, "symbol variable", SymbolVariable)

enum class DeclKind
{
#define X(a,b,c) a,
    DECLARATION_KINDS(X)
#undef X
};

const char* to_string(DeclKind kind);

class DeclarationScope;
class DataSumScope;
class DataProductScope;
class ProcedureScope;
class ValueExpression;

class Declaration : public INode
{
protected:
    Declaration(DeclKind kind,
                Symbol&& symbol,
                DeclarationScope* scope);

public:
    ~Declaration();

    // IIO
public:
    void io(IStream& stream) const override;

public:
    virtual void resolveSymbols(Diagnostics& dgn) = 0;

public:
    DeclKind kind() const;
    Symbol& symbol();
    Symbol const& symbol() const;
    lexer::Token const& identifier() const;

    template <typename T> T* as() = delete;
    template <typename T> T const* as() const = delete;

public:
    DeclarationScope* scope();
    void setScope(DeclarationScope& parent);

    codegen::CustomData* codegenData();
    codegen::CustomData* codegenData() const;
    void setCodegenData(std::unique_ptr<codegen::CustomData> data);
    void setCodegenData(std::unique_ptr<codegen::CustomData> data) const;

protected:
    DeclKind myKind;
    Symbol mySymbol;
    DeclarationScope* myScope = nullptr;
    mutable std::unique_ptr<codegen::CustomData> myCodeGenData;
};

class VariableDeclaration;

class DataSumDeclaration : public Declaration
{
public:
    class Constructor : public Declaration
    {
    public:
        Constructor(Symbol&& symbol,
                    std::vector<std::unique_ptr<VariableDeclaration>>&& parameters);
        ~Constructor();

        // IIO
    public:
        void io(IStream& stream) const override;

        // Declaration
    public:
        void resolveSymbols(Diagnostics& dgn) override;

    public:
        void setParent(DataSumDeclaration* parent);
        DataSumDeclaration* parent();
        DataSumDeclaration const* parent() const;

        Slice<VariableDeclaration*> fields() const;

    private:
        DataSumDeclaration* myParent = nullptr;
        std::vector<std::unique_ptr<VariableDeclaration>> myParameters;
    };

public:
    explicit DataSumDeclaration(Symbol&& symbol);
    ~DataSumDeclaration();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;

public:
    void define(std::unique_ptr<DataSumScope> scope);
    DataSumScope* definition();
    DataSumScope const* definition() const;

private:
    std::unique_ptr<DataSumScope> myDefinition;
};

class DataProductDeclaration : public Declaration
{
public:
    explicit DataProductDeclaration(Symbol&& symbol);
    ~DataProductDeclaration();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;

public:
    void define(std::unique_ptr<DataProductScope> scope);
    DataProductScope* definition();
    DataProductScope const* definition() const;

private:
    std::unique_ptr<DataProductScope> myDefinition;
};

class SymbolDeclaration : public Declaration
{
public:
    SymbolDeclaration(Symbol&& symbol,
                      std::unique_ptr<Expression> expression);
    ~SymbolDeclaration();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;

public:
    Expression* expression();
    Expression const* expression() const;

private:
    std::unique_ptr<Expression> myExpression;
};

class VariableDeclaration : public Declaration
{
public:
    VariableDeclaration(Symbol&& symbol,
                        std::unique_ptr<Expression> constraint,
                        std::unique_ptr<Expression> init);

    ~VariableDeclaration();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;

public:
    Expression* constraint();
    Expression const* constraint() const;

protected:
    std::unique_ptr<Expression> myConstraint;
    std::unique_ptr<Expression> myInitialization;
};

class ProcedureDeclaration;
class ProcedureParameter : public VariableDeclaration
{
public:
    ProcedureParameter(Symbol&& symbol,
                       std::unique_ptr<Expression> constraint);
    ~ProcedureParameter();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;

public:
    void setParent(ProcedureDeclaration* procDecl);
    ProcedureDeclaration* parent();

private:
    ProcedureDeclaration* myParent = nullptr;
};

class ProcedureDeclaration : public Declaration
{
public:
    ProcedureDeclaration(Symbol&& symbol,
                         std::vector<std::unique_ptr<ProcedureParameter>> parameters,
                         std::unique_ptr<ast::Expression> returnTypeExpression);
    ~ProcedureDeclaration();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;

public:
    void resolvePrototypeSymbols(Diagnostics& dgn);

    ProcedureScope* definition();
    ProcedureScope const* definition() const;
    void define(std::unique_ptr<ProcedureScope> definition);

    std::vector<std::unique_ptr<ProcedureParameter>>& parameters();
    Slice<ProcedureParameter*> parameters() const;
    Expression* returnType();
    Expression const* returnType() const;
    ProcedureParameter* result();
    ProcedureParameter const* result() const;

private:
    std::vector<std::unique_ptr<ProcedureParameter>> myParameters;
    std::unique_ptr<ProcedureParameter> myResult;
    std::unique_ptr<ProcedureScope> myDefinition;
};

class ImportDeclaration : public Declaration
{
public:
    explicit ImportDeclaration(Symbol&& symbol);
    ~ImportDeclaration();

    // IIO
public:
    void io(IStream& stream) const override;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn) override;
};

class SymbolVariable : public Declaration
{
public:
    SymbolVariable(Symbol& parent, std::string const& name);
    ~SymbolVariable();

    // IIO
public:
    void io(IStream& stream) const;

    // Declaration
public:
    void resolveSymbols(Diagnostics& dgn);

public:
    Symbol const& parent() const;
    std::string const& name() const;

private:
    Symbol* myParent = nullptr;
    std::string myName;
};

// sugar to avoid switching on DeclKind
#define X(a,b,c) template<> inline c* Declaration::as<c>() { return myKind == DeclKind::a ? static_cast<c*>(this) : nullptr; }
    DECLARATION_KINDS(X)
#undef X
#define X(a,b,c) template<> inline c const* Declaration::as<c>() const { return myKind == DeclKind::a ? static_cast<c const*>(this) : nullptr; }
    DECLARATION_KINDS(X)
#undef X

bool isDataDeclaration(DeclKind kind);

    } // namespace ast
} // namespace kyfoo
