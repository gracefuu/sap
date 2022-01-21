// lookup.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "error.h"

#include "font/font.h"
#include "font/features.h"

namespace font::off::gpos
{
	static OptionalGA parse_value_record(zst::byte_span& buf, uint16_t format)
	{
		if(format == 0)
			return std::nullopt;

		GlyphAdjustment ret { };

		if(format & 0x01)   ret.horz_placement = consume_u16(buf);
		if(format & 0x02)   ret.vert_placement = consume_u16(buf);
		if(format & 0x04)   ret.horz_advance = consume_u16(buf);
		if(format & 0x08)   ret.vert_advance = consume_u16(buf);
		if(format & 0x10)   consume_u16(buf);   // X_PLACEMENT_DEVICE
		if(format & 0x20)   consume_u16(buf);   // Y_PLACEMENT_DEVICE
		if(format & 0x40)   consume_u16(buf);   // X_ADVANCE_DEVICE
		if(format & 0x80)   consume_u16(buf);   // Y_ADVANCE_DEVICE

		return ret;
	}

	constexpr static size_t get_value_record_size(uint16_t format)
	{
		size_t ret = 0;
		if(format & 0x01)   ret += sizeof(uint16_t);
		if(format & 0x02)   ret += sizeof(uint16_t);
		if(format & 0x04)   ret += sizeof(uint16_t);
		if(format & 0x08)   ret += sizeof(uint16_t);
		if(format & 0x10)   ret += sizeof(uint16_t);
		if(format & 0x20)   ret += sizeof(uint16_t);
		if(format & 0x40)   ret += sizeof(uint16_t);
		if(format & 0x80)   ret += sizeof(uint16_t);
		return ret;
	}


	OptionalGA lookupSingleAdjustment(LookupTable& lookup, uint32_t gid)
	{
		assert(lookup.type == LOOKUP_SINGLE);

		for(auto subtable : lookup.subtables)
		{
			auto subtable_start = subtable;

			auto format = consume_u16(subtable);
			auto cov_ofs = consume_u16(subtable);
			auto value_fmt = consume_u16(subtable);

			if(format != 1 && format != 2)
			{
				sap::warn("font/gpos", "unknown subtable format '{}' in GPOS/Single", format);
				continue;
			}

			if(auto cov_idx = off::getGlyphCoverageIndex(subtable_start.drop(cov_ofs), gid); cov_idx.has_value())
			{
				if(format == 1)
				{
					return parse_value_record(subtable, value_fmt);
				}
				else
				{
					auto num_records = consume_u16(subtable);
					assert(cov_idx < num_records);

					subtable.remove_prefix(get_value_record_size(value_fmt) * (*cov_idx));
					return parse_value_record(subtable, value_fmt);
				}
			}
		}

		return std::nullopt;
	}


	std::pair<OptionalGA, OptionalGA> lookupPairAdjustment(LookupTable& lookup, uint32_t gid1, uint32_t gid2)
	{
		assert(lookup.type == LOOKUP_PAIR);
		for(auto subtable : lookup.subtables)
		{
			auto subtable_start = subtable;

			auto format = consume_u16(subtable);
			auto cov_ofs = consume_u16(subtable);
			auto value_fmt1 = consume_u16(subtable);
			auto value_fmt2 = consume_u16(subtable);

			if(format != 1 && format != 2)
			{
				sap::warn("font/gpos", "unknown subtable format '{}' in GPOS/Pair", format);
				continue;
			}

			// the coverage table only lists the first glyph id.
			if(auto cov_idx = off::getGlyphCoverageIndex(subtable_start.drop(cov_ofs), gid1); cov_idx.has_value())
			{
				if(format == 1)
				{
					auto num_pair_sets = consume_u16(subtable);
					assert(*cov_idx < num_pair_sets);

					const auto PairRecordSize = sizeof(uint16_t)
						+ get_value_record_size(value_fmt1)
						+ get_value_record_size(value_fmt2);

					auto pairset_offset = peek_u16(subtable.drop(*cov_idx * sizeof(uint16_t)));
					auto pairset_table = subtable_start.drop(pairset_offset);

					auto num_pairs = consume_u16(pairset_table);

					// now, binary search the second set.
					size_t low = 0;
					size_t high = num_pairs;

					while(low < high)
					{
						auto mid = (low + high) / 2u;
						auto glyph = peek_u16(pairset_table.drop(mid * PairRecordSize));

						if(glyph == gid2)
						{
							// we need to drop the glyph id itself to get to the actual data
							auto tmp = pairset_table.drop(mid * PairRecordSize).drop(sizeof(uint16_t));

							auto a1 = parse_value_record(tmp, value_fmt1);
							auto a2 = parse_value_record(tmp, value_fmt2);
							return std::make_pair(a1, a2);
						}
						else if(glyph < gid2)
						{
							low = mid + 1;
						}
						else
						{
							high = mid;
						}
					}
				}
				else
				{
					auto cls_ofs1 = consume_u16(subtable);
					auto cls_ofs2 = consume_u16(subtable);

					auto num_cls1 = consume_u16(subtable);
					auto num_cls2 = consume_u16(subtable);

					auto g1_class = off::getGlyphClass(subtable_start.drop(cls_ofs1), gid1);
					auto g2_class = off::getGlyphClass(subtable_start.drop(cls_ofs2), gid2);

					// note that num_cls1/2 include class 0
					if(g1_class < num_cls1 && g2_class < num_cls2)
					{
						const auto RecordSize = get_value_record_size(value_fmt1) + get_value_record_size(value_fmt2);

						// skip all the way to the correct Class2Record
						auto cls2_start = subtable.drop(g1_class * num_cls2 * RecordSize);
						auto pair_start = cls2_start.drop(g2_class * RecordSize);

						auto a1 = parse_value_record(pair_start, value_fmt1);
						auto a2 = parse_value_record(pair_start, value_fmt2);

						return std::make_pair(a1, a2);
					}
				}
			}
		}

		return { std::nullopt, std::nullopt };
	}
}
