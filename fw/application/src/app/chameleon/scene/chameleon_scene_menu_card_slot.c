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

#define CARD_SLOT_MENU_SLOT_NUM ((void *) -2)

void chameleon_scene_menu_card_slot_on_event(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                             mui_list_item_t *p_item)
{
    app_chameleon_t *app = p_list_view->user_data;
    char buff[16];
    if (p_item->user_data == CARD_SLOT_MENU_SLOT_NUM)
    {
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, CHAMELEON_SCENE_MENU_SLOT_NUM_SELECT);
    }
    else if (p_item->user_data == NULL_USER_DATA)
    {
        mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
    }
    else
    {
        uint8_t slot = (uint8_t) (uintptr_t) p_item->user_data;
        tag_emulation_slot_set_enable(slot, TAG_SENSE_HF, !tag_emulation_slot_is_enabled(slot, TAG_SENSE_HF));
        sprintf(buff, "%s", tag_emulation_slot_is_enabled(slot, TAG_SENSE_HF) ? _T(ON_F) : _T(OFF_F));
        mui_list_view_item_set_sub_text(p_item, buff);
    }
}

void chameleon_scene_menu_card_slot_on_enter(void *user_data)
{
    app_chameleon_t *app = user_data;
    settings_data_t *settings = settings_get_data();

    char buff[32];
    sprintf(buff, "[%0d]", settings->chameleon_slot_num);
    mui_list_view_add_item_ext(app->p_list_view, ICON_GRID, _T(APP_CHAMELEON_CARD_SLOT_NUM), buff,
                               CARD_SLOT_MENU_SLOT_NUM);
    for (uint32_t i = 0; i < settings->chameleon_slot_num; i++)
    {
        sprintf(buff, "%s %02d", _T(APP_CHAMELEON_CARD_SLOT), i + 1);
        mui_list_view_add_item_ext(app->p_list_view, ICON_SLOT, buff,
                                   tag_emulation_slot_is_enabled(i, TAG_SENSE_HF) ? _T(ON_F) : _T(OFF_F), (void *) i);
    }
    mui_list_view_add_item(app->p_list_view, ICON_BACK, _T(MAIN_RETURN), NULL_USER_DATA);

    mui_list_view_set_selected_cb(app->p_list_view, chameleon_scene_menu_card_slot_on_event);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, CHAMELEON_VIEW_ID_LIST);
}

void chameleon_scene_menu_card_slot_on_exit(void *user_data)
{
    app_chameleon_t *app = user_data;
    mui_list_view_clear_items(app->p_list_view);
}
