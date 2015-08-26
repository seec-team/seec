//===- include/seec/wxWidgets/XmlNodeIterator.hpp ------------------- C++ -===//
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

#ifndef SEEC_WXWIDGETS_XMLNODEITERATOR_HPP
#define SEEC_WXWIDGETS_XMLNODEITERATOR_HPP

#include <wx/xml/xml.h>

namespace seec {

//------------------------------------------------------------------------------
// Make wxXmlNode work as an STL iterator.
//------------------------------------------------------------------------------

class wxXmlNodeIterator
: public std::iterator<std::forward_iterator_tag, wxXmlNode>
{
  wxXmlNode *m_NodePtr;

public:
  wxXmlNodeIterator()
  : m_NodePtr(nullptr)
  {}

  wxXmlNodeIterator(wxXmlNode *NodePtr)
  : m_NodePtr(NodePtr)
  {}

  bool operator==(wxXmlNodeIterator const &RHS) const {
    return m_NodePtr == RHS.m_NodePtr;
  }

  bool operator!=(wxXmlNodeIterator const &RHS) const {
    return m_NodePtr != RHS.m_NodePtr;
  }

  wxXmlNodeIterator &operator++() {
    if (m_NodePtr)
      m_NodePtr = m_NodePtr->GetNext();
    return *this;
  }

  wxXmlNode &operator*() const { return *m_NodePtr; }

  wxXmlNode *operator->() const { return m_NodePtr; }
};

} // namespace seec

inline seec::wxXmlNodeIterator begin(wxXmlNode &Node)
{
  return seec::wxXmlNodeIterator(Node.GetChildren());
}

inline seec::wxXmlNodeIterator end(wxXmlNode &Node)
{
  return seec::wxXmlNodeIterator();
}

#endif // SEEC_WXWIDGETS_XMLNODEITERATOR_HPP
