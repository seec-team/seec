//===- MDNames.hpp - Metadata names used by seec-clang --------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MDNAMES_HPP
#define SEEC_CLANG_MDNAMES_HPP

namespace seec {

namespace seec_clang {

char const * const MDStmtPtrStr = "seec.clang.stmt.ptr";
char const * const MDStmtIdxStr = "seec.clang.stmt.idx";
char const * const MDDeclPtrStrClang = "clang.decl.ptr";
char const * const MDDeclPtrStr = "seec.clang.decl.ptr";
char const * const MDDeclIdxStr = "seec.clang.decl.idx";

char const * const MDStmtCompletionPtrsStr = "seec.clang.stmt.completion.ptrs";
char const * const MDStmtCompletionIdxsStr = "seec.clang.stmt.completion.idxs";
char const * const MDDeclCompletionPtrsStr = "seec.clang.decl.completion.ptrs";
char const * const MDDeclCompletionIdxsStr = "seec.clang.decl.completion.idxs";

char const * const MDGlobalDeclPtrsStr = "clang.global.decl.ptrs";
char const * const MDGlobalDeclIdxsStr = "seec.clang.global.decl.idxs";

char const * const MDGlobalValueMapStr = "seec.clang.map.stmt.idxs";
char const * const MDGlobalParamMapStr = "seec.clang.map.param.idxs";
char const * const MDGlobalLocalMapStr = "seec.clang.map.local.idxs";

char const * const MDCompileInfo = "seec.clang.compile.info";

}

}

#endif // SEEC_CLANG_MDNAMES_HPP
