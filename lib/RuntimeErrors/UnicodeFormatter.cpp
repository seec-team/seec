#include "seec/ICU/Output.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Util/Printing.hpp"

#include "llvm/Support/raw_ostream.h"

#include "unicode/fmtable.h"
#include "unicode/unistr.h"
#include "unicode/msgfmt.h"
#include "unicode/locid.h"

#include <string>

namespace seec {

namespace runtime_errors {
  
Formattable formatArg(ArgAddress const &A) {
  // TODO: convert address to string
  std::string AddressString;
  
  llvm::raw_string_ostream AddressStringStream(AddressString);
  seec::util::write_hex_padded(AddressStringStream, A.address());
  AddressStringStream.flush();
  
  return Formattable(AddressString.c_str());
}

Formattable formatArg(ArgObject const &A) {
  return Formattable();
}

Formattable formatArg(ArgSelectBase const &A) {
  return Formattable(getCString(A.getSelectID(), A.getRawItemValue()));
}

Formattable formatArg(ArgSize const &A) {
  // TODO: convert size to string
  return Formattable(static_cast<int64_t>(A.size()));
}
  
Formattable formatArg(Arg const &A) {
  switch (A.type()) {
    case ArgType::Address:
      return formatArg(static_cast<ArgAddress const &>(A));
    case ArgType::Object:
      return formatArg(static_cast<ArgObject const &>(A));
    case ArgType::Select:
      return formatArg(static_cast<ArgSelectBase const &>(A));
    case ArgType::Size:
      return formatArg(static_cast<ArgSize const &>(A));
  }
  
  return Formattable();
}

UnicodeString format(RunError const &RunErr) {
  UnicodeString Result;
  
  UErrorCode Status = U_ZERO_ERROR;
  
  // get the entire RuntimeErrors bundle
  auto Resources = getResourceBundle("RuntimeErrors", Locale::getDefault());
  if (!Resources)
    return Result;
  
  // get descriptions
  auto Descriptions = Resources->get("descriptions", Status);
  assert(U_SUCCESS(Status) && "Couldn't get Descriptions.");
  
  // get error description
  auto FormatString = Descriptions.getStringEx(describe(RunErr.type()), Status);
  if (!U_SUCCESS(Status)) {
    llvm::errs() << "Couldn't get FormatString for " << describe(RunErr.type())
                 << ", Status = " << u_errorName(Status) << "\n";
    exit(EXIT_FAILURE);
  }
  
  // For every argument, create a Formattable object and a name string.
  std::vector<UnicodeString> ArgumentNames;
  std::vector<Formattable> Arguments;
  
  for (auto &Arg: RunErr.args()) {
    // Get the name for this argument.
    auto Name = getArgumentName(RunErr.type(), ArgumentNames.size());
    
    ArgumentNames.push_back(UnicodeString::fromUTF8(Name));
    Arguments.push_back(formatArg(*Arg));
  }
  
  MessageFormat Formatter(FormatString, Status);
  assert(U_SUCCESS(Status) && "Couldn't create MessageFormat object.");
  
  Formatter.format(ArgumentNames.data(),
                   Arguments.data(),
                   Arguments.size(),
                   Result,
                   Status);
  assert(U_SUCCESS(Status) && "MessageFormat::format failed.");
  
  return Result;
}

} // namespace runtime_errors (in seec)

} // namespace seec
