/**
 *
 * Copyright (c)2022 The Adamnite C++ Authors.
 *
 * This code is open-sourced under the MIT license.
 */

#include <CoreLib/Tokenizer/ReservedToken.hpp>

#include <algorithm>
#include <utility>
#include <array>
#include <stack>

namespace A1
{

namespace
{
    template< typename T, std::size_t N1, std::size_t N2 >
    [[ nodiscard ]]
    constexpr std::array< T, N1 + N2 > concat( std::array< T, N1 > arr1, std::array< T, N2 > arr2 ) noexcept
    {
        std::array< T, N1 + N2 > result;
        std::size_t index{ 0 };

        for ( auto & item : arr1 ) { result[ index++ ] = std::move( item ); }
        for ( auto & item : arr2 ) { result[ index++ ] = std::move( item ); }
        return result;
    }

    template< typename T, std::size_t N >
    [[ nodiscard ]]
    constexpr std::array< T, N > sort( std::array< T, N > arr ) noexcept
    {
        std::sort( std::begin( arr ), std::end( arr ) );
        return arr;
    }

    struct StringifiedToken
    {
        std::string_view tokenStr;
        ReservedToken    token{ ReservedToken::Unknown };

        [[ nodiscard ]]
        constexpr bool operator<( StringifiedToken const & rhs ) const
        {
            return tokenStr < rhs.tokenStr;
        }
    };

    constexpr std::array keywords
    {
        sort
        (
            std::array
            {
                StringifiedToken{ "False"   , ReservedToken::KwFalse    },
                StringifiedToken{ "None"    , ReservedToken::KwNone     },
                StringifiedToken{ "True"    , ReservedToken::KwTrue     },
                StringifiedToken{ "and"     , ReservedToken::KwAnd      },
                StringifiedToken{ "as"      , ReservedToken::KwAs       },
                StringifiedToken{ "assert"  , ReservedToken::KwAssert   },
                StringifiedToken{ "break"   , ReservedToken::KwBreak    },
                StringifiedToken{ "class"   , ReservedToken::KwClass    },
                StringifiedToken{ "continue", ReservedToken::KwContinue },
                StringifiedToken{ "contract", ReservedToken::KwContract },
                StringifiedToken{ "def"     , ReservedToken::KwDef      },
                StringifiedToken{ "del"     , ReservedToken::KwDel      },
                StringifiedToken{ "elif"    , ReservedToken::KwElif     },
                StringifiedToken{ "else"    , ReservedToken::KwElse     },
                StringifiedToken{ "except"  , ReservedToken::KwExcept   },
                StringifiedToken{ "finally" , ReservedToken::KwFinally  },
                StringifiedToken{ "for"     , ReservedToken::KwFor      },
                StringifiedToken{ "from"    , ReservedToken::KwFrom     },
                StringifiedToken{ "global"  , ReservedToken::KwGlobal   },
                StringifiedToken{ "if"      , ReservedToken::KwIf       },
                StringifiedToken{ "import"  , ReservedToken::KwImport   },
                StringifiedToken{ "in"      , ReservedToken::KwIn       },
                StringifiedToken{ "is"      , ReservedToken::KwIs       },
                StringifiedToken{ "lambda"  , ReservedToken::KwLambda   },
                StringifiedToken{ "let"     , ReservedToken::KwLet      },
                StringifiedToken{ "non"     , ReservedToken::KwNon      },
                StringifiedToken{ "nonlocal", ReservedToken::KwNonLocal },
                StringifiedToken{ "not"     , ReservedToken::KwNot      },
                StringifiedToken{ "num"     , ReservedToken::KwNumber   },
                StringifiedToken{ "or"      , ReservedToken::KwOr       },
                StringifiedToken{ "pass"    , ReservedToken::KwPass     },
                StringifiedToken{ "raise"   , ReservedToken::KwRaise    },
                StringifiedToken{ "return"  , ReservedToken::KwReturn   },
                StringifiedToken{ "str"     , ReservedToken::KwString   },
                StringifiedToken{ "try"     , ReservedToken::KwTry      },
                StringifiedToken{ "while"   , ReservedToken::KwWhile    },
                StringifiedToken{ "with"    , ReservedToken::KwWith     },
                StringifiedToken{ "yield"   , ReservedToken::KwYield    }
            }
        )
    };

    constexpr auto operators
    {
        sort
        (
            std::array
            {
                StringifiedToken{ "!"  , ReservedToken::OpLogicalNot              },
                StringifiedToken{ "!=" , ReservedToken::OpNotEqual                },
                StringifiedToken{ "%"  , ReservedToken::OpMod                     },
                StringifiedToken{ "%=" , ReservedToken::OpAssignMod               },
                StringifiedToken{ "&"  , ReservedToken::OpBitwiseAnd              },
                StringifiedToken{ "&&" , ReservedToken::OpLogicalAnd              },
                StringifiedToken{ "&=" , ReservedToken::OpAssignBitwiseAnd        },
                StringifiedToken{ "("  , ReservedToken::OpParenthesisOpen         },
                StringifiedToken{ ")"  , ReservedToken::OpParenthesisClose        },
                StringifiedToken{ "*"  , ReservedToken::OpMul                     },
                StringifiedToken{ "**" , ReservedToken::OpExp                     },
                StringifiedToken{ "**=", ReservedToken::OpAssignExp               },
                StringifiedToken{ "*=" , ReservedToken::OpAssignMul               },
                StringifiedToken{ "+"  , ReservedToken::OpAdd                     },
                StringifiedToken{ "+=" , ReservedToken::OpAssignAdd               },
                StringifiedToken{ ","  , ReservedToken::OpComma                   },
                StringifiedToken{ "-"  , ReservedToken::OpSub                     },
                StringifiedToken{ "-=" , ReservedToken::OpAssignSub               },
                StringifiedToken{ "->" , ReservedToken::OpArrow                   },
                StringifiedToken{ "."  , ReservedToken::OpDot                     },
                StringifiedToken{ "/"  , ReservedToken::OpDiv                     },
                StringifiedToken{ "//" , ReservedToken::OpFloorDiv                },
                StringifiedToken{ "//=", ReservedToken::OpAssignFloorDiv          },
                StringifiedToken{ "/=" , ReservedToken::OpAssignDiv               },
                StringifiedToken{ ":"  , ReservedToken::OpColon                   },
                StringifiedToken{ "<"  , ReservedToken::OpLessThan                },
                StringifiedToken{ "<<" , ReservedToken::OpBitwiseLeftShift        },
                StringifiedToken{ "<<=", ReservedToken::OpAssignBitwiseLeftShift  },
                StringifiedToken{ "<=" , ReservedToken::OpLessThanEqual           },
                StringifiedToken{ "="  , ReservedToken::OpAssign                  },
                StringifiedToken{ "==" , ReservedToken::OpEqual                   },
                StringifiedToken{ ">"  , ReservedToken::OpGreaterThan             },
                StringifiedToken{ ">=" , ReservedToken::OpGreaterThanEqual        },
                StringifiedToken{ ">>" , ReservedToken::OpBitwiseRightShift       },
                StringifiedToken{ ">>=", ReservedToken::OpAssignBitwiseRightShift },
                StringifiedToken{ "["  , ReservedToken::OpSubscriptOpen           },
                StringifiedToken{ "]"  , ReservedToken::OpSubscriptClose          },
                StringifiedToken{ "^"  , ReservedToken::OpBitwiseXor              },
                StringifiedToken{ "^=" , ReservedToken::OpAssignBitwiseXor        },
                StringifiedToken{ "|"  , ReservedToken::OpBitwiseOr               },
                StringifiedToken{ "|=" , ReservedToken::OpAssignBitwiseOr         },
                StringifiedToken{ "||" , ReservedToken::OpLogicalOr               },
                StringifiedToken{ "~"  , ReservedToken::OpBitwiseNot              }
            }
        )
    };

    constexpr auto allTokens{ sort( concat( keywords, operators ) ) };
    static_assert
    (
        std::size( allTokens ) == static_cast< std::size_t >( ReservedToken::Count ) - 1 /* Unknown */,
        "There are reserved tokens that are not included in tokens map"
    );

    class MaximalMunchComp
    {
    public:
        MaximalMunchComp( std::size_t const idx ) : idx_{ idx } {}

        [[ nodiscard ]]
        bool operator()( StringifiedToken const lhs, char const rhs ) const noexcept
        {
            return lhs.tokenStr.size() <= idx_ || lhs.tokenStr[ idx_ ] < rhs;
        }

        [[ nodiscard ]]
        bool operator()( char const lhs, StringifiedToken const rhs ) const noexcept
        {
            return rhs.tokenStr.size() > idx_ && rhs.tokenStr[ idx_ ] > lhs;
        }

    private:
        std::size_t idx_{ 0U };
    };

} // namespace

std::string_view toStringView( ReservedToken const token ) noexcept
{
    auto const it
    {
        std::find_if
        (
            std::begin( allTokens ), std::end( allTokens ),
            [ token ]( auto const & t ) noexcept
            {
                return t.token == token;
            }
        )
    };
    return it == std::end( allTokens ) ? "" : it->tokenStr;
}

ReservedToken getKeyword( std::string_view const word ) noexcept
{
    auto const it
    {
        std::find_if
        (
            std::begin( keywords ), std::end( keywords ),
            [ word ]( auto const & t ) noexcept
            {
                return t.tokenStr == word;
            }
        )
    };

    return it == std::end( keywords ) ? ReservedToken::Unknown : it->token;
}

ReservedToken getOperator( PushBackStream & stream ) noexcept
{
    auto result{ ReservedToken::Unknown };

    auto candidates{ std::make_pair( std::begin( operators ), std::end( operators ) ) };

    std::size_t matchSize{ 0U };

    std::stack< int > chars;

    for ( std::size_t idx{ 0U }; candidates.first != candidates.second; ++idx )
    {
        if ( auto const c{ stream.pop() }; c )
        {
            chars.push( *c );
        }
        else
        {
            break;
        }

        candidates = std::equal_range
        (
            candidates.first,
            candidates.second,
            chars.top(),
            MaximalMunchComp{ idx }
        );

        if ( candidates.first != candidates.second && candidates.first->tokenStr.size() == idx + 1 )
        {
            matchSize = idx + 1;
            result = candidates.first->token;
        }
    }

    while ( chars.size() > matchSize )
    {
        stream.push( chars.top() );
        chars.pop();
    }

    return result;
}

} // namespace A1