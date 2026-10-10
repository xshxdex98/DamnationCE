/*
HUD_HIRES_TAGS.C

The bitmaps the high-res HUD's textures stand for (port/linux/src/hud_hires.c),
found in each map's tags as it loads (scenario_tags_load): every map holds
its own copy of the HUD's bitmap groups.

A bitmap's pixels are loaded into the texture cache's memory at its
base_address (xbox_texture_cache.c), which it keeps until its cache block is
reused (when cache_block_index and base_address are cleared). The texture
cache of the platform layer uploads the pixels at an address whenever they
are written there, and asks hud_hires_asset_at which bitmap they are: a block
reused for another bitmap, or a map unloaded, is a write, which asks again.

A texture drawn for only some of a bitmap's sprites (hud_msg_icons_sm's
buttons) does not stand for its bitmap: the game draws those sprites from a
placeholder (hud_hires_sprite_bitmap), a bitmap of the same size and format,
so the same coordinates, made when first needed and deleted with the map;
the texture cache draws the texture for it (hud_hires_placeholder_texture).
When the texture cannot be drawn, the sprites are drawn from their bitmap.
*/

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "bitmaps/bitmaps.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "cache/cache_files.h"

/* the platform layer's (port/linux/src) */
void platform_log(char const *format, ...);
unsigned long config_changes(void);
long hud_hires_asset_count(void);
char const *hud_hires_asset_tag(long asset);
long hud_hires_asset_bitmap(long asset);
long hud_hires_asset_fits(long asset, long width, long height);
int hud_hires_asset_custom_edition(long asset);
/* (custom_edition_cache.c) */
boolean custom_edition_cache_stock_tag(long tag_index);
unsigned long hud_hires_asset_sprites(long asset);
int hud_hires_sprites_drawable(long asset, unsigned long address, unsigned long level0_size);
void hud_hires_register_placeholder(long asset, unsigned long const *texture);

void hud_hires_tags_loaded(void);
void hud_hires_tags_unloaded(void);
long hud_hires_asset_at(unsigned long address, long width, long height);
long hud_hires_asset_after(unsigned long address, long width, long height, long asset);
struct bitmap_data const *hud_hires_sprite_bitmap(struct bitmap_data const *bitmap, short sequence_index);

/* ---------- constants */

enum
{
	/* (the HUD's, the menus' titles, and the menus' look's pictures) */
	MAXIMUM_HIRES_BITMAPS = 256,
	MAXIMUM_HIRES_SPRITE_BITMAPS = 8,
};

/* ---------- globals */

static struct
{
	struct bitmap_data *bitmap;
	long asset;
	/* a Custom Edition map's stock bitmap of a layout the texture was made
	for, whose pixels are its own: drawn without the pixels' check */
	boolean stock_custom_edition;
} hires_bitmaps[MAXIMUM_HIRES_BITMAPS];
static long hires_bitmap_count = 0;

/* the bitmaps some of whose sprites have a texture, with their placeholder,
and whether the texture can be drawn for the pixels at checked_address
(checked again when the texture cache loads them elsewhere, or the settings
change) */
static struct
{
	struct bitmap_data *bitmap;
	long asset;
	struct bitmap_data *placeholder;
	boolean placeholder_failed;
	void *checked_address;
	unsigned long checked_at;
	boolean drawable;
} hires_sprite_bitmaps[MAXIMUM_HIRES_SPRITE_BITMAPS];
static long hires_sprite_bitmap_count = 0;

/* ---------- private code */

static void hud_hires_sprite_bitmaps_forget(
	void)
{
	long index;

	for (index = 0; index < hires_sprite_bitmap_count; index++)
	{
		if (hires_sprite_bitmaps[index].placeholder)
		{
			hud_hires_register_placeholder(hires_sprite_bitmaps[index].asset, NULL);
			bitmap_delete(hires_sprite_bitmaps[index].placeholder);
		}
	}
	hires_sprite_bitmap_count = 0;

	return;
}

/* whether the pixels of a high-res bitmap (an index of hires_bitmaps) are in
the texture cache at address, at that size */
static boolean hires_bitmap_at(
	long index,
	unsigned long address,
	long width,
	long height)
{
	struct bitmap_data const *bitmap = hires_bitmaps[index].bitmap;

	return bitmap->cache_block_index != NONE && (unsigned long)bitmap->base_address == address &&
		bitmap->width == width && bitmap->height == height;
}

/* ---------- public code */

void hud_hires_tags_loaded(
	void)
{
	long asset_count = hud_hires_asset_count();
	long asset;
	long missing = 0;

	hires_bitmap_count = 0;
	hud_hires_sprite_bitmaps_forget();
	for (asset = 0; asset < asset_count && hires_bitmap_count < MAXIMUM_HIRES_BITMAPS; asset++)
	{
		long group_index = tag_loaded(BITMAP_GROUP_TAG, hud_hires_asset_tag(asset));
		struct bitmap_data *bitmap = group_index == NONE ? NULL :
			bitmap_group_try_and_get_bitmap(group_index, (short)hud_hires_asset_bitmap(asset));

		if (!bitmap)
		{
			/* (the main menu's map has only some of the HUD) */
			missing++;
		}
		else if (!hud_hires_asset_fits(asset, bitmap->width, bitmap->height))
		{
			platform_log("high-res hud: %s bitmap %ld is %dx%d here, which its texture does not fit",
				hud_hires_asset_tag(asset), hud_hires_asset_bitmap(asset), bitmap->width, bitmap->height);
		}
		else if (hud_hires_asset_sprites(asset))
		{
			if (hires_sprite_bitmap_count < MAXIMUM_HIRES_SPRITE_BITMAPS)
			{
				csmemset(&hires_sprite_bitmaps[hires_sprite_bitmap_count], 0, sizeof(hires_sprite_bitmaps[0]));
				hires_sprite_bitmaps[hires_sprite_bitmap_count].bitmap = bitmap;
				hires_sprite_bitmaps[hires_sprite_bitmap_count].asset = asset;
				hires_sprite_bitmap_count++;
			}
		}
		else
		{
			hires_bitmaps[hires_bitmap_count].bitmap = bitmap;
			hires_bitmaps[hires_bitmap_count].asset = asset;
			hires_bitmaps[hires_bitmap_count].stock_custom_edition =
				hud_hires_asset_custom_edition(asset) && custom_edition_cache_stock_tag(group_index);
			hires_bitmap_count++;
		}
	}
	platform_log("high-res hud: %ld of %ld bitmaps in this map, %ld for some of their sprites (%ld not in it)",
		hires_bitmap_count + hires_sprite_bitmap_count, asset_count, hires_sprite_bitmap_count, missing);

	return;
}

void hud_hires_tags_unloaded(
	void)
{
	hires_bitmap_count = 0;
	hud_hires_sprite_bitmaps_forget();

	return;
}

struct bitmap_data const *hud_hires_sprite_bitmap(
	struct bitmap_data const *bitmap,
	short sequence_index)
{
	long index;

	for (index = 0; index < hires_sprite_bitmap_count; index++)
	{
		struct bitmap_data *sprite_bitmap = hires_sprite_bitmaps[index].bitmap;

		if (sprite_bitmap != bitmap ||
			sequence_index < 0 ||
			sequence_index >= 32 ||
			!(hud_hires_asset_sprites(hires_sprite_bitmaps[index].asset) & (1UL << sequence_index)))
		{
			continue;
		}
		/* (checked on its pixels in the texture cache, once they are loaded) */
		if (sprite_bitmap->cache_block_index == NONE || !sprite_bitmap->base_address)
			break;
		if (hires_sprite_bitmaps[index].checked_address != sprite_bitmap->base_address ||
			hires_sprite_bitmaps[index].checked_at != config_changes())
		{
			hires_sprite_bitmaps[index].checked_address = sprite_bitmap->base_address;
			hires_sprite_bitmaps[index].checked_at = config_changes();
			hires_sprite_bitmaps[index].drawable = hud_hires_sprites_drawable(
				hires_sprite_bitmaps[index].asset,
				(unsigned long)sprite_bitmap->base_address,
				(unsigned long)bitmap_mipmap_get_pixel_data_size(sprite_bitmap, 0)) != 0;
		}
		if (hires_sprite_bitmaps[index].drawable &&
			!hires_sprite_bitmaps[index].placeholder &&
			!hires_sprite_bitmaps[index].placeholder_failed)
		{
			struct bitmap_data *placeholder = bitmap_2d_new(
				sprite_bitmap->width,
				sprite_bitmap->height,
				0,
				sprite_bitmap->format);

			if (placeholder && !rasterizer_bitmap_new(placeholder))
			{
				bitmap_delete(placeholder);
				placeholder = NULL;
			}
			if (placeholder)
			{
				hud_hires_register_placeholder(
					hires_sprite_bitmaps[index].asset,
					(unsigned long const *)placeholder->hardware_format);
			}
			else
			{
				platform_log("high-res hud: no placeholder for %s's sprites: drawn as they are",
					hud_hires_asset_tag(hires_sprite_bitmaps[index].asset));
				hires_sprite_bitmaps[index].placeholder_failed = TRUE;
			}
			hires_sprite_bitmaps[index].placeholder = placeholder;
		}
		if (hires_sprite_bitmaps[index].drawable && hires_sprite_bitmaps[index].placeholder)
			return hires_sprite_bitmaps[index].placeholder;
		break;
	}

	return bitmap;
}

/* whether the bitmap at address that `asset` stands for is a Custom Edition
map's stock one (its pixels are not those the texture was drawn from) */
long hud_hires_asset_stock_custom_edition(
	unsigned long address,
	long width,
	long height,
	long asset)
{
	long index;

	for (index = 0; index < hires_bitmap_count; index++)
	{
		if (hires_bitmaps[index].asset == asset && hires_bitmap_at(index, address, width, height))
			return hires_bitmaps[index].stock_custom_edition;
	}

	return FALSE;
}

long hud_hires_asset_at(
	unsigned long address,
	long width,
	long height)
{
	return hud_hires_asset_after(address, width, height, NONE);
}

/* the next texture after `asset` (NONE: the first) standing for the bitmap
whose pixels are at address: some bitmaps have one in each menus theme */
long hud_hires_asset_after(
	unsigned long address,
	long width,
	long height,
	long asset)
{
	long index;
	boolean passed = asset == NONE;

	for (index = 0; index < hires_bitmap_count; index++)
	{
		if (!passed)
			passed = hires_bitmaps[index].asset == asset;
		else if (hires_bitmap_at(index, address, width, height))
			return hires_bitmaps[index].asset;
	}

	return NONE;
}

/* the bitmap tag whose pixels the texture cache holds at `address` (its
base_address), by name; NULL for none (debug.gpu_trace_heavy, d3d8_gl.c) */
char const *bitmap_tag_name_at(
	unsigned long address)
{
	struct tag_iterator iterator;
	long tag_index;

	tag_iterator_new(&iterator, BITMAP_GROUP_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct bitmap_group *group = bitmap_group_get(tag_index);
		long index;

		for (index = 0; index < group->bitmaps.count; index++)
		{
			struct bitmap_data *bitmap = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, index, struct bitmap_data);

			if (bitmap->cache_block_index != NONE && (unsigned long)bitmap->base_address == address)
				return tag_get_name(tag_index);
		}
	}

	return NULL;
}
