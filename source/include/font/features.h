// features.h
// Copyright (c) 2021, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <map>
#include <utility>

#include "font/tag.h"

namespace font
{
	struct Table;
	struct FontFile;

	struct GlyphAdjustment
	{
		int16_t horz_placement;
		int16_t vert_placement;
		int16_t horz_advance;
		int16_t vert_advance;
	};

	constexpr size_t MAX_LIGATURE_LENGTH = 8;
	struct GlyphLigature
	{
		uint32_t num_glyphs;
		uint32_t glyphs[MAX_LIGATURE_LENGTH];
		uint32_t substitute;

		bool operator== (const GlyphLigature& gl) const
		{
			return memcmp(this, &gl, sizeof(GlyphLigature)) == 0;
		}
	};

	// honestly, this struct shouldn't have padding.
	static_assert(sizeof(GlyphLigature) == (2 + MAX_LIGATURE_LENGTH) * sizeof(uint32_t));

	struct GlyphLigatureSet
	{
		std::vector<GlyphLigature> ligatures;

		bool operator== (const GlyphLigatureSet& gls) const { return this->ligatures == gls.ligatures; }
	};
}

namespace font::off
{
	/*
		From the OFF specification:

		1. get the current script (latin, cyrillic, etc.), and lookup the Script struct
		2. get the current language (eg. english, greek) and lookup the Language struct
			(within the Script)

		3. for the current language, get the required feature (if any) and any optional
			features. Intersect this with the enabled features from the provided FeatureSet

		4. each feature has some number (possibly 0?) of lookups; visit the features and
			lookups in order, and apply (all) the lookups that match.

		This is the same set of steps for both GSUB and GPOS (but we apply gsub first).

		For steps (1) and (2), if the current script is unknown (or the font does not
		contain an entry for that script), use `DFLT`, and similar for the language.
	*/
	struct Language
	{
		Tag tag;
		std::optional<uint16_t> required_feature;
		std::vector<uint16_t> features;
	};

	struct Script
	{
		Tag tag;
		std::map<Tag, Language> languages;
	};

	struct Feature
	{
		Tag tag;
		std::optional<zst::byte_span> parameters_table;
		std::vector<uint16_t> lookups;
	};

	struct LookupTable
	{
		uint16_t type;
		uint16_t flags;
		uint16_t mark_filtering_set;
		std::vector<zst::byte_span> subtables;
	};

	struct GPosTable
	{
		std::vector<Feature> features;
		std::map<Tag, Script> scripts;
		std::vector<LookupTable> lookups;

		std::optional<zst::byte_span> feature_variations_table;
	};


	struct GSubTable
	{
		std::vector<Feature> features;
		std::map<Tag, Script> scripts;
		std::vector<LookupTable> lookups;

		std::optional<zst::byte_span> feature_variations_table;
	};





	struct FeatureSet
	{
		Tag script;
		Tag language;
		std::set<Tag> enabled_features {};
	};

	/*
		Using the GPOS table, try to look for positioning adjustments for the input glyph
		sequence, using the enabled features in the `feature_set`.

		The return type is a map of glyph index (of the input sequence) to a GlyphAdjustment,
		if that particular glyph needs to be adjusted. Only glyphs that need to be adjusted
		will be part of the map.
	*/
	std::map<size_t, GlyphAdjustment> getPositioningAdjustmentsForGlyphSequence(FontFile* font,
		zst::span<uint32_t> glyphs, const FeatureSet& features);

	/*
		Using the GSUB table, perform glyph substitutions using the enabled features in the feature set.

		For simplicity of use, this API returns a vector of glyphs, which *wholesale* replace the
		input glyph sequence -- even if no substitutions took place.
	*/
	std::vector<uint32_t> performSubstitutionsForGlyphSequence(FontFile* font, zst::span<uint32_t> glyphs,
		const FeatureSet& features);


	/*
		Parse the GPOS and GSUB tables from the OTF top-level Table.
	*/
	void parseGPos(FontFile* font, const Table& gpos_table);
	void parseGSub(FontFile* font, const Table& gsub_table);



	/*
		private, implementation stuff
		=============================
	*/

	struct TaggedTable
	{
		Tag tag;
		zst::byte_span data;
	};


	/*
		Return a list of LookupTable indices for the given feature set in either the GPOS or GSUB tables.
	*/
	template <typename TableKind>
	std::vector<uint16_t> getLookupTablesForFeatures(TableKind& gpos_or_gsub, const FeatureSet& features);

	/*
		Parse the script and language tables from the given buffer.
	*/
	std::map<Tag, Script> parseScriptAndLanguageTables(zst::byte_span buf);

	/*
		Parse the FeatureList.
	*/
	std::vector<Feature> parseFeatureList(zst::byte_span buf);

	/*
		Parse the LookupList.
	*/
	std::vector<LookupTable> parseLookupList(zst::byte_span buf);

	/*
		Parse a "Tagged List" -- with the following layout:

		u16 count;
		Record records[count];

		Record:
			u32 Tag
			u16 offset_from_start_of_table
	*/
	std::vector<TaggedTable> parseTaggedList(zst::byte_span buf);


	/*
		Get the class of a specific glyph id; 0 is returned if the glyph was not in any of
		the classes specified in the given `classdef_table`.
	*/
	int getGlyphClass(zst::byte_span classdef_table, uint32_t glyphId);

	/*
		Returns a mapping from classid to a set of glyph ids. Note that class 0 (the default class)
		is not included, because it would otherwise contain every other glyph (potentially a lot)
	*/
	std::map<int, std::set<uint32_t>> parseAllClassDefs(zst::byte_span classdef_table);

	/*
		Parses the same ClassDef table as `parseAllClassDefs`, but returns the inverse mapping;
		a map from glyph id to class id. Again, glyphs with class 0 are not included.
	*/
	std::map<uint32_t, int> parseGlyphToClassMapping(zst::byte_span classdef_table);

	/*
		Returns the coverage index for the given glyphid within the given coverage table. Unlike classes,
		there is no "default" coverage index -- you just skip the lookup if the glyph is not in the
		coveraged table.
	*/
	std::optional<int> getGlyphCoverageIndex(zst::byte_span coverage_table, uint32_t glyphId);

	/*
		Returns a map from coverageIndex -> glyphId, for every glyph in the coverage table.
	*/
	std::map<int, uint32_t> parseCoverageTable(zst::byte_span coverage_table);

	struct ContextualLookupRecord
	{
		uint16_t glyph_idx;
		uint16_t lookup_idx;
	};

	/*
		Parse and match the input glyphstring with the lookup *subtable* provided. Again, this should be a
		lookup *subtable*, not the LookupTable itself.

		Returns value:
			(first)  the list of lookup records, PosLookupRecord / SubstLookupRecord
			(second) the number of glyphs in the input glyphstring that were matched

		The data layouts for GPOS and GSUB are identical, so this is a common implementation. Use for GPOS type 7
		and GSUB type 5.
	*/
	std::optional<std::pair<std::vector<ContextualLookupRecord>, size_t>> performContextualLookup(zst::byte_span subtable,
		zst::span<uint32_t> glyphs);

	/*
		Parse and match the input glyphstring (where the current glyph is at glyphs[position], using the provided *subtable*.
		The same caveats apply as for `performContextualLookup`. Use for GPOS type 8 and GSUB type 6.
	*/
	std::optional<std::pair<std::vector<ContextualLookupRecord>, size_t>> performChainedContextLookup(zst::byte_span subtable,
		zst::span<uint32_t> glyphs, size_t position);
}


// declares GPOS-specific lookup functions
namespace font::off::gpos
{
	constexpr uint16_t LOOKUP_SINGLE            = 1;
	constexpr uint16_t LOOKUP_PAIR              = 2;
	constexpr uint16_t LOOKUP_CURSIVE           = 3;
	constexpr uint16_t LOOKUP_MARK_TO_BASE      = 4;
	constexpr uint16_t LOOKUP_MARK_TO_LIGA      = 5;
	constexpr uint16_t LOOKUP_MARK_TO_MARK      = 6;
	constexpr uint16_t LOOKUP_CONTEXTUAL        = 7;
	constexpr uint16_t LOOKUP_CHAINING_CONTEXT  = 8;
	constexpr uint16_t LOOKUP_EXTENSION_POS     = 9;
	constexpr uint16_t LOOKUP_MAX               = 10;

	using OptionalGA = std::optional<GlyphAdjustment>;

	/*
		Lookup a single glyph adjustment (type 1, LOOKUP_SINGLE) for the given glyph id.
	*/
	OptionalGA lookupSingleAdjustment(const LookupTable& lookup, uint32_t gid);

	/*
		Lookup a pair glyph adjustment (type 2, LOOKUP_PAIR).

		It is important to know whether there was an adjustment for the second glyph in the pair
		or not; see OFF 1.9, page 220:

		If valueFormat2 is set to 0, then the second glyph of the pair is the "next" glyph
		for which a lookup should be performed.

		So, instead of returning an optional of pair, we return a pair of optionals.
	*/
	std::pair<OptionalGA, OptionalGA> lookupPairAdjustment(const LookupTable& lookup, uint32_t gid1, uint32_t gid2);


	struct AdjustmentResult
	{
		size_t input_consumed;
		std::map<size_t, GlyphAdjustment> adjustments;
	};

	/*
		Lookup contextual glyph adjustments (type 7, LOOKUP_CONTEXTUAL).

		This one needs to "recursively" perform lookups, so it needs the GPOS table as well. Returns a mapping
		from the index in the given sequence to the adjustment.
	*/
	std::optional<AdjustmentResult> lookupContextualPositioning(const GPosTable& gpos, const LookupTable& lookup,
		zst::span<uint32_t> glyphs);

	/*
		Lookup chained-context glyph adjustments (type 8, LOOKUP_CHAINING_CONTEXT).

		This one also needs to recursively perform lookups. Additionally, this requires both lookahead and lookbehind,
		so we take the *entire glyphstring*, as well as an index which is the position in the glyphstring that the
		'current' glyph is actually at.

		Again, similar to `lookupForGlyphSequence` (whose behaviour is actually motivated by this lookup type...),
		return[0] adjusts glyphs[position], return[1] adjsts glyphs[position + 1], etc.
	*/
	std::optional<AdjustmentResult> lookupChainedContextPositioning(const GPosTable& gpos, const LookupTable& lookup,
		zst::span<uint32_t> glyphs, size_t position);
}

// declares GSUB-specific lookup functions
namespace font::off::gsub
{
	constexpr uint16_t LOOKUP_SINGLE            = 1;
	constexpr uint16_t LOOKUP_MULTIPLE          = 2;
	constexpr uint16_t LOOKUP_ALTERNATE         = 3;
	constexpr uint16_t LOOKUP_LIGATURE          = 4;
	constexpr uint16_t LOOKUP_CONTEXTUAL        = 5;
	constexpr uint16_t LOOKUP_CHAINING_CONTEXT  = 6;
	constexpr uint16_t LOOKUP_EXTENSION_SUBST   = 7;
	constexpr uint16_t LOOKUP_REVERSE_CHAIN     = 8;
	constexpr uint16_t LOOKUP_MAX               = 9;

	/*
		Lookup a single substitution (type 1, LOOKUP_SINGLE); replaces one input glyph with one output glyph.
	*/
	std::optional<uint32_t> lookupSingleSubstitution(const LookupTable& lookup, uint32_t glyph);

	/*
		Lookup a multiple glyph substitution (type 2, LOOKUP_MULTIPLE). Replaces one input glyph with
		multiple output glyphs.
	*/
	std::optional<std::vector<uint32_t>> lookupMultipleSubstitution(const LookupTable& lookup, uint32_t glyph);

	/*
		Lookup a ligature substitution (type 4, LOOKUP_LIGATURE). Replaces multiple input glyphs with
		a single output glyph.

		The given input stream should be from the current glyph till the end of the glyphstring.
		The return value is
			(first) the output glyph id
			(second) the number of input glyphs consumed.
	*/
	std::optional<std::pair<uint32_t, size_t>> lookupLigatureSubstitution(const LookupTable& lookup,
		zst::span<uint32_t> glyphs);


	struct SubstitutionResult
	{
		size_t input_start;
		size_t input_consumed;
		std::vector<uint32_t> glyphs;
	};

	/*
		Lookup a contextual substitution (type 5, LOOKUP_CONTEXTUAL). Same semantics as GPOS contextual positioning.

		The returned result replaces glyphs from `glyphs[input_start]` to `glyphs[input_consumed - 1]`
		inclusive, with `result.glyphs`.
	*/
	std::optional<SubstitutionResult> lookupContextualSubstitution(const GSubTable& gsub, const LookupTable& lookup,
		zst::span<uint32_t> glyphs);

	/*
		Lookup a chaining context substitution (type 6, LOOKUP_CHAINING_CONTEXT). Same semantics as GPOS chaining-context
		positioning wrt. the `glyphs` and `position` values.

		The returned result (if it exists) replaces glyphs from
			`glyphs[input_start + position]` to `glyphs[input_start + position + input_consumed - 1]`

		inclusive, with `result.glyphs`.
	*/
	std::optional<SubstitutionResult> lookupChainedContextSubstitution(const GSubTable& gsub, const LookupTable& lookup,
		zst::span<uint32_t> glyphs, size_t position);
}


namespace font::off
{
	/*
		GPOS FEATURES
		=============
	*/
	namespace feature
	{
		/*
			kerning: self explanatory
		*/
		constexpr auto kern = Tag("kern");

		/*
			capital spacing: for all-caps words, this usually adjusts the glyph advance to be
			slightly larger, so the glyphs are not so close together.
		*/
		constexpr auto cpsp = Tag("cpsp");
	}


	/*
		GSUB FEATURES
		=============
	*/
	namespace feature
	{
	}
}

