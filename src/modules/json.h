#ifndef JSON_H
#define JSON_H

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <stack>

#if __GNUC__ >= 3
# define maybe_expect(expr,value)       __builtin_expect ((expr),(value))
# define is_const(expr)                 __builtin_constant_p ((expr))
#else
# define maybe_expect(expr,value)       (expr)
# define is_const(expr)                 0
#endif

#define expect_false(expr) maybe_expect ((expr) != 0, 0)
#define expect_true(expr)  maybe_expect ((expr) != 0, 1)

#ifdef __GNUC__
# define CURFUNC       __PRETTY_FUNCTION__
#elif defined(__BORLANDC__)
# define CURFUNC       __FUNC__
#else
# define CURFUNC       __FUNCTION__
#endif

namespace json
{
  class ValueIterator;
  class ValueConstIterator;

  enum ValueType
    {
      nullValue = 0,
      intValue,
      uintValue,
      realValue,
      stringValue,
      booleanValue,
      arrayValue,
      objectValue
    };

  class StaticString
  {
  public:
    explicit StaticString (char const *czstring)
      : str_ (czstring)
    {
    }

    operator char const *() const
    {
      return str_;
    }

    char const *c_str() const
    {
      return str_;
    }

  private:
    char const *str_;
  };

  class Value 
  {
    friend class ValueIteratorBase;
  public:
    typedef std::vector<std::string> Members;
    typedef ValueIterator iterator;
    typedef ValueConstIterator const_iterator;
    typedef unsigned ArrayIndex;

    static const Value null;
    static const int minInt;
    static const int maxInt;
    static const unsigned maxUInt;

  private:
    class CZString 
    {
    public:
      enum DuplicationPolicy 
        {
          noDuplication = 0,
          duplicate,
          duplicateOnCopy
        };
      CZString (int index);
      CZString (char const *cstr, DuplicationPolicy allocate);
      CZString (const CZString &other);
      ~CZString ();
      CZString &operator = (const CZString &other);
      bool operator < (const CZString &other) const;
      bool operator == (const CZString &other) const;
      int index () const;
      char const *c_str () const;
      bool isStaticString () const;
    private:
      void swap (CZString &other);
      char const *cstr_;
      int index_;
    };

  public:
    typedef std::map<CZString, Value> ObjectValues;

  public:
    Value (ValueType type = nullValue);
    Value (int value);
    Value (unsigned value);
    Value (double value);
    Value (char const *value);
    Value (const StaticString &value);
    Value (const std::string &value);
    Value (bool value);
    Value (const Value &other);
    ~Value ();

    Value &operator = (const Value &other);

    void swap (Value &other);

    ValueType type () const;

    bool operator < (const Value &other) const;
    bool operator <= (const Value &other) const;
    bool operator >= (const Value &other) const;
    bool operator > (const Value &other) const;

    bool operator == (const Value &other) const;
    bool operator != (const Value &other) const;

    operator char const * () const;
    operator std::string () const;
    operator int () const;
    operator unsigned () const;
    operator double () const;
    operator bool () const;

    bool isNull () const;
    bool isBool () const;
    bool isInt () const;
    bool isUInt () const;
    bool isIntegral () const;
    bool isDouble () const;
    bool isNumeric () const;
    bool isString () const;
    bool isArray () const;
    bool isObject () const;

    bool isConvertibleTo (ValueType other) const;

    unsigned size () const;

    bool empty () const;

    bool operator ! () const;

    void clear ();

    void resize (unsigned size);

    Value &operator [] (int index);
    Value &operator [] (unsigned index);
    const Value &operator [] (int index) const;
    const Value &operator [] (unsigned index) const;
    Value get (int index, const Value &defaultValue) const;
    Value get (unsigned index, const Value &defaultValue) const;
    bool isValidIndex (int index) const;
    bool isValidIndex (unsigned index) const;

    Value &append (const Value &value);

    Value &operator [] (char const *key);
    const Value &operator [] (char const *key) const;
    Value &operator [] (const std::string &key);
    const Value &operator [] (const std::string &key) const;
    Value &operator [] (const StaticString &key);
    Value get (char const *key, const Value &defaultValue) const;
    Value get (const std::string &key, const Value &defaultValue) const;
    Value removeMember (char const* key);
    Value removeMember (const std::string &key);

    bool isMember (char const *key) const;
    bool isMember (const std::string &key) const;

    Members getMemberNames () const;

    const_iterator begin () const;
    const_iterator end () const;

    iterator begin ();
    iterator end ();

  private:
    Value &resolveReference (char const *key, bool isStatic);

    struct MemberNamesTransform
    {
      typedef char const *result_type;
      char const *operator () (const CZString &name) const
      {
        return name.c_str ();
      }
    };

    union ValueHolder
    {
      int               int_;
      unsigned          uint_;
      double            real_;
      bool              bool_;
      char              *string_;
      ObjectValues      *map_;
    } value_;

    ValueType   type_      : 8;
    int         allocated_ : 1;
  };


  class ValueAllocator
  {
  public:
    enum { unknown = (unsigned)-1 };

    virtual ~ValueAllocator ();

    virtual char *makeMemberName (char const *memberName) = 0;
    virtual void releaseMemberName (char *memberName) = 0;
    virtual char *duplicateStringValue (char const *value, 
                                        unsigned length = unknown) = 0;
    virtual void releaseStringValue (char *value) = 0;
  };

  class ValueIteratorBase
  {
  public:
    typedef unsigned size_t;
    typedef int difference_type;
    typedef ValueIteratorBase SelfType;

    ValueIteratorBase ();
    explicit ValueIteratorBase (const Value::ObjectValues::iterator &current);

    bool operator == (const SelfType &other) const
    {
      return isEqual (other);
    }

    bool operator != (const SelfType &other) const
    {
      return !isEqual (other);
    }

    difference_type operator - (const SelfType &other) const
    {
      return computeDistance (other);
    }

    Value key () const;

    unsigned index () const;

    char const *memberName () const;

  protected:
    Value &deref () const;

    void increment ();

    void decrement ();

    difference_type computeDistance (const SelfType &other) const;

    bool isEqual (const SelfType &other) const;

    void copy (const SelfType &other);

  private:
    Value::ObjectValues::iterator current_;
  };

  class ValueConstIterator : public ValueIteratorBase
  {
    friend class Value;
  public:
    typedef unsigned size_t;
    typedef int difference_type;
    typedef const Value &reference;
    typedef const Value *pointer;
    typedef ValueConstIterator SelfType;

    ValueConstIterator ();
  private:
    explicit ValueConstIterator (const Value::ObjectValues::iterator &current);
  public:
    SelfType &operator = (const ValueIteratorBase &other);

    SelfType operator ++ (int)
    {
      SelfType temp (*this);
      ++*this;
      return temp;
    }

    SelfType operator -- (int)
    {
      SelfType temp (*this);
      --*this;
      return temp;
    }

    SelfType &operator -- ()
    {
      decrement ();
      return *this;
    }

    SelfType &operator ++ ()
    {
      increment ();
      return *this;
    }

    reference operator * () const
    {
      return deref ();
    }
  };


  class ValueIterator : public ValueIteratorBase
  {
    friend class Value;
  public:
    typedef unsigned size_t;
    typedef int difference_type;
    typedef Value &reference;
    typedef Value *pointer;
    typedef ValueIterator SelfType;

    ValueIterator ();
    ValueIterator (const ValueConstIterator &other);
    ValueIterator (const ValueIterator &other);

  private:
    explicit ValueIterator (const Value::ObjectValues::iterator &current);

  public:

    SelfType &operator = (const SelfType &other);

    SelfType operator ++ (int)
    {
      SelfType temp (*this);
      ++*this;
      return temp;
    }

    SelfType operator -- (int)
    {
      SelfType temp (*this);
      --*this;
      return temp;
    }

    SelfType &operator -- ()
    {
      decrement ();
      return *this;
    }

    SelfType &operator ++ ()
    {
      increment ();
      return *this;
    }

    reference operator * () const
    {
      return deref ();
    }
  };
} // namespace json

namespace json
{
  class Value;

  class Reader
  {
  public:
    typedef char const *Location;

    Reader ();

    bool parse (const std::string &document, Value &root);
    bool parse (char const *beginDoc, char const *endDOc, Value &root);
    bool parse (std::istream &, Value &root);

    std::string error_msgs () const;

  private:
    enum TokenType
      {
        tokenEndOfStream = 0,
        tokenObjectBegin,
        tokenObjectEnd,
        tokenArrayBegin,
        tokenArrayEnd,
        tokenString,
        tokenNumber,
        tokenTrue,
        tokenFalse,
        tokenNull,
        tokenArraySeparator,
        tokenMemberSeparator,
        tokenComment,
        tokenError
      };

    class Token
    {
    public:
      TokenType type_;
      Location start_;
      Location end_;
    };

    class ErrorInfo
    {
    public:
      Token token_;
      std::string message_;
      Location extra_;
    };

    typedef std::deque<ErrorInfo> Errors;

    bool expectToken (TokenType type, Token &token, char const *message);
    bool readToken (Token &token);
    void skipSpaces ();
    bool match (Location pattern, 
                int patternLength);
    bool readString ();
    void readNumber ();
    bool readValue ();
    bool readObject ();
    bool readArray ();
    bool decodeNumber (Token &token);
    bool decodeString (Token &token);
    bool decodeString (Token &token, std::string &decoded);
    bool decodeDouble (Token &token);
    bool decodeUnicodeEscapeSequence (Token &token, 
                                      Location &current, 
                                      Location end, 
                                      unsigned &unicode);
    bool addError (const std::string &message, 
                   Token &token,
                   Location extra = 0);
    bool recoverFromError (TokenType skipUntilToken);
    bool addErrorAndRecover (const std::string &message, 
                             Token &token,
                             TokenType skipUntilToken);
    void skipUntilSpace ();
    Value &currentValue ();
    char getNextChar ();
    void getLocationLineAndColumn (Location location,
                                   int &line,
                                   int &column) const;
    std::string getLocationLineAndColumn (Location location) const;
   
    typedef std::stack<Value *> Nodes;
    Nodes nodes_;
    Errors errors_;
    std::string document_;
    Location begin_;
    Location end_;
    Location current_;
    Location lastValueEnd_;
    Value *lastValue_;
  };

  std::istream& operator >> (std::istream&, Value&);

} // namespace json

namespace json
{
  class Value;

  class Writer
  {
  public:
    Writer () { }
    ~Writer () { }

  public:
    std::string write (const Value &root);

  private:
    void writeValue (const Value &value);

    std::string document_;
  };

  std::string valueToString (int value);
  std::string valueToString (unsigned value);
  std::string valueToString (double value);
  std::string valueToString (bool value);
  std::string valueToQuotedString (char const *value);
} // namespace json

/*
 * JSON-RPC implementation in C++
 */

namespace json
{
  namespace rpc
  {
    typedef void (Module::*method) (HTTPRequest *http, Value &request, Value &response);

    struct mfp
    {
      Module const *mod;
      method mth;
    };

    typedef std::map<std::string, mfp> method_map;
    extern method_map methods;
  
    void add_method (char *name, Module const *mod, method mth);
    void service (HTTPRequest *http, Value &request, Value &response);
    void process (HTTPRequest *http, std::string &response, char const *request);
  }
}

#endif // JSON_H
