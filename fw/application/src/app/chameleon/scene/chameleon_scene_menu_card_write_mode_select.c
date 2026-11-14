#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#include "amiibo_helper.h"
#include "app_chameleon.h"
#include "chameleon_scene.h"
#include "i18n/language.h"
#include "mini_app_launcher.h"
#include "mini_app_registry.h"
#include "mui_icons.h"
#include "settings.h"
#include "tag_helper.h"

void chameleon_scene_menu_card_write_mode_select_on_event(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                                          mui_list_item_t *p_item)
{
    app_chameleon_t *app = p_list_view->user_data;
    if (p_item->user_data != NULL_USER_DATA)
    {
        nfc_tag_mf1_set_write_mode((nfc_tag_mf1_write_mode_t) p_item->user_data);
    }
    mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
}

void chameleon_scene_menu_card_write_mode_select_on_enter(void *user_data)
{
    app_chameleon_t *app = user_data;
    for (uint32_t i = 0; i < 4; i++)
    {
        mui_list_view_add_item(app->p_list_view, ICON_EDIT, tag_helper_get_mf_write_mode_name(i),
                               (void *) (uintptr_t) i);
    }
    mui_list_view_add_item(app->p_list_view, ICON_BACK, _T(MAIN_RETURN), NULL_USER_DATA);

    mui_list_view_set_focus(app->p_list_view, nfc_tag_mf1_get_write_mode());

    mui_list_view_set_selected_cb(app->p_list_view, chameleon_scene_menu_card_write_mode_select_on_event);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, CHAMELEON_VIEW_ID_LIST);
}

void chameleon_scene_menu_card_write_mode_select_on_exit(void *user_data)
{
    app_chameleon_t *app = user_data;
    mui_list_view_clear_items(app->p_list_view);
}
