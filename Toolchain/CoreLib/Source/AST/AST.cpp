/**
 *
 * Copyright (c)2022 The Adamnite C++ Authors.
 *
 * This code is open-sourced under the MIT license.
 */

#include <CoreLib/AST/AST.hpp>
#include <CoreLib/AST/ASTNode.hpp>
#include <CoreLib/Errors/ParsingError.hpp>
#include <CoreLib/Utils/Macros.hpp>

#include "ASTHelper.hpp"

#include <fmt/format.h>

#include <stdexcept>
#include <cstdint>

namespace A1::AST
{

namespace
{
    template< ReservedToken ... tokens >
    struct Either
    {
        [[ nodiscard ]]
        static bool match( TokenIterator const & tokenIt )
        {
            return tokenIt->is< ReservedToken >() && ( ( tokenIt->get< ReservedToken >() == tokens ) || ... );
        }

        [[ nodiscard ]]
        static std::string errorMessage() noexcept
        {
            return fmt::format
            (
                "Expecting one of: {}",
                (
                    ( std::string{ "'" } + std::string{ toStringView( tokens ) } + std::string{ "'" } ) + ...
                )
            );
        }
    };

    template< typename T >
        requires requires ( T )
        {
            T::match( std::declval< TokenIterator >() );
            T::errorMessage();
        }
    void skip( TokenIterator & tokenIt )
    {
        if ( T::match( tokenIt ) )
        {
            ++tokenIt;
        }
        else
        {
            throw ParsingError( tokenIt->errorInfo(), T::errorMessage() );
        }
    }

    template< typename T > requires requires ( T ) { T::toString(); }
    void skip( TokenIterator & tokenIt )
    {
        if ( tokenIt->is< T >() )
        {
            ++tokenIt;
        }
        else
        {
            throw ParsingError( tokenIt->errorInfo(), fmt::format( "Expecting {}", T::toString() ) );
        }
    }

    template< ReservedToken token >
    void skip( TokenIterator & tokenIt )
    {
        if ( tokenIt->is< ReservedToken >() && tokenIt->get< ReservedToken >() == token )
        {
            ++tokenIt;
        }
        else
        {
            throw ParsingError( tokenIt->errorInfo(), fmt::format( "Expecting '{}'", toStringView( token ) ) );
        }
    }

    [[ nodiscard ]] bool isEndOfExpression( Token const & token ) noexcept
    {
        if ( token.is< Indentation >() || token.is< Newline >() || token.is< Eof >() )
        {
            return true;
        }

        if ( token.is< ReservedToken >() )
        {
            auto const reservedToken{ token.get< ReservedToken >() };
            return reservedToken == ReservedToken::OpParenthesisClose ||
                   reservedToken == ReservedToken::OpSubscriptClose   ||
                   reservedToken == ReservedToken::OpColon            ||
                   reservedToken == ReservedToken::OpComma;
        }

        return false;
    }

    struct NodeInfo
    {
        NodeType     type         { NodeType::Unknown };
        std::size_t  operandsCount{ 0U };
        ErrorInfo    errorInfo;
    };

    [[ nodiscard ]] NodeInfo getNodeInfo
    (
        ReservedToken const token,
        ErrorInfo           errorInfo,
        bool          const isPrefix
    )
    {
#define MAP_TOKEN_TO_NODE( token, nodeType )                         \
    case ReservedToken::token:                                       \
        return                                                       \
        {                                                            \
            .type          = NodeType::nodeType,                     \
            .operandsCount = getOperandsCount( NodeType::nodeType ), \
            .errorInfo     = std::move( errorInfo )                  \
        }

        switch ( token )
        {
            MAP_TOKEN_TO_NODE( OpParenthesisOpen, Parentheses );
            MAP_TOKEN_TO_NODE( OpSubscriptOpen  , Index       );
            MAP_TOKEN_TO_NODE( OpDot            , MemberCall  );

            // Arithmetic operators
            MAP_TOKEN_TO_NODE( OpDiv     , Division       );
            MAP_TOKEN_TO_NODE( OpExp     , Exponent       );
            MAP_TOKEN_TO_NODE( OpFloorDiv, FloorDivision  );
            MAP_TOKEN_TO_NODE( OpMod     , Modulus        );
            MAP_TOKEN_TO_NODE( OpMul     , Multiplication );

            /**
             * Reserved tokens '+' and '-' can be used for two operations.
             * Thus, we need to make a check if reserved token is a prefix
             * or a suffix to the operand.
             */
            case ReservedToken::OpAdd:
                return
                {
                    .type          = isPrefix ? NodeType::UnaryPlus : NodeType::Addition,
                    .operandsCount = getOperandsCount( isPrefix ? NodeType::UnaryPlus : NodeType::Addition ),
                    .errorInfo     = std::move( errorInfo )
                };
            case ReservedToken::OpSub:
                return
                {
                    .type          = isPrefix ? NodeType::UnaryMinus : NodeType::Subtraction,
                    .operandsCount = getOperandsCount( isPrefix ? NodeType::UnaryMinus : NodeType::Subtraction ),
                    .errorInfo     = std::move( errorInfo )
                };

            // Bitwise operators
            MAP_TOKEN_TO_NODE( OpBitwiseAnd       , BitwiseAnd        );
            MAP_TOKEN_TO_NODE( OpBitwiseLeftShift , BitwiseLeftShift  );
            MAP_TOKEN_TO_NODE( OpBitwiseNot       , BitwiseNot        );
            MAP_TOKEN_TO_NODE( OpBitwiseOr        , BitwiseOr         );
            MAP_TOKEN_TO_NODE( OpBitwiseRightShift, BitwiseRightShift );
            MAP_TOKEN_TO_NODE( OpBitwiseXor       , BitwiseXor        );

            // Logical operators
            MAP_TOKEN_TO_NODE( OpLogicalAnd, LogicalAnd );
            MAP_TOKEN_TO_NODE( OpLogicalNot, LogicalNot );
            MAP_TOKEN_TO_NODE( OpLogicalOr , LogicalOr  );

            // Assignment operators
            MAP_TOKEN_TO_NODE( OpAssign        , Assign               );
            MAP_TOKEN_TO_NODE( OpAssignAdd     , AssignAddition       );
            MAP_TOKEN_TO_NODE( OpAssignDiv     , AssignDivision       );
            MAP_TOKEN_TO_NODE( OpAssignExp     , AssignExponent       );
            MAP_TOKEN_TO_NODE( OpAssignFloorDiv, AssignFloorDivision  );
            MAP_TOKEN_TO_NODE( OpAssignMod     , AssignModulus        );
            MAP_TOKEN_TO_NODE( OpAssignMul     , AssignMultiplication );
            MAP_TOKEN_TO_NODE( OpAssignSub     , AssignSubtraction    );

            // Bitwise assignment operators
            MAP_TOKEN_TO_NODE( OpAssignBitwiseAnd       , AssignBitwiseAnd        );
            MAP_TOKEN_TO_NODE( OpAssignBitwiseLeftShift , AssignBitwiseLeftShift  );
            MAP_TOKEN_TO_NODE( OpAssignBitwiseOr        , AssignBitwiseOr         );
            MAP_TOKEN_TO_NODE( OpAssignBitwiseRightShift, AssignBitwiseRightShift );
            MAP_TOKEN_TO_NODE( OpAssignBitwiseXor       , AssignBitwiseXor        );

            // Comparison operators
            MAP_TOKEN_TO_NODE( OpEqual           , Equality         );
            MAP_TOKEN_TO_NODE( OpGreaterThan     , GreaterThan      );
            MAP_TOKEN_TO_NODE( OpGreaterThanEqual, GreaterThanEqual );
            MAP_TOKEN_TO_NODE( OpLessThan        , LessThan         );
            MAP_TOKEN_TO_NODE( OpLessThanEqual   , LessThanEqual    );
            MAP_TOKEN_TO_NODE( OpNotEqual        , Inequality       );

            // Statements
            MAP_TOKEN_TO_NODE( KwIf    , StatementIf     );
            MAP_TOKEN_TO_NODE( KwElif  , StatementElif   );
            MAP_TOKEN_TO_NODE( KwElse  , StatementElse   );
            MAP_TOKEN_TO_NODE( KwWhile , StatementWhile  );
            MAP_TOKEN_TO_NODE( KwPass  , StatementPass   );
            MAP_TOKEN_TO_NODE( KwReturn, StatementReturn );
            MAP_TOKEN_TO_NODE( KwImport, StatementImport );
            MAP_TOKEN_TO_NODE( KwAssert, StatementAssert );

            // Definitions
            MAP_TOKEN_TO_NODE( KwDef     , FunctionDefinition );
            MAP_TOKEN_TO_NODE( KwLet     , VariableDefinition );
            MAP_TOKEN_TO_NODE( KwContract, ContractDefinition );

#define IGNORE_TOKEN( token ) case ReservedToken::token: break;

            IGNORE_TOKEN( Unknown            );
            IGNORE_TOKEN( OpParenthesisClose );
            IGNORE_TOKEN( OpSubscriptClose   );
            IGNORE_TOKEN( OpColon            );
            IGNORE_TOKEN( OpComma            );

            IGNORE_TOKEN( KwFalse );
            IGNORE_TOKEN( KwTrue  );

            IGNORE_TOKEN( KwAddress );

            IGNORE_TOKEN( KwBool );
            IGNORE_TOKEN( KwNum  );
            IGNORE_TOKEN( KwStr  );

            IGNORE_TOKEN( KwI8  );
            IGNORE_TOKEN( KwI16 );
            IGNORE_TOKEN( KwI32 );
            IGNORE_TOKEN( KwI64 );

            IGNORE_TOKEN( KwU8  );
            IGNORE_TOKEN( KwU16 );
            IGNORE_TOKEN( KwU32 );
            IGNORE_TOKEN( KwU64 );

#undef IGNORE_TOKEN

            default:
                throw ParsingError{ errorInfo, fmt::format( "Invalid token: {}", toStringView( token ) ) };
        }

        return {};

#undef MAP_TOKEN_TO_NODE
    }

    [[ nodiscard ]]
    Node::Pointer parseOperand( TokenIterator const & token )
    {
        if ( token->is< Number >() )
        {
            return std::make_unique< Node >( token->get< Number >(), token->errorInfo() );
        }
        else if ( token->is< String >() )
        {
            return std::make_unique< Node >( token->get< String >(), token->errorInfo() );
        }
        else if ( token->is< Identifier >() )
        {
            return std::make_unique< Node >( token->get< Identifier >(), token->errorInfo() );
        }
        else if ( token->is< ReservedToken >() )
        {
            switch ( token->get< ReservedToken >() )
            {
                case ReservedToken::KwFalse: return std::make_unique< Node >( false, token->errorInfo() );
                case ReservedToken::KwTrue : return std::make_unique< Node >( true , token->errorInfo() );

                case ReservedToken::KwAddress: return std::make_unique< Node >( Registry::getAddressHandle(), token->errorInfo() );

                case ReservedToken::KwBool: return std::make_unique< Node >( Registry::getBoolHandle(), token->errorInfo() );
                case ReservedToken::KwNum : return std::make_unique< Node >( Registry::getNumHandle (), token->errorInfo() );
                case ReservedToken::KwStr : return std::make_unique< Node >( Registry::getStrHandle (), token->errorInfo() );

                case ReservedToken::KwI8 : return std::make_unique< Node >( Registry::getI8Handle (), token->errorInfo() );
                case ReservedToken::KwI16: return std::make_unique< Node >( Registry::getI16Handle(), token->errorInfo() );
                case ReservedToken::KwI32: return std::make_unique< Node >( Registry::getI32Handle(), token->errorInfo() );
                case ReservedToken::KwI64: return std::make_unique< Node >( Registry::getI64Handle(), token->errorInfo() );

                case ReservedToken::KwU8 : return std::make_unique< Node >( Registry::getU8Handle (), token->errorInfo() );
                case ReservedToken::KwU16: return std::make_unique< Node >( Registry::getU16Handle(), token->errorInfo() );
                case ReservedToken::KwU32: return std::make_unique< Node >( Registry::getU32Handle(), token->errorInfo() );
                case ReservedToken::KwU64: return std::make_unique< Node >( Registry::getU64Handle(), token->errorInfo() );

                default:
                    break;
            }
        }

        throw ParsingError{ token->errorInfo(), "Unexpected operand" };
    }

    void popOperator
    (
        std::stack< Node::Pointer > & operands,
        std::stack< NodeInfo      > & operators,
        ErrorInfo                     errorInfo
    )
    {
        auto const & lastOperator{ operators.top() };

        if ( operands.size() < lastOperator.operandsCount )
        {
            throw ParsingError
            {
                errorInfo,
                fmt::format( "Expecting {} operands ({} given)", lastOperator.operandsCount, operands.size() )
            };
        }

        std::vector< Node::Pointer > lastOperatorOperands;
        lastOperatorOperands.resize( lastOperator.operandsCount );

        for ( auto i{ lastOperator.operandsCount }; i > 0U; --i )
        {
            lastOperatorOperands[ i - 1 ] = std::move( operands.top() );
            operands.pop();
        }

        operands.push
        (
            std::make_unique< Node >
            (
                lastOperator.type,
                std::move( lastOperatorOperands ),
                lastOperator.errorInfo
            )
        );

        operators.pop();
    }

    Node::Pointer parseImpl
    (
        TokenIterator       & token,
        std::size_t   const   indentationLevel = 0U,
        bool          const   alreadyInModule  = true
    );

    [[ nodiscard ]]
    std::size_t parseBody( TokenIterator & token, std::stack< Node::Pointer > & operands, std::size_t const indentationLevel )
    {
        std::size_t operandsCount{ 0U };

        while ( token->is_not< Eof >() )
        {
            // Parse body statement
            operands.push( parseImpl( token, indentationLevel ) );
            operandsCount++;

            auto newLineBeginToken{ token };

            auto currentIndentationLevel{ 0U };
            while ( currentIndentationLevel != indentationLevel )
            {
                if ( token->is< Indentation >() )
                {
                    ++token;
                    currentIndentationLevel++;
                }
                else if ( token->is< Newline >() )
                {
                    skip< Newline >( token );
                }
                else
                {
                    break;
                }
            }

            if ( currentIndentationLevel < indentationLevel )
            {
                // All body statements are parsed, get back to the beginning of the line
                token = std::move( newLineBeginToken );
                break;
            }
        }

        return operandsCount;
    }

    Node::Pointer parseImpl
    (
        TokenIterator       & token,
        std::size_t   const   indentationLevel,
        bool          const   alreadyInModule
    )
    {
        std::stack< Node::Pointer > operands;
        std::stack< NodeInfo      > operators;

        auto expectingOperand{ true };

        std::size_t currentIndentationLevel{ 0U };
        while ( currentIndentationLevel != indentationLevel )
        {
            if ( token->is< Indentation >() )
            {
                ++token;
                currentIndentationLevel++;
            }
            else
            {
                currentIndentationLevel = indentationLevel;
                break;
            }
        }

        if ( !alreadyInModule )
        {
            operators.push
            (
                NodeInfo
                {
                    .type      = NodeType::ModuleDefinition,
                    .errorInfo = token->errorInfo()
                }
            );
        }

        while ( token->is< Newline >() || token->is< Eof >() )
        {
            // skip empty lines or comment lines
            ++token;
            if ( token->is< Eof >() )
            {
                expectingOperand = false;
                break;
            }
        }

        for ( ; !isEndOfExpression( *token ); ++token )
        {
            if ( token->is< ReservedToken >() )
            {
                auto nodeInfo
                {
                    getNodeInfo
                    (
                        token->get< ReservedToken >(),
                        token->errorInfo(),
                        expectingOperand
                    )
                };

                if ( nodeInfo.type == NodeType::Unknown )
                {
                    if
                    (
                        auto const reservedToken{ token->get< ReservedToken >() };
                        reservedToken == ReservedToken::KwFalse ||
                        reservedToken == ReservedToken::KwTrue  ||
                        isTypeSpecifier( reservedToken )
                    )
                    {
                        if ( !expectingOperand )
                        {
                            throw ParsingError{ token->errorInfo(), "Unexpected operand" };
                        }

                        operands.push( parseOperand( token ) );
                        expectingOperand = false;

                        ++token;
                        break;
                    }

                    continue;
                }

                if ( !operators.empty() && operators.top().type != NodeType::ModuleDefinition && hasHigherPrecedence( operators.top().type, nodeInfo.type ) )
                {
                    popOperator( operands, operators, token->errorInfo() );

                    if ( !operators.empty() && operators.top().type == NodeType::ModuleDefinition )
                    {
                        operators.top().operandsCount--;
                    }
                }

                if ( nodeInfo.type == NodeType::Parentheses )
                {
                    skip< ReservedToken::OpParenthesisOpen >( token );

                    if ( token->is< ReservedToken >() && token->get< ReservedToken >() == ReservedToken::OpParenthesisClose )
                    {
                        throw ParsingError{ token->errorInfo(), "Expecting an expression inside parentheses" };
                    }

                    operands.push( parseImpl( token ) );
                    if ( !token->is< ReservedToken >() || token->get< ReservedToken >() != ReservedToken::OpParenthesisClose )
                    {
                        throw ParsingError{ token->errorInfo(), "Expecting closing parenthesis" };
                    }
                }
                else if ( nodeInfo.type == NodeType::Index )
                {
                    skip< ReservedToken::OpSubscriptOpen >( token );
                    operands.push( parseImpl( token ) );
                }
                else if ( nodeInfo.type == NodeType::StatementIf || nodeInfo.type == NodeType::StatementElif )
                {
                    skip< Either< ReservedToken::KwIf, ReservedToken::KwElif > >( token );
                    operands.push( parseImpl( token ) ); // parse condition
                    skip< ReservedToken::OpColon >( token );
                    skip< Newline >( token );

                    nodeInfo.operandsCount += parseBody( token, operands, currentIndentationLevel + 1U );

                    if ( token->is< Newline >() ) { ++token; }

                    if
                    (
                        token->is< ReservedToken >() &&
                        (
                            token->get< ReservedToken >() == ReservedToken::KwElif ||
                            token->get< ReservedToken >() == ReservedToken::KwElse
                        )
                    )
                    {
                        // Parse elif / else expression
                        operands.push( parseImpl( token, currentIndentationLevel ) );
                        nodeInfo.operandsCount++;
                    }
                }
                else if ( nodeInfo.type == NodeType::StatementElse )
                {
                    skip< ReservedToken::KwElse  >( token );
                    skip< ReservedToken::OpColon >( token );
                    skip< Newline >( token );

                    nodeInfo.operandsCount += parseBody( token, operands, currentIndentationLevel + 1U );
                }
                else if ( nodeInfo.type == NodeType::StatementWhile )
                {
                    skip< ReservedToken::KwWhile >( token );
                    operands.push( parseImpl( token ) ); // parse condition
                    skip< ReservedToken::OpColon >( token );
                    skip< Newline >( token );

                    nodeInfo.operandsCount += parseBody( token, operands, currentIndentationLevel + 1U );
                }
                else if ( nodeInfo.type == NodeType::FunctionDefinition )
                {
                    skip< ReservedToken::KwDef >( token );

                    operands.push( parseOperand( token ) ); // parse function name
                    token++;

                    {
                        // we are parsing function parameters

                        skip< ReservedToken::OpParenthesisOpen >( token );

                        if ( token->is< Identifier >() && token->get< Identifier >().name == "self" )
                        {
                            NodeInfo const parameterDefinition
                            {
                                .type          = NodeType::FunctionParameterDefinition,
                                .operandsCount = 1U,
                                .errorInfo     = token->errorInfo()
                            };

                            operands.push( parseImpl( token ) ); // parse self parameter name
                            operators.push( parameterDefinition );

                            while ( !operators.empty() && operators.top().type != NodeType::ModuleDefinition )
                            {
                                popOperator( operands, operators, token->errorInfo());
                            }

                            ++nodeInfo.operandsCount;

                            if ( token->is< ReservedToken >() )
                            {
                                auto const reservedToken{ token->get< ReservedToken >() };
                                if ( reservedToken == ReservedToken::OpComma )
                                {
                                    ++token;
                                }
                                else if ( reservedToken != ReservedToken::OpParenthesisClose )
                                {
                                    throw ParsingError{ token->errorInfo(), "Expecting closing parenthesis" };
                                }
                            }
                        }

                        if ( token->is_not< ReservedToken >() || token->get< ReservedToken >() != ReservedToken::OpParenthesisClose )
                        {
                            while ( true )
                            {
                                NodeInfo const parameterDefinition
                                {
                                    .type          = NodeType::FunctionParameterDefinition,
                                    .operandsCount = getOperandsCount( NodeType::FunctionParameterDefinition ),
                                    .errorInfo     = token->errorInfo()
                                };

                                operands.push( parseImpl( token ) ); // parse parameter name
                                skip< ReservedToken::OpColon >( token );
                                operands.push( parseImpl( token ) ); // parse parameter type

                                operators.push( parameterDefinition );

                                while ( !operators.empty() && operators.top().type != NodeType::ModuleDefinition )
                                {
                                    popOperator( operands, operators, token->errorInfo());
                                }

                                nodeInfo.operandsCount++;

                                if ( token->is< ReservedToken >() )
                                {
                                    auto const reservedToken{ token->get< ReservedToken >() };
                                    if ( reservedToken == ReservedToken::OpParenthesisClose )
                                    {
                                        break;
                                    }
                                    else if ( reservedToken == ReservedToken::OpComma )
                                    {
                                        ++token;
                                    }
                                    else
                                    {
                                        throw ParsingError{ token->errorInfo(), "Expecting closing parenthesis" };
                                    }
                                }
                                else
                                {
                                    throw ParsingError{ token->errorInfo(), "Expecting closing parenthesis" };
                                }
                            }
                        }
                        skip< ReservedToken::OpParenthesisClose >( token );
                    }

                    if ( token->is< ReservedToken >() && token->get< ReservedToken >() == ReservedToken::OpArrow )
                    {
                        ++token;
                        operands.push( parseImpl( token ) ); // parse type
                        nodeInfo.operandsCount++;
                    }

                    skip< ReservedToken::OpColon >( token );
                    skip< Newline >( token );

                    nodeInfo.operandsCount += parseBody( token, operands, currentIndentationLevel + 1U );
                }
                else if ( nodeInfo.type == NodeType::VariableDefinition )
                {
                    skip< ReservedToken::KwLet >( token );
                    operands.push( parseOperand( token ) ); // parse variable name
                    ++token;

                    if ( token->is< ReservedToken >() && token->get< ReservedToken >() == ReservedToken::OpColon )
                    {
                        // there is type declaration in this variable definition
                        ++token;
                        operands.push( parseOperand( token ) ); // parse type
                        nodeInfo.operandsCount++;
                        ++token;
                    }

                    if ( token->is< ReservedToken >() && token->get< ReservedToken >() == ReservedToken::OpAssign )
                    {
                        // there is initialization in this variable definition
                        ++token;
                        operands.push( parseImpl( token ) );
                        nodeInfo.operandsCount++;
                    }
                }
                else if ( nodeInfo.type == NodeType::ContractDefinition )
                {
                    skip< ReservedToken::KwContract >( token );
                    operands.push( parseImpl( token ) ); // parse contract name
                    skip< ReservedToken::OpColon >( token );
                    skip< Newline >( token );

                    nodeInfo.operandsCount += parseBody( token, operands, currentIndentationLevel + 1U );
                }

                if ( !operators.empty() && operators.top().type == NodeType::ModuleDefinition )
                {
                    operators.top().operandsCount++;
                }

                operators.push( nodeInfo );

                expectingOperand = nodeInfo.operandsCount != 0;
            }
            else
            {
                if ( !expectingOperand )
                {
                    throw ParsingError{ token->errorInfo(), "Unexpected operand" };
                }

                operands.push( parseOperand( token ) );
                expectingOperand = false;

                if ( operands.top()->is< Identifier >() )
                {
                    auto oldToken{ token };
                    ++token;

                    if ( token->is< ReservedToken >() && token->get< ReservedToken >() == ReservedToken::OpParenthesisOpen )
                    {
                        NodeInfo nodeInfo
                        {
                            .type          = NodeType::Call,
                            .operandsCount = getOperandsCount( NodeType::Call ),
                            .errorInfo     = token->errorInfo()
                        };

                        skip< ReservedToken::OpParenthesisOpen >( token );

                        if ( token->is_not< ReservedToken >() || token->get< ReservedToken >() != ReservedToken::OpParenthesisClose )
                        {
                            while ( true )
                            {
                                operands.push( parseImpl( token ) );

                                ++nodeInfo.operandsCount;

                                if ( token->is< ReservedToken >() )
                                {
                                    auto const reservedToken{ token->get< ReservedToken >() };
                                    if ( reservedToken == ReservedToken::OpParenthesisClose )
                                    {
                                        break;
                                    }
                                    else if ( reservedToken == ReservedToken::OpComma )
                                    {
                                        ++token;
                                    }
                                    else
                                    {
                                        throw ParsingError{ token->errorInfo(), "Expecting closing parenthesis" };
                                    }
                                }
                                else
                                {
                                    throw ParsingError{ token->errorInfo(), "Expecting closing parenthesis" };
                                }
                            }
                        }

                        if ( !operators.empty() && operators.top().type == NodeType::ModuleDefinition )
                        {
                            operators.top().operandsCount++;
                        }

                        operators.push( nodeInfo );

                        expectingOperand = false;
                    }
                    else
                    {
                        token = std::move( oldToken );
                    }
                }
            }

            if ( token->is< Newline >() )
            {
                break;
            }
        }

        if ( expectingOperand && operands.empty() )
        {
            throw ParsingError{ token->errorInfo(), "Expecting an operand" };
        }

        while ( !operators.empty() && operators.top().type != NodeType::ModuleDefinition )
        {
            popOperator( operands, operators, token->errorInfo());
        }

        if ( !operators.empty() && operators.top().type == NodeType::ModuleDefinition )
        {
            while ( token->is_not< Eof >() )
            {
                operands.push( parseImpl( token ) );
                operators.top().operandsCount++;
            }

            while ( !operators.empty() )
            {
                popOperator( operands, operators, token->errorInfo());
            }
        }

        return operands.empty() ? nullptr : std::move( operands.top() );
    }
} // namespace

Node::Pointer parse( TokenIterator & token )
{
    return parseImpl( token, 0U, false );
}

} // namespace A1::AST