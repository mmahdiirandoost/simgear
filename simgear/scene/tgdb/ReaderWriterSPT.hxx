// ReaderWriterSPT.cxx -- Provide a paged database for flightgear scenery.
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

#ifndef _READERWRITERSPT_HXX
#define _READERWRITERSPT_HXX

#include <osgDB/ReaderWriter>

namespace simgear {

class BucketBox;

class ReaderWriterSPT : public osgDB::ReaderWriter {
public:
    ReaderWriterSPT();
    virtual ~ReaderWriterSPT();

    virtual const char* className() const;

    virtual osgDB::ReaderWriter::ReadResult readObject(const std::string& fileName, const osgDB::Options* options) const;
    virtual osgDB::ReaderWriter::ReadResult readNode(const std::string& fileName, const osgDB::Options* options) const;

protected:
    osg::Node* createTree(const BucketBox& bucketBox, const osgDB::Options* options, bool topLevel) const;
    osg::Node* createPagedLOD(const BucketBox& bucketBox, const osgDB::Options* options) const;
    osg::Node* createSeaLevelTile(const BucketBox& bucketBox, const osgDB::Options* options) const;
    osg::StateSet* getLowLODStateSet(const osgDB::Options* options) const;

private:
    struct CullCallback;
};

} // namespace simgear

#endif
