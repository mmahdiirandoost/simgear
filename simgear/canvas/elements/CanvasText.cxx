// A text on the Canvas
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

#include "CanvasText.hxx"
#include <simgear/canvas/Canvas.hxx>
#include <simgear/canvas/CanvasSystemAdapter.hxx>
#include <simgear/scene/util/parse_color.hxx>
#include <osgText/Text>

namespace simgear
{
namespace canvas
{
  class Text::TextOSG:
    public osgText::Text
  {
    public:

      TextOSG(canvas::Text* text);

      void setCharacterAspect(float aspect);
      void setFill(const std::string& fill);
      void setBackgroundColor(const std::string& fill);

      osg::Vec2 handleHit(const osg::Vec2f& pos);

      virtual osg::BoundingBox computeBound() const;

    protected:

      canvas::Text *_text_element;
  };

  //----------------------------------------------------------------------------
  Text::TextOSG::TextOSG(canvas::Text* text):
    _text_element(text)
  {

  }

  //----------------------------------------------------------------------------
  void Text::TextOSG::setCharacterAspect(float aspect)
  {
    setCharacterSize(getCharacterHeight(), aspect);
  }

  //----------------------------------------------------------------------------
  void Text::TextOSG::setFill(const std::string& fill)
  {
//    if( fill == "none" )
//      TODO No text
//    else
    osg::Vec4 color;
    if( parseColor(fill, color) )
      setColor( color );
  }

  //----------------------------------------------------------------------------
  void Text::TextOSG::setBackgroundColor(const std::string& fill)
  {
    osg::Vec4 color;
    if( parseColor(fill, color) )
      setBoundingBoxColor( color );
  }

  //----------------------------------------------------------------------------
  osg::Vec2 Text::TextOSG::handleHit(const osg::Vec2f& pos)
  {
    float line_height = _characterHeight + _lineSpacing;

    // TODO check with align other than TOP
    float first_line_y = -0.5 * _lineSpacing;//_offset.y() - _characterHeight;
    size_t line = std::max<int>(0, (pos.y() - first_line_y) / line_height);

    if( _textureGlyphQuadMap.empty() )
      return osg::Vec2(-1, -1);

    // TODO check when it can be larger
    assert( _textureGlyphQuadMap.size() == 1 );

    const GlyphQuads& glyphquad = _textureGlyphQuadMap.begin()->second;
    const GlyphQuads::Glyphs& glyphs = glyphquad._glyphs;
    const GlyphQuads::Coords2& coords = glyphquad._coords;
    const GlyphQuads::LineNumbers& line_numbers = glyphquad._lineNumbers;

    const float HIT_FRACTION = 0.6;
    const float character_width = getCharacterHeight()
                                * getCharacterAspectRatio();

    float y = (line + 0.5) * line_height;

    bool line_found = false;
    for(size_t i = 0; i < line_numbers.size(); ++i)
    {
      if( line_numbers[i] != line )
      {
        if( !line_found )
        {
          if( line_numbers[i] < line )
            // Wait for the correct line...
            continue;

          // We have already passed the correct line -> It's empty...
          return osg::Vec2(0, y);
        }

        // Next line and not returned -> not before any character
        // -> return position after last character of line
        return osg::Vec2(coords[(i - 1) * 4 + 2].x(), y);
      }

      line_found = true;

      // Get threshold for mouse x position for setting cursor before or after
      // current character
      float threshold = coords[i * 4].x()
                      + HIT_FRACTION * glyphs[i]->getHorizontalAdvance()
                                     * character_width;

      if( pos.x() <= threshold )
      {
        osg::Vec2 hit(0, y);
        if( i == 0 || line_numbers[i - 1] != line )
          // first character of line
          hit.x() = coords[i * 4].x();
        else if( coords[(i - 1) * 4].x() == coords[(i - 1) * 4 + 2].x() )
          // If previous character width is zero set to begin of next character
          // (Happens eg. with spaces)
          hit.x() = coords[i * 4].x();
        else
          // position at center between characters
          hit.x() = 0.5 * (coords[(i - 1) * 4 + 2].x() + coords[i * 4].x());

        return hit;
      }
    }

    // Nothing found -> return position after last character
    return osg::Vec2
    (
      coords.back().x(),
      (_lineCount - 0.5) * line_height
    );
  }

  //----------------------------------------------------------------------------
  osg::BoundingBox Text::TextOSG::computeBound() const
  {
    osg::BoundingBox bb = osgText::Text::computeBound();
    if( !bb.valid() )
      return bb;

    // TODO bounding box still doesn't seem always right (eg. with center
    //      horizontal alignment not completely accurate)
    bb._min.y() += _offset.y();
    bb._max.y() += _offset.y();

    _text_element->setBoundingBox(bb);

    return bb;
  }

  //----------------------------------------------------------------------------
  Text::Text( const CanvasWeakPtr& canvas,
              const SGPropertyNode_ptr& node,
              const Style& parent_style,
              Element* parent ):
    Element(canvas, node, parent_style, parent),
    _text( new Text::TextOSG(this) )
  {
    setDrawable(_text);
    _text->setCharacterSizeMode(osgText::Text::OBJECT_COORDS);
    _text->setAxisAlignment(osgText::Text::USER_DEFINED_ROTATION);
    _text->setRotation(osg::Quat(osg::PI, osg::X_AXIS));

    addStyle("fill", &TextOSG::setFill, _text);
    addStyle("background", &TextOSG::setBackgroundColor, _text);
    addStyle("character-size",
             static_cast<void (TextOSG::*)(float)>(&TextOSG::setCharacterSize),
             _text);
    addStyle("character-aspect-ratio", &TextOSG::setCharacterAspect, _text);
    addStyle("padding", &TextOSG::setBoundingBoxMargin, _text);
    //  TEXT              = 1 default
    //  BOUNDINGBOX       = 2
    //  FILLEDBOUNDINGBOX = 4
    //  ALIGNMENT         = 8
    addStyle<int>("draw-mode", &TextOSG::setDrawMode, _text);
    addStyle("max-width", &TextOSG::setMaximumWidth, _text);
    addStyle("font", &Text::setFont, this);
    addStyle("alignment", &Text::setAlignment, this);
    addStyle("text", &Text::setText, this);

    setupStyle();
  }

  //----------------------------------------------------------------------------
  Text::~Text()
  {

  }

  //----------------------------------------------------------------------------
  void Text::setText(const char* text)
  {
    _text->setText(text, osgText::String::ENCODING_UTF8);
  }

  //----------------------------------------------------------------------------
  void Text::setFont(const char* name)
  {
    _text->setFont( _canvas.lock()->getSystemAdapter()->getFont(name) );
  }

  //----------------------------------------------------------------------------
  void Text::setAlignment(const char* align)
  {
    const std::string align_string(align);
    if( 0 ) return;
#define ENUM_MAPPING(enum_val, string_val) \
    else if( align_string == string_val )\
      _text->setAlignment( osgText::Text::enum_val );
#include "text-alignment.hxx"
#undef ENUM_MAPPING
    else
    {
      if( !align_string.empty() )
        SG_LOG
        (
          SG_GENERAL,
          SG_WARN,
          "canvas::Text: unknown alignment '" << align_string << "'"
        );
      _text->setAlignment(osgText::Text::LEFT_BASE_LINE);
    }
  }

  //----------------------------------------------------------------------------
#if 0
  const char* Text::getAlignment() const
  {
    switch( _text->getAlignment() )
    {
#define ENUM_MAPPING(enum_val, string_val) \
      case osgText::Text::enum_val:\
        return string_val;
#include "text-alignment.hxx"
#undef ENUM_MAPPING
      default:
        return "unknown";
    }
  }
#endif

  //----------------------------------------------------------------------------
  osg::Vec2 Text::getNearestCursor(const osg::Vec2& pos) const
  {
    return _text->handleHit(pos);
  }

} // namespace canvas
} // namespace simgear
