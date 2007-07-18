/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "configreader.h"
#include "modules.h"
#include "inspsocket.h"
#include "httpd.h"
#include "json.h"

/* $ModDesc: Provides a JSON-RPC interface for modules using m_httpd.so */

class ModuleRpcJson : public Module
{
	void MthModuleVersion (HTTPRequest *http, json::Value &request, json::Value &response)
	{
		std::string result = "GetVersion().ToString()";
		response["result"] = result;
	}

	void system_list_methods (HTTPRequest *http, json::Value &request, json::Value &response)
	{
		unsigned i = 0;
		json::Value method_list (json::arrayValue);
		
		json::rpc::method_map::iterator it;
		for (it = json::rpc::methods.begin(); it != json::rpc::methods.end(); ++it)
		{
			method_list[i] = json::Value (it->first);
			i++;
		}
		
		response["result"] = method_list;
	}

 public:
	ModuleRpcJson(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->PublishInterface("JSON-RPC", this);
		json::rpc::add_method ("system.listMethods", (Module *)this, (void (Module::*)(HTTPRequest*, json::Value&, json::Value&))&ModuleRpcJson::system_list_methods);
		json::rpc::add_method ("ircd.moduleVersion", (Module *)this, (void (Module::*)(HTTPRequest*, json::Value&, json::Value&))&ModuleRpcJson::MthModuleVersion);
	}

	void OnEvent(Event* event)
	{
		std::stringstream data("");

		if (event->GetEventID() == "httpd_url")
		{
			HTTPRequest* http = (HTTPRequest*)event->GetData();

			if (http->GetURI() == "/jsonrpc" && http->GetType() == "POST")
			{
				try
				{
					std::string response_text;
					json::rpc::process (http, response_text, http->GetPostData().c_str());
					data << response_text;
				}
				catch (std::runtime_error &)
				{
					data << "{ \"result\": \"JSON Fault\", \"error\": \"Invalid RPC call\", \"id\": 1}";
				}

				/* Send the document back to m_httpd */
				HTTPDocument response(http->sock, &data, 200, "X-Powered-By: m_rpc_json.so\r\n"
									      "Content-Type: application/json; charset=iso-8859-1\r\n");
				Request req((char*)&response, (Module*)this, event->GetSource());
				req.Send();
			}
		}
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = 1;
	}

	virtual ~ModuleRpcJson()
	{
		ServerInstance->UnpublishInterface("JSON-RPC", this);
	}

	virtual Version GetVersion()
	{
		return Version(0, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

namespace json
{
  ValueIteratorBase::ValueIteratorBase ()
  {
  }


  ValueIteratorBase::ValueIteratorBase (const Value::ObjectValues::iterator &current)
    : current_ (current)
  {
  }

  Value &
  ValueIteratorBase::deref () const
  {
    return current_->second;
  }


  void 
  ValueIteratorBase::increment ()
  {
    ++current_;
  }


  void 
  ValueIteratorBase::decrement ()
  {
    --current_;
  }


  ValueIteratorBase::difference_type 
  ValueIteratorBase::computeDistance (const SelfType &other) const
  {
    return difference_type (std::distance (current_, other.current_));
  }


  bool 
  ValueIteratorBase::isEqual (const SelfType &other) const
  {
    return current_ == other.current_;
  }


  void 
  ValueIteratorBase::copy (const SelfType &other)
  {
    current_ = other.current_;
  }


  Value 
  ValueIteratorBase::key () const
  {
    const Value::CZString czstring = (*current_).first;
    if (czstring.c_str ())
      {
	if (czstring.isStaticString ())
	  return Value (StaticString (czstring.c_str ()));
	return Value (czstring.c_str ());
      }
    return Value (czstring.index ());
  }


  unsigned 
  ValueIteratorBase::index () const
  {
    const Value::CZString czstring = (*current_).first;
    if (!czstring.c_str ())
      return czstring.index ();
    return unsigned (-1);
  }


  char const *
  ValueIteratorBase::memberName () const
  {
    char const *name = (*current_).first.c_str ();
    return name ? name : "";
  }


  ValueConstIterator::ValueConstIterator ()
  {
  }


  ValueConstIterator::ValueConstIterator (const Value::ObjectValues::iterator &current)
    : ValueIteratorBase (current)
  {
  }

  ValueConstIterator &
  ValueConstIterator::operator = (const ValueIteratorBase &other)
  {
    copy (other);
    return *this;
  }


  ValueIterator::ValueIterator ()
  {
  }


  ValueIterator::ValueIterator (const Value::ObjectValues::iterator &current)
    : ValueIteratorBase (current)
  {
  }

  ValueIterator::ValueIterator (const ValueConstIterator &other)
    : ValueIteratorBase (other)
  {
  }

  ValueIterator::ValueIterator (const ValueIterator &other)
    : ValueIteratorBase (other)
  {
  }

  ValueIterator &
  ValueIterator::operator = (const SelfType &other)
  {
    copy (other);
    return *this;
  }
}

namespace json
{
  inline bool 
  in (char c, char c1, char c2, char c3, char c4)
  {
    return c == c1 || c == c2 || c == c3 || c == c4;
  }

  inline bool 
  in (char c, char c1, char c2, char c3, char c4, char c5)
  {
    return c == c1 || c == c2 || c == c3 || c == c4 || c == c5;
  }


  Reader::Reader ()
  {
  }

  bool
  Reader::parse (const std::string &document, 
		 Value &root)
  {
    document_ = document;
    char const *begin = document_.c_str ();
    char const *end = begin + document_.length ();
    return parse (begin, end, root);
  }

  bool
  Reader::parse (std::istream& sin,
		 Value &root)
  {
    std::string doc;
    std::getline (sin, doc, (char)EOF);
    return parse (doc, root);
  }

  bool 
  Reader::parse (char const *beginDoc, char const *endDOc, 
		 Value &root)
  {
    begin_ = beginDoc;
    end_ = endDOc;
    current_ = begin_;
    lastValueEnd_ = 0;
    lastValue_ = 0;
    errors_.clear ();
    while (!nodes_.empty ())
      nodes_.pop ();
    nodes_.push (&root);
   
    bool successful = readValue ();
    return successful;
  }


  bool
  Reader::readValue ()
  {
    Token token;
    do
      readToken (token);
    while (token.type_ == tokenComment);
    bool successful = true;

    switch (token.type_)
      {
      case tokenObjectBegin:
	successful = readObject ();
	break;
      case tokenArrayBegin:
	successful = readArray ();
	break;
      case tokenNumber:
	successful = decodeNumber (token);
	break;
      case tokenString:
	successful = decodeString (token);
	break;
      case tokenTrue:
	currentValue () = true;
	break;
      case tokenFalse:
	currentValue () = false;
	break;
      case tokenNull:
	currentValue () = Value ();
	break;
      default:
	return addError ("Syntax error: value, object or array expected.", token);
      }

    return successful;
  }


  bool 
  Reader::expectToken (TokenType type, Token &token, char const *message)
  {
    readToken (token);
    if (token.type_ != type)
      return addError (message, token);
    return true;
  }


  bool 
  Reader::readToken (Token &token)
  {
    skipSpaces ();
    token.start_ = current_;
    char c = getNextChar ();
    bool ok = true;
    switch (c)
      {
      case '{':
	token.type_ = tokenObjectBegin;
	break;
      case '}':
	token.type_ = tokenObjectEnd;
	break;
      case '[':
	token.type_ = tokenArrayBegin;
	break;
      case ']':
	token.type_ = tokenArrayEnd;
	break;
      case '"':
	token.type_ = tokenString;
	ok = readString ();
	break;
#if 0
#ifdef __GNUC__
      case '0'...'9':
#endif
#else
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
      case '8': case '9':
#endif
      case '-':
	token.type_ = tokenNumber;
	readNumber ();
	break;
      case 't':
	token.type_ = tokenTrue;
	ok = match ("rue", 3);
	break;
      case 'f':
	token.type_ = tokenFalse;
	ok = match ("alse", 4);
	break;
      case 'n':
	token.type_ = tokenNull;
	ok = match ("ull", 3);
	break;
      case ',':
	token.type_ = tokenArraySeparator;
	break;
      case ':':
	token.type_ = tokenMemberSeparator;
	break;
      case 0:
	token.type_ = tokenEndOfStream;
	break;
      default:
	ok = false;
	break;
      }
    if (!ok)
      token.type_ = tokenError;
    token.end_ = current_;
    return true;
  }


  void 
  Reader::skipSpaces ()
  {
    while (current_ != end_)
      {
	char c = *current_;
	if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
	  ++current_;
	else
	  break;
      }
  }


  bool 
  Reader::match (Location pattern, int patternLength)
  {
    if (end_ - current_ < patternLength)
      return false;
    int index = patternLength;
    while (index--)
      if (current_[index] != pattern[index])
	return false;
    current_ += patternLength;
    return true;
  }


  void 
  Reader::readNumber ()
  {
    while (current_ != end_)
      {
	if (!(*current_ >= '0' && *current_ <= '9')  &&
	     !in (*current_, '.', 'e', 'E', '+', '-'))
	  break;
	++current_;
      }
  }

  bool
  Reader::readString ()
  {
    char c = 0;
    while (current_ != end_)
      {
	c = getNextChar ();
	if (c == '\\')
	  getNextChar ();
	else if (c == '"')
	  break;
      }
    return c == '"';
  }


  bool 
  Reader::readObject ()
  {
    Token tokenName;
    std::string name;
    currentValue () = Value (objectValue);
    while (readToken (tokenName))
      {
	if (tokenName.type_ == tokenObjectEnd && name.empty ())  // empty object
	  return true;
	if (tokenName.type_ != tokenString)
	  break;
      
	name = "";
	if (!decodeString (tokenName, name))
	  return recoverFromError (tokenObjectEnd);

	Token colon;
	if (!readToken (colon) ||  colon.type_ != tokenMemberSeparator)
	  {
	    return addErrorAndRecover ("Missing ':' after object member name", 
				       colon, 
				       tokenObjectEnd);
	  }
	Value &value = currentValue ()[ name ];
	nodes_.push (&value);
	bool ok = readValue ();
	nodes_.pop ();
	if (!ok) // error already set
	  return recoverFromError (tokenObjectEnd);

	Token comma;
	if (!readToken (comma)
	     ||   (comma.type_ != tokenObjectEnd && 
		   comma.type_ != tokenArraySeparator))
	  {
	    return addErrorAndRecover ("Missing ',' or '}' in object declaration", 
				       comma, 
				       tokenObjectEnd);
	  }
	if (comma.type_ == tokenObjectEnd)
	  return true;
      }
    return addErrorAndRecover ("Missing '}' or object member name", 
			       tokenName, 
			       tokenObjectEnd);
  }


  bool 
  Reader::readArray ()
  {
    currentValue () = Value (arrayValue);
    skipSpaces ();
    if (*current_ == ']') // empty array
      {
	Token endArray;
	readToken (endArray);
	return true;
      }
    int index = 0;
    while (true)
      {
	Value &value = currentValue ()[ index++ ];
	nodes_.push (&value);
	bool ok = readValue ();
	nodes_.pop ();
	if (!ok) // error already set
	  return recoverFromError (tokenArrayEnd);

	Token token;
	if (!readToken (token) 
	     ||   (token.type_ != tokenArraySeparator && 
		   token.type_ != tokenArrayEnd))
	  {
	    return addErrorAndRecover ("Missing ',' or ']' in array declaration", 
				       token, 
				       tokenArrayEnd);
	  }
	if (token.type_ == tokenArrayEnd)
	  break;
      }
    return true;
  }


  bool 
  Reader::decodeNumber (Token &token)
  {
    bool isDouble = false;
    for (Location inspect = token.start_; inspect != token.end_; ++inspect)
      {
	isDouble = isDouble  
	  ||  in (*inspect, '.', 'e', 'E', '+')  
	  ||   (*inspect == '-' && inspect != token.start_);
      }
    if (isDouble)
      return decodeDouble (token);
    Location current = token.start_;
    bool isNegative = *current == '-';
    if (isNegative)
      ++current;
    unsigned threshold = (isNegative ? unsigned (-Value::minInt) 
			     : Value::maxUInt) / 10;
    unsigned value = 0;
    while (current < token.end_)
      {
	char c = *current++;
	if (c < '0'  ||  c > '9')
	  return addError ("'" + std::string (token.start_, token.end_) + "' is not a number.", token);
	if (value >= threshold)
	  return decodeDouble (token);
	value = value * 10 + unsigned (c - '0');
      }
    if (isNegative)
      currentValue () = -int (value);
    else if (value <= unsigned (Value::maxInt))
      currentValue () = int (value);
    else
      currentValue () = value;
    return true;
  }


  bool 
  Reader::decodeDouble (Token &token)
  {
    double value = 0;
    const int bufferSize = 32;
    int count;
    int length = int (token.end_ - token.start_);
    if (length <= bufferSize)
      {
	char buffer[bufferSize];
	memcpy (buffer, token.start_, length);
	buffer[length] = 0;
	count = sscanf (buffer, "%lf", &value);
      }
    else
      {
	std::string buffer (token.start_, token.end_);
	count = sscanf (buffer.c_str (), "%lf", &value);
      }

    if (count != 1)
      return addError ("'" + std::string (token.start_, token.end_) + "' is not a number.", token);
    currentValue () = value;
    return true;
  }


  bool 
  Reader::decodeString (Token &token)
  {
    std::string decoded;
    if (!decodeString (token, decoded))
      return false;
    currentValue () = decoded;
    return true;
  }


  bool 
  Reader::decodeString (Token &token, std::string &decoded)
  {
    Location current = token.start_ + 1; // skip '"'
    Location end = token.end_ - 1;       // do not include '"'
    decoded.reserve (long (end - current));

    while (current != end)
      {
	char c = *current++;
	if (expect_false (c == '"'))
	  break;
	else if (expect_false (c == '\\'))
	  {
	    if (expect_false (current == end))
	      return addError ("Empty escape sequence in string", token, current);
	    char escape = *current++;
	    switch (escape)
	      {
	      case '"':
	      case '/':
	      case '\\': decoded += escape; break;

	      case 'b': decoded += '\010'; break;
	      case 't': decoded += '\011'; break;
	      case 'n': decoded += '\012'; break;
	      case 'f': decoded += '\014'; break;
	      case 'r': decoded += '\015'; break;
	      case 'u':
		{
		  unsigned unicode;
		  if (!decodeUnicodeEscapeSequence (token, current, end, unicode))
		    return false;
		  // @todo encode unicode as utf8.
		  // @todo remember to alter the writer too.
		}
		break;
	      default:
		return addError ("Bad escape sequence in string", token, current);
	      }
	  }
	else
	  {
	    decoded += c;
	  }
      }

    return true;
  }


  bool 
  Reader::decodeUnicodeEscapeSequence (Token &token, 
				       Location &current, 
				       Location end, 
				       unsigned &unicode)
  {
    if (end - current < 4)
      return addError ("Bad unicode escape sequence in string: four digits expected.", token, current);
    unicode = 0;
    for (int index = 0; index < 4; ++index)
      {
	char c = *current++;
	unicode *= 16;
	if (c >= '0' && c <= '9')
	  unicode += c - '0';
	else if (c >= 'a' && c <= 'f')
	  unicode += c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	  unicode += c - 'A' + 10;
	else
	  return addError ("Bad unicode escape sequence in string: hexadecimal digit expected.", token, current);
      }
    return true;
  }


  bool 
  Reader::addError (const std::string &message, 
		    Token &token,
		    Location extra)
  {
    ErrorInfo info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = extra;
    errors_.push_back (info);
    return false;
  }


  bool 
  Reader::recoverFromError (TokenType skipUntilToken)
  {
    int errorCount = int (errors_.size ());
    Token skip;
    while (true)
      {
	if (!readToken (skip))
	  errors_.resize (errorCount); // discard errors caused by recovery
	if (skip.type_ == skipUntilToken  ||  skip.type_ == tokenEndOfStream)
	  break;
      }
    errors_.resize (errorCount);
    return false;
  }


  bool 
  Reader::addErrorAndRecover (const std::string &message, 
			      Token &token,
			      TokenType skipUntilToken)
  {
    addError (message, token);
    return recoverFromError (skipUntilToken);
  }


  Value &
  Reader::currentValue ()
  {
    return *(nodes_.top ());
  }


  char 
  Reader::getNextChar ()
  {
    if (current_ == end_)
      return 0;
    return *current_++;
  }


  void 
  Reader::getLocationLineAndColumn (Location location,
				    int &line,
				    int &column) const
  {
    Location current = begin_;
    Location lastLineStart = current;
    line = 0;
    while (current < location && current != end_)
      {
	char c = *current++;
	if (c == '\r')
	  {
	    if (*current == '\n')
	      ++current;
	    lastLineStart = current;
	    ++line;
	  }
	else if (c == '\n')
	  {
	    lastLineStart = current;
	    ++line;
	  }
      }
    // column & line start at 1
    column = int (location - lastLineStart) + 1;
    ++line;
  }


  std::string
  Reader::getLocationLineAndColumn (Location location) const
  {
    int line, column;
    getLocationLineAndColumn (location, line, column);
    char buffer[18+16+16+1];
    sprintf (buffer, "Line %d, Column %d", line, column);
    return buffer;
  }


  std::string 
  Reader::error_msgs () const
  {
    std::string formattedMessage;
    for (Errors::const_iterator itError = errors_.begin ();
	  itError != errors_.end ();
	  ++itError)
      {
	const ErrorInfo &error = *itError;
	formattedMessage += "* " + getLocationLineAndColumn (error.token_.start_) + "\n";
	formattedMessage += "  " + error.message_ + "\n";
	if (error.extra_)
	  formattedMessage += "See " + getLocationLineAndColumn (error.extra_) + " for detail.\n";
      }
    return formattedMessage;
  }
} // namespace json

namespace json
{
  void
  unreachable_internal (char const *file, int line, char const *function)
  {
    char buf[1024];
    snprintf (buf, 1024, "%s (%d) [%s] critical: Unreachable line reached.",
	      file, line, function);
  
    throw std::runtime_error (buf);
  }
  
  void
  throw_unless_internal (char const *file, int line, char const *function, char const *condition)
  {
    char buf[1024];
    snprintf (buf, 1024, "%s (%d) [%s] critical: Assertion `%s' failed.",
	      file, line, function, condition);
  
    throw std::runtime_error (buf);
  }
  
  void
  throw_msg_unless_internal (char const *file, int line, char const *function, char const *message)
  {
    char buf[1024];
    snprintf (buf, 1024, "%s (%d) [%s] critical: %s.",
	      file, line, function, message);
  
    throw std::runtime_error (buf);
  }

#define throw_unreachable                       unreachable_internal (__FILE__, __LINE__, CURFUNC)
#define throw_unless(condition)                 if (!expect_false (condition)) throw_unless_internal (__FILE__, __LINE__, CURFUNC, #condition)
#define throw_msg_unless(condition, message)    if (!expect_false (condition)) throw_msg_unless_internal (__FILE__, __LINE__, CURFUNC, message)
  
  const Value Value::null;
  const int Value::minInt = int (~ (unsigned (-1)/2));
  const int Value::maxInt = int (unsigned (-1)/2);
  const unsigned Value::maxUInt = unsigned (-1);

  ValueAllocator::~ValueAllocator ()
  {
  }

  class DefaultValueAllocator : public ValueAllocator
  {
  public:
    virtual ~DefaultValueAllocator ()
    {
    }

    virtual char *makeMemberName (char const *memberName)
    {
      return duplicateStringValue (memberName);
    }

    virtual void releaseMemberName (char *memberName)
    {
      releaseStringValue (memberName);
    }

    virtual char *duplicateStringValue (char const *value, unsigned length = unknown)
    {
      //@todo invesgate this old optimization
#if 0
      if (!value || value[0] == 0)
	return 0;
#endif

      if (length == unknown)
	length = (unsigned)strlen (value);
      char *newString = static_cast<char *> (malloc (length + 1));
      memcpy (newString, value, length);
      newString[length] = 0;
      return newString;
    }

    virtual void releaseStringValue (char *value)
    {
      if (value)
	free (value);
    }
  };

  static ValueAllocator *&valueAllocator ()
  {
    static DefaultValueAllocator defaultAllocator;
    static ValueAllocator *valueAllocator = &defaultAllocator;
    return valueAllocator;
  }

  static struct DummyValueAllocatorInitializer {
    DummyValueAllocatorInitializer () 
    {
      valueAllocator ();      // ensure valueAllocator () statics are initialized before main ().
    }
  } dummyValueAllocatorInitializer;



  Value::CZString::CZString (int index)
    : cstr_ (0)
    , index_ (index)
  {
  }

  Value::CZString::CZString (char const *cstr, DuplicationPolicy allocate)
    : cstr_ (allocate == duplicate ? valueAllocator()->makeMemberName (cstr) 
	     : cstr)
    , index_ (allocate)
  {
  }

  Value::CZString::CZString (const CZString &other)
    : cstr_ (other.index_ != noDuplication &&  other.cstr_ != 0
	     ?  valueAllocator()->makeMemberName (other.cstr_)
	     : other.cstr_)
    , index_ (other.cstr_ ? (other.index_ == noDuplication ? noDuplication : duplicate)
	      : other.index_)
  {
  }

  Value::CZString::~CZString ()
  {
    if (cstr_ && index_ == duplicate)
      valueAllocator()->releaseMemberName (const_cast<char *> (cstr_));
  }

  void 
  Value::CZString::swap (CZString &other)
  {
    std::swap (cstr_, other.cstr_);
    std::swap (index_, other.index_);
  }

  Value::CZString &
  Value::CZString::operator = (const CZString &other)
  {
    CZString temp (other);
    swap (temp);
    return *this;
  }

  bool 
  Value::CZString::operator < (const CZString &other) const 
  {
    if (cstr_)
      return strcmp (cstr_, other.cstr_) < 0;
    return index_ < other.index_;
  }

  bool 
  Value::CZString::operator == (const CZString &other) const 
  {
    if (cstr_)
      return strcmp (cstr_, other.cstr_) == 0;
    return index_ == other.index_;
  }


  int 
  Value::CZString::index () const
  {
    return index_;
  }


  char const *
  Value::CZString::c_str () const
  {
    return cstr_;
  }

  bool 
  Value::CZString::isStaticString () const
  {
    return index_ == noDuplication;
  }


  // class Value::Value

  Value::Value (ValueType type)
    : type_ (type)
    , allocated_ (0)
  {
    switch (type)
      {
      case nullValue:
	break;
      case intValue:
      case uintValue:
	value_.int_ = 0;
	break;
      case realValue:
	value_.real_ = 0.0;
	break;
      case stringValue:
	value_.string_ = 0;
	break;
      case arrayValue:
      case objectValue:
	value_.map_ = new ObjectValues ();
	break;
      case booleanValue:
	value_.bool_ = false;
	break;
      default:
	throw_unreachable;
      }
  }


  Value::Value (int value)
    : type_ (intValue)
  {
    value_.int_ = value;
  }


  Value::Value (unsigned value)
    : type_ (uintValue)
  {
    value_.uint_ = value;
  }

  Value::Value (double value)
    : type_ (realValue)
  {
    value_.real_ = value;
  }

  Value::Value (char const *value)
    : type_ (stringValue)
    , allocated_ (true)
  {
    value_.string_ = valueAllocator()->duplicateStringValue (value);
  }

  Value::Value (const std::string &value)
    : type_ (stringValue)
    , allocated_ (true)
  {
    value_.string_ = valueAllocator()->duplicateStringValue (value.c_str (), (unsigned)value.length ());
  }

  Value::Value (const StaticString &value)
    : type_ (stringValue)
    , allocated_ (false)
  {
    value_.string_ = const_cast<char *> (value.c_str ());
  }


  Value::Value (bool value)
    : type_ (booleanValue)
  {
    value_.bool_ = value;
  }


  Value::Value (const Value &other)
    : type_ (other.type_)
  {
    switch (type_)
      {
      case nullValue:
      case intValue:
      case uintValue:
      case realValue:
      case booleanValue:
	value_ = other.value_;
	break;
      case stringValue:
	if (other.value_.string_)
	  {
	    value_.string_ = valueAllocator()->duplicateStringValue (other.value_.string_);
	    allocated_ = true;
	  }
	else
	  value_.string_ = 0;
	break;
      case arrayValue:
      case objectValue:
	value_.map_ = new ObjectValues (*other.value_.map_);
	break;
      default:
	throw_unreachable;
      }
  }


  Value::~Value ()
  {
    switch (type_)
      {
      case nullValue:
      case intValue:
      case uintValue:
      case realValue:
      case booleanValue:
	break;
      case stringValue:
	if (allocated_)
	  valueAllocator()->releaseStringValue (value_.string_);
	break;
      case arrayValue:
      case objectValue:
	delete value_.map_;
	break;
      default:
	throw_unreachable;
      }
  }

  Value &
  Value::operator = (const Value &other)
  {
    Value temp (other);
    swap (temp);
    return *this;
  }

  void 
  Value::swap (Value &other)
  {
    ValueType temp = type_;
    type_ = other.type_;
    other.type_ = temp;
    std::swap (value_, other.value_);
    int temp2 = allocated_;
    allocated_ = other.allocated_;
    other.allocated_ = temp2;
  }

  ValueType 
  Value::type () const
  {
    return type_;
  }

  bool 
  Value::operator < (const Value &other) const
  {
    int typeDelta = type_ - other.type_;
    if (typeDelta)
      return typeDelta < 0 ? true : false;
    switch (type_)
      {
      case nullValue:
	return false;
      case intValue:
	return value_.int_ < other.value_.int_;
      case uintValue:
	return value_.uint_ < other.value_.uint_;
      case realValue:
	return value_.real_ < other.value_.real_;
      case booleanValue:
	return value_.bool_ < other.value_.bool_;
      case stringValue:
	return (value_.string_ == 0 && other.value_.string_)
	  || (other.value_.string_  
	      && value_.string_  
	      && strcmp (value_.string_, other.value_.string_) < 0);
      case arrayValue:
      case objectValue:
	{
	  int delta = int (value_.map_->size () - other.value_.map_->size ());
	  if (delta)
	    return delta < 0;
	  return (*value_.map_) < (*other.value_.map_);
	}
      default:
	throw_unreachable;
      }
    return 0;  // unreachable
  }

  bool 
  Value::operator <= (const Value &other) const
  {
    return !(other > *this);
  }

  bool 
  Value::operator >= (const Value &other) const
  {
    return !(*this < other);
  }

  bool 
  Value::operator > (const Value &other) const
  {
    return other < *this;
  }

  bool 
  Value::operator == (const Value &other) const
  {
    if (type_ != other.type_)
      return false;

    switch (type_)
      {
      case nullValue:
	return true;
      case intValue:
	return value_.int_ == other.value_.int_;
      case uintValue:
	return value_.uint_ == other.value_.uint_;
      case realValue:
	return value_.real_ == other.value_.real_;
      case booleanValue:
	return value_.bool_ == other.value_.bool_;
      case stringValue:
	return (value_.string_ == other.value_.string_)
	  || (other.value_.string_  
	      && value_.string_  
	      && strcmp (value_.string_, other.value_.string_) == 0);
      case arrayValue:
      case objectValue:
	return value_.map_->size () == other.value_.map_->size ()
	  && (*value_.map_) == (*other.value_.map_);
      default:
	throw_unreachable;
      }
    return 0;  // unreachable
  }

  bool 
  Value::operator != (const Value &other) const
  {
    return !(*this == other);
  }

  Value::operator char const * () const
  {
    throw_unless (type_ == stringValue);
    return value_.string_;
  }


  Value::operator std::string () const
  {
    switch (type_)
      {
      case nullValue:
	return "";
      case stringValue:
	return value_.string_ ? value_.string_ : "";
      case booleanValue:
	return value_.bool_ ? "true" : "false";
      case intValue:
      case uintValue:
      case realValue:
      case arrayValue:
      case objectValue:
	throw_msg_unless (false, "Type is not convertible to string");
      default:
	throw_unreachable;
      }
    return ""; // unreachable
  }

  Value::operator int () const
  {
    switch (type_)
      {
      case nullValue:
	return 0;
      case intValue:
	return value_.int_;
      case uintValue:
	throw_msg_unless (value_.uint_ < (unsigned)maxInt, "integer out of signed integer range");
	return value_.uint_;
      case realValue:
	throw_msg_unless (value_.real_ >= minInt && value_.real_ <= maxInt, "Real out of signed integer range");
	return int (value_.real_);
      case booleanValue:
	return value_.bool_ ? 1 : 0;
      case stringValue:
      case arrayValue:
      case objectValue:
	throw_msg_unless (false, "Type is not convertible to int");
      default:
	throw_unreachable;
      }
    return 0; // unreachable;
  }

  Value::operator unsigned () const
  {
    switch (type_)
      {
      case nullValue:
	return 0;
      case intValue:
	throw_msg_unless (value_.int_ >= 0, "Negative integer can not be converted to unsigned integer");
	return value_.int_;
      case uintValue:
	return value_.uint_;
      case realValue:
	throw_msg_unless (value_.real_ >= 0 && value_.real_ <= maxUInt,  "Real out of unsigned integer range");
	return unsigned (value_.real_);
      case booleanValue:
	return value_.bool_ ? 1 : 0;
      case stringValue:
      case arrayValue:
      case objectValue:
	throw_msg_unless (false, "Type is not convertible to uint");
      default:
	throw_unreachable;
      }
    return 0; // unreachable;
  }

  Value::operator double () const
  {
    switch (type_)
      {
      case nullValue:
	return 0.0;
      case intValue:
	return value_.int_;
      case uintValue:
	return value_.uint_;
      case realValue:
	return value_.real_;
      case booleanValue:
	return value_.bool_ ? 1.0 : 0.0;
      case stringValue:
      case arrayValue:
      case objectValue:
	throw_msg_unless (false, "Type is not convertible to double");
      default:
	throw_unreachable;
      }
    return 0; // unreachable;
  }

  Value::operator bool () const
  {
    switch (type_)
      {
      case nullValue:
	return false;
      case intValue:
      case uintValue:
	return value_.int_ != 0;
      case realValue:
	return value_.real_ != 0.0;
      case booleanValue:
	return value_.bool_;
      case stringValue:
	return value_.string_ && value_.string_[0] != 0;
      case arrayValue:
      case objectValue:
	return value_.map_->size () != 0;
      default:
	throw_unreachable;
      }
    return false; // unreachable;
  }


  bool 
  Value::isConvertibleTo (ValueType other) const
  {
    switch (type_)
      {
      case nullValue:
	return true;
      case intValue:
	return (other == nullValue && value_.int_ == 0)
	  || other == intValue
	  || (other == uintValue  && value_.int_ >= 0)
	  || other == realValue
	  || other == stringValue
	  || other == booleanValue;
      case uintValue:
	return (other == nullValue && value_.uint_ == 0)
	  || (other == intValue  && value_.uint_ <= (unsigned)maxInt)
	  || other == uintValue
	  || other == realValue
	  || other == stringValue
	  || other == booleanValue;
      case realValue:
	return (other == nullValue && value_.real_ == 0.0)
	  || (other == intValue && value_.real_ >= minInt && value_.real_ <= maxInt)
	  || (other == uintValue && value_.real_ >= 0 && value_.real_ <= maxUInt)
	  || other == realValue
	  || other == stringValue
	  || other == booleanValue;
      case booleanValue:
	return (other == nullValue && value_.bool_ == false)
	  || other == intValue
	  || other == uintValue
	  || other == realValue
	  || other == stringValue
	  || other == booleanValue;
      case stringValue:
	return other == stringValue
	  || (other == nullValue && (!value_.string_ || value_.string_[0] == 0));
      case arrayValue:
	return other == arrayValue
	  || (other == nullValue && value_.map_->size () == 0);
      case objectValue:
	return other == objectValue
	  || (other == nullValue && value_.map_->size () == 0);
      default:
	throw_unreachable;
      }
    return false; // unreachable;
  }


  /// Number of values in array or object
  unsigned 
  Value::size () const
  {
    switch (type_)
      {
      case nullValue:
      case intValue:
      case uintValue:
      case realValue:
      case booleanValue:
      case stringValue:
	return 0;
      case arrayValue:  // size of the array is highest index + 1
	if (!value_.map_->empty ())
	  {
	    ObjectValues::const_iterator itLast = value_.map_->end ();
	    --itLast;
	    return itLast->first.index ()+1;
	  }
	return 0;
      case objectValue:
	return int (value_.map_->size ());
      default:
	throw_unreachable;
      }
    return 0; // unreachable;
  }


  bool 
  Value::empty () const
  {
    if (isNull () || isArray () || isObject ())
      return size () == 0u;
    else
      return false;
  }


  bool
  Value::operator ! () const
  {
    return isNull ();
  }


  void 
  Value::clear ()
  {
    throw_unless (type_ == nullValue || type_ == arrayValue  || type_ == objectValue);

    switch (type_)
      {
      case arrayValue:
      case objectValue:
	value_.map_->clear ();
	break;
      default:
	break;
      }
  }

  void 
  Value::resize (unsigned newSize)
  {
    throw_unless (type_ == nullValue || type_ == arrayValue);
    if (type_ == nullValue)
      *this = Value (arrayValue);
    unsigned oldSize = size ();
    if (newSize == 0)
      clear ();
    else if (newSize > oldSize)
      (*this)[ newSize - 1 ];
    else
      {
	for (unsigned index = newSize; index < oldSize; ++index)
	  value_.map_->erase (index);
	throw_unless (size () == newSize);
      }
  }


  Value &
  Value::operator [] (int index)
  {
    return operator [] (static_cast<unsigned> (index));
  }


  Value &
  Value::operator [] (unsigned index)
  {
    throw_unless (type_ == nullValue || type_ == arrayValue);
    if (type_ == nullValue)
      *this = Value (arrayValue);
    CZString key (index);
    ObjectValues::iterator it = value_.map_->lower_bound (key);
    if (it != value_.map_->end () && it->first == key)
      return it->second;

    ObjectValues::value_type defaultValue (key, null);
    it = value_.map_->insert (it, defaultValue);
    return it->second;
  }


  const Value &
  Value::operator [] (int index) const
  {
    return operator [] (static_cast<unsigned> (index));
  }


  const Value &
  Value::operator [] (unsigned index) const
  {
    throw_unless (type_ == nullValue || type_ == arrayValue);
    if (type_ == nullValue)
      return null;
    CZString key (index);
    ObjectValues::const_iterator it = value_.map_->find (key);
    if (it == value_.map_->end ())
      return null;
    return it->second;
  }


  Value &
  Value::operator [] (char const *key)
  {
    return resolveReference (key, false);
  }


  Value &
  Value::resolveReference (char const *key, bool isStatic)
  {
    throw_unless (type_ == nullValue || type_ == objectValue);
    if (type_ == nullValue)
      *this = Value (objectValue);
    CZString actualKey (key, isStatic ? CZString::noDuplication 
			: CZString::duplicateOnCopy);
    ObjectValues::iterator it = value_.map_->lower_bound (actualKey);
    if (it != value_.map_->end () && it->first == actualKey)
      return it->second;

    ObjectValues::value_type defaultValue (actualKey, null);
    it = value_.map_->insert (it, defaultValue);
    Value &value = it->second;
    return value;
  }


  Value 
  Value::get (int index, const Value &defaultValue) const
  {
    return get (static_cast<unsigned> (index), defaultValue);
  }


  Value 
  Value::get (unsigned index, const Value &defaultValue) const
  {
    const Value *value = &((*this)[index]);
    return value == &null ? defaultValue : *value;
  }


  bool 
  Value::isValidIndex (int index) const
  {
    return isValidIndex (static_cast<unsigned> (index));
  }


  bool 
  Value::isValidIndex (unsigned index) const
  {
    return index < size ();
  }



  const Value &
  Value::operator [] (char const *key) const
  {
    throw_unless (type_ == nullValue || type_ == objectValue);
    if (type_ == nullValue)
      return null;
    CZString actualKey (key, CZString::noDuplication);
    ObjectValues::const_iterator it = value_.map_->find (actualKey);
    if (it == value_.map_->end ())
      return null;
    return it->second;
  }


  Value &
  Value::operator [] (const std::string &key)
  {
    return (*this)[ key.c_str () ];
  }


  const Value &
  Value::operator [] (const std::string &key) const
  {
    return (*this)[ key.c_str () ];
  }

  Value &
  Value::operator [] (const StaticString &key)
  {
    return resolveReference (key, true);
  }


  Value &
  Value::append (const Value &value)
  {
    return (*this)[size ()] = value;
  }


  Value 
  Value::get (char const *key, const Value &defaultValue) const
  {
    const Value *value = &((*this)[key]);
    return value == &null ? defaultValue : *value;
  }


  Value 
  Value::get (const std::string &key, const Value &defaultValue) const
  {
    return get (key.c_str (), defaultValue);
  }

  Value
  Value::removeMember (char const *key)
  {
    throw_unless (type_ == nullValue || type_ == objectValue);
    if (type_ == nullValue)
      return null;
    CZString actualKey (key, CZString::noDuplication);
    ObjectValues::iterator it = value_.map_->find (actualKey);
    if (it == value_.map_->end ())
      return null;
    Value old (it->second);
    value_.map_->erase (it);
    return old;
  }

  Value
  Value::removeMember (const std::string &key)
  {
    return removeMember (key.c_str ());
  }

  bool 
  Value::isMember (char const *key) const
  {
    const Value *value = &((*this)[key]);
    return value != &null;
  }


  bool 
  Value::isMember (const std::string &key) const
  {
    return isMember (key.c_str ());
  }


  Value::Members 
  Value::getMemberNames () const
  {
    throw_unless (type_ == nullValue || type_ == objectValue);
    if (type_ == nullValue)
      return Value::Members ();
    Members members;
    members.reserve (value_.map_->size ());
    ObjectValues::const_iterator it;
    ObjectValues::const_iterator itEnd = value_.map_->end ();
    for (it = value_.map_->begin (); it != itEnd; ++it)
      members.push_back (std::string (it->first.c_str()));
    return members;
  }

  bool
  Value::isNull () const
  {
    return type_ == nullValue;
  }


  bool 
  Value::isBool () const
  {
    return type_ == booleanValue;
  }


  bool 
  Value::isInt () const
  {
    return type_ == intValue;
  }


  bool 
  Value::isUInt () const
  {
    return type_ == uintValue;
  }


  bool 
  Value::isIntegral () const
  {
    return type_ == intValue  
      || type_ == uintValue  
      || type_ == booleanValue;
  }


  bool 
  Value::isDouble () const
  {
    return type_ == realValue;
  }


  bool 
  Value::isNumeric () const
  {
    return isIntegral () || isDouble ();
  }


  bool 
  Value::isString () const
  {
    return type_ == stringValue;
  }


  bool 
  Value::isArray () const
  {
    return type_ == nullValue || type_ == arrayValue;
  }


  bool 
  Value::isObject () const
  {
    return type_ == nullValue || type_ == objectValue;
  }


  Value::const_iterator 
  Value::begin () const
  {
    switch (type_)
      {
      case arrayValue:
      case objectValue:
	if (value_.map_)
	  return const_iterator (value_.map_->begin ());
	break;
      default:
	break;
      }
    return const_iterator ();
  }

  Value::const_iterator 
  Value::end () const
  {
    switch (type_)
      {
      case arrayValue:
      case objectValue:
	if (value_.map_)
	  return const_iterator (value_.map_->end ());
	break;
      default:
	break;
      }
    return const_iterator ();
  }


  Value::iterator 
  Value::begin ()
  {
    switch (type_)
      {
      case arrayValue:
      case objectValue:
	if (value_.map_)
	  return iterator (value_.map_->begin ());
	break;
      default:
	break;
      }
    return iterator ();
  }

  Value::iterator 
  Value::end ()
  {
    switch (type_)
      {
      case arrayValue:
      case objectValue:
	if (value_.map_)
	  return iterator (value_.map_->end ());
	break;
      default:
	break;
      }
    return iterator ();
  }
} // namespace json

namespace json
{
  static void uintToString (unsigned value, 
			    char *&current)
  {
    *--current = 0;
    do
      {
	*--current = (value % 10) + '0';
	value /= 10;
      }
    while (value != 0);
  }

  std::string valueToString (int value)
  {
    char buffer[32];
    char *current = buffer + sizeof (buffer);
    bool isNegative = value < 0;
    if (isNegative)
      value = -value;
    uintToString (unsigned (value), current);
    if (isNegative)
      *--current = '-';
    throw_unless (current >= buffer);
    return current;
  }


  std::string valueToString (unsigned value)
  {
    char buffer[32];
    char *current = buffer + sizeof (buffer);
    uintToString (value, current);
    throw_unless (current >= buffer);
    return current;
  }

  std::string valueToString (double value)
  {
    char buffer[32];
    sprintf (buffer, "%.16g", value); 
    return buffer;
  }


  std::string valueToString (bool value)
  {
    return value ? "true" : "false";
  }

  std::string valueToQuotedString (char const *value)
  {
    // Not sure how to handle unicode...
    if (std::strpbrk (value, "\"\\\b\f\n\r\t") == NULL)
      return std::string ("\"") + value + "\"";
    // We have to walk value and escape any special characters.
    // Appending to std::string is not efficient, but this should be rare.
    // (Note: forward slashes are *not* rare, but I am not escaping them.)
    unsigned maxsize = strlen (value) * 2 + 3; // allescaped+quotes+NULL
    std::string result;
    result.reserve (maxsize); // to avoid lots of mallocs
    result += "\"";
    for (char const* c=value; *c != 0; ++c){
      switch (*c){
      case '\"':
	result += "\\\"";
	break;
      case '\\':
	result += "\\\\";
	break;
      case '\b':
	result += "\\b";
	break;
      case '\f':
	result += "\\f";
	break;
      case '\n':
	result += "\\n";
	break;
      case '\r':
	result += "\\r";
	break;
      case '\t':
	result += "\\t";
	break;
      case '/':
	// Even though \/ is considered a legal escape in JSON, a bare
	// slash is also legal, so I see no reason to escape it.
	// (I hope I am not misunderstanding something.)
      default:
	result += *c;
      }
    }
    result += "\"";
    return result;
  }

  // Class Writer
  std::string 
  Writer::write (const Value &root)
  {
    document_ = "";
    writeValue (root);
    document_ += "\n";
    return document_;
  }


  void 
  Writer::writeValue (const Value &value)
  {
    switch (value.type ())
      {
      case nullValue:
	document_ += "null";
	break;
      case intValue:
	document_ += valueToString (static_cast<int> (value));
	break;
      case uintValue:
	document_ += valueToString (static_cast<unsigned> (value));
	break;
      case realValue:
	document_ += valueToString (static_cast<double> (value));
	break;
      case stringValue:
	document_ += valueToQuotedString (static_cast<char const *> (value));
	break;
      case booleanValue:
	document_ += valueToString (static_cast<bool> (value));
	break;
      case arrayValue:
	{
	  document_ += "[";
	  int size = value.size ();
	  for (int index = 0; index < size; ++index)
	    {
	      if (index > 0)
		document_ += ",";
	      writeValue (value[index]);
	    }
	  document_ += "]";
	}
	break;
      case objectValue:
	{
	  Value::Members members (value.getMemberNames ());
	  document_ += "{";
	  for (Value::Members::iterator it = members.begin (); 
		it != members.end (); 
		++it)
	    {
	      const std::string &name = *it;
	      if (it != members.begin ())
		document_ += ",";
	      document_ += valueToQuotedString (name.c_str ());
	      document_ += ":";
	      writeValue (value[name]);
	    }
	  document_ += "}";
	}
	break;
      }
  }
} // namespace json

/**
 * RPC
 */

namespace json
{
  namespace rpc
  {
    method_map methods;
  
    void
    add_method (char *name, Module const *mod, method mth)
    {
      mfp m = { mod, mth };
      methods[name] = m;
    }
  
    void
    service (HTTPRequest *http, Value &request, Value &response)
    {
      char const *methodName = static_cast<char const *> (request["method"]);
      
      method_map::iterator mthit = methods.find (methodName);
      if (mthit != methods.end ())
	{
	  mfp m = mthit->second;
	  Module *mod = new Module (*m.mod);
	  method mth = m.mth;
	  (mod->*mth) (http, request, response);
	  delete mod;
	}
    }
    
    void
    process (HTTPRequest *http, std::string &response_text, char const *request_text)
    {
      std::string text;
      bool parse_success;
      Value request (objectValue);
      Value response (objectValue);
      Reader r;
      Writer w;
  
      parse_success = r.parse (request_text, request_text + strlen (request_text), request);
  
      service (http, request, response);
  
      text = w.write (response);
  
      response_text = text.c_str ();
  
      return;
    }
  } // namespace rpc
} // namespace json

MODULE_INIT(ModuleRpcJson)
