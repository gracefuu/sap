// glyph.cpp
// Copyright (c) 2021, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "util.h"
#include "error.h"
#include "font/font.h"

namespace font
{
	GlyphMetrics FontFile::getGlyphMetrics(uint32_t glyphId) const
	{
		auto u16_array = this->hmtx_table.cast<uint16_t>();

		GlyphMetrics ret { };

		// note the *2, because there's also an 2-byte "lsb".
		if(glyphId >= this->num_hmetrics)
		{
			// the remaining glyphs not in the array use the last value.
			ret.horz_advance = util::convertBEU16(u16_array[(this->num_hmetrics - 1) * 2]);

			// there is an array of lsbs for glyph_ids > num_hmetrics
			auto lsb_array = this->hmtx_table.drop(2 * this->num_hmetrics);
			auto tmp = lsb_array[glyphId - this->num_hmetrics];

			ret.left_side_bearing = (int16_t) util::convertBEU16(tmp);
		}
		else
		{
			ret.horz_advance = util::convertBEU16(u16_array[glyphId * 2]);
			ret.left_side_bearing = (int16_t) util::convertBEU16(u16_array[glyphId * 2 + 1]);
		}

		// now, figure out xmin and xmax
		if(this->outline_type == OUTLINES_TRUETYPE)
		{
			size_t glyph_offset = 0;
			auto glyph_loca = this->loca_table.drop(this->loca_bytes_per_entry * glyphId);

			// first, read the loca table to determine where the glyph data starts
			if(this->loca_bytes_per_entry == 2)
				glyph_offset = 2 * consume_u16(glyph_loca);
			else
				glyph_offset = consume_u32(glyph_loca);

			auto glyph_data = this->glyf_table.drop(glyph_offset);

			// layout: numContours (16); xmin (16); ymin (16); xmax (16); ymax (16);
			auto foozle = glyph_data.cast<uint16_t>();

			ret.xmin = (int16_t) util::convertBEU16(foozle[1]);
			ret.ymin = (int16_t) util::convertBEU16(foozle[2]);
			ret.xmax = (int16_t) util::convertBEU16(foozle[3]);
			ret.ymax = (int16_t) util::convertBEU16(foozle[4]);

			// calculate RSB
			ret.right_side_bearing = ret.horz_advance - ret.left_side_bearing - (ret.xmax - ret.xmin);
		}
		else if(this->outline_type == OUTLINES_CFF)
		{
			// sap::warn("font/metrics", "bounding-box metrics for CFF-outline fonts are not supported yet!");

			// well, we're shit out of luck.
			// best effort, i guess?
			auto metrics = this->metrics;

			ret.xmin = metrics.xmin;
			ret.xmax = metrics.xmax;
			ret.ymin = metrics.ymin;
			ret.ymax = metrics.ymax;
		}
		else
		{
			sap::internal_error("unsupported outline type?!");
		}

		return ret;
	}
}
