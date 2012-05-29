/*
 * terrainbrush.cpp
 * Copyright 2009-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2010, Stefan Beller <stefanbeller@googlemail.com>
 * Copyright 2012, Manu Evans <turkeyman@gmail.com>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "terrainbrush.h"

#include "brushitem.h"
#include "geometry.h"
#include "map.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"
#include "tilelayer.h"
#include "tileset.h"
#include "tile.h"

#include <math.h>
#include <QVector>
#include <climits>

using namespace Tiled;
using namespace Tiled::Internal;

TerrainBrush::TerrainBrush(QObject *parent)
    : AbstractTileTool(tr("Terrain Brush"),
                       QIcon(QLatin1String(
                               ":images/24x24/terrain-edit.png")),
                       QKeySequence(tr("T")),
                       parent)
    , mTerrain(NULL)
    , mPaintX(0), mPaintY(0)
    , mOffsetX(0), mOffsetY(0)
    , mBrushBehavior(Free)
    , mLineReferenceX(0)
    , mLineReferenceY(0)
{
    setBrushMode(PaintTile);
}

TerrainBrush::~TerrainBrush()
{
}

void TerrainBrush::tilePositionChanged(const QPoint &pos)
{
    switch (mBrushBehavior) {
    case Paint:
    {
        int x = mPaintX;
        int y = mPaintY;
        foreach (const QPoint &p, pointsOnLine(x, y, pos.x(), pos.y())) {
            updateBrush(p);
            doPaint(true, p.x(), p.y());
        }
        // HACK-ish: because the line may traverse in the reverse direction, updateBrush() leaves these at the line start point
        mPaintX = pos.x();
        mPaintY = pos.y();
        break;
    }
    case LineStartSet:
    {
        QVector<QPoint> lineList = pointsOnLine(mLineReferenceX, mLineReferenceY,
                                                pos.x(), pos.y());
        updateBrush(pos, &lineList);
        break;
    }
    case Line:
    case Free:
        updateBrush(pos);
        break;
    }
}

void TerrainBrush::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!brushItem()->isVisible())
        return;

    if (event->button() == Qt::LeftButton) {
        switch (mBrushBehavior) {
        case Line:
            mLineReferenceX = mPaintX;
            mLineReferenceY = mPaintY;
            mBrushBehavior = LineStartSet;
            break;
        case LineStartSet:
            doPaint(false, mPaintX, mPaintY);
            mLineReferenceX = mPaintX;
            mLineReferenceY = mPaintY;
            break;
        case Paint:
            beginPaint();
            break;
        case Free:
            beginPaint();
            mBrushBehavior = Paint;
            break;
        }
    } else {
        if (event->button() == Qt::RightButton)
            capture();
    }
}

void TerrainBrush::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    switch (mBrushBehavior) {
    case Paint:
        if (event->button() == Qt::LeftButton)
            mBrushBehavior = Free;
    default:
        // do nothing?
        break;
    }
}

void TerrainBrush::modifiersChanged(Qt::KeyboardModifiers modifiers)
{
    if (modifiers & Qt::ShiftModifier) {
        mBrushBehavior = Line;
    } else {
        mBrushBehavior = Free;
    }

    setBrushMode((modifiers & Qt::ControlModifier) ? PaintVertex : PaintTile);
    updateBrush(tilePosition());
}

void TerrainBrush::languageChanged()
{
    setName(tr("Terain Brush"));
    setShortcut(QKeySequence(tr("T")));
}

static Terrain *firstTerrain(MapDocument *mapDocument)
{
    if (!mapDocument)
        return 0;

    foreach (Tileset *tileset, mapDocument->map()->tilesets())
        if (tileset->terrainCount() > 0)
            return tileset->terrain(0);

    return 0;
}

void TerrainBrush::mapDocumentChanged(MapDocument *oldDocument,
                                      MapDocument *newDocument)
{
    AbstractTileTool::mapDocumentChanged(oldDocument, newDocument);

    // Reset the brush, since it probably became invalid
    brushItem()->setTileLayer(0);

    // Don't use setTerrain since we do not want to update the brush right now
    mTerrain = firstTerrain(newDocument);
}

void TerrainBrush::setTerrain(const Terrain *terrain)
{
    if (mTerrain == terrain)
        return;

    mTerrain = terrain;
    setTilePositionMethod(brushMode() == PaintTile ? OnTiles : BetweenTiles);

    updateBrush(tilePosition());
}

void TerrainBrush::setBrushMode(BrushMode mode)
{
    if (mBrushMode == mode)
        return;

    mBrushMode = mode;
    setTilePositionMethod(brushMode() == PaintTile ? OnTiles : BetweenTiles);

    updateBrush(tilePosition());
}

void TerrainBrush::beginPaint()
{
    if (mBrushBehavior != Free)
        return;

    mBrushBehavior = Paint;
    doPaint(false, mPaintX, mPaintY);
}

void TerrainBrush::capture()
{
    TileLayer *tileLayer = currentTileLayer();
    Q_ASSERT(tileLayer);

    // TODO: we need to know which corner the mouse is closest to...

    Terrain *t = NULL;
    const Cell &cell = tileLayer->cellAt(tilePosition());
    if (cell.tile) {
        if (cell.tile->isQuadrantType())
            t = cell.tile->terrainAtCorner(TerrainInfo::TopLeft);
        else if (cell.tile->isAdjacencyType())
            t = cell.tile->terrain();
    }
    setTerrain(t);
}

void TerrainBrush::doPaint(bool mergeable, int whereX, int whereY)
{
    TileLayer *stamp = brushItem()->tileLayer();

    if (!stamp)
        return;

    // This method shouldn't be called when current layer is not a tile layer
    TileLayer *tileLayer = currentTileLayer();
    Q_ASSERT(tileLayer);

    whereX -= mOffsetX;
    whereY -= mOffsetY;

    if (!tileLayer->bounds().intersects(QRect(whereX, whereY, stamp->width(), stamp->height())))
        return;

    PaintTileLayer *paint = new PaintTileLayer(mapDocument(), tileLayer, whereX, whereY, stamp);
    paint->setMergeable(mergeable);
    mapDocument()->undoStack()->push(paint);
    mapDocument()->emitRegionEdited(brushItem()->tileRegion(), tileLayer);
}

static inline unsigned int makeTerrain(int t)
{
    t &= 0xFF;
    return t << 24 | t << 16 | t << 8 | t;
}

static inline unsigned int makeTerrain(int tl, int tr, int bl, int br)
{
    return (tl & 0xFF) << 24 | (tr & 0xFF) << 16 | (bl & 0xFF) << 8 | (br & 0xFF);
}

static Tile *pickRandomTile(QList<Tile*> &matches)
{
    // choose a candidate at random, with consideration for terrain probability
    if (!matches.isEmpty()) {
        float random = ((float)rand() / RAND_MAX) * 100.f;
        float total = 0, unassigned = 0;

        // allow the tiles with assigned probability to take their share
        for (int i = 0; i < matches.size(); ++i) {
            float probability = matches[i]->terrainInfo()->terrainProbability();
            if (probability < 0.f) {
                ++unassigned;
                continue;
            }
            if (random < total + probability)
                return matches[i];
            total += probability;
        }

        // divide the remaining percentile by the numer of unassigned tiles
        float remainingShare = (100.f - total) / (float)unassigned;
        for (int i = 0; i < matches.size(); ++i) {
            if (matches[i]->terrainInfo()->terrainProbability() >= 0.f)
                continue;
            if (random < total + remainingShare)
                return matches[i];
            total += remainingShare;
        }
    }

    // TODO: conveniently, the NULL tile doesn't currently work, but when it does, we need to signal a failure to find any matches some other way
    return NULL;
}

Tile *TerrainBrush::findBestTileByQuadrant(Tileset *tileset, unsigned int terrain, unsigned int considerationMask)
{
    // if all quadrants are set to 'no terrain', then the 'empty' tile is the only choice we can deduce
    if (terrain == 0xFFFFFFFF)
        return NULL;

    QList<Tile*> matches;
    int penalty = INT_MAX;

    // TODO: this is a slow linear search, perhaps we could use a better find algorithm...
    int tileCount = tileset->tileCount();
    for (int i = 0; i < tileCount; ++i) {
        Tile *t = tileset->tileAt(i);
        TerrainInfo *ti = t ? t->terrainInfo() : NULL;
        if (ti->type() != TerrainInfo::Quadrant || (ti->terrain() & considerationMask) != (terrain & considerationMask))
            continue;

        // calculate the tile transition penalty based on shortest distance to target terrain type
        int tr = tileset->terrainTransitionPenalty(ti->terrain() >> 24, terrain >> 24);
        int tl = tileset->terrainTransitionPenalty((ti->terrain() >> 16) & 0xFF, (terrain >> 16) & 0xFF);
        int br = tileset->terrainTransitionPenalty((ti->terrain() >> 8) & 0xFF, (terrain >> 8) & 0xFF);
        int bl = tileset->terrainTransitionPenalty(ti->terrain() & 0xFF, terrain & 0xFF);

        // if there is no path to the destination terrain, this isn't a useful transition
        if (tr < 0 || tl < 0 || br < 0 || bl < 0)
            continue;

        // add tile to the candidate list
        int transitionPenalty = tr + tl + br + bl;
        if (transitionPenalty <= penalty) {
            if (transitionPenalty < penalty)
                matches.clear();
            penalty = transitionPenalty;

            matches.push_back(t);
        }
    }

    return pickRandomTile(matches);
}

Tile *TerrainBrush::findBestTileByAdjacency(Tileset *tileset, int terrain, unsigned int connections)
{
    // if we are erasing, return the 'none' tile
    if (terrain == -1)
        return NULL;

    QList<Tile*> matches;
    int penalty = INT_MAX;

    // TODO: this is a slow linear search, perhaps we could use a better find algorithm...
    int tileCount = tileset->tileCount();
    for (int i = 0; i < tileCount; ++i) {
        Tile *t = tileset->tileAt(i);
        TerrainInfo *ti = t ? t->terrainInfo() : NULL;
        if (ti->type() != TerrainInfo::Adjacency)
            continue;

        // calculate the connection penalty based on shortest distance to target terrain type
        int terrainId = ti->terrainId();
        int up = tileset->terrainTransitionPenalty(ti->connections() >> 24, connections >> 24);
        int down = tileset->terrainTransitionPenalty((ti->connections() >> 16) & 0xFF, (connections >> 16) & 0xFF);
        int left = tileset->terrainTransitionPenalty((ti->connections() >> 8) & 0xFF, (connections >> 8) & 0xFF);
        int right = tileset->terrainTransitionPenalty(ti->connections() & 0xFF, connections & 0xFF);

        // if there is no path to the destination terrain, this isn't a useful transition
        if (up < 0 || down < 0 || left < 0 || right < 0)
            continue;

        // add tile to the candidate list
        int transitionPenalty = up + down + left + right + (terrainId != terrain ? 2 : 0);
        if (transitionPenalty <= penalty) {
            if (transitionPenalty < penalty)
                matches.clear();
            penalty = transitionPenalty;

            matches.push_back(t);
        }
    }

    return pickRandomTile(matches);
}

void TerrainBrush::updateBrush(QPoint cursorPos, const QVector<QPoint> *list)
{
    // get the current tile layer
    TileLayer *currentLayer = currentTileLayer();
    if (!currentLayer)
        return;

    int layerWidth = currentLayer->width();
    int layerHeight = currentLayer->height();
    int numTiles = layerWidth * layerHeight;
    int paintCorner = 0;

    // if we are in vertex paint mode, the bottom right corner on the map will appear as an invalid tile offset...
    if (brushMode() == PaintVertex) {
        if (cursorPos.x() == layerWidth) {
            cursorPos.setX(cursorPos.x() - 1);
            paintCorner |= 1;
        }
        if (cursorPos.y() == layerHeight) {
            cursorPos.setY(cursorPos.y() - 1);
            paintCorner |= 2;
        }
    }

    // if the cursor is outside of the map, bail out
    if (!currentLayer->bounds().contains(cursorPos))
        return;

    // TODO: this seems like a problem... there's nothing to say that 2 adjacent tiles are from the same tileset, or have any relation to eachother...
    Tileset *terrainTileset = mTerrain ? mTerrain->tileset() : NULL;

    // allocate a buffer to build the terrain tilemap (TODO: this could be retained per layer to save regular allocation)
    Tile **newTerrain = new Tile*[numTiles];

    // allocate a buffer of flags for each tile that may be considered (TODO: this could be retained per layer to save regular allocation)
    char *checked = new char[numTiles];
    memset(checked, 0, numTiles);

    // create a consideration list, and push the start points
    QList<QPoint> transitionList;
    int initialTiles = 0;

    if (list) {
        // if we were supplied a list of start points
        foreach (const QPoint &p, *list) {
            transitionList.push_back(p);
            ++initialTiles;
        }
    } else {
        transitionList.push_back(cursorPos);
        initialTiles = 1;
    }

    QRect brushRect(cursorPos, cursorPos);

    // produce terrain with transitions using a simple, relative naive approach (considers each tile once, and doesn't allow re-consideration if selection was bad)
    while (!transitionList.isEmpty()) {
        // get the next point in the consideration list
        QPoint p = transitionList.front();
        transitionList.pop_front();
        int x = p.x(), y = p.y();
        int i = y*layerWidth + x;

        // if we have already considered this point, skip to the next
        // TODO: we might want to allow re-consideration if prior tiles... but not for now, this would risk infinite loops
        if (checked[i])
            continue;

        const Tile *tile = currentLayer->cellAt(p).tile;
        const TerrainInfo *terrainInfo = tile ? tile->terrainInfo() : NULL;

        // get the tileset for this tile
        Tileset *tileset = NULL;
        if (terrainTileset) // if we are painting a terrain, then we'll use the terrains tileset
            tileset = terrainTileset;
        else if(tile) // if we're erasing terrain, use the individual tiles tileset (to search for transitions)
            tileset = tile->tileset();

        // work out the type of terrain we are editing. ** Note: if the brush is NULL, we need to paint according to the terrain type of the tile we are erasing **
        bool quadrantMode = mTerrain ? mTerrain->type() == Terrain::MatchQuadrants : tile->terrainInfo()->type() == TerrainInfo::Quadrant;
        bool adjacencyMode = mTerrain ? mTerrain->type() == Terrain::MatchAdjacency : tile->terrainInfo()->type() == TerrainInfo::Adjacency;

        // calculate the ideal tile for this position
        Tile *paste = NULL;

        if (quadrantMode) {
            unsigned int preferredTerrain, mask;

            if (initialTiles) {
                // for the initial tiles, we will insert the selected terrain and add the surroundings for consideration
                unsigned int currentTerrain = terrainInfo->terrain();

                if (brushMode() == PaintTile) {
                    // set the whole tile to the selected terrain
                    preferredTerrain = makeTerrain(mTerrain->id());
                    mask = 0xFFFFFFFF;
                } else {
                    // calculate the corner mask
                    mask = 0xFF << (3 - paintCorner)*8;

                    // mask in the selected terrain
                    preferredTerrain = (currentTerrain & ~mask) | (mTerrain->id() << (3 - paintCorner)*8);
                }

                --initialTiles;

                // if there's nothing to paint... skip this tile
                if (preferredTerrain == currentTerrain)
                    continue;
            } else {
                // following tiles each need consideration against their surroundings
                preferredTerrain = terrainInfo->terrain();
                mask = 0;

                // depending which connections have been set, we update the preferred terrain of the tile accordingly
                if (y > 0 && checked[i - layerWidth]) {
                    preferredTerrain = (newTerrain[i - layerWidth]->terrainInfo()->terrain() << 16) | (preferredTerrain & 0x0000FFFF);
                    mask |= 0xFFFF0000;
                }
                if (y < layerHeight - 1 && checked[i + layerWidth]) {
                    preferredTerrain = (newTerrain[i + layerWidth]->terrainInfo()->terrain() >> 16) | (preferredTerrain & 0xFFFF0000);
                    mask |= 0x0000FFFF;
                }
                if (x > 0 && checked[i - 1]) {
                    preferredTerrain = ((newTerrain[i - 1]->terrainInfo()->terrain() << 8) & 0xFF00FF00) | (preferredTerrain & 0x00FF00FF);
                    mask |= 0xFF00FF00;
                }
                if (x < layerWidth - 1 && checked[i + 1]) {
                    preferredTerrain = ((newTerrain[i + 1]->terrainInfo()->terrain() >> 8) & 0x00FF00FF) | (preferredTerrain & 0xFF00FF00);
                    mask |= 0x00FF00FF;
                }
            }

            // find the most appropriate tile in the tileset
            if (preferredTerrain != 0xFFFFFFFF) {
                paste = findBestTileByQuadrant(tileset, preferredTerrain, mask);
                if (!paste)
                    continue;
            }

            // consider surrounding tiles if terrain constraints were not satisfied
            TerrainInfo *pasteTerrain = paste->terrainInfo();
            if (y > 0 && !checked[i - layerWidth]) {
                const Tile *above = currentLayer->cellAt(x, y - 1).tile;
                if (pasteTerrain->topEdge() != above->terrainInfo()->bottomEdge())
                    transitionList.push_back(QPoint(x, y - 1));
            }
            if (y < layerHeight - 1 && !checked[i + layerWidth]) {
                const Tile *below = currentLayer->cellAt(x, y + 1).tile;
                if (pasteTerrain->bottomEdge() != below->terrainInfo()->topEdge())
                    transitionList.push_back(QPoint(x, y + 1));
            }
            if (x > 0 && !checked[i - 1]) {
                const Tile *left = currentLayer->cellAt(x - 1, y).tile;
                if (pasteTerrain->leftEdge() != left->terrainInfo()->rightEdge())
                    transitionList.push_back(QPoint(x - 1, y));
            }
            if (x < layerWidth - 1 && !checked[i + 1]) {
                const Tile *right = currentLayer->cellAt(x + 1, y).tile;
                if (pasteTerrain->rightEdge() != right->terrainInfo()->leftEdge())
                    transitionList.push_back(QPoint(x + 1, y));
            }
        } else if (adjacencyMode) {
            unsigned int connections = 0xFFFFFFFF;

            // find the terrain ID
            int terrainId = mTerrain ? mTerrain->id() : tile->terrainInfo()->terrainId();

            // search for connections surrounding the new tile
            const Tile *above = y > 0 ? (checked[i - layerWidth] ? newTerrain[i - layerWidth] : currentLayer->cellAt(x, y - 1).tile) : NULL;
            const Tile *below = y < layerHeight - 1 ? (checked[i + layerWidth] ? newTerrain[i + layerWidth] : currentLayer->cellAt(x, y + 1).tile) : NULL;
            const Tile *left = x > 0 ? (checked[i - 1] ? newTerrain[i - 1] : currentLayer->cellAt(x - 1, y).tile) : NULL;
            const Tile *right = x < layerWidth - 1 ? (checked[i + 1] ? newTerrain[i + 1] : currentLayer->cellAt(x + 1, y).tile) : NULL;

            // if the surrounding tiles are not adjacency terrain tiles, reset to NULL
            if (above->terrainInfo()->type() != TerrainInfo::Adjacency)
                above = NULL;
            if (below->terrainInfo()->type() != TerrainInfo::Adjacency)
                below = NULL;
            if (left->terrainInfo()->type() != TerrainInfo::Adjacency)
                left = NULL;
            if (right->terrainInfo()->type() != TerrainInfo::Adjacency)
                right = NULL;

            // find connections...
            // if the adjacent tile has already be considered, match it's connection terrain type
            // if it has not been considered, connect to the tile's terrain type
            if (above) {
                if (checked[i - layerWidth])
                    connections = (connections & 0x00FFFFFF) | (above->terrainInfo()->connection(TerrainInfo::Bottom) & 0xFF) << 24;
                else
                    connections = (connections & 0x00FFFFFF) | (above->terrainInfo()->terrainId() & 0xFF) << 24;
            }
            if (below) {
                if (checked[i + layerWidth])
                    connections = (connections & 0xFF00FFFF) | (below->terrainInfo()->connection(TerrainInfo::Top) & 0xFF) << 16;
                else
                    connections = (connections & 0xFF00FFFF) | (below->terrainInfo()->terrainId() & 0xFF) << 16;
            }
            if (left) {
                if (checked[i - 1])
                    connections = (connections & 0xFFFF00FF) | (left->terrainInfo()->connection(TerrainInfo::Right) & 0xFF) << 8;
                else
                    connections = (connections & 0xFFFF00FF) | (left->terrainInfo()->terrainId() & 0xFF) << 8;
            }
            if (right) {
                if (checked[i + 1])
                    connections = (connections & 0xFFFFFF00) | right->terrainInfo()->connection(TerrainInfo::Left) & 0xFF;
                else
                    connections = (connections & 0xFFFFFF00) | right->terrainInfo()->terrainId() & 0xFF;
            }

            // find the best tile to satisfy the adjacent connections
            if (terrainId != -1) {
                paste = findBestTileByAdjacency(tileset, terrainId, connections);
                if (!paste)
                    continue;
            }

            // consider adjacent tiles if connections are left dangling
            TerrainInfo *pasteTerrain = paste->terrainInfo();
            if (above && !checked[i - layerWidth]) {
                if (pasteTerrain->connection(TerrainInfo::Top) != above->terrainInfo()->connection(TerrainInfo::Bottom))
                    transitionList.push_back(QPoint(x, y - 1));
            }
            if (below && !checked[i + layerWidth]) {
                if (pasteTerrain->connection(TerrainInfo::Bottom) != below->terrainInfo()->connection(TerrainInfo::Top))
                    transitionList.push_back(QPoint(x, y + 1));
            }
            if (left && !checked[i - 1]) {
                if (pasteTerrain->connection(TerrainInfo::Left) != left->terrainInfo()->connection(TerrainInfo::Right))
                    transitionList.push_back(QPoint(x - 1, y));
            }
            if (right && !checked[i + 1]) {
                if (pasteTerrain->connection(TerrainInfo::Right) != right->terrainInfo()->connection(TerrainInfo::Left))
                    transitionList.push_back(QPoint(x + 1, y));
            }
        }

        // add tile to the brush
        newTerrain[i] = paste;
        checked[i] = true;

        // expand the brush rect to fit the edit set
        brushRect |= QRect(p, p);
    }

    // create a stamp for the terrain block
    TileLayer *stamp = new TileLayer(QString(), 0, 0, brushRect.width(), brushRect.height());

    for (int y = brushRect.top(); y <= brushRect.bottom(); ++y) {
        for (int x = brushRect.left(); x <= brushRect.right(); ++x) {
            int i = y*layerWidth + x;
            if (!checked[i])
                continue;

            Tile *tile = newTerrain[i];
            if (tile)
                stamp->setCell(x - brushRect.left(), y - brushRect.top(), Cell(tile));
            else {
                // TODO: we need to do something to erase tiles where checked[i] is true, and newTerrain[i] is NULL
                // is there an eraser stamp? investigate how the eraser works...
            }
        }
    }

    // set the new tile layer as the brush
    brushItem()->setTileLayer(stamp);

    delete[] checked;
    delete[] newTerrain;

/*
    const QPoint tilePos = tilePosition();

    if (!brushItem()->tileLayer()) {
        brushItem()->setTileRegion(QRect(tilePos, QSize(1, 1)));
    }
*/

    brushItem()->setTileLayerPosition(QPoint(brushRect.left(), brushRect.top()));

    mPaintX = cursorPos.x();
    mPaintY = cursorPos.y();
    mOffsetX = cursorPos.x() - brushRect.left();
    mOffsetY = cursorPos.y() - brushRect.top();
}
