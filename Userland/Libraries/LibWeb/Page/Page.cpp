/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ScopeGuard.h>
#include <AK/SourceLocation.h>
#include <LibIPC/Decoder.h>
#include <LibIPC/Encoder.h>
#include <LibWeb/CSS/StyleComputer.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/BrowsingContext.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/HTMLInputElement.h>
#include <LibWeb/HTML/HTMLMediaElement.h>
#include <LibWeb/HTML/HTMLSelectElement.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/TraversableNavigable.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Platform/EventLoopPlugin.h>

namespace Web {

JS::NonnullGCPtr<Page> Page::create(JS::VM& vm, JS::NonnullGCPtr<PageClient> page_client)
{
    return vm.heap().allocate_without_realm<Page>(page_client);
}

Page::Page(JS::NonnullGCPtr<PageClient> client)
    : m_client(client)
{
}

Page::~Page() = default;

void Page::visit_edges(JS::Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_top_level_traversable);
    visitor.visit(m_client);
}

HTML::BrowsingContext& Page::focused_context()
{
    if (m_focused_context)
        return *m_focused_context;
    return top_level_browsing_context();
}

void Page::set_focused_browsing_context(Badge<EventHandler>, HTML::BrowsingContext& browsing_context)
{
    m_focused_context = browsing_context.make_weak_ptr();
}

void Page::load(const AK::URL& url)
{
    (void)top_level_traversable()->navigate({ .url = url, .source_document = *top_level_traversable()->active_document(), .user_involvement = HTML::UserNavigationInvolvement::BrowserUI });
}

void Page::load_html(StringView html)
{

    (void)top_level_traversable()->navigate({ .url = "about:srcdoc"sv,
        .source_document = *top_level_traversable()->active_document(),
        .document_resource = String::from_utf8(html).release_value_but_fixme_should_propagate_errors(),
        .user_involvement = HTML::UserNavigationInvolvement::BrowserUI });
}

Gfx::Palette Page::palette() const
{
    return m_client->palette();
}

// https://w3c.github.io/csswg-drafts/cssom-view-1/#web-exposed-screen-area
CSSPixelRect Page::web_exposed_screen_area() const
{
    auto device_pixel_rect = m_client->screen_rect();
    auto scale = client().device_pixels_per_css_pixel();
    return {
        device_pixel_rect.x().value() / scale,
        device_pixel_rect.y().value() / scale,
        device_pixel_rect.width().value() / scale,
        device_pixel_rect.height().value() / scale
    };
}

CSS::PreferredColorScheme Page::preferred_color_scheme() const
{
    return m_client->preferred_color_scheme();
}

CSSPixelPoint Page::device_to_css_point(DevicePixelPoint point) const
{
    return {
        point.x().value() / client().device_pixels_per_css_pixel(),
        point.y().value() / client().device_pixels_per_css_pixel(),
    };
}

DevicePixelPoint Page::css_to_device_point(CSSPixelPoint point) const
{
    return {
        point.x() * client().device_pixels_per_css_pixel(),
        point.y() * client().device_pixels_per_css_pixel(),
    };
}

DevicePixelRect Page::css_to_device_rect(CSSPixelRect rect) const
{
    return {
        rect.location().to_type<double>() * client().device_pixels_per_css_pixel(),
        rect.size().to_type<double>() * client().device_pixels_per_css_pixel(),
    };
}

CSSPixelRect Page::device_to_css_rect(DevicePixelRect rect) const
{
    auto scale = client().device_pixels_per_css_pixel();
    return {
        rect.x().value() / scale,
        rect.y().value() / scale,
        rect.width().value() / scale,
        rect.height().value() / scale
    };
}

DevicePixelRect Page::enclosing_device_rect(CSSPixelRect rect) const
{
    auto scale = client().device_pixels_per_css_pixel();
    return DevicePixelRect(
        floor(rect.x().to_double() * scale),
        floor(rect.y().to_double() * scale),
        ceil(rect.width().to_double() * scale),
        ceil(rect.height().to_double() * scale));
}

DevicePixelRect Page::rounded_device_rect(CSSPixelRect rect) const
{
    auto scale = client().device_pixels_per_css_pixel();
    return {
        roundf(rect.x().to_double() * scale),
        roundf(rect.y().to_double() * scale),
        roundf(rect.width().to_double() * scale),
        roundf(rect.height().to_double() * scale)
    };
}

bool Page::handle_mouseup(DevicePixelPoint position, DevicePixelPoint screen_position, unsigned button, unsigned buttons, unsigned modifiers)
{
    return top_level_browsing_context().event_handler().handle_mouseup(device_to_css_point(position), device_to_css_point(screen_position), button, buttons, modifiers);
}

bool Page::handle_mousedown(DevicePixelPoint position, DevicePixelPoint screen_position, unsigned button, unsigned buttons, unsigned modifiers)
{
    return top_level_browsing_context().event_handler().handle_mousedown(device_to_css_point(position), device_to_css_point(screen_position), button, buttons, modifiers);
}

bool Page::handle_mousemove(DevicePixelPoint position, DevicePixelPoint screen_position, unsigned buttons, unsigned modifiers)
{
    return top_level_browsing_context().event_handler().handle_mousemove(device_to_css_point(position), device_to_css_point(screen_position), buttons, modifiers);
}

bool Page::handle_mousewheel(DevicePixelPoint position, DevicePixelPoint screen_position, unsigned button, unsigned buttons, unsigned modifiers, DevicePixels wheel_delta_x, DevicePixels wheel_delta_y)
{
    return top_level_browsing_context().event_handler().handle_mousewheel(device_to_css_point(position), device_to_css_point(screen_position), button, buttons, modifiers, wheel_delta_x.value(), wheel_delta_y.value());
}

bool Page::handle_doubleclick(DevicePixelPoint position, DevicePixelPoint screen_position, unsigned button, unsigned buttons, unsigned modifiers)
{
    return top_level_browsing_context().event_handler().handle_doubleclick(device_to_css_point(position), device_to_css_point(screen_position), button, buttons, modifiers);
}

bool Page::handle_keydown(KeyCode key, unsigned modifiers, u32 code_point)
{
    return focused_context().event_handler().handle_keydown(key, modifiers, code_point);
}

bool Page::handle_keyup(KeyCode key, unsigned modifiers, u32 code_point)
{
    return focused_context().event_handler().handle_keyup(key, modifiers, code_point);
}

void Page::set_top_level_traversable(JS::NonnullGCPtr<HTML::TraversableNavigable> navigable)
{
    VERIFY(!m_top_level_traversable); // Replacement is not allowed!
    VERIFY(&navigable->page() == this);
    m_top_level_traversable = navigable;
}

bool Page::top_level_traversable_is_initialized() const
{
    return m_top_level_traversable;
}

HTML::BrowsingContext& Page::top_level_browsing_context()
{
    return *m_top_level_traversable->active_browsing_context();
}

HTML::BrowsingContext const& Page::top_level_browsing_context() const
{
    return *m_top_level_traversable->active_browsing_context();
}

JS::NonnullGCPtr<HTML::TraversableNavigable> Page::top_level_traversable() const
{
    return *m_top_level_traversable;
}

template<typename ResponseType>
static ResponseType spin_event_loop_until_dialog_closed(PageClient& client, Optional<ResponseType>& response, SourceLocation location = SourceLocation::current())
{
    auto& event_loop = Web::HTML::current_settings_object().responsible_event_loop();

    ScopeGuard guard { [&] { event_loop.set_execution_paused(false); } };
    event_loop.set_execution_paused(true);

    Web::Platform::EventLoopPlugin::the().spin_until([&]() {
        return response.has_value() || !client.is_connection_open();
    });

    if (!client.is_connection_open()) {
        dbgln("WebContent client disconnected during {}. Exiting peacefully.", location.function_name());
        exit(0);
    }

    return response.release_value();
}

void Page::did_request_alert(String const& message)
{
    m_pending_dialog = PendingDialog::Alert;
    m_client->page_did_request_alert(message);

    if (!message.is_empty())
        m_pending_dialog_text = message;

    spin_event_loop_until_dialog_closed(*m_client, m_pending_alert_response);
}

void Page::alert_closed()
{
    if (m_pending_dialog == PendingDialog::Alert) {
        m_pending_dialog = PendingDialog::None;
        m_pending_alert_response = Empty {};
        m_pending_dialog_text.clear();
    }
}

bool Page::did_request_confirm(String const& message)
{
    m_pending_dialog = PendingDialog::Confirm;
    m_client->page_did_request_confirm(message);

    if (!message.is_empty())
        m_pending_dialog_text = message;

    return spin_event_loop_until_dialog_closed(*m_client, m_pending_confirm_response);
}

void Page::confirm_closed(bool accepted)
{
    if (m_pending_dialog == PendingDialog::Confirm) {
        m_pending_dialog = PendingDialog::None;
        m_pending_confirm_response = accepted;
        m_pending_dialog_text.clear();
    }
}

Optional<String> Page::did_request_prompt(String const& message, String const& default_)
{
    m_pending_dialog = PendingDialog::Prompt;
    m_client->page_did_request_prompt(message, default_);

    if (!message.is_empty())
        m_pending_dialog_text = message;

    return spin_event_loop_until_dialog_closed(*m_client, m_pending_prompt_response);
}

void Page::prompt_closed(Optional<String> response)
{
    if (m_pending_dialog == PendingDialog::Prompt) {
        m_pending_dialog = PendingDialog::None;
        m_pending_prompt_response = move(response);
        m_pending_dialog_text.clear();
    }
}

void Page::dismiss_dialog()
{
    switch (m_pending_dialog) {
    case PendingDialog::None:
        break;
    case PendingDialog::Alert:
        m_client->page_did_request_accept_dialog();
        break;
    case PendingDialog::Confirm:
    case PendingDialog::Prompt:
        m_client->page_did_request_dismiss_dialog();
        break;
    }
}

void Page::accept_dialog()
{
    switch (m_pending_dialog) {
    case PendingDialog::None:
        break;
    case PendingDialog::Alert:
    case PendingDialog::Confirm:
    case PendingDialog::Prompt:
        m_client->page_did_request_accept_dialog();
        break;
    }
}

void Page::did_request_color_picker(WeakPtr<HTML::HTMLInputElement> target, Color current_color)
{
    if (m_pending_non_blocking_dialog == PendingNonBlockingDialog::None) {
        m_pending_non_blocking_dialog = PendingNonBlockingDialog::ColorPicker;
        m_pending_non_blocking_dialog_target = move(target);

        m_client->page_did_request_color_picker(current_color);
    }
}

void Page::color_picker_update(Optional<Color> picked_color, HTML::ColorPickerUpdateState state)
{
    if (m_pending_non_blocking_dialog == PendingNonBlockingDialog::ColorPicker) {
        if (state == HTML::ColorPickerUpdateState::Closed)
            m_pending_non_blocking_dialog = PendingNonBlockingDialog::None;

        if (m_pending_non_blocking_dialog_target) {
            auto& input_element = verify_cast<HTML::HTMLInputElement>(*m_pending_non_blocking_dialog_target);
            input_element.did_pick_color(move(picked_color));
            if (state == HTML::ColorPickerUpdateState::Closed)
                m_pending_non_blocking_dialog_target.clear();
        }
    }
}

void Page::did_request_select_dropdown(WeakPtr<HTML::HTMLSelectElement> target, Web::CSSPixelPoint content_position, Web::CSSPixels minimum_width, Vector<Web::HTML::SelectItem> items)
{
    if (m_pending_non_blocking_dialog == PendingNonBlockingDialog::None) {
        m_pending_non_blocking_dialog = PendingNonBlockingDialog::Select;
        m_pending_non_blocking_dialog_target = move(target);
        m_client->page_did_request_select_dropdown(content_position, minimum_width, move(items));
    }
}

void Page::select_dropdown_closed(Optional<String> value)
{
    if (m_pending_non_blocking_dialog == PendingNonBlockingDialog::Select) {
        m_pending_non_blocking_dialog = PendingNonBlockingDialog::None;

        if (m_pending_non_blocking_dialog_target) {
            auto& select_element = verify_cast<HTML::HTMLSelectElement>(*m_pending_non_blocking_dialog_target);
            select_element.did_select_value(move(value));
            m_pending_non_blocking_dialog_target.clear();
        }
    }
}

void Page::did_request_media_context_menu(i32 media_id, CSSPixelPoint position, ByteString const& target, unsigned modifiers, MediaContextMenu menu)
{
    m_media_context_menu_element_id = media_id;
    client().page_did_request_media_context_menu(position, target, modifiers, move(menu));
}

WebIDL::ExceptionOr<void> Page::toggle_media_play_state()
{
    auto media_element = media_context_menu_element();
    if (!media_element)
        return {};

    // AD-HOC: An execution context is required for Promise creation hooks.
    HTML::TemporaryExecutionContext execution_context { media_element->document().relevant_settings_object() };

    if (media_element->potentially_playing())
        TRY(media_element->pause());
    else
        TRY(media_element->play());

    return {};
}

void Page::toggle_media_mute_state()
{
    auto media_element = media_context_menu_element();
    if (!media_element)
        return;

    // AD-HOC: An execution context is required for Promise creation hooks.
    HTML::TemporaryExecutionContext execution_context { media_element->document().relevant_settings_object() };

    media_element->set_muted(!media_element->muted());
}

WebIDL::ExceptionOr<void> Page::toggle_media_loop_state()
{
    auto media_element = media_context_menu_element();
    if (!media_element)
        return {};

    // AD-HOC: An execution context is required for Promise creation hooks.
    HTML::TemporaryExecutionContext execution_context { media_element->document().relevant_settings_object() };

    if (media_element->has_attribute(HTML::AttributeNames::loop))
        media_element->remove_attribute(HTML::AttributeNames::loop);
    else
        TRY(media_element->set_attribute(HTML::AttributeNames::loop, {}));

    return {};
}

WebIDL::ExceptionOr<void> Page::toggle_media_controls_state()
{
    auto media_element = media_context_menu_element();
    if (!media_element)
        return {};

    HTML::TemporaryExecutionContext execution_context { media_element->document().relevant_settings_object() };

    if (media_element->has_attribute(HTML::AttributeNames::controls))
        media_element->remove_attribute(HTML::AttributeNames::controls);
    else
        TRY(media_element->set_attribute(HTML::AttributeNames::controls, {}));

    return {};
}

JS::GCPtr<HTML::HTMLMediaElement> Page::media_context_menu_element()
{
    if (!m_media_context_menu_element_id.has_value())
        return nullptr;

    auto* dom_node = DOM::Node::from_unique_id(*m_media_context_menu_element_id);
    if (dom_node == nullptr)
        return nullptr;

    if (!is<HTML::HTMLMediaElement>(dom_node))
        return nullptr;

    return static_cast<HTML::HTMLMediaElement*>(dom_node);
}

void Page::set_user_style(String source)
{
    m_user_style_sheet_source = source;
    if (top_level_traversable_is_initialized() && top_level_traversable()->active_document()) {
        top_level_traversable()->active_document()->style_computer().invalidate_rule_cache();
    }
}

}

template<>
ErrorOr<void> IPC::encode(Encoder& encoder, Web::Page::MediaContextMenu const& menu)
{
    TRY(encoder.encode(menu.media_url));
    TRY(encoder.encode(menu.is_video));
    TRY(encoder.encode(menu.is_playing));
    TRY(encoder.encode(menu.is_muted));
    TRY(encoder.encode(menu.has_user_agent_controls));
    TRY(encoder.encode(menu.is_looping));
    return {};
}

template<>
ErrorOr<Web::Page::MediaContextMenu> IPC::decode(Decoder& decoder)
{
    return Web::Page::MediaContextMenu {
        .media_url = TRY(decoder.decode<AK::URL>()),
        .is_video = TRY(decoder.decode<bool>()),
        .is_playing = TRY(decoder.decode<bool>()),
        .is_muted = TRY(decoder.decode<bool>()),
        .has_user_agent_controls = TRY(decoder.decode<bool>()),
        .is_looping = TRY(decoder.decode<bool>()),
    };
}
