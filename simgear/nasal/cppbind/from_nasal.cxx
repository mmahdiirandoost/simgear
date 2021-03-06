// Conversion functions to convert Nasal types to C++ types
//
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

#include "from_nasal_detail.hxx"
#include "NasalHash.hxx"

#include <simgear/misc/sg_path.hxx>

namespace nasal
{
  //----------------------------------------------------------------------------
  bad_nasal_cast::bad_nasal_cast()
  {

  }

  //----------------------------------------------------------------------------
  bad_nasal_cast::bad_nasal_cast(const std::string& msg):
   _msg( msg )
  {

  }

  //----------------------------------------------------------------------------
  bad_nasal_cast::~bad_nasal_cast() throw()
  {

  }

  //----------------------------------------------------------------------------
  const char* bad_nasal_cast::what() const throw()
  {
    return _msg.empty() ? bad_cast::what() : _msg.c_str();
  }

  //----------------------------------------------------------------------------
  std::string from_nasal_helper(naContext c, naRef ref, std::string*)
  {
    naRef na_str = naStringValue(c, ref);
    return std::string(naStr_data(na_str), naStr_len(na_str));
  }

  SGPath from_nasal_helper(naContext c, naRef ref, SGPath*)
  {
      naRef na_str = naStringValue(c, ref);
      return SGPath(std::string(naStr_data(na_str), naStr_len(na_str)));
  }

  //----------------------------------------------------------------------------
  Hash from_nasal_helper(naContext c, naRef ref, Hash*)
  {
    if( !naIsHash(ref) )
      throw bad_nasal_cast("Not a hash");

    return Hash(ref, c);
  }

} // namespace nasal
