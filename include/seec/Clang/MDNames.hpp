//===- MDNames.hpp - Metadata names used by seec-clang --------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MDNAMES_HPP
#define SEEC_CLANG_MDNAMES_HPP

namespace seec {

namespace seec_clang {

char const * const MDStmtPtrStr = "seec.clang.stmt.ptr";
char const * const MDStmtIdxStr = "seec.clang.stmt.idx";
char const * const MDDeclPtrStr = "clang.decl.ptr";
char const * const MDDeclIdxStr = "seec.clang.decl.idx";

char const * const MDGlobalDeclPtrsStr = "clang.global.decl.ptrs";
char const * const MDGlobalDeclIdxsStr = "seec.clang.global.decl.idxs";

char const * const MDSources = "seec.clang.sources";

}

}

#endif // SEEC_CLANG_MDNAMES_HPP
