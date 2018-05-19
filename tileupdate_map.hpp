static void resolve_color(int fg, int bg, int bold, struct texture_fullid &ret)
{
    if (fg >= 100)
    {
        fg = (fg-100) % df::global::world->raws.descriptors.colors.size();
        df::descriptor_color *fgdc = df::global::world->raws.descriptors.colors[fg];

        ret.r = fgdc->red;
        ret.g = fgdc->green;
        ret.b = fgdc->blue;
    }
    else
    {
        fg = (fg + bold * 8) % 16;

        ret.r = enabler->ccolor[fg][0];
        ret.g = enabler->ccolor[fg][1];
        ret.b = enabler->ccolor[fg][2];
    }

    if (bg >= 100)
    {
        bg = (bg-100) % df::global::world->raws.descriptors.colors.size();
        df::descriptor_color *bgdc = df::global::world->raws.descriptors.colors[bg];

        ret.br = bgdc->red;
        ret.bg = bgdc->green;
        ret.bb = bgdc->blue;
    }
    else
    {
        bg = bg % 16;

        ret.br = enabler->ccolor[bg][0];
        ret.bg = enabler->ccolor[bg][1];
        ret.bb = enabler->ccolor[bg][2];
    }    
}

static void screen_to_texid_map(renderer_cool *r, int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    const long texpos = gscreentexpos[tile];

    if (!(texpos && init->display.flag.is_set(init_display_flags::USE_GRAPHICS)))
    {
        ret.texpos = map_texpos[ch];
        ret.bg_texpos = tilesets[0].bg_texpos[ch];
        ret.top_texpos = tilesets[0].top_texpos[ch];

        resolve_color(fg, bg, bold, ret);
        return;
    }        

    ret.texpos = texpos;
    ret.bg_texpos = unit_transparency ? transparent_texpos : white_texpos;
    ret.top_texpos = transparent_texpos;

    if (gscreentexpos_grayscale[tile])
    {
        const unsigned char cf = gscreentexpos_cf[tile];
        const unsigned char cbr = gscreentexpos_cbr[tile];

        resolve_color(cf, cbr, 0, ret);
    }
    else if (gscreentexpos_addcolor[tile])
        resolve_color(fg, bg, bold, ret);
    else
    {
        ret.r = ret.g = ret.b = 1;
        ret.br = ret.bg = ret.bb = 0;
    }
}

static void screen_to_texid_under(renderer_cool *r, int tile, struct texture_fullid &ret)
{
    const unsigned char *s = gscreen_under + tile*4;

    int ch   = s[0];
    int fg   = s[1];
    int bg   = s[2];
    int bold = s[3] & 0x0f;

    ret.texpos = map_texpos[ch];
    ret.bg_texpos = tilesets[0].bg_texpos[ch];
    ret.top_texpos = tilesets[0].top_texpos[ch];

    resolve_color(fg, bg, bold, ret);
}

static void apply_override (texture_fullid &ret, override &o, unsigned int seed)
{
    if (o.small_texpos.size())
    {
        switch (o.multi)
        {
        case multi_none:
        default:
            ret.texpos = o.get_small_texpos(seed);
            ret.bg_texpos = o.get_bg_texpos(seed);
            ret.top_texpos = o.get_top_texpos(seed);
            break;
        }
    }

    if (o.bg != -1)
    {
        ret.br = enabler->ccolor[o.bg][0];
        ret.bg = enabler->ccolor[o.bg][1];
        ret.bb = enabler->ccolor[o.bg][2];        
    }

    if (o.fg != -1)
    {
        ret.r = enabler->ccolor[o.fg][0];
        ret.g = enabler->ccolor[o.fg][1];
        ret.b = enabler->ccolor[o.fg][2];        
    }
}

static df::tiletype get_tiletype(int xx, int yy, int zz) {
    if (xx >= 0 && xx < 16 * world->map.x_count_block &&
        yy >= 0 && yy < 16 * world->map.y_count_block &&
        zz >= 0 && zz < 16 * world->map.z_count_block) {
        df::map_block *block = my_block_index[(xx>>4)*world->map.y_count_block*world->map.z_count_block + (yy>>4)*world->map.z_count_block + zz];//[xx>>4][yy>>4][zz];
        if (block)
        {
            return block->tiletype[xx&15][yy&15];
        }
    }
    return df::tiletype::Void;
}

static void write_tile_arrays_map(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;        
    screen_to_texid_map(r, tile, ret);
    
    if (has_overrides && my_block_index)
    {
        const unsigned char *s = gscreen + tile*4;
        int s0 = s[0];

        if (overrides[s0])
        {
            int xx = gwindow_x + x;
            int yy = gwindow_y + y;

            if (xx >= 0 && yy >= 0 && xx < world->map.x_count && yy < world->map.y_count)
            {
                if (s0 == 88 && df::global::cursor->x == xx && df::global::cursor->y == yy)
                {
                    long texpos = cursor_small_texpos;
                    if (texpos)
                        ret.texpos = texpos;
                }
                else
                {
                    int zz = gwindow_z - ((s[3]&0xf0)>>4);

                    tile_overrides *to = overrides[s0];

                    // Items
                    for (auto it = to->item_overrides.begin(); it != to->item_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->items.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::item *item = *it2;
                            if (!(zz == item->pos.z && xx == item->pos.x && yy == item->pos.y))
                                continue;
                            if (item->flags.whole & bad_item_flags.whole)
                                continue;

                            MaterialInfo mat_info(item->getMaterial(), item->getMaterialIndex());


                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && item->getType() != o.type)
                                    continue;
                                if (o.subtype != -1 && item->getSubtype() != o.subtype)
                                    continue;

                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }

                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;

                                apply_override(ret, o, item->id);
                                goto matched;
                            }
                        }
                    }

                    // Buildings
                    for (auto it = to->building_overrides.begin(); it != to->building_overrides.end(); it++)
                    {
                        override_group &og = *it;

                        auto &ilist = world->buildings.other[og.other_id];
                        for (auto it2 = ilist.begin(); it2 != ilist.end(); it2++)
                        {
                            df::building *bld = *it2;
                            if (zz != bld->z || xx < bld->x1 || xx > bld->x2 || yy < bld->y1 || yy > bld->y2)
                                continue;

                            MaterialInfo mat_info(bld->mat_type, bld->mat_index);

                            for (auto it3 = og.overrides.begin(); it3 != og.overrides.end(); it3++)
                            {
                                override &o = *it3;

                                if (o.type != -1 && bld->getType() != o.type)
                                    continue;
                                
                                if (o.subtype != -1)
                                {
                                    int subtype = (og.other_id == buildings_other_id::WORKSHOP_CUSTOM || og.other_id == buildings_other_id::FURNACE_CUSTOM) ?
                                        bld->getCustomType() : bld->getSubtype();

                                    if (subtype != o.subtype)
                                        continue;
                                }
                                if (o.mat_flag != -1)
                                {
                                    if (!mat_info.material)
                                        continue;
                                    if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                        continue;
                                }

                                if (!o.material_matches(mat_info.type, mat_info.index))
                                    continue;

                                apply_override(ret, o, bld->id);
                                goto matched;
                            }
                        }
                    }

                    // Tile types
                    df::map_block *block = my_block_index[(xx>>4)*world->map.y_count_block*world->map.z_count_block + (yy>>4)*world->map.z_count_block + zz];//[xx>>4][yy>>4][zz];
                    if (block)
                    {
                        int tiletype = block->tiletype[xx&15][yy&15];

                        df::tiletype tt = (df::tiletype)tiletype;
                        uint8_t walkmask = 0;

                        t_matpair mat(-1,-1);

                        if (to->has_material_overrides && Maps::IsValid())
                        {
                            if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                            {
                                //material is ice.
                                mat.mat_index = 6;
                                mat.mat_type = -1;
                            }
                            else
                            {
                                if (!r->map_cache)
                                    r->map_cache = new MapExtras::MapCache();
                                mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                            }
                        }
                        MaterialInfo mat_info(mat);

                        //if ((tt == df::tiletype::StoneFortification) ||
                        //    (tt >= df::tiletype::StonePillar && tt <= df::tiletype::FrozenPillar) ||
                        //    (tt >= df::tiletype::StoneWallWorn1 && tt <= df::tiletype::StoneWallWorn3) ||
                        //    (tt == df::tiletype::StoneWall) ||
                        //    (tt >= df::tiletype::LavaWallSmoothRD2 && tt <= df::tiletype::FeatureWall) ||
                        //    (tt >= df::tiletype::FrozenFortification && tt <= df::tiletype::FrozenWall) ||
                        //    (tt >= df::tiletype::MineralWallSmoothRD2 && tt <= df::tiletype::MineralWall) ||
                        //    (tt >= df::tiletype::FrozenWallSmoothRD2 && tt <= df::tiletype::FrozenWallSmoothLR) ||
                        //    (tt >= df::tiletype::ConstructedFortification && tt <= df::tiletype::ConstructedWallLR)) {
                            // TODO(db48x): possibly we should just compute the walkmask for every tile type...
                            for (int j = -1, n = 7; j <= 1; j++) {
                                for (int i = -1; i <= 1; i++) {
                                    if (!(i == 0 && j == 0)) {
                                        walkmask |= DFHack::isWalkable(get_tiletype(xx+i, yy+j, zz)) << n--;
                                    }
                                }
                            }
                        //}

                        for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                        {
                            override &o = *it3;

                            if (tiletype != o.type)
                                continue;

                            if (o.mat_flag != -1)
                            {
                                if (!mat_info.material)
                                    continue;
                                if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                    continue;
                            }

                            if (!o.material_matches(mat_info.type, mat_info.index))
                                continue;

                            if (!o.walkmask_matches(walkmask))
                                continue;

                            apply_override(ret, o, coord_hash(xx,yy,zz));
                            goto matched;
                        }
                    }
                }
            }
        }
    }
    matched:;

    // Set colour
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.r;
        *(fg++) = ret.g;
        *(fg++) = ret.b;
        *(fg++) = 1;
        
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;

        fg_top += 8;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
    }    
    
    // Set texture coordinates
    {
        long texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
        *(tex++) = txt[texpos].left;   // Upper left
        *(tex++) = txt[texpos].bottom;
        *(tex++) = txt[texpos].right;  // Upper right
        *(tex++) = txt[texpos].bottom;
        *(tex++) = txt[texpos].left;   // Lower left
        *(tex++) = txt[texpos].top;
        
        *(tex++) = txt[texpos].left;   // Lower left
        *(tex++) = txt[texpos].top;
        *(tex++) = txt[texpos].right;  // Upper right
        *(tex++) = txt[texpos].bottom;
        *(tex++) = txt[texpos].right;  // Lower right
        *(tex++) = txt[texpos].top;
    }

    // Set bg texture coordinates
    {
        long texpos = ret.bg_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
        *(tex_bg++) = txt[texpos].left;   // Upper left
        *(tex_bg++) = txt[texpos].bottom;
        *(tex_bg++) = txt[texpos].right;  // Upper right
        *(tex_bg++) = txt[texpos].bottom;
        *(tex_bg++) = txt[texpos].left;   // Lower left
        *(tex_bg++) = txt[texpos].top;
        
        *(tex_bg++) = txt[texpos].left;   // Lower left
        *(tex_bg++) = txt[texpos].top;
        *(tex_bg++) = txt[texpos].right;  // Upper right
        *(tex_bg++) = txt[texpos].bottom;
        *(tex_bg++) = txt[texpos].right;  // Lower right
        *(tex_bg++) = txt[texpos].top;
    }

    // Set top texture coordinates
    {
        long texpos = ret.top_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
        *(tex_top++) = txt[texpos].left;   // Upper left
        *(tex_top++) = txt[texpos].bottom;
        *(tex_top++) = txt[texpos].right;  // Upper right
        *(tex_top++) = txt[texpos].bottom;
        *(tex_top++) = txt[texpos].left;   // Lower left
        *(tex_top++) = txt[texpos].top;
        
        *(tex_top++) = txt[texpos].left;   // Lower left
        *(tex_top++) = txt[texpos].top;
        *(tex_top++) = txt[texpos].right;  // Upper right
        *(tex_top++) = txt[texpos].bottom;
        *(tex_top++) = txt[texpos].right;  // Lower right
        *(tex_top++) = txt[texpos].top;
    }    
}

static void write_tile_arrays_under(renderer_cool *r, int x, int y, GLfloat *fg, GLfloat *bg, GLfloat *tex, GLfloat *tex_bg, GLfloat *fg_top, GLfloat *tex_top)
{
    struct texture_fullid ret;
    const int tile = x * r->gdimy + y;        
    screen_to_texid_under(r, tile, ret);

    if (has_overrides && my_block_index)
    {
        const unsigned char *s = gscreen_under + tile*4;
        int s0 = s[0];

        if (overrides[s0])
        {
            int xx = gwindow_x + x;
            int yy = gwindow_y + y;

            if (xx >= 0 && yy >= 0 && xx < world->map.x_count && yy < world->map.y_count)
            {
                int zz = gwindow_z - ((s[3]&0xf0)>>4);

                tile_overrides *to = overrides[s0];

                // Tile types
                df::map_block *block = my_block_index[(xx>>4)*world->map.y_count_block*world->map.z_count_block + (yy>>4)*world->map.z_count_block + zz];//[xx>>4][yy>>4][zz];
                if (block)
                {
                    int tiletype = block->tiletype[xx&15][yy&15];

                    df::tiletype tt = (df::tiletype)tiletype;

                    t_matpair mat(-1, -1);

                    if (to->has_material_overrides && Maps::IsValid())
                    {
                        if (tileMaterial(tt) == tiletype_material::FROZEN_LIQUID)
                        {
                            //material is ice.
                            mat.mat_index = 6;
                            mat.mat_type = -1;
                        }
                        else
                        {
                            if (!r->map_cache)
                                r->map_cache = new MapExtras::MapCache();
                            mat = r->map_cache->staticMaterialAt(DFCoord(xx, yy, zz));
                        }
                    }
                    MaterialInfo mat_info(mat);

                    for (auto it3 = to->tiletype_overrides.begin(); it3 != to->tiletype_overrides.end(); it3++)
                    {
                        override &o = *it3;

                        if (tiletype != o.type)
                            continue;

                        if (o.mat_flag != -1)
                        {
                            if (!mat_info.material)
                                continue;
                            if (!mat_info.material->flags.is_set((material_flags::material_flags)o.mat_flag))
                                continue;
                        }

                        if (!o.material_matches(mat_info.type, mat_info.index))
                            continue;

                        apply_override(ret, o, coord_hash(xx,yy,zz));
                        goto matched;
                    }
                }
            }
        }
    }
    matched:;
    
    // Set colour
    for (int i = 0; i < 2; i++) {
        fg += 8;
        *(fg++) = ret.r;
        *(fg++) = ret.g;
        *(fg++) = ret.b;
        *(fg++) = 1;
        
        bg += 8;
        *(bg++) = ret.br;
        *(bg++) = ret.bg;
        *(bg++) = ret.bb;
        *(bg++) = 1;

        fg_top += 8;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
        *(fg_top++) = 1;
    }    
    
    // Set texture coordinates
    {
        long texpos = ret.texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
        *(tex++) = txt[texpos].left;   // Upper left
        *(tex++) = txt[texpos].bottom;
        *(tex++) = txt[texpos].right;  // Upper right
        *(tex++) = txt[texpos].bottom;
        *(tex++) = txt[texpos].left;   // Lower left
        *(tex++) = txt[texpos].top;
        
        *(tex++) = txt[texpos].left;   // Lower left
        *(tex++) = txt[texpos].top;
        *(tex++) = txt[texpos].right;  // Upper right
        *(tex++) = txt[texpos].bottom;
        *(tex++) = txt[texpos].right;  // Lower right
        *(tex++) = txt[texpos].top;
    }

    // Set bg texture coordinates
    {
        long texpos = ret.bg_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
        *(tex_bg++) = txt[texpos].left;   // Upper left
        *(tex_bg++) = txt[texpos].bottom;
        *(tex_bg++) = txt[texpos].right;  // Upper right
        *(tex_bg++) = txt[texpos].bottom;
        *(tex_bg++) = txt[texpos].left;   // Lower left
        *(tex_bg++) = txt[texpos].top;
        
        *(tex_bg++) = txt[texpos].left;   // Lower left
        *(tex_bg++) = txt[texpos].top;
        *(tex_bg++) = txt[texpos].right;  // Upper right
        *(tex_bg++) = txt[texpos].bottom;
        *(tex_bg++) = txt[texpos].right;  // Lower right
        *(tex_bg++) = txt[texpos].top;
    }

    // Set top texture coordinates
    {
        long texpos = ret.top_texpos;
        gl_texpos *txt = (gl_texpos*) enabler->textures.gl_texpos;
        *(tex_top++) = txt[texpos].left;   // Upper left
        *(tex_top++) = txt[texpos].bottom;
        *(tex_top++) = txt[texpos].right;  // Upper right
        *(tex_top++) = txt[texpos].bottom;
        *(tex_top++) = txt[texpos].left;   // Lower left
        *(tex_top++) = txt[texpos].top;
        
        *(tex_top++) = txt[texpos].left;   // Lower left
        *(tex_top++) = txt[texpos].top;
        *(tex_top++) = txt[texpos].right;  // Upper right
        *(tex_top++) = txt[texpos].bottom;
        *(tex_top++) = txt[texpos].right;  // Lower right
        *(tex_top++) = txt[texpos].top;
    }    
}
