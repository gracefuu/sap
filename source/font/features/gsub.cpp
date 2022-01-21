// gsub.cpp
// Copyright (c) 2021, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cassert>

#include "util.h"
#include "error.h"
#include "font/font.h"
#include "font/features.h"

namespace font
{
	std::map<uint32_t, GlyphLigatureSet> FontFile::getAllGlyphLigatures() const
	{
		std::map<uint32_t, GlyphLigatureSet> ligature_map;

#if 0
		for(auto& lookup : this->gsub_tables.lookup_tables[GSUB_LOOKUP_LIGATURE])
		{
			auto ofs = lookup.file_offset;
			auto buf = zst::byte_span(this->file_bytes, this->file_size).drop(ofs);

			auto table_start = buf;

			// we already know the type, so just skip it.
			consume_u16(buf);
			auto flags = consume_u16(buf);
			auto num_subs = consume_u16(buf);

			(void) flags;

			for(size_t i = 0; i < num_subs; i++)
			{
				auto ofs = consume_u16(buf);
				auto subtable = table_start.drop(ofs);
				auto subtable_start = subtable;

				auto format = consume_u16(subtable);
				auto cov_ofs = consume_u16(subtable);

				auto cov_table = parseCoverageTable(subtable_start.drop(cov_ofs));

				if(format != 1)
					sap::internal_error("unknown GSUB LookupTable format {}", format);

				// this thing is 2 levels of indirection deep
				auto num_liga_sets = consume_u16(subtable);

				// there should be 1 LigatureSet for every covered glyph in the coverage table
				assert(num_liga_sets == cov_table.size());

				for(size_t i = 0; i < num_liga_sets; i++)
				{
					auto first_gid = cov_table[i];
					auto liga_set = subtable_start.drop(consume_u16(subtable));
					auto liga_set_start = liga_set;

					auto num_ligatures = consume_u16(liga_set);

					for(size_t k = 0; k < num_ligatures; k++)
					{
						auto liga_table = liga_set_start.drop(consume_u16(liga_set));
						auto substitute = consume_u16(liga_table);
						auto num_components = consume_u16(liga_table);

						if(num_components > MAX_LIGATURE_LENGTH)
						{
							zpr::println("warning: skipping ligature with {} components (more than maximum of {})",
								num_components, MAX_LIGATURE_LENGTH);
							continue;
						}

						GlyphLigature lig { };
						lig.num_glyphs = num_components;
						lig.substitute = substitute;
						lig.glyphs[0] = first_gid;

						for(size_t i = 1; i < num_components; i++)
							lig.glyphs[i] = consume_u16(liga_table);

						// ligature_map[lig] = substitute;
						ligature_map[first_gid].ligatures.push_back(lig);
					}
				}
			}
		}
#endif
		return ligature_map;
	}
}


namespace font::off
{
}
