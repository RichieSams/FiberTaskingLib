// cpptempl
// =================
// This is a template engine for C++.
// 
// Syntax
// =================
// Variables: {$variable_name}
// Loops: {% for person in people %}Name: {$person.name}{% endfor %}
// If: {% for person.name == "Bob" %}Full name: Robert{% endif %}
// 
// Copyright
// ==================
// Copyright (c) Ryan Ginstrom
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Modified by: Martinho Fernandes
//
// Usage
// =======================
//     std::string text = "{% if item %}{$item}{% endif %}\n"
//      "{% if thing %}{$thing}{% endif %}" ;
//  cpptempl::data_map data ;
//  data["item"] = cpptempl::make_data("aaa") ;
//  data["thing"] = cpptempl::make_data("bbb") ;
// 
//     std::string result = cpptempl::parse(text, data) ;
// 
// Handy Functions
// ========================
// make_data() : Feed it a string, data_map, or data_list to create a data entry.
// Example:
//  data_map person ;
//  person["name"] = make_data("Bob") ;
//  person["occupation"] = make_data("Plumber") ;
//  data_map data ;
//  data["person"] = make_data(person) ;
//     std::string result = parse(templ_text, data) ;

#ifndef CPPTEMPL_H
#define CPPTEMPL_H

#ifdef _MSC_VER
#pragma warning( disable : 4996 ) // 'std::copy': Function call with parameters that may be unsafe - this call relies on the caller to check that the passed values are correct. To disable this warning, use -D_SCL_SECURE_NO_WARNINGS. See documentation on how to use Visual C++ 'Checked Iterators'
#pragma warning( disable : 4512 ) // 'std::copy': Function call with parameters that may be unsafe - this call relies on the caller to check that the passed values are correct. To disable this warning, use -D_SCL_SECURE_NO_WARNINGS. See documentation on how to use Visual C++ 'Checked Iterators'
#endif

#include <string>
#include <vector>
#include <map>                          
#include <memory>
#include <unordered_map>

#include <ostream>

#include <sstream>

#include <nonius/detail/noexcept.hpp>
#include <nonius/detail/string_utils.hpp>

namespace cpptempl
{
    // various typedefs

    // data classes
    class Data ;
    class DataValue ;
    class DataList ;
    class DataMap ;

    class data_ptr {
    public:
        data_ptr() {}
        template<typename T> data_ptr(const T& data) {
            this->operator =(data);
        }
        data_ptr(DataValue* data);
        data_ptr(DataList* data);
        data_ptr(DataMap* data);
        data_ptr(const data_ptr& data) {
            ptr = data.ptr;
        }
        template<typename T> void operator = (const T& data);
        void push_back(const data_ptr& data);
        virtual ~data_ptr() {}
        Data* operator ->() {
            return ptr.get();
        }
    private:
        std::shared_ptr<Data> ptr;
    };
    typedef std::vector<data_ptr> data_list ;

    class data_map {
    public:
        data_ptr& operator [](const std::string& key);
        bool empty();
        bool has(const std::string& key);
    private:
        std::unordered_map<std::string, data_ptr> data;
    };

    template<> inline void data_ptr::operator = (const data_ptr& data);
    template<> void data_ptr::operator = (const std::string& data);
    template<> void data_ptr::operator = (const std::string& data);
    template<> void data_ptr::operator = (const data_map& data);
    template<typename T>
    void data_ptr::operator = (const T& data) {
        std::stringstream ss;
        ss << data;
        ss.exceptions(std::ios::failbit);
        std::string data_str = ss.str();
        this->operator =(data_str);
    }

    // token classes
    class Token ;
    typedef std::shared_ptr<Token> token_ptr ;
    typedef std::vector<token_ptr> token_vector ;

    // Custom exception class for library errors
    class TemplateException : public std::exception
    {
    public:
        TemplateException(std::string reason) : m_reason(std::move(reason)){}
        char const* what() const NONIUS_NOEXCEPT override {
            return m_reason.c_str();
        }
    private:
        std::string m_reason;
    };

    // Data types used in templates
    class Data
    {
    public:
        virtual bool empty() = 0 ;
        virtual std::string getvalue();
        virtual data_list& getlist();
        virtual data_map& getmap() ;
    };

    class DataValue : public Data
    {
        std::string m_value ;
    public:
        DataValue(std::string value) : m_value(value){}
        std::string getvalue();
        bool empty();
    };

    class DataList : public Data
    {
        data_list m_items ;
    public:
        DataList(const data_list &items) : m_items(items){}
        data_list& getlist() ;
        bool empty();
    };

    class DataMap : public Data
    {
        data_map m_items ;
    public:
        DataMap(const data_map &items) : m_items(items){}
        data_map& getmap();
        bool empty();
    };

    inline data_ptr::data_ptr(DataValue* data) : ptr(data) {}
    inline data_ptr::data_ptr(DataList* data) : ptr(data) {}
    inline data_ptr::data_ptr(DataMap* data) : ptr(data) {}

    // convenience functions for making data objects
    inline data_ptr make_data(std::string val)
    {
        return data_ptr(new DataValue(val)) ;
    }
    inline data_ptr make_data(data_list &val)
    {
        return data_ptr(new DataList(val)) ;
    }
    inline data_ptr make_data(data_map &val)
    {
        return data_ptr(new DataMap(val)) ;
    }
    // get a data value from a data map
    // e.g. foo.bar => data["foo"]["bar"]
    data_ptr parse_val(std::string key, data_map &data) ;

    typedef enum 
    {
        TOKEN_TYPE_NONE,
        TOKEN_TYPE_TEXT,
        TOKEN_TYPE_VAR,
        TOKEN_TYPE_IF,
        TOKEN_TYPE_FOR,
        TOKEN_TYPE_ENDIF,
        TOKEN_TYPE_ENDFOR,
    } TokenType;

    // Template tokens
    // base class for all token types
    class Token
    {
    public:
        virtual TokenType gettype() = 0 ;
        virtual void gettext(std::ostream &stream, data_map &data) = 0 ;
        virtual void set_children(token_vector &children);
        virtual token_vector & get_children();
    };

    // normal text
    class TokenText : public Token
    {
        std::string m_text ;
    public:
        TokenText(std::string text) : m_text(text){}
        TokenType gettype();
        void gettext(std::ostream &stream, data_map &data);
    };

    // variable
    class TokenVar : public Token
    {
        std::string m_key ;
    public:
        TokenVar(std::string key) : m_key(key){}
        TokenType gettype();
        void gettext(std::ostream &stream, data_map &data);
    };

    // for block
    class TokenFor : public Token 
    {
    public:
        std::string m_key ;
        std::string m_val ;
        token_vector m_children ;
        TokenFor(std::string expr);
        TokenType gettype();
        void gettext(std::ostream &stream, data_map &data);
        void set_children(token_vector &children);
        token_vector &get_children();
    };

    // if block
    class TokenIf : public Token
    {
    public:
        std::string m_expr ;
        token_vector m_children ;
        TokenIf(std::string expr) : m_expr(expr){}
        TokenType gettype();
        void gettext(std::ostream &stream, data_map &data);
        bool is_true(std::string expr, data_map &data);
        void set_children(token_vector &children);
        token_vector &get_children();
    };

    // end of block
    class TokenEnd : public Token // end of control block
    {
        std::string m_type ;
    public:
        TokenEnd(std::string text) : m_type(text){}
        TokenType gettype();
        void gettext(std::ostream &stream, data_map &data);
    };

    std::string gettext(token_ptr token, data_map &data) ;

    void parse_tree(token_vector &tokens, token_vector &tree, TokenType until=TOKEN_TYPE_NONE) ;
    token_vector & tokenize(std::string text, token_vector &tokens) ;

    // The big daddy. Pass in the template and data, 
    // and get out a completed doc.
    void parse(std::ostream &stream, std::string templ_text, data_map &data) ;
    std::string parse(std::string templ_text, data_map &data);
    std::string parse(std::string templ_text, data_map &data);

// *********** Implementation ************

    //////////////////////////////////////////////////////////////////////////
    // Data classes
    //////////////////////////////////////////////////////////////////////////

    // data_map
    inline data_ptr& data_map::operator [](const std::string& key) {
        return data[key];
    }
    inline bool data_map::empty() {
        return data.empty();
    }
    inline bool data_map::has(const std::string& key) {
        return data.find(key) != data.end();
    }

    // data_ptr
    template<>
    inline void data_ptr::operator = (const data_ptr& data) {
        ptr = data.ptr;
    }

    template<>
    inline void data_ptr::operator = (const std::string& data) {
        ptr.reset(new DataValue(data));
    }

    template<>
    inline void data_ptr::operator = (const data_map& data) {
        ptr.reset(new DataMap(data));
    }

    inline void data_ptr::push_back(const data_ptr& data) {
        if (!ptr) {
            ptr.reset(new DataList(data_list()));
        }
        data_list& list = ptr->getlist();
        list.push_back(data);
    }

    // base data
    inline std::string Data::getvalue()
    {
        throw TemplateException("Data item is not a value") ;
    }

    inline data_list& Data::getlist()
    {
        throw TemplateException("Data item is not a list") ;
    }
    inline data_map& Data::getmap()
    {
        throw TemplateException("Data item is not a dictionary") ;
    }
    // data value
    inline std::string DataValue::getvalue()
    {
        return m_value ;
    }
    inline bool DataValue::empty()
    {
        return m_value.empty();
    }
    // data list
    inline data_list& DataList::getlist()
    {
        return m_items ;
    }

    inline bool DataList::empty()
    {
        return m_items.empty();
    }
    // data map
    inline data_map& DataMap:: getmap()
    {
        return m_items ;
    }
    inline bool DataMap::empty()
    {
        return m_items.empty();
    }
    //////////////////////////////////////////////////////////////////////////
    // parse_val
    //////////////////////////////////////////////////////////////////////////
    inline data_ptr parse_val(std::string key, data_map &data)
    {
        // quoted string
        if (key[0] == '\"')
        {
            return make_data(nonius::trim_copy_if(key, [](char c){ return c == '"'; }));
        }
        // check for dotted notation, i.e [foo.bar]
        size_t index = key.find(".") ;
        if (index == std::string::npos)
        {
            if (!data.has(key))
            {
                return make_data("{$" + key + "}") ;
            }
            return data[key] ;
        }

        std::string sub_key = key.substr(0, index) ;
        if (!data.has(sub_key))
        {
            return make_data("{$" + key + "}") ;
        }
        data_ptr item = data[sub_key] ;
        return parse_val(key.substr(index+1), item->getmap()) ;
    }

    //////////////////////////////////////////////////////////////////////////
    // Token classes
    //////////////////////////////////////////////////////////////////////////

    // defaults, overridden by subclasses with children
    inline void Token::set_children( token_vector & )
    {
        throw TemplateException("This token type cannot have children") ;
    }

    inline token_vector & Token::get_children()
    {
        throw TemplateException("This token type cannot have children") ;
    }

    // TokenText
    inline TokenType TokenText::gettype()
    {
        return TOKEN_TYPE_TEXT ;
    }

    inline void TokenText::gettext( std::ostream &stream, data_map & )
    {
        stream << m_text ;
    }

    // TokenVar
    inline TokenType TokenVar::gettype()
    {
        return TOKEN_TYPE_VAR ;
    }

    inline void TokenVar::gettext( std::ostream &stream, data_map &data )
    {
        stream << parse_val(m_key, data)->getvalue() ;
    }

    // TokenFor
    inline TokenFor::TokenFor(std::string expr)
    {
        std::vector<std::string> elements ;
        nonius::split(elements, expr, nonius::is_space()) ;
        if (elements.size() != 4u)
        {
            throw TemplateException("Invalid syntax in for statement") ;
        }
        m_val = elements[1] ;
        m_key = elements[3] ;
    }

    inline TokenType TokenFor::gettype()
    {
        return TOKEN_TYPE_FOR ;
    }

    inline void TokenFor::gettext( std::ostream &stream, data_map &data )
    {
        data_ptr value = parse_val(m_key, data) ;
        data_list &items = value->getlist() ;
        for (size_t i = 0 ; i < items.size() ; ++i)
        {
            data_map loop ;
            loop["index"] = make_data(std::to_string(i+1)) ;
            loop["index0"] = make_data(std::to_string(i)) ;
            data["loop"] = make_data(loop);
            data[m_val] = items[i] ;
            for(size_t j = 0 ; j < m_children.size() ; ++j)
            {
                m_children[j]->gettext(stream, data) ;
            }
        }
    }

    inline void TokenFor::set_children( token_vector &children )
    {
        m_children.assign(children.begin(), children.end()) ;
    }

    inline token_vector & TokenFor::get_children()
    {
        return m_children;
    }

    // TokenIf
    inline TokenType TokenIf::gettype()
    {
        return TOKEN_TYPE_IF ;
    }

    inline void TokenIf::gettext( std::ostream &stream, data_map &data )
    {
        if (is_true(m_expr, data))
        {
            for(size_t j = 0 ; j < m_children.size() ; ++j)
            {
                m_children[j]->gettext(stream, data) ;
            }
        }
    }

    inline bool TokenIf::is_true( std::string expr, data_map &data )
    {
        std::vector<std::string> elements ;
        nonius::split(elements, expr, nonius::is_space()) ;

        if (elements[1] == "not")
        {
            return parse_val(elements[2], data)->empty() ;
        }
        if (elements.size() == 2)
        {
            return ! parse_val(elements[1], data)->empty() ;
        }
        data_ptr lhs = parse_val(elements[1], data) ;
        data_ptr rhs = parse_val(elements[3], data) ;
        if (elements[2] == "==")
        {
            return lhs->getvalue() == rhs->getvalue() ;
        }
        return lhs->getvalue() != rhs->getvalue() ;
    }

    inline void TokenIf::set_children( token_vector &children )
    {
        m_children.assign(children.begin(), children.end()) ;
    }

    inline token_vector & TokenIf::get_children()
    {
        return m_children;
    }

    // TokenEnd
    inline TokenType TokenEnd::gettype()
    {
        return m_type == "endfor" ? TOKEN_TYPE_ENDFOR : TOKEN_TYPE_ENDIF ;
    }

    inline void TokenEnd::gettext( std::ostream &, data_map &)
    {
        throw TemplateException("End-of-control statements have no associated text") ;
    }

    // gettext
    // generic helper for getting text from tokens.

    inline std::string gettext(token_ptr token, data_map &data)
    {
        std::ostringstream stream ;
        token->gettext(stream, data) ;
        return stream.str() ;
    }
    //////////////////////////////////////////////////////////////////////////
    // parse_tree
    // recursively parses list of tokens into a tree
    //////////////////////////////////////////////////////////////////////////
    inline void parse_tree(token_vector &tokens, token_vector &tree, TokenType until)
    {
        while(! tokens.empty())
        {
            // 'pops' first item off list
            token_ptr token = tokens[0] ;
            tokens.erase(tokens.begin()) ;

            if (token->gettype() == TOKEN_TYPE_FOR)
            {
                token_vector children ;
                parse_tree(tokens, children, TOKEN_TYPE_ENDFOR) ;
                token->set_children(children) ;
            }
            else if (token->gettype() == TOKEN_TYPE_IF)
            {
                token_vector children ;
                parse_tree(tokens, children, TOKEN_TYPE_ENDIF) ;
                token->set_children(children) ;
            }
            else if (token->gettype() == until)
            {
                return ;
            }
            tree.push_back(token) ;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    // tokenize
    // parses a template into tokens (text, for, if, variable)
    //////////////////////////////////////////////////////////////////////////
    inline token_vector & tokenize(std::string text, token_vector &tokens)
    {
        while(! text.empty())
        {
            size_t pos = text.find("{") ;
            if (pos == std::string::npos)
            {
                if (! text.empty())
                {
                    tokens.push_back(token_ptr(new TokenText(text))) ;
                }
                return tokens ;
            }
            std::string pre_text = text.substr(0, pos) ;
            if (! pre_text.empty())
            {
                tokens.push_back(token_ptr(new TokenText(pre_text))) ;
            }
            text = text.substr(pos+1) ;
            if (text.empty())
            {
                tokens.push_back(token_ptr(new TokenText("{"))) ;
                return tokens ;
            }

            // variable
            if (text[0] == '$')
            {
                pos = text.find("}") ;
                if (pos != std::string::npos)
                {
                    tokens.push_back(token_ptr (new TokenVar(text.substr(1, pos-1)))) ;
                    text = text.substr(pos+1) ;
                }
            }
            // control statement
            else if (text[0] == '%')
            {
                pos = text.find("}") ;
                if (pos != std::string::npos)
                {
                    std::string expression = nonius::trim_copy(text.substr(1, pos-2)) ;
                    text = text.substr(pos+1) ;
                    if (nonius::starts_with(expression, "for"))
                    {
                        tokens.push_back(token_ptr (new TokenFor(expression))) ;
                    }
                    else if (nonius::starts_with(expression, "if"))
                    {
                        tokens.push_back(token_ptr (new TokenIf(expression))) ;
                    }
                    else
                    {
                        tokens.push_back(token_ptr (new TokenEnd(nonius::trim_copy(expression)))) ;
                    }
                }
            }
            else
            {
                tokens.push_back(token_ptr(new TokenText("{"))) ;
            }
        }
        return tokens ;
    }

    /************************************************************************
    * parse
    *
    *  1. tokenizes template
    *  2. parses tokens into tree
    *  3. resolves template
    *  4. returns converted text
    ************************************************************************/
    inline std::string parse(std::string templ_text, data_map &data)
    {
        std::ostringstream stream ;
        parse(stream, templ_text, data) ;
        return stream.str() ;
    }
    inline void parse(std::ostream &stream, std::string templ_text, data_map &data)
    {
        token_vector tokens ;
        tokenize(templ_text, tokens) ;
        token_vector tree ;
        parse_tree(tokens, tree) ;

        for (size_t i = 0 ; i < tree.size() ; ++i)
        {
            // Recursively calls gettext on each node in the tree.
            // gettext returns the appropriate text for that node.
            // for text, itself;
            // for variable, substitution;
            // for control statement, recursively gets kids
            tree[i]->gettext(stream, data) ;
        }
    }
}

#endif // CPPTEMPL_H
