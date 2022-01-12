// sap.h
// Copyright (c) 2021, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>
#include <cstdint>

#include <vector>
#include <utility>

#include <zst.h>

#include "pdf/units.h"

namespace pdf
{
	struct Font;
	struct Page;
	struct Document;
	struct Writer;
}

namespace sap
{
	using Scalar = pdf::Scalar;
	using Vector = pdf::Vector;


	// add colour and all that
	struct DisplaySettings
	{
		Scalar fontSize;
		const pdf::Font* font;
	};

	// obviously needs more settings.
	struct GeometrySettings
	{
		Scalar left_margin;
		Scalar right_margin;
		Scalar top_margin;
		Scalar bottom_margin;

		Scalar line_spacing;
		Scalar paragraph_spacing;
	};


	/*
		for now, the Word acts as the smallest indivisible unit of text in sap; we do not plan to adjust
		intra-word (ie. letter) spacing at this time.

		since the Word is used as the unit of typesetting, it needs to hold and *cache* a bunch of important
		information, notably the bounding box (metrics) and the glyph ids, the former of which can only be
		computed with the latter.

		since we need to run ligature substitution and kerning adjustments to determine the bounding box,
		these will be done on a Word. Again, since letter spacing is not really changing here, these computed
		values will be cached so less has to be computed on the PDF layer.
	*/
	struct Word
	{
		Word(int kind) : kind(kind) { }
		Word(int kind, DisplaySettings ds, zst::str_view sv) : Word(kind, std::move(ds), sv.str()) { }
		Word(int kind, DisplaySettings ds, std::string str) : kind(kind), display(std::move(ds)), text(std::move(str)) { }

		// TODO: this computes a bunch of stuff, so i'm not sure what to name this function.
		// this fills in the `size` and the `glyphs` using the DisplaySettings and the text.
		void compute();

		int kind = 0;
		DisplaySettings display { };
		std::string text { };

		// these are in sap units, which is in mm. note that `position` is not set internally, but rather
		// is used by whoever is laying out the Word to store its position temporarily (instead of forcing
		// some external associative container)
		Vector position { };
		Vector size { };

		// first element is the glyph id, second one is the adjustment to make for kerning (0 if none)
		std::vector<std::pair<uint32_t, int>> glyphs { };

		// the kind of word. this affects automatic handling of certain things during paragraph
		// layout, eg. whether or not to insert a space (or how large of a space).
		static constexpr int KIND_LATIN = 0;
		static constexpr int KIND_CJK   = 1;
		static constexpr int KIND_PUNCT = 2;

		friend struct Paragraph;
	};


	// for now we are not concerned with lines.
	struct Paragraph
	{
		Paragraph() { }

		void add(Word word);

		std::vector<Word> words { };
	};


	struct Page
	{
		Page();

		Page(const Page&) = delete;
		Page& operator= (const Page&) = delete;

		Page(Page&&) = default;
		Page& operator= (Page&&) = default;

		// add a paragraph to the page, optionally returning the remainder if
		// the entire thing could not fit. `Paragraph` is a value type, so this
		// is totally fine without pointers.
		std::optional<Paragraph> add(Paragraph para);

		std::vector<Paragraph> paragraphs { };
		pdf::Page* pdf_page = 0;

	private:

	};

	struct Document
	{
		Document();

		Document(const Document&) = delete;
		Document& operator= (const Document&) = delete;

		Document(Document&&) = default;
		Document& operator= (Document&&) = default;

		void add(Page&& para);

		void finalise(pdf::Writer* writer);

		std::vector<Page> m_pages { };
		pdf::Document m_pdf_document;
	};
}
