// =============================================================================
//
// Copyright (c) 2014-2016 Christopher Baker <http://christopherbaker.net>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// =============================================================================


#include "ofx/Maps/TileLayer.h"
#include "ofGraphics.h"


namespace ofx {
namespace Maps {


TileLayer::TileLayer():
    _width(0),
    _height(0),
    _loader(),
    _coordsDirty(true),
    _center(0.5, 0.5, 0),
    _padColumn(0),
    _padRow(0)

{
    ofAddListener(_loader.onTileCached, this, &TileLayer::onTileCached);
    ofAddListener(_loader.onTileUncached, this, &TileLayer::onTileUncached);
}


TileLayer::~TileLayer()
{
    _loader.cancelAll();
    _loader.joinAll();

    ofRemoveListener(_loader.onTileCached, this, &TileLayer::onTileCached);
    ofRemoveListener(_loader.onTileUncached, this, &TileLayer::onTileUncached);
}


void TileLayer::setup(std::shared_ptr<BaseURITileProvider> provider,
                      int width,
                      int height)
{
    if (provider)
    {
        _provider = provider;
        _width = width;
        _height = height;

        double z = log2(std::min(_width, _height) /
                        std::min(_provider->getTileWidth(),
                                 _provider->getTileHeight()));

        _center = _center.zoomTo(z);
        
        _fbo.allocate(_width, _height);
    }

    else
    {
        ofLogError("TileLayer::setup") << "Provider is not defined.";
    }

}


void TileLayer::draw(float x, float y) const
{
    if (_coordsDirty)
    {
        _visisbleCoords = getVisibleCoordinates();
        _coordsDirty = false;
    }

    _fbo.begin();

    ofClear(0, 0, 0);

	std::set<TileCoordinate>::const_reverse_iterator iter = _visisbleCoords.rbegin();

    while (iter != _visisbleCoords.rend())
    {
        const TileCoordinate& coord = *iter;

        SharedTile tile = _loader.getTile(coord);

        if (tile)
        {
            double scale = pow(2.0, _center.getZoom() - coord.getZoom());

            glm::dvec2 tileSize(_provider->getTileWidth(),
                                _provider->getTileHeight());

            tileSize *= scale;

            glm::dvec2 center = glm::dvec2(_width, _height) * 0.5;
            
            TileCoordinate theCoord = _center.zoomTo(coord.getZoom());

            double tx = center.x + tileSize.x * (coord.getColumn() - theCoord.getColumn());
            double ty = center.y + tileSize.y * (coord.getRow()    - theCoord.getRow());
            
            tile->draw(tx, ty, tileSize.x, tileSize.y);
        }

        ++iter;
    }

    _fbo.end();

    _fbo.draw(x, y);
}


void TileLayer::draw(float x, float y, float w, float h) const
{
    ofPushMatrix();
    ofScale(_width / w, _height / h);

    draw(x, y);

    ofPopMatrix();
}


glm::dvec2 TileLayer::getSize() const
{
    return glm::dvec2(_width, _height);
}


float TileLayer::getWidth() const
{
    return _width;
}


void TileLayer::setWidth(double width)
{
    _width = width;
    _coordsDirty = true;
}


float TileLayer::getHeight() const
{
    return _height;
}


void TileLayer::setHeight(double height)
{
    _height = height;
    _coordsDirty = true;
}


const TileCoordinate& TileLayer::getCenter() const
{
    return _center;
}


void TileLayer::setCenter(const TileCoordinate& center)
{
    _center = center;
    _coordsDirty = true;
}


void TileLayer::setCenter(const Geo::Coordinate& center, double zoom)
{
    if (_provider)
    {
        setCenter(_provider->geoToTile(center).zoomTo(zoom));
    }
    else
    {
        ofLogError("TileLayer::setCenter") << "Provider is not defined.";
    }
}


std::shared_ptr<BaseURITileProvider> TileLayer::getProvider()
{
    return _provider;
}


std::set<TileCoordinate> TileLayer::getVisibleCoordinates() const
{
    if (!_provider)
    {
        ofLogError("TileLayer::layerPointToTileCoordinate") << "Provider is not defined.";
        std::set<TileCoordinate> coordinates;
        return coordinates;
    }

    // Round the current zoom in case we are in between levels.
	int baseZoom = CLAMP(round(_center.getZoom()),
                         _provider->getMinZoom(),
                         _provider->getMaxZoom());

    double gridSize = pow(2.0, baseZoom);

    // Get the layers points of the current screen.
    glm::dvec2 topLeftPoint(0, 0);
    glm::dvec2 topRightPoint(_width, 0);
    glm::dvec2 bottomLeftPoint(0, _height);
    glm::dvec2 bottomRightPoint(_width, _height);

    // Translate the layer points to tile coordinates, then zoom them to baseZoom.
	TileCoordinate topLeft     = layerPointToTileCoordinate(topLeftPoint).zoomTo(baseZoom);
	TileCoordinate topRight    = layerPointToTileCoordinate(topRightPoint).zoomTo(baseZoom);
	TileCoordinate bottomLeft  = layerPointToTileCoordinate(bottomLeftPoint).zoomTo(baseZoom);
	TileCoordinate bottomRight = layerPointToTileCoordinate(bottomRightPoint).zoomTo(baseZoom);

	int minCol = std::floor(std::min(std::min(topLeft.getColumn(),
                                              topRight.getColumn()),
                                     std::min(bottomLeft.getColumn(),
                                              bottomRight.getColumn())));

	int maxCol = std::ceil(std::max(std::max(topLeft.getColumn(),
                                             topRight.getColumn()),
                                    std::max(bottomLeft.getColumn(),
                                             bottomRight.getColumn())));

	int minRow = std::floor(std::min(std::min(topLeft.getRow(),
                                              topRight.getRow()),
                                     std::min(bottomLeft.getRow(),
                                              bottomRight.getRow())));

	int maxRow = std::ceil(std::max(std::max(topLeft.getRow(),
                                             topRight.getRow()),
                                    std::max(bottomLeft.getRow(),
                                             bottomRight.getRow())));

	minCol -= _padColumn;
	minRow -= _padRow;
	maxCol += _padColumn;
	maxRow += _padRow;

    minCol = CLAMP(minCol, 0, gridSize);
    maxCol = CLAMP(maxCol, 0, gridSize);
    minRow = CLAMP(minRow, 0, gridSize);
    maxRow = CLAMP(maxRow, 0, gridSize);

    std::set<TileCoordinate> coordinatesToDraw;
    std::set<TileCoordinate> requestedCoordinates;

    // Collect visible tile coordinates.
	for (int col = minCol; col < maxCol; ++col)
    {
		for (int row = minRow; row < maxRow; ++row)
        {
			TileCoordinate coord(row, col, baseZoom);

            // Do we have this tile?
            if (!_loader.getTile(coord))
            {
                requestedCoordinates.insert(coord);

                // Since we don't have this coordinate, let's try to find an
                // existing coordinate at a different scale that we can use.

                // First we look backwards from the current zoom level.

                bool foundParent = false;

				for (int i = coord.getZoom(); i >= _provider->getMinZoom(); --i)
                {
					TileCoordinate parentCoord = coord.zoomTo(i);
                    parentCoord = parentCoord.getFloored();
                    parentCoord = parentCoord.getClamped();

                    if (!_loader.getTile(parentCoord))
                    {
                        // We don't have the parent coordinate, so let's request it.
                        requestedCoordinates.insert(parentCoord);
                    }
                    else
                    {
                        coordinatesToDraw.insert(parentCoord);
                        foundParent = true;
                        break;
                    }
				}
            }
            else
            {
                // We have it so
                coordinatesToDraw.insert(coord);
            }
        }
    }

// or if we have any of the children
//				if (!gotParent) {
//					Coordinate zoomed = coord.zoomBy(1).container();
//					std::vector<Coordinate> kids;
//					kids.push_back(zoomed);
//					kids.push_back(zoomed.right());
//					kids.push_back(zoomed.down());
//					kids.push_back(zoomed.right().down());
//					for (int i = 0; i < kids.size(); i++) {
//						if (images.count(kids[i]) > 0) {
//							visibleKeys.insert(kids[i]);
//						}
//					}            
//				}


    _loader.cancelQueued();

    std::vector<TileCoordinate> tilesToRequest(requestedCoordinates.begin(),
                                               requestedCoordinates.end());

    QueueSorter sorter(_center);

    std::sort(tilesToRequest.begin(),
              tilesToRequest.end(),
              sorter);

    std::vector<TileCoordinate>::iterator iter = tilesToRequest.begin();

    while (iter != tilesToRequest.end())
    {
        try
        {
            _loader.start(*iter, _provider->requestTile(*iter));
        }
        catch (const Poco::ExistsException& exc)
        {
        }

        ++iter;
    }

    return coordinatesToDraw;
}


TileCoordinate TileLayer::layerPointToTileCoordinate(const glm::dvec2& layerPoint) const
{
    TileCoordinate coord = getCenter();

    if (_provider != nullptr)
    {

        glm::dvec2 layerSize(_width, _height);

        glm::dvec2 tileSize(_provider->getTileWidth(),
                            _provider->getTileHeight());

        glm::dvec2 factor = (layerPoint - layerSize / 2.0) / tileSize;

        coord.x += factor.x;
        coord.y += factor.y;
    }
    else
    {
        ofLogError("TileLayer::layerPointToTileCoordinate") << "Provider is not defined.";
    }

    return coord;
}


void TileLayer::onTileCached(const TileCoordinate& args)
{
    _coordsDirty = true;
}


void TileLayer::onTileUncached(const TileCoordinate& args)
{
    _coordsDirty = true;
}


} } // namespace ofx::Maps
