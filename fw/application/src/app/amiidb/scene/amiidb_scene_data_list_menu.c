#include "app_error.h"
#include "nrf_log.h"

#include "amiibo_helper.h"
#include "amiidb_api_slot.h"
#include "amiidb_scene.h"
#include "app_amiidb.h"
#include "cwalk2.h"
#include "db_header.h"
#include "i18n/language.h"
#include "mini_app_launcher.h"
#include "mini_app_registry.h"
#include "mui_icons.h"
#include "mui_list_view.h"
#include "ntag_emu.h"
#include "ntag_store.h"
#include "settings.h"
#include "vfs.h"

typedef enum
{
    AMIIDB_DATA_LIST_MENU_DELETE,
    AMIIDB_DATA_LIST_MENU_BACK,
} amiidb_data_list_menu_t;

static void amiidb_scene_data_list_menu_on_selected(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                                    mui_list_item_t *p_item)
{
    app_amiidb_t *app = p_list_view->user_data;
    switch ((amiidb_data_list_menu_t) p_item->user_data)
    {
        case AMIIDB_DATA_LIST_MENU_BACK:
            mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
            break;

        case AMIIDB_DATA_LIST_MENU_DELETE:
        {
            amiidb_api_slot_remove(app->cur_slot_index);
            mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
        }
        break;
    }
}

void amiidb_scene_data_list_menu_on_enter(void *user_data)
{
    app_amiidb_t *app = user_data;
    mui_list_view_clear_items(app->p_list_view);
    mui_list_view_add_item(app->p_list_view, ICON_DELETE, getLangString(_L_APP_AMIIDB_SLOT_DELETE),
                           (void *) AMIIDB_DATA_LIST_MENU_DELETE);
    mui_list_view_add_item(app->p_list_view, ICON_EXIT, getLangString(_L_APP_AMIIDB_BACK),
                           (void *) AMIIDB_DATA_LIST_MENU_BACK);

    mui_list_view_set_selected_cb(app->p_list_view, amiidb_scene_data_list_menu_on_selected);
    mui_list_view_set_user_data(app->p_list_view, app);

    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, AMIIDB_VIEW_ID_LIST);
}

void amiidb_scene_data_list_menu_on_exit(void *user_data)
{
    app_amiidb_t *app = user_data;
    mui_list_view_set_selected_cb(app->p_list_view, NULL);
    mui_list_view_clear_items(app->p_list_view);
}
