/*
 * Copyright (c) 2022, Matthew Olsson <mattco@serenityos.org>
 * Copyright (c) 2022, Julian Offenhäuser <offenhaeuser@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/Painter.h>
#include <LibPDF/CommonNames.h>
#include <LibPDF/Fonts/CFF.h>
#include <LibPDF/Fonts/PS1FontProgram.h>
#include <LibPDF/Fonts/Type1Font.h>
#include <LibPDF/Renderer.h>

namespace PDF {

PDFErrorOr<void> Type1Font::initialize(Document* document, NonnullRefPtr<DictObject> const& dict, float font_size)
{
    TRY(SimpleFont::initialize(document, dict, font_size));

    m_base_font_name = TRY(dict->get_name(document, CommonNames::BaseFont))->name();

    // auto is_standard_font = is_standard_latin_font(font->base_font_name());

    // If there's an embedded font program we use that; otherwise we try to find a replacement font
    if (dict->contains(CommonNames::FontDescriptor)) {
        auto descriptor = TRY(dict->get_dict(document, CommonNames::FontDescriptor));
        if (descriptor->contains(CommonNames::FontFile3)) {
            auto font_file_stream = TRY(descriptor->get_stream(document, CommonNames::FontFile3));
            auto font_file_dict = font_file_stream->dict();
            if (font_file_dict->contains(CommonNames::Subtype) && font_file_dict->get_name(CommonNames::Subtype)->name() == CommonNames::Type1C) {
                m_font_program = TRY(CFF::create(font_file_stream->bytes(), encoding()));
            }
        } else if (descriptor->contains(CommonNames::FontFile)) {
            auto font_file_stream = TRY(descriptor->get_stream(document, CommonNames::FontFile));
            auto font_file_dict = font_file_stream->dict();

            if (!font_file_dict->contains(CommonNames::Length1, CommonNames::Length2))
                return Error::parse_error("Embedded type 1 font is incomplete"sv);

            auto length1 = TRY(document->resolve(font_file_dict->get_value(CommonNames::Length1))).get<int>();
            auto length2 = TRY(document->resolve(font_file_dict->get_value(CommonNames::Length2))).get<int>();

            m_font_program = TRY(PS1FontProgram::create(font_file_stream->bytes(), encoding(), length1, length2));
        }
    }

    if (m_font_program && m_font_program->kind() == Type1FontProgram::Kind::CIDKeyed)
        return Error::parse_error("Type1 fonts must not be CID-keyed"sv);

    if (!m_font_program) {
        m_font = TRY(replacement_for(base_font_name().to_lowercase(), font_size));
    }

    VERIFY(m_font_program || m_font);
    return {};
}

Optional<float> Type1Font::get_glyph_width(u8 char_code) const
{
    if (m_font)
        return m_font->glyph_width(char_code);
    return OptionalNone {};
}

void Type1Font::set_font_size(float font_size)
{
    if (m_font)
        m_font = m_font->with_size((font_size * POINTS_PER_INCH) / DEFAULT_DPI);
}

PDFErrorOr<void> Type1Font::draw_glyph(Gfx::Painter& painter, Gfx::FloatPoint point, float width, u8 char_code, Renderer const& renderer)
{
    auto style = renderer.state().paint_style;

    if (!m_font_program) {
        // Undo shift in Glyf::Glyph::append_simple_path() via OpenType::Font::rasterize_glyph().
        auto position = point.translated(0, -m_font->pixel_metrics().ascent);
        // FIXME: Bounding box and sample point look to be pretty wrong
        if (style.has<Color>()) {
            painter.draw_glyph(position, char_code, *m_font, style.get<Color>());
        } else {
            style.get<NonnullRefPtr<Gfx::PaintStyle>>()->paint(Gfx::IntRect(position.x(), position.y(), width, 0), [&](auto sample) {
                painter.draw_glyph(position, char_code, *m_font, sample(Gfx::IntPoint(position.x(), position.y())));
            });
        }
        return {};
    }

    auto effective_encoding = encoding();
    if (!effective_encoding)
        effective_encoding = m_font_program->encoding();
    if (!effective_encoding)
        effective_encoding = Encoding::standard_encoding();
    auto char_name = effective_encoding->get_name(char_code);
    auto translation = m_font_program->glyph_translation(char_name, width);
    point = point.translated(translation);

    auto glyph_position = Gfx::GlyphRasterPosition::get_nearest_fit_for(point);
    Type1GlyphCacheKey index { char_code, glyph_position.subpixel_offset, width };

    RefPtr<Gfx::Bitmap> bitmap;
    auto maybe_bitmap = m_glyph_cache.get(index);
    if (maybe_bitmap.has_value()) {
        bitmap = maybe_bitmap.value();
    } else {
        bitmap = m_font_program->rasterize_glyph(char_name, width, glyph_position.subpixel_offset);
        m_glyph_cache.set(index, bitmap);
    }

    if (style.has<Color>()) {
        painter.blit_filtered(glyph_position.blit_position, *bitmap, bitmap->rect(), [style](Color pixel) -> Color {
            return pixel.multiply(style.get<Color>());
        });
    } else {
        style.get<NonnullRefPtr<Gfx::PaintStyle>>()->paint(bitmap->physical_rect(), [&](auto sample) {
            painter.blit_filtered(glyph_position.blit_position, *bitmap, bitmap->rect(), [&](Color pixel) -> Color {
                // FIXME: Presumably we need to sample at every point in the glyph, not just the top left?
                return pixel.multiply(sample(glyph_position.blit_position));
            });
        });
    }
    return {};
}
}
