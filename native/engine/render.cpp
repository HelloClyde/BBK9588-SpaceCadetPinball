#include "pch.h"
#include "render.h"

#include "GroupData.h"
#include "pb.h"
#include "score.h"
#include "TPinballTable.h"
#include "proj.h"

std::vector<render_sprite*> render::sprite_list, render::ball_list;
zmap_header_type* render::background_zmap;
int render::zmap_offsetX, render::zmap_offsetY, render::offset_x, render::offset_y;
rectangle_type render::vscreen_rect;
gdrv_bitmap8 *render::vscreen, *render::background_bitmap, *render::ball_bitmap[20];
zmap_header_type* render::zscreen;

render_sprite::render_sprite(VisualTypes visualType, gdrv_bitmap8* bmp, zmap_header_type* zMap,
	int xPosition, int yPosition, rectangle_type* boundingRect)
{
	Bmp = bmp;
	ZMap = zMap;
	VisualType = visualType;
	DeleteFlag = false;
	OccludedSprites = nullptr;
	DirtyRect = rectangle_type{};
	DirtyFlag = visualType != VisualTypes::Ball;
	ZMapOffestX = 0;
	ZMapOffestY = 0;
	Depth = 0xffFF;

	if (boundingRect)
	{
		BoundingRect = *boundingRect;
	}
	else
	{
		BoundingRect.Width = -1;
		BoundingRect.Height = -1;
		BoundingRect.XPosition = 0;
		BoundingRect.YPosition = 0;
	}

	BmpRect.XPosition = xPosition;
	BmpRect.YPosition = yPosition;
	if (bmp)
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
	else
	{
		BmpRect.Width = 0;
		BmpRect.Height = 0;
	}
	DirtyRectPrev = BmpRect;

	if (!ZMap && VisualType != VisualTypes::Ball)
	{
		assertm(false, "Background zMap should not be used");
		ZMap = render::background_zmap;
		ZMapOffestY = xPosition - render::zmap_offsetX;
		ZMapOffestX = yPosition - render::zmap_offsetY;
	}

	render::AddSprite(*this);
}

render_sprite::~render_sprite()
{
	render::RemoveSprite(*this);
	delete OccludedSprites;
}

void render_sprite::set(gdrv_bitmap8* bmp, zmap_header_type* zMap, int xPos, int yPos)
{
	if (Bmp == bmp && ZMap == zMap && BmpRect.XPosition == xPos && BmpRect.YPosition == yPos)
		return;

	Bmp = bmp;
	ZMap = zMap;
	DirtyFlag = VisualType != VisualTypes::Ball;
	BmpRect.XPosition = xPos;
	BmpRect.YPosition = yPos;
	if (bmp)
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
}

void render_sprite::ball_set(gdrv_bitmap8* bmp, float depth, int xPos, int yPos)
{
	set(bmp, ZMap,xPos, yPos);
	Depth = proj::NormalizeDepth(depth);
}

void render::init(gdrv_bitmap8* bmp, int width, int height)
{
	vscreen = new gdrv_bitmap8(width, height, false);
	zscreen = new zmap_header_type(width, height, width);
	zdrv::fill(zscreen, zscreen->Width, zscreen->Height, 0, 0, 0xFFFF);
	vscreen_rect.YPosition = 0;
	vscreen_rect.XPosition = 0;
	vscreen_rect.Width = width;
	vscreen_rect.Height = height;
	vscreen->YPosition = 0;
	vscreen->XPosition = 0;
	for (auto& ballBmp : ball_bitmap)
		ballBmp = new gdrv_bitmap8(64, 64, false);

	background_bitmap = bmp;
	if (bmp)
		gdrv::copy_bitmap(vscreen, width, height, 0, 0, bmp, 0, 0);
	else
		gdrv::fill_bitmap(vscreen, vscreen->Width, vscreen->Height, 0, 0, 0);

	recreate_screen_texture();
}

void render::uninit()
{
	delete vscreen;
	delete zscreen;

	// Sprite destructor removes it from the list.
	while (!sprite_list.empty())
		delete sprite_list[0];
	while (!ball_list.empty())
		delete ball_list[0];
	for (auto& ballBmp : ball_bitmap)
		delete ballBmp;
}

void render::recreate_screen_texture()
{
	// No GPU texture; native frontend scales vscreen CPU pixels.
}

void render::update()
{
	unpaint_balls();

	// Clip dirty sprites with vScreen, clear clipping (dirty) rectangles
	for (const auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		bool clearSprite = false;
		switch (sprite->VisualType)
		{
		case VisualTypes::Sprite:
			if (sprite->DirtyRectPrev.Width > 0)
				maths::enclosing_box(sprite->DirtyRectPrev, sprite->BmpRect, sprite->DirtyRect);

			if (maths::rectangle_clip(sprite->DirtyRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = true;
			else
				sprite->DirtyRect.Width = -1;
			break;
		case VisualTypes::Background:
			if (maths::rectangle_clip(sprite->BmpRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = !sprite->Bmp;
			else
				sprite->DirtyRect.Width = -1;
			break;
		default: break;
		}

		if (clearSprite)
		{
			auto yPos = sprite->DirtyRect.YPosition;
			auto width = sprite->DirtyRect.Width;
			auto xPos = sprite->DirtyRect.XPosition;
			auto height = sprite->DirtyRect.Height;
			zdrv::fill(zscreen, width, height, xPos, yPos, 0xFFFF);
			if (background_bitmap)
				gdrv::copy_bitmap(vscreen, width, height, xPos, yPos, background_bitmap, xPos, yPos);
			else
				gdrv::fill_bitmap(vscreen, width, height, xPos, yPos, 0);
		}
	}

	// Paint dirty rectangles of dirty sprites
	for (auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		repaint(*sprite);
		sprite->DirtyFlag = false;
		sprite->DirtyRectPrev = sprite->DirtyRect;
		if (sprite->DeleteFlag)
			delete sprite;
	}

	paint_balls();
}

void render::AddSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	list.push_back(&sprite);
}

void render::RemoveSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	auto it = std::find(list.begin(), list.end(), &sprite);
	if (it != list.end())
		list.erase(it);
}

void render::set_background_zmap(zmap_header_type* zMap, int offsetX, int offsetY)
{
	background_zmap = zMap;
	zmap_offsetX = offsetX;
	zmap_offsetY = offsetY;
}

void render::repaint(const render_sprite& sprite)
{
	rectangle_type clipRect{};
	if (!sprite.OccludedSprites || sprite.VisualType == VisualTypes::Ball || sprite.DirtyRect.Width <= 0)
		return;

	for (auto refSprite : *sprite.OccludedSprites)
	{
		if (!refSprite->DeleteFlag && refSprite->Bmp)
		{
			if (maths::rectangle_clip(refSprite->BmpRect, sprite.DirtyRect, &clipRect))
				zdrv::paint(
					clipRect.Width,
					clipRect.Height,
					vscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					zscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					refSprite->Bmp,
					clipRect.XPosition - refSprite->BmpRect.XPosition,
					clipRect.YPosition - refSprite->BmpRect.YPosition,
					refSprite->ZMap,
					clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
					clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition);
		}
	}
}


void render::paint_balls()
{
	// Sort ball sprites by ascending depth
	std::sort(ball_list.begin(), ball_list.end(), [](const render_sprite* lhs, const render_sprite* rhs)
	{
		return lhs->Depth < rhs->Depth;
	});

	// For balls that clip vScreen: save original vScreen contents and paint ball bitmap.
	for (auto index = 0u; index < ball_list.size(); ++index)
	{
		auto ball = ball_list[index];
		auto dirty = &ball->DirtyRect;
		if (ball->Bmp && maths::rectangle_clip(ball->BmpRect, vscreen_rect, &ball->DirtyRect))
		{
			int xPos = dirty->XPosition;
			int yPos = dirty->YPosition;
			gdrv::copy_bitmap(ball_bitmap[index], dirty->Width, dirty->Height, 0, 0, vscreen, xPos, yPos);
			zdrv::paint_flat(
				dirty->Width,
				dirty->Height,
				vscreen,
				xPos,
				yPos,
				zscreen,
				xPos,
				yPos,
				ball->Bmp,
				xPos - ball->BmpRect.XPosition,
				yPos - ball->BmpRect.YPosition,
				ball->Depth);
		}
		else
		{
			dirty->Width = -1;
		}
	}
}

void render::unpaint_balls()
{
	// Restore portions of vScreen saved during previous paint_balls call.
	for (int index = static_cast<int>(ball_list.size()) - 1; index >= 0; index--)
	{
		auto curBall = ball_list[index];
		if (curBall->DirtyRect.Width > 0)
			gdrv::copy_bitmap(
				vscreen,
				curBall->DirtyRect.Width,
				curBall->DirtyRect.Height,
				curBall->DirtyRect.XPosition,
				curBall->DirtyRect.YPosition,
				ball_bitmap[index],
				0,
				0);

		curBall->DirtyRectPrev = curBall->DirtyRect;
	}
}

void render::shift(int offsetX, int offsetY)
{
	offset_x += offsetX;
	offset_y += offsetY;
}

void render::build_occlude_list()
{
	std::vector<render_sprite*>* spriteArr = nullptr;
	for (auto mainSprite : sprite_list)
	{
		if (mainSprite->OccludedSprites)
		{
			delete mainSprite->OccludedSprites;
			mainSprite->OccludedSprites = nullptr;
		}

		if (!mainSprite->DeleteFlag && mainSprite->BoundingRect.Width != -1)
		{
			if (!spriteArr)
				spriteArr = new std::vector<render_sprite*>();

			for (auto refSprite : sprite_list)
			{
				if (!refSprite->DeleteFlag
					&& refSprite->BoundingRect.Width != -1
					&& maths::rectangle_clip(mainSprite->BoundingRect, refSprite->BoundingRect, nullptr))
				{
					spriteArr->push_back(refSprite);
				}
			}

			if (mainSprite->Bmp && spriteArr->size() < 2)
				spriteArr->clear();
			if (!spriteArr->empty())
			{
				mainSprite->OccludedSprites = spriteArr;
				spriteArr = nullptr;
			}
		}
	}

	delete spriteArr;
}

void render::SpriteViewer(bool* show)
{
	(void)show;
}

void render::PresentVScreen()
{
	// The 9588 loop presents vscreen using its native compatible context.
}
