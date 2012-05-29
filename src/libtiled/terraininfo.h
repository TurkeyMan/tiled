/*
 * terraininfo.h
 * Copyright 2012, Manu Evans <turkeyman@gmail.com>
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TERRAININFO_H
#define TERRAININFO_H

namespace Tiled {

/**
 * Represents the terrain info for a tile.
 */
class TerrainInfo
{
public:
    enum Type {
        None,
        Quadrant,
        Adjacency
    };

    enum Corner
    {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    enum Edge
    {
        Top,
        Bottom,
        Left,
        Right
    };

    TerrainInfo(Type type, unsigned int terrain, unsigned int connections, float probability = -1.f):
        mType(type),
        mTerrain(terrain),
        mConnections(connections),
        mTerrainProbability(probability)
    {}

    /**
     * Returns the terrain type.
     */
    Type type() const { return this == NULL ? None : mType; }

    /**
     * Returns the terrain id at a given corner.
     */
    int cornerTerrainId(Corner corner) const
    {
        Q_ASSERT(type() != Adjacency);
        unsigned int t = (terrain() >> (3 - corner)*8) & 0xFF;
        return t == 0xFF ? -1 : (int)t;
    }

    /**
     * Set the terrain type of a given corner.
     */
    void setCornerTerrain(Corner corner, int terrainId)
    {
        mType = Quadrant;
        unsigned int mask = 0xFF << (3 - corner)*8;
        unsigned int insert = terrainId << (3 - corner)*8;
        mTerrain = (mTerrain & ~mask) | (insert & mask);
    }

    /**
     * Functions to get various terrain type information from tiles for comparison.
     */
    unsigned short topEdge() const { return terrain() >> 16; }                                              // top left + top right       == 0xFFFF0000
    unsigned short bottomEdge() const { return terrain() & 0xFFFF; }                                        // bottom left + bottom right == 0x0000FFFF
    unsigned short leftEdge() const { return ((terrain() >> 16) & 0xFF00) | ((terrain() >> 8) & 0xFF); }    // top left + bottom left     == 0xFF00FF00
    unsigned short rightEdge() const { return ((terrain() >> 8) & 0xFF00) | (terrain() & 0xFF); }           // top right + bottom right   == 0x00FF00FF
    unsigned int terrain() const { return this == NULL ? 0xFFFFFFFF : mTerrain; } // HACK: NULL Tile has 'none' terrain type.

    /**
     * Get the tiles terrain id.
     */
    int terrainId() const
    {
        Q_ASSERT(mType == Adjacency);
        return (int)mTerrain;
    }

    /**
     * Set the tiles terrain id.
     */
    void setTerrainId(int terrainId)
    {
        mType = Adjacency;
        mTerrain = (unsigned int)terrainId;
    }

    /**
     * Get the connection terrain type for an edge.
     */
    int connection(Edge edge) const
    {
        Q_ASSERT(type() == Adjacency);
        unsigned int t = (mConnections >> (3 - edge)*8) & 0xFF;
        return t == 0xFF ? -1 : (int)t;
    }

    /**
     * Set the terrain type of a given corner.
     */
    void setConnection(Edge edge, int terrainId)
    {
        mType = Adjacency;
        unsigned int mask = 0xFF << (3 - edge)*8;
        unsigned int insert = terrainId << (3 - edge)*8;
        mConnections = (mConnections & ~mask) | (insert & mask);
    }

    /**
     * Returns the packed connections.
     */
    unsigned int connections() const { return mConnections; }

    /**
     * Returns the probability of this terrain type appearing while painting (0-100%).
     */
    float terrainProbability() const { return mTerrainProbability; }

    /**
     * Set the probability of this terrain type appearing while painting (0-100%).
     */
    void setTerrainProbability(float probability) { mTerrainProbability = probability; }

private:
    Type mType;                     // terrain type
    unsigned int mTerrain;          // terrain ID for this tile (or 4x id's packed as 0xTL_TR_BL_BR for quadrant based terrain)
    unsigned int mConnections;      // connectivity information for adjacency based terrain
    float mTerrainProbability;      // appearance probability while painting (0-100%)
};

} // namespace Tiled

#endif // TERRAININFO_H
