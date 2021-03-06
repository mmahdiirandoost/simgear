///@file
/// Conversion functions to convert Nasal types to C++ types
///
// Copyright (C) 2012  Thomas Geymayer <tomgey@gmail.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA

#ifndef SG_FROM_NASAL_HXX_
#define SG_FROM_NASAL_HXX_

#include "from_nasal_detail.hxx"

namespace nasal
{

  /**
   * Convert a Nasal type to any supported C++ type.
   *
   * @param c   Active Nasal context
   * @param ref Nasal object to be converted
   * @tparam T  Target type of conversion
   *
   * @throws bad_nasal_cast if conversion is not possible
   *
   * @note  Every type which should be supported needs a function with the
   *        following signature declared:
   *
   *        Type from_nasal_helper(naContext, naRef, Type*)
   */
  template<class T>
  T from_nasal(naContext c, naRef ref)
  {
    return from_nasal_helper(c, ref, static_cast<T*>(0));
  }

} // namespace nasal

#endif /* SG_FROM_NASAL_HXX_ */
