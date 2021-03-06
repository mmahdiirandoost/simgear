// BucketBox.cxx -- Helper for on demand database paging.
//
// Copyright (C) 2010 - 2011  Mathias Froehlich
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#ifndef _BUCKETBOX_HXX
#define _BUCKETBOX_HXX

#include <cassert>
#include <istream>
#include <ostream>

#include <simgear/bucket/newbucket.hxx>
#include <simgear/math/SGGeometry.hxx>

namespace simgear {

#define Elements(x) (sizeof(x)/sizeof((x)[0]))

// 3*5*3 * 8 = 360
static const unsigned _lonFactors[] = { 3, 5, 3,  2, 2, 2, /* sub degree */ 2, 2, 2 };
// 5*3*3 * 4 = 180
static const unsigned _latFactors[] = { 5, 3, 3,  2, 2, /* sub degree */ 2, 2, 2, 1 };

static unsigned product(const unsigned* factors, unsigned count)
{
    unsigned index = 1;
    while (count--)
        index *= *factors++;
    return index;
}

/// /Rectangular/ sub part of the earths surface.
/// Stored with offset point in lon/lat and width and height.
/// The values are stored in a fixed point format having 3 fractional
/// bits which matches the SGBuckets maximum tile resolution.
///
/// Notable /design/ decision:
/// * The longitude maps to the interval [0,360[ which appears to be
///   counter productive for the file/directory names and the
///   texture coordinates which map [-180,180[.
///   The reason is that the buckets at 89deg longitude are 8deg
///   latitude width. So there is a bunch of buckets that range from
///   [176, -184[ longitude. So the wrap happens at 0deg instead
///   of 180deg since we have a cut edge for all latitudes.
/// * This is not meant to be an API class for simgear. This is
///   just an internal tool that I would like to keep in the SPT loader.
///   But I want to have coverage somehow tested with the usual unit
///   tests, which is the reason to have this file split out.
///
class BucketBox {
public:
    BucketBox()
    { _offset[0] = 0; _offset[1] = 0; _size[0] = 0; _size[1] = 0; }
    BucketBox(double lon, double lat, double width, double height)
    {
        _offset[0] = _longitudeDegToOffset(lon);
        _offset[1] = _latitudeDegToOffset(lat);
        _size[0] = _degToSize(width);
        _size[1] = _degToSize(height);
    }
    BucketBox(const BucketBox& bucketBox)
    {
        _offset[0] = bucketBox._offset[0];
        _offset[1] = bucketBox._offset[1];
        _size[0] = bucketBox._size[0];
        _size[1] = bucketBox._size[1];
    }

    BucketBox& operator=(const BucketBox& bucketBox)
    {
        _offset[0] = bucketBox._offset[0];
        _offset[1] = bucketBox._offset[1];
        _size[0] = bucketBox._size[0];
        _size[1] = bucketBox._size[1];
        return *this;
    }

    bool empty() const
    { return _size[0] == 0 || _size[1] == 0; }

    unsigned getOffset(unsigned i) const
    { return _offset[i]; }
    void setOffset(unsigned i, unsigned offset)
    { _offset[i] = offset; }

    unsigned getSize(unsigned i) const
    { return _size[i]; }
    void setSize(unsigned i, unsigned size)
    { _size[i] = size; }

    double getLongitudeDeg() const
    { return _offsetToLongitudeDeg(_offset[0]); }
    void setLongitudeDeg(double lon)
    { _offset[0] = _longitudeDegToOffset(lon); }

    double getLatitudeDeg() const
    { return _offsetToLatitudeDeg(_offset[1]); }
    void setLatitudeDeg(double lat)
    { _offset[1] = _latitudeDegToOffset(lat); }

    double getWidthDeg() const
    { return _sizeToDeg(_size[0]); }
    void setWidthDeg(double width)
    { _size[0] = _degToSize(width); }

    double getHeightDeg() const
    { return _sizeToDeg(_size[1]); }
    void setHeightDeg(double height)
    { _size[1] = _degToSize(height); }

    bool getWidthIsBucketSize() const
    {
        if (_size[0] <= _bucketSpanAtOffset(_offset[1]))
            return true;
        return _size[0] <= _bucketSpanAtOffset(_offset[1] + _size[1] - 1);
    }

    bool getHeightIsBucketSize() const
    { return _size[1] == 1; }
    bool getIsBucketSize() const
    { return getHeightIsBucketSize() && getWidthIsBucketSize(); }

    SGBucket getBucket() const
    {
        // left align longitude offsets
        unsigned offset = _offset[0] - _offset[0] % _bucketSpanAtOffset(_offset[1]);
        return SGBucket(_offsetToLongitudeDeg(offset), _offsetToLatitudeDeg(_offset[1]));
    }

    BucketBox getSubBoxHeight(unsigned j, unsigned level) const
    {
        assert(0 < level);
        BucketBox box;
        unsigned plat = product(_latFactors + level, Elements(_latFactors) - level);
        unsigned plat1 = plat*_latFactors[level - 1];
        box._offset[0] = _offset[0];
        box._offset[1] = _offset[1] - _offset[1] % plat1 + j*plat;
        box._size[0] = _size[0];
        box._size[1] = plat;

        return box;
    }

    BucketBox getSubBoxWidth(unsigned i, unsigned level) const
    {
        assert(0 < level);
        BucketBox box;
        unsigned plon = product(_lonFactors + level, Elements(_lonFactors) - level);
        unsigned plon1 = plon*_lonFactors[level - 1];
        box._offset[0] = _offset[0] - _offset[0] % plon1 + i*plon;
        box._offset[1] = _offset[1];
        box._size[0] = plon;
        box._size[1] = _size[1];

        box._offset[0] = _normalizeLongitude(box._offset[0]);
    
        return box;
    }

    unsigned getWidthLevel() const
    { return _getLevel(_lonFactors, Elements(_lonFactors), _offset[0], _offset[0] + _size[0]); }
    unsigned getHeightLevel() const
    { return _getLevel(_latFactors, Elements(_latFactors), _offset[1], _offset[1] + _size[1]); }

    unsigned getWidthIncrement(unsigned level) const
    {
        level = SGMisc<unsigned>::clip(level, 5, Elements(_lonFactors));
        return product(_lonFactors + level, Elements(_lonFactors) - level);
    }
    unsigned getHeightIncrement(unsigned level) const
    {
        level = SGMisc<unsigned>::clip(level, 5, Elements(_latFactors));
        return product(_latFactors + level, Elements(_latFactors) - level);
    }

    unsigned getStartLevel() const
    {
        if (getWidthIsBucketSize())
            return getHeightLevel();
        return std::min(getWidthLevel(), getHeightLevel());
    }

    SGSpheref getBoundingSphere() const
    {
        SGSpheref sphere;
        unsigned incx = 10*8;
        for (unsigned i = 0; incx != 0; i += incx) {
            unsigned incy = 10*8;
            for (unsigned j = 0; incy != 0; j += incy) {
                sphere.expandBy(SGVec3f::fromGeod(_offsetToGeod(_offset[0] + i, _offset[1] + j, -1000)));
                sphere.expandBy(SGVec3f::fromGeod(_offsetToGeod(_offset[0] + i, _offset[1] + j, 10000)));
                incy = std::min(incy, _size[1] - j);
            }
            incx = std::min(incx, _size[0] - i);
        }
        return SGSpheref(sphere.getCenter(), sphere.getRadius() + 5000);
    }

    // Split the current box into up to two boxes that do not cross the 360 deg border.
    unsigned periodicSplit(BucketBox bucketBoxList[2]) const
    {
        if (empty())
            return 0;

        bucketBoxList[0] = *this;
        bucketBoxList[0]._offset[0] = _normalizeLongitude(bucketBoxList[0]._offset[0]);
        if (bucketBoxList[0]._offset[0] + bucketBoxList[0]._size[0] <= 360*8)
            return 1;

        bucketBoxList[1] = bucketBoxList[0];
        bucketBoxList[0]._size[0] = 360*8 - bucketBoxList[0]._offset[0];

        bucketBoxList[1]._offset[0] = 0;
        bucketBoxList[1]._size[0] = _size[0] - bucketBoxList[0]._size[0];

        return 2;
    }

    unsigned getSubDivision(BucketBox bucketBoxList[], unsigned bucketBoxListSize) const
    {
        unsigned numTiles = 0;
    
        // Quad tree like structure in x and y direction
        unsigned widthLevel = getWidthLevel();
        unsigned heightLevel = getHeightLevel();
        
        unsigned level;
        if (getWidthIsBucketSize()) {
            level = heightLevel;
        } else {
            level = std::min(widthLevel, heightLevel);
        }
        for (unsigned j = 0; j < _latFactors[level]; ++j) {
            BucketBox heightSplitBox = getSubBoxHeight(j, level + 1);
      
            heightSplitBox = _intersection(*this, heightSplitBox);
            if (heightSplitBox.empty())
                continue;
      
            if (heightSplitBox.getWidthIsBucketSize()) {
                bucketBoxList[numTiles++] = heightSplitBox;
                assert(numTiles <= bucketBoxListSize);
            } else {
                for (unsigned i = 0; i < _lonFactors[widthLevel]; ++i) {
                    BucketBox childBox = _intersection(heightSplitBox, heightSplitBox.getSubBoxWidth(i, widthLevel + 1));
                    if (childBox.empty())
                        continue;
          
                    bucketBoxList[numTiles++] = childBox;
                    assert(numTiles <= bucketBoxListSize);
                }
            }
        }
        return numTiles;
    }
  
    unsigned getTileTriangles(unsigned i, unsigned j, unsigned width, unsigned height,
                              SGVec3f points[6], SGVec3f normals[6], SGVec2f texCoords[6]) const
    {
        unsigned numPoints = 0;

        unsigned x0 = _offset[0] + i;
        unsigned x1 = x0 + width;

        unsigned y0 = _offset[1] + j;
        unsigned y1 = y0 + height;
    
        SGGeod p00 = _offsetToGeod(x0, y0, 0);
        SGVec3f v00 = SGVec3f::fromGeod(p00);
        SGVec3f n00 = SGQuatf::fromLonLat(p00).backTransform(SGVec3f(0, 0, -1));
        SGVec2f t00(x0*1.0/(360*8) + 0.5, y0*1.0/(180*8));

        SGGeod p10 = _offsetToGeod(x1, y0, 0);
        SGVec3f v10 = SGVec3f::fromGeod(p10);
        SGVec3f n10 = SGQuatf::fromLonLat(p10).backTransform(SGVec3f(0, 0, -1));
        SGVec2f t10(x1*1.0/(360*8) + 0.5, y0*1.0/(180*8));

        SGGeod p11 = _offsetToGeod(x1, y1, 0);
        SGVec3f v11 = SGVec3f::fromGeod(p11);
        SGVec3f n11 = SGQuatf::fromLonLat(p11).backTransform(SGVec3f(0, 0, -1));
        SGVec2f t11(x1*1.0/(360*8) + 0.5, y1*1.0/(180*8));

        SGGeod p01 = _offsetToGeod(x0, y1, 0);
        SGVec3f v01 = SGVec3f::fromGeod(p01);
        SGVec3f n01 = SGQuatf::fromLonLat(p01).backTransform(SGVec3f(0, 0, -1));
        SGVec2f t01(x0*1.0/(360*8) + 0.5, y1*1.0/(180*8));
    
        if (y0 != 0) {
            points[numPoints] = v00;
            normals[numPoints] = n00;
            texCoords[numPoints] = t00;
            ++numPoints;

            points[numPoints] = v10;
            normals[numPoints] = n10;
            texCoords[numPoints] = t10;
            ++numPoints;

            points[numPoints] = v01;
            normals[numPoints] = n01;
            texCoords[numPoints] = t01;
            ++numPoints;
        }
        if (y1 != 180*8) {
            points[numPoints] = v11;
            normals[numPoints] = n11;
            texCoords[numPoints] = t11;
            ++numPoints;

            points[numPoints] = v01;
            normals[numPoints] = n01;
            texCoords[numPoints] = t01;
            ++numPoints;

            points[numPoints] = v10;
            normals[numPoints] = n10;
            texCoords[numPoints] = t10;
            ++numPoints;
        }
        return numPoints;
    }

private:
    static unsigned _normalizeLongitude(unsigned offset)
    { return offset - (360*8)*(offset/(360*8)); }

    static unsigned _longitudeDegToOffset(double lon)
    {
        lon = SGMiscd::normalizePeriodic(0, 360, lon);
        unsigned offset = (unsigned)(8*lon + 0.5);
        return _normalizeLongitude(offset);
    }
    static double _offsetToLongitudeDeg(unsigned offset)
    {
        if (180*8 <= offset)
            return offset*0.125 - 360;
        else
            return offset*0.125;
    }

    static unsigned _latitudeDegToOffset(double lat)
    {
        if (lat < -90)
            return 0;
        unsigned offset = (unsigned)(8*(lat + 90) + 0.5);
        if (8*180 < offset)
            return 8*180;
        return offset;
    }
    static double _offsetToLatitudeDeg(unsigned offset)
    { return offset*0.125 - 90; }

    static unsigned _degToSize(double deg)
    {
        if (deg <= 0)
            return 0;
        return (unsigned)(8*deg + 0.5);
    }
    static double _sizeToDeg(unsigned size)
    { return size*0.125; }

    static unsigned _bucketSpanAtOffset(unsigned offset)
    { return (unsigned)(8*sg_bucket_span(_offsetToLatitudeDeg(offset) + 0.0625) + 0.5); }

    static SGGeod _offsetToGeod(unsigned offset0, unsigned offset1, double elev)
    { return SGGeod::fromDegM(_offsetToLongitudeDeg(offset0), _offsetToLatitudeDeg(offset1), elev); }
  
    static unsigned _getLevel(const unsigned factors[], unsigned nFactors, unsigned begin, unsigned end)
    {
        unsigned rbegin = end - 1;
        for (; 0 < nFactors;) {
            if (begin == rbegin)
                break;
            --nFactors;
            begin /= factors[nFactors];
            rbegin /= factors[nFactors];
        }

        return nFactors;
    }

    static BucketBox _intersection(const BucketBox& box0, const BucketBox& box1)
    {
        BucketBox box;
        for (unsigned i = 0; i < 2; ++i) {
            box._offset[i] = std::max(box0._offset[i], box1._offset[i]);
            unsigned m = std::min(box0._offset[i] + box0._size[i], box1._offset[i] + box1._size[i]);
            if (m <= box._offset[i])
                box._size[i] = 0;
            else
                box._size[i] = m - box._offset[i];
        }

        box._offset[0] = _normalizeLongitude(box._offset[0]);

        return box;
    }

    unsigned _offset[2];
    unsigned _size[2];
};

inline bool
operator==(const BucketBox& bucketBox0, const BucketBox& bucketBox1)
{
    if (bucketBox0.getOffset(0) != bucketBox1.getOffset(0))
        return false;
    if (bucketBox0.getOffset(1) != bucketBox1.getOffset(1))
        return false;
    if (bucketBox0.getSize(0) != bucketBox1.getSize(0))
        return false;
    if (bucketBox0.getSize(1) != bucketBox1.getSize(1))
        return false;
    return true;
}

inline bool
operator!=(const BucketBox& bucketBox0, const BucketBox& bucketBox1)
{ return !operator==(bucketBox0, bucketBox1); }

inline bool
operator<(const BucketBox& bucketBox0, const BucketBox& bucketBox1)
{
    if (bucketBox0.getOffset(0) < bucketBox1.getOffset(0)) return true;
    else if (bucketBox1.getOffset(0) < bucketBox0.getOffset(0)) return false;
    else if (bucketBox0.getOffset(1) < bucketBox1.getOffset(1)) return true;
    else if (bucketBox1.getOffset(1) < bucketBox0.getOffset(1)) return false;
    else if (bucketBox0.getSize(0) < bucketBox1.getSize(0)) return true;
    else if (bucketBox1.getSize(0) < bucketBox0.getSize(0)) return false;
    else return bucketBox0.getSize(1) < bucketBox1.getSize(1);
}

inline bool
operator>(const BucketBox& bucketBox0, const BucketBox& bucketBox1)
{ return operator<(bucketBox1, bucketBox0); }

inline bool
operator<=(const BucketBox& bucketBox0, const BucketBox& bucketBox1)
{ return !operator>(bucketBox0, bucketBox1); }

inline bool
operator>=(const BucketBox& bucketBox0, const BucketBox& bucketBox1)
{ return !operator<(bucketBox0, bucketBox1); }

/// Stream output operator.
/// Note that this is not only used for pretty printing but also for
/// generating the meta file names for on demand paging.
/// So, don't modify unless you know where else this is used.
template<typename char_type, typename traits_type>
std::basic_ostream<char_type, traits_type>&
operator<<(std::basic_ostream<char_type, traits_type>& os, const BucketBox& bucketBox)
{
    double lon = bucketBox.getLongitudeDeg();
    if (lon < 0)
        os << "w" << -lon;
    else
        os << "e" << lon;
    
    double lat = bucketBox.getLatitudeDeg();
    if (lat < -90)
        lat = -90;
    if (90 < lat)
        lat = 90;
    if (lat < 0)
        os << "s" << -lat;
    else
        os << "n" << lat;
    
    os << "-" << bucketBox.getWidthDeg() << "x" << bucketBox.getHeightDeg();
    return os;
}

/// Stream inout operator.
/// Note that this is used for reading the meta file names for on demand paging.
/// So, don't modify unless you know where else this is used.
template<typename char_type, typename traits_type>
std::basic_istream<char_type, traits_type>&
operator>>(std::basic_istream<char_type, traits_type>& is, BucketBox& bucketBox)
{
    char c;
    is >> c;
    if (is.fail())
        return is;

    int sign = 0;
    if (c == 'w')
        sign = -1;
    else if (c == 'e')
        sign = 1;
    else {
        is.setstate(std::ios::failbit | is.rdstate());
        return is;
    }
    
    double num;
    is >> num;
    if (is.fail()) {
        is.setstate(std::ios::failbit | is.rdstate());
        return is;
    }
    bucketBox.setLongitudeDeg(sign*num);
    
    is >> c;
    if (is.fail())
        return is;
    
    sign = 0;
    if (c == 's')
        sign = -1;
    else if (c == 'n')
        sign = 1;
    else {
        is.setstate(std::ios::failbit | is.rdstate());
        return is;
    }
    
    is >> num;
    if (is.fail())
        return is;
    bucketBox.setLatitudeDeg(sign*num);
    
    is >> c;
    if (is.fail())
        return is;
    if (c != '-'){
        is.setstate(std::ios::failbit | is.rdstate());
        return is;
    }
    
    is >> num;
    if (is.fail())
        return is;
    bucketBox.setWidthDeg(SGMiscd::min(num, 360));
    
    is >> c;
    if (is.fail())
        return is;
    if (c != 'x'){
        is.setstate(std::ios::failbit | is.rdstate());
        return is;
    }
    
    is >> num;
    if (is.fail())
        return is;
    bucketBox.setHeightDeg(SGMiscd::min(num, 90 - bucketBox.getLatitudeDeg()));

    return is;
}

} // namespace simgear

#endif
