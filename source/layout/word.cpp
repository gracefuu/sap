// word.cpp
// Copyright (c) 2021, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "sap.h"
#include "util.h"

#include "font/font.h"

#include "pdf/pdf.h"
#include "pdf/text.h"
#include "pdf/misc.h"
#include "pdf/font.h"

namespace sap
{
	// TODO: this needs to handle unicode composing/decomposing also, which is a massive pain
	static uint32_t read_one_glyphid(const pdf::Font* font, zst::byte_span& utf8)
	{
		assert(utf8.size() > 0);

		auto cp = unicode::consumeCodepointFromUtf8(utf8);
		return font->getGlyphIdFromCodepoint(cp);
	}


	static std::vector<Word::GlyphInfo> convert_to_glyphs(const pdf::Font* font, zst::str_view text)
	{
		auto utf8 = text.bytes();

		// first, convert all codepoints to glyphs
		std::vector<uint32_t> glyphs {};
		while(utf8.size() > 0)
			glyphs.push_back(read_one_glyphid(font, utf8));

		using font::Tag;
		font::off::FeatureSet features {};
		features.script = Tag("latn");
		features.language = Tag("DFLT");
		features.enabled_features = {
			Tag("kern")
		};

		// TODO: next, use GSUB to substitute the glyphs.


		// next, get base metrics for each glyph.
		std::vector<Word::GlyphInfo> glyph_infos {};
		for(auto g : glyphs)
		{
			Word::GlyphInfo info {};
			info.gid = g;
			info.metrics = font->getMetricsForGlyph(g);
			glyph_infos.push_back(std::move(info));
		}

		// finally, use GPOS
		auto glyphs_span = zst::span<uint32_t>(glyphs.data(), glyphs.size());
		auto adjustment_map = font->getPositioningAdjustmentsForGlyphSequence(glyphs_span, features);
		for(auto& [ i, adj ] : adjustment_map)
		{
			auto& info = glyph_infos[i];
			info.adjustments.horz_advance += adj.horz_advance;
			info.adjustments.vert_advance += adj.vert_advance;
			info.adjustments.horz_placement += adj.horz_placement;
			info.adjustments.vert_placement += adj.vert_placement;

			zpr::println("adjusting {}: {}", i, adj.horz_advance);
		}

		return glyph_infos;
	}







	void Word::computeMetrics(const Style* parent_style)
	{
		auto style = Style::combine(m_style, parent_style);
		this->setStyle(style);

		auto font = style->font();
		auto font_size = style->font_size();

		m_glyphs = convert_to_glyphs(font, this->text);

		// we shouldn't have 0 glyphs in a word... right?
		assert(m_glyphs.size() > 0);

		// size is in sap units, which is in mm; metrics are in typographic units, so 72dpi;
		// calculate the scale accordingly.
		const auto font_metrics = font->getFontMetrics();
		constexpr auto tpu = [](auto... xs) -> auto { return pdf::typographic_unit(xs...); };

		// TODO: what is this complicated formula???
		this->size = { 0, 0 };
		this->size.y() = ((tpu(font_metrics.ymax) - tpu(font_metrics.ymin))
						* (font_size.value() / pdf::GLYPH_SPACE_UNITS)).into(sap::Scalar{});

		auto font_size_tpu = font_size.into(dim::units::pdf_typographic_unit{});
		for(auto& glyph : m_glyphs)
		{
			auto width = glyph.metrics.horz_advance + glyph.adjustments.horz_advance;
			this->size.x() += font->scaleMetricForFontSize(width, font_size_tpu).into(sap::Scalar{});
		}

		{
			auto space_gid = font->getGlyphIdFromCodepoint(' ');
			auto space_adv = font->getMetricsForGlyph(space_gid).horz_advance;
			auto space_width = font->scaleMetricForFontSize(space_adv, font_size_tpu);

			m_space_width = space_width.into(sap::Scalar{});
		}
	}

	Scalar Word::spaceWidth() const
	{
		return m_space_width;
	}

	void Word::render(pdf::Text* text) const
	{
		const auto font = m_style->font();
		const auto font_size = m_style->font_size();
		text->setFont(font, font_size.into(pdf::Scalar{}));

		auto add_gid = [&font, text](uint32_t gid) {
			if(font->encoding_kind == pdf::Font::ENCODING_CID)
				text->addEncoded(2, gid);
			else
				text->addEncoded(1, gid);
		};

		for(auto& glyph : m_glyphs)
		{
			add_gid(glyph.gid);

			// TODO: handle placement as well
			text->offset(font->scaleMetricForPDFTextSpace(glyph.adjustments.horz_advance));
		}

		if(!m_linebreak_after && m_next_word != nullptr)
		{
			auto space_gid = font->getGlyphIdFromCodepoint(' ');
			add_gid(space_gid);

			if(m_post_space_ratio != 1.0)
			{
				auto space_adv = font->getMetricsForGlyph(space_gid).horz_advance;

				// ratio > 1 = expand, < 1 = shrink
				auto extra = font->scaleMetricForPDFTextSpace(space_adv) * (m_post_space_ratio - 1.0);
				text->offset(extra);
			}

			/*
				TODO: here, we also want to handle kerning between the space and the start of the next word
				(see the longer explanation in layout/paragraph.cpp)
			*/
		}
	}
}
