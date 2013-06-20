//===- include/seec/wxWidgets/CallbackFSHandler.hpp ----------------- C++ -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_WXWIDGETS_CALLBACKFSHANDLER_HPP
#define SEEC_WXWIDGETS_CALLBACKFSHANDLER_HPP

#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/TemplateSequence.hpp"

#include "llvm/Support/raw_ostream.h"

#include <wx/filesys.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace seec {

/// Contains implementation details for CallbackFSHandler
///
namespace callbackfs {


//===----------------------------------------------------------------------===//
// Standard argument parsing
//===----------------------------------------------------------------------===//

template<typename>
struct ParseImpl; // Undefined.

template<> struct ParseImpl<int> {
  static int impl(std::string const &Arg) { return std::stoi(Arg); }
};

template<> struct ParseImpl<unsigned> {
  static unsigned impl(std::string const &Arg) {
    return static_cast<unsigned>(std::stoul(Arg));
  }
};

template<> struct ParseImpl<long> {
  static long impl(std::string const &Arg) { return std::stol(Arg); }
};

template<> struct ParseImpl<long long> {
  static long long impl(std::string const &Arg) { return std::stoll(Arg); }
};

template<> struct ParseImpl<unsigned long> {
  static unsigned long impl(std::string const &Arg) { return std::stoul(Arg); }
};

template<> struct ParseImpl<unsigned long long> {
  static unsigned long long impl(std::string const &Arg) {
    return std::stoull(Arg);
  }
};

template<> struct ParseImpl<float> {
  static float impl(std::string const &Arg) { return std::stof(Arg); }
};

template<> struct ParseImpl<double> {
  static double impl(std::string const &Arg) { return std::stod(Arg); }
};

template<> struct ParseImpl<long double> {
  static long double impl(std::string const &Arg) { return std::stold(Arg); }
};


//===----------------------------------------------------------------------===//
// Standard response formatting
//===----------------------------------------------------------------------===//

template<typename, typename Enable = void>
struct FormatImpl; // Undefined.

template<typename T>
struct FormatImpl
<T, typename std::enable_if<std::is_integral<T>::value>::type>
{
  static void impl(llvm::raw_ostream &Out, T const Result) {
    Out << Result;
  }
};

template<typename T>
struct FormatImpl
<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
{
  static void impl(llvm::raw_ostream &Out, T const Result) {
    Out << Result;
  }
};

template<>
struct FormatImpl<std::string>
{
  static void impl(llvm::raw_ostream &Out, std::string const &Result) {
    Out << '"';
    
    for (auto const Char : Result) {
      switch (Char) {
        // Characters that must be escaped:
        case '"':  SEEC_FALLTHROUGH;
        case '\\': SEEC_FALLTHROUGH;
        case '/':
          Out << '\\';
          break;
        
        // Characters that need special treatment:
        case '\b': Out << "\b"; continue;
        case '\f': Out << "\f"; continue;
        case '\n': Out << "\n"; continue;
        case '\r': Out << "\r"; continue;
        case '\t': Out << "\t"; continue;
        
        // Otherwise output the character as normal.
        default:
          break;
      }
      
      Out << Char;
    }
    
    Out << '"';
  }
};


//===----------------------------------------------------------------------===//
// CallbackBase
//===----------------------------------------------------------------------===//

class CallbackBase
{
  std::size_t ArgCount;
  
  virtual std::string impl(std::vector<std::string> const &Args) const =0;
  
protected:
  CallbackBase(std::size_t WithArgCount)
  : ArgCount{WithArgCount}
  {}
  
public:
  virtual ~CallbackBase();
  
  std::string call(wxString const &Right) const;
};


//===----------------------------------------------------------------------===//
// CallbackImpl
//===----------------------------------------------------------------------===//

template<typename ResultT, typename... ArgTs>
class CallbackImpl final : public CallbackBase
{
  // TODO: static_assert that the Result is formattable.
  // TODO: static_assert that the Args are parsable.
  
  typedef typename seec::ct::generate_sequence_int<0, sizeof...(ArgTs)>::type
          IndexSeqTy;
  
  std::function<ResultT (ArgTs...)> CallbackFn;
  
  template<int... ArgIs>
  std::string dispatch(std::vector<std::string> const &Args,
                       seec::ct::sequence_int<ArgIs...>) const
  {
    std::string Result;
    llvm::raw_string_ostream ResultStream(Result);
    
    FormatImpl<typename std::remove_reference<ResultT>::type>
      ::impl(ResultStream, CallbackFn(ParseImpl<ArgTs>::impl(Args[ArgIs])...));
    
    ResultStream.flush();
    return Result;
  }
  
  ///
  /// pre: Args.size() == sizeof...(ArgTs)
  ///
  virtual std::string impl(std::vector<std::string> const &Args) const override
  {
    return dispatch(Args, IndexSeqTy{});
  }
  
public:
  /// \brief Constructor.
  ///
  CallbackImpl(std::function<ResultT (ArgTs...)> WithCallback)
  : CallbackBase(sizeof...(ArgTs)),
    CallbackFn(std::move(WithCallback))
  {}
};


} // namespace callbackfs (in seec)


//===----------------------------------------------------------------------===//
// CallbackFSHandler
//===----------------------------------------------------------------------===//

/// \brief A virtual file system that gets file contents by invoking callback
///        functions.
///
/// After creation, a number of callback functions can be registered using the
/// addCallback() methods. Each callback has a string identifier, which must be
/// unique. When a file is requested from the handler, the location is split by
/// the first occurence of '/'. For example if the following file was requested:
///   protocol:add/5/7
///
/// then the location would be split into "add" and "5/7". The first fragment
/// is used to identify the callback. If there is no registered callback with an
/// identifier that matches this fragment, then the file lookup will fail. If
/// there is a registered callback, then it will be called using the remainder
/// of the location as the argument,
/// using seec::callbackfs::CallbackBase::call().
///
/// CallbackBase will again split this location using '/' as a delimiter. The
/// resulting vector of fragments will be used as the argument to the internal
/// implementation virtual method seec::callbackfs::CallbackBase::impl().
///
/// When adding a callback by passing a std::function to addCallback(), the
/// template class seec::callbackfs::CallbackImpl is instantiated for this
/// function type. This template class does the following:
///  - Ensure that the size of the argument vector matches the number of
///    arguments required by the callback's std::function.
///  - Convert each argument's string into the type expected by the callback's
///    std::function, using seec::callbackfs::ParseImpl<T>::impl(), where T is
///    the type of the argument.
///  - Call the callback's std::function using the converted arguments.
///  - Create a std::string representation of the result using
///    seec::callbackfs::formatAsJSON().
/// 
/// After this is complete, seec::callbackfs::CallbackBase::call() will wrap
/// the resulting JSON string like so:
///   "{ success: true, result: <result> }"
/// and this will be returned as the file contents.
///
/// Finally, this handler supports JSONP style requests. If a query string is
/// found in the location then it will be removed (it will not be part of the
/// location that is sent to CallbackBase::call()). If the query string contains
/// a key=value pair for the key "callback", then the final file will be:
///   "query-callback-value(result-json)"
///
/// e.g. if the previous example's request was "protocol:add/5/7?callback=foo"
/// then the returned file would contain:
///   "foo({ success: true, result: <result> })"
///
class CallbackFSHandler final : public wxFileSystemHandler
{
  /// The protocol to be server by this handler.
  wxString Protocol;
  
  /// A map from identifiers to callbacks.
  std::map<wxString, std::unique_ptr<seec::callbackfs::CallbackBase>> Callbacks;
  
public:
  /// \brief Constructor.
  /// \param ForProtocol the protocol that this handler will service.
  ///
  CallbackFSHandler(wxString const &ForProtocol)
  : Protocol(ForProtocol)
  {}
  
  /// \brief Add a new callback.
  ///
  bool addCallback(wxString const &Identifier,
                   std::unique_ptr<seec::callbackfs::CallbackBase> Callback);
  
  /// \brief Add a new callback by wrapping a std::function.
  ///
  template<typename ResultT, typename... ArgTs>
  bool addCallback(wxString const &Identifier,
                   std::function<ResultT (ArgTs...)> Callback)
  {
    typedef seec::callbackfs::CallbackImpl<ResultT, ArgTs...> ImplTy;
    
    return addCallback(Identifier,
                       seec::makeUnique<ImplTy>(std::move(Callback)));
  }
  
  /// \brief Check if this handler can open a file location.
  ///
  virtual bool CanOpen(wxString const &Location) override;
  
  /// \brief Open the file at a given location.
  ///
  virtual wxFSFile *
  OpenFile(wxFileSystem &Parent, wxString const &Location) override;
};


} // namespace seec

#endif // SEEC_WXWIDGETS_ICUBUNDLEFSHANDLER_HPP
