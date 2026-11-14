#include "app_error.h"
#include "nrf_log.h"

#include "amiibo_helper.h"
#include "amiidb_scene.h"
#include "app_amiidb.h"
#include "cwalk2.h"
#include "i18n/language.h"
#include "mini_app_launcher.h"
#include "mui_icons.h"
#include "ntag_emu.h"
#include "settings.h"

static void amiidb_scene_settings_slot_num_on_selected(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                                       mui_list_item_t *p_item)
{
    app_amiidb_t *app = p_list_view->user_data;

    uint32_t slot_num = (uint32_t) p_item->user_data;
    if (slot_num > 0)
    {
        settings_data_t *p_settings = settings_get_data();
        p_settings->amiidb_data_slot_num = slot_num;
    }
    mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
}

void amiidb_scene_settings_slot_num_on_enter(void *user_data)
{
    app_amiidb_t *app = user_data;

    settings_data_t *p_settings = settings_get_data();
    mui_list_view_clear_items(app->p_list_view);
    mui_list_view_add_item(app->p_list_view, ICON_GRID, "10", (void *) 10);
    mui_list_view_add_item(app->p_list_view, ICON_GRID, "20", (void *) 20);
    mui_list_view_add_item(app->p_list_view, ICON_GRID, "30", (void *) 30);
    mui_list_view_add_item(app->p_list_view, ICON_GRID, "40", (void *) 40);
    mui_list_view_add_item(app->p_list_view, ICON_GRID, "50", (void *) 50);
    mui_list_view_add_item(app->p_list_view, ICON_EXIT, getLangString(_L_APP_AMIIDB_BACK), (void *) 0);
    uint8_t focus = p_settings->amiidb_data_slot_num / 10 - 1;
    if (focus >= 5)
    {
        focus = 0;
    }
    mui_list_view_set_focus(app->p_list_view, focus);
    mui_list_view_set_selected_cb(app->p_list_view, amiidb_scene_settings_slot_num_on_selected);
    mui_list_view_set_user_data(app->p_list_view, app);

    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, AMIIDB_VIEW_ID_LIST);
}

void amiidb_scene_settings_slot_num_on_exit(void *user_data)
{
    app_amiidb_t *app = user_data;
    mui_list_view_set_selected_cb(app->p_list_view, NULL);
    mui_list_view_clear_items(app->p_list_view);
}
