//===- include/seec/Clang/DotGraph.hpp ------------------------------------===//
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

#ifndef SEEC_CLANG_DOTGRAPH_HPP
#define SEEC_CLANG_DOTGRAPH_HPP


namespace llvm {
  class raw_ostream;
}

namespace seec {

namespace cm {


class ProcessState;


/// \brief Write a graph in dot format describing a complete ProcessState.
///
void writeDotGraph(seec::cm::ProcessState const &State,
                   llvm::raw_ostream &Stream);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_DOTGRAPH_HPP
