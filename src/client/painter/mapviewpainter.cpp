/*
 * Copyright (c) 2010-2020 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <client/painter/mapviewpainter.h>
#include <client/painter/tilepainter.h>
#include <client/painter/thingpainter.h>
#include <client/painter/lightviewpainter.h>
#include <client/map/map.h>
#include <client/game.h>
#include <client/thing/missile.h>
#include <client/manager/shadermanager.h>

#include <framework/core/declarations.h>
#include <framework/graphics/framebuffermanager.h>
#include <framework/graphics/graphics.h>
#include <framework/graphics/drawpool.h>

void MapViewPainter::draw(const MapViewPtr& mapView, const Rect& rect)
{
	// update visible tiles cache when needed
	if(mapView->m_mustUpdateVisibleTilesCache)
		mapView->updateVisibleTilesCache();

	if(mapView->m_rectCache.rect != rect) {
		mapView->m_rectCache.rect = rect;
		mapView->m_rectCache.srcRect = mapView->calcFramebufferSource(rect.size());
		mapView->m_rectCache.drawOffset = mapView->m_rectCache.srcRect.topLeft();
		mapView->m_rectCache.horizontalStretchFactor = rect.width() / static_cast<float>(mapView->m_rectCache.srcRect.width());
		mapView->m_rectCache.verticalStretchFactor = rect.height() / static_cast<float>(mapView->m_rectCache.srcRect.height());
	}

	const Position cameraPosition = mapView->getCameraPosition();

	if(g_drawPool.drawUp(DRAWTYPE_MAP, mapView->m_rectDimension.size(), mapView->m_rectCache.rect, mapView->m_rectCache.srcRect)) {
		const auto& lightView = mapView->m_drawLights ? mapView->m_lightView.get() : nullptr;
		for(int_fast8_t z = mapView->m_floorMax; z >= mapView->m_floorMin; --z) {
			if(lightView) {
				const int8 nextFloor = z - 1;
				if(nextFloor >= mapView->m_floorMin) {
					lightView->setFloor(nextFloor);
					for(const auto& tile : mapView->m_cachedVisibleTiles[nextFloor]) {
						const auto& ground = tile->getGround();
						if(ground && !ground->isTranslucent()) {
							auto pos2D = mapView->transformPositionTo2D(tile->getPosition(), cameraPosition);
							if(ground->isTopGround()) {
								const auto currentPos = tile->getPosition();
								for(const auto& pos : currentPos.translatedToDirections({ Otc::South, Otc::East })) {
									const auto& nextDownTile = g_map.getTile(pos);
									if(nextDownTile && nextDownTile->hasGround() && !nextDownTile->isTopGround()) {
										lightView->setShade(pos2D);
										break;
									}
								}

								pos2D -= mapView->m_tileSize;
							}

							lightView->setShade(pos2D);
						}
					}
				}
			}

			mapView->onFloorDrawingStart(z);

			if(lightView) lightView->setFloor(z);

			for(const auto& tile : mapView->m_cachedVisibleTiles[z]) {
				if(!canRenderTile(mapView, tile, mapView->m_viewport, lightView)) continue;

				TilePainter::drawStart(tile, mapView);
				TilePainter::drawGround(tile, mapView->transformPositionTo2D(tile->getPosition(), cameraPosition), mapView->m_scaleFactor, Otc::FUpdateAll, lightView);
				TilePainter::drawEnd(tile, mapView);
			}

			for(const auto& tile : mapView->m_cachedVisibleTiles[z]) {
				if(!canRenderTile(mapView, tile, mapView->m_viewport, lightView)) continue;

				TilePainter::drawStart(tile, mapView);
				TilePainter::drawBottom(tile, mapView->transformPositionTo2D(tile->getPosition(), cameraPosition), mapView->m_scaleFactor, Otc::FUpdateAll, lightView);
				TilePainter::drawTop(tile, mapView->transformPositionTo2D(tile->getPosition(), cameraPosition), mapView->m_scaleFactor, Otc::FUpdateAll, lightView);
				TilePainter::drawEnd(tile, mapView);
			}

			for(const MissilePtr& missile : g_map.getFloorMissiles(z)) {
				ThingPainter::draw(missile, mapView->transformPositionTo2D(missile->getPosition(), cameraPosition), mapView->m_scaleFactor, Otc::FUpdateAll, lightView);
			}

			mapView->onFloorDrawingEnd(z);
		}

		if(mapView->m_crosshairTexture && mapView->m_mousePosition.isValid()) {
			const Point& point = mapView->transformPositionTo2D(mapView->m_mousePosition, cameraPosition);
			if(mapView->m_crosshairEffect && mapView->m_crosshairEffect->getId() > 0) {
				ThingPainter::draw(mapView->m_crosshairEffect, point, mapView->m_scaleFactor, Otc::FUpdateThing, nullptr);
				g_painter->setOpacity(.65);
			}

			const auto crosshairRect = Rect(point, mapView->m_tileSize, mapView->m_tileSize);
			g_drawPool.addTexturedRect(crosshairRect, mapView->m_crosshairTexture);
			g_painter->resetOpacity();
		}
	}

	/*float fadeOpacity = 1.0f;
	if(!mapView->m_shaderSwitchDone && mapView->m_fadeOutTime > 0) {
			fadeOpacity = 1.0f - (mapView->m_fadeTimer.timeElapsed() / mapView->m_fadeOutTime);
			if(fadeOpacity < 0.0f) {
					mapView->m_shader = mapView->m_nextShader;
					mapView->m_nextShader = nullptr;
					mapView->m_shaderSwitchDone = true;
					mapView->m_fadeTimer.restart();
			}
	}

	if(mapView->m_shaderSwitchDone && mapView->m_shader && mapView->m_fadeInTime > 0)
			fadeOpacity = std::min<float>(mapView->m_fadeTimer.timeElapsed() / mapView->m_fadeInTime, 1.0f);

	if(mapView->m_shader && g_painter->hasShaders() && g_graphics.shouldUseShaders() && mapView->m_viewMode == MapView::NEAR_VIEW) {
			const Point center = mapView->m_rectCache.srcRect.center();
			const Point globalCoord = Point(cameraPosition.x - mapView->m_drawDimension.width() / 2, -(cameraPosition.y - mapView->m_drawDimension.height() / 2)) * mapView->m_tileSize;
			mapView->m_shader->bind();
			mapView->m_shader->setUniformValue(ShaderManager::MAP_CENTER_COORD, center.x / static_cast<float>(mapView->m_rectDimension.width()), 1.0f - center.y / static_cast<float>(mapView->m_rectDimension.height()));
			mapView->m_shader->setUniformValue(ShaderManager::MAP_GLOBAL_COORD, globalCoord.x / static_cast<float>(mapView->m_rectDimension.height()), globalCoord.y / static_cast<float>(mapView->m_rectDimension.height()));
			mapView->m_shader->setUniformValue(ShaderManager::MAP_ZOOM, mapView->m_scaleFactor);
			g_painter->setShaderProgram(mapView->m_shader);
	}

	g_painter->setOpacity(fadeOpacity);*/

	// this could happen if the player position is not known yet
	//if(!cameraPosition.isValid())
			//return;

	if(mapView->m_drawLights && g_drawPool.drawUp(DRAWTYPE_LIGHT, mapView->m_rectDimension.size(), mapView->m_rectCache.rect, mapView->m_rectCache.srcRect)) {
		LightViewPainter::draw(mapView->m_lightView);
	}

	drawCreatureInformation(mapView);
	drawText(mapView);
}

void MapViewPainter::drawCreatureInformation(const MapViewPtr& mapView)
{
	if(!mapView->m_drawNames && !mapView->m_drawHealthBars && !mapView->m_drawManaBar) return;

	if(g_drawPool.drawUp(DRAWTYPE_CREATURE_INFORMATION, g_graphics.getViewportSize()))
	{
		const Position cameraPosition = mapView->getCameraPosition();

		uint32_t flags = 0;
		if(mapView->m_drawNames) { flags = Otc::DrawNames; }
		if(mapView->m_drawHealthBars) { flags |= Otc::DrawBars; }
		if(mapView->m_drawManaBar) { flags |= Otc::DrawManaBar; }

		for(const auto& creature : mapView->m_visibleCreatures) {
			CreaturePainter::drawInformation(creature, mapView->m_rectCache.rect, mapView->transformPositionTo2D(creature->getPosition(), cameraPosition), mapView->m_scaleFactor, mapView->m_rectCache.drawOffset, mapView->m_rectCache.horizontalStretchFactor, mapView->m_rectCache.verticalStretchFactor, flags);
		}
	}
}

void MapViewPainter::drawText(const MapViewPtr& mapView)
{
	if(!mapView->m_drawTexts) return;

	const Position cameraPosition = mapView->getCameraPosition();

	if(!g_map.getStaticTexts().empty() && g_drawPool.drawUp(DRAWTYPE_STATIC_TEXT, g_graphics.getViewportSize())) {
		for(const StaticTextPtr& staticText : g_map.getStaticTexts()) {
			const Position pos = staticText->getPosition();

			if(pos.z != cameraPosition.z && staticText->getMessageMode() == Otc::MESSAGE_NONE)
				continue;

			Point p = mapView->transformPositionTo2D(pos, cameraPosition) - mapView->m_rectCache.drawOffset;
			p.x *= mapView->m_rectCache.horizontalStretchFactor;
			p.y *= mapView->m_rectCache.verticalStretchFactor;
			p += mapView->m_rectCache.rect.topLeft();
			ThingPainter::drawText(staticText, p, mapView->m_rectCache.rect);
		}
	}

	if(!g_map.getAnimatedTexts().empty() && g_drawPool.drawUp(DRAWTYPE_DYNAMIC_TEXT, g_graphics.getViewportSize()))
	{
		for(const AnimatedTextPtr& animatedText : g_map.getAnimatedTexts()) {
			const Position pos = animatedText->getPosition();

			if(pos.z != cameraPosition.z)
				continue;

			Point p = mapView->transformPositionTo2D(pos, cameraPosition) - mapView->m_rectCache.drawOffset;
			p.x *= mapView->m_rectCache.horizontalStretchFactor;
			p.y *= mapView->m_rectCache.verticalStretchFactor;
			p += mapView->m_rectCache.rect.topLeft();

			ThingPainter::drawText(animatedText, p, mapView->m_rectCache.rect);
		}
	}
}

bool MapViewPainter::canRenderTile(const MapViewPtr& mapView, const TilePtr& tile, const AwareRange& viewPort, LightView* lightView)
{
	if(mapView->m_drawViewportEdge || (lightView && lightView->isDark() && tile->hasLight())) return true;

	const Position cameraPosition = mapView->getCameraPosition();
	const Position& tilePos = tile->getPosition();

	const int8 dz = tilePos.z - cameraPosition.z;
	const Position checkPos = tilePos.translated(dz, dz);

	// Check for non-visible tiles on the screen and ignore them
	{
		if((cameraPosition.x - checkPos.x >= viewPort.left) || (checkPos.x - cameraPosition.x == viewPort.right && !tile->hasWideThings() && !tile->hasDisplacement()))
			return false;

		if((cameraPosition.y - checkPos.y >= viewPort.top) || (checkPos.y - cameraPosition.y == viewPort.bottom && !tile->hasTallThings() && !tile->hasDisplacement()))
			return false;

		if((checkPos.x - cameraPosition.x > viewPort.right && (!tile->hasWideThings() || !tile->hasDisplacement())) || (checkPos.y - cameraPosition.y > viewPort.bottom))
			return false;
	}

	return true;
}
