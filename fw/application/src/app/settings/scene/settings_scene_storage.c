
#include "nrf_log.h"

#include "app_settings.h"
#include "i18n/language.h"
#include "mui_list_view.h"
#include "vfs.h"

#define ICON_HOME 0xe1f0

enum storage_list_menu_t
{
    STORAGE_LIST_MENU_MOUNT,
    STORAGE_LIST_MENU_BACK
};

static void settings_scene_storage_on_selected(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                               mui_list_item_t *p_item)
{
    app_settings_t *app = p_list_view->user_data;
    uint32_t index = (uint32_t) p_item->user_data;
    switch (index)
    {
        case STORAGE_LIST_MENU_BACK:
            mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
            break;
    }
}

void settings_scene_storage_on_enter(void *user_data)
{
    app_settings_t *app = user_data;

    vfs_driver_t *p_driver = vfs_get_default_driver();
    vfs_stat_t stat;
    int32_t res = p_driver->stat(&stat);
    char txt[64];
    if (res == VFS_OK)
    {
#define mount_str getLangString(_L_MOUNTED_FFS)
        mui_list_view_add_item(app->p_list_view, 0xe146, stat.avaliable ? mount_str : getLangString(_L_NOT_MOUNTED),
                               (void *) -1);
        if (stat.avaliable)
        {
            snprintf(txt, sizeof(txt), "[%d kB]", stat.total_bytes / 1024);
            mui_list_view_add_item_ext(app->p_list_view, 0xe254, getLangString(_L_TOTAL_SPACE), txt, (void *) -1);
            snprintf(txt, sizeof(txt), "[%d kB]", stat.free_bytes / 1024);
            mui_list_view_add_item_ext(app->p_list_view, 0xe248, getLangString(_L_AVAILABLE_SPACE), txt, (void *) -1);
        }
    }

    mui_list_view_add_item(app->p_list_view, 0xe069, getLangString(_L_BACK), (void *) STORAGE_LIST_MENU_BACK);

    mui_list_view_set_selected_cb(app->p_list_view, settings_scene_storage_on_selected);

    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, SETTINGS_VIEW_ID_MAIN);
}

void settings_scene_storage_on_exit(void *user_data)
{
    app_settings_t *app = user_data;
    mui_list_view_set_selected_cb(app->p_list_view, NULL);
    mui_list_view_clear_items(app->p_list_view);
}
