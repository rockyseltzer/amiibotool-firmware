#include "cwalk.h"
#include "nrf_log.h"

#include "amiibo_scene.h"
#include "app_amiibo.h"
#include "i18n/language.h"
#include "mini_app_launcher.h"
#include "mini_app_registry.h"
#include "mui_list_view.h"
#include "mui_mem.h"
#include "settings.h"
#include "vfs.h"
#include "vfs_meta.h"

#define ICON_FOLDER 0xe1d6
#define ICON_FILE   0xe1ed
#define ICON_BACK   0xe069
#define ICON_ERROR  0xe0a7
#define ICON_HOME   0xe1f0

#define FOLDER_LIST_PARENT 0xFFFF
#define FOLDER_LIST_NOTICE 0xFFFE

#define FILE_BROWSER_POOL_RESERVE 1024

static int amiibo_scene_file_browser_list_item_cmp(const mui_list_item_t *p_item_a, const mui_list_item_t *p_item_b)
{
    if (p_item_a->icon == ICON_HOME)
    {
        return -1;
    }
    if (p_item_b->icon == ICON_HOME)
    {
        return 1;
    }
    if (p_item_a->icon != p_item_b->icon)
    {
        return p_item_a->icon - p_item_b->icon;
    }
    else
    {
        return string_cmp(p_item_a->text, p_item_b->text);
    }
}

static bool amiibo_scene_file_browser_has_room(app_amiibo_t *app)
{
    size_t size = mui_list_view_item_size(app->p_list_view);
    size_t capacity = mui_list_item_array_capacity(app->p_list_view->items);
    size_t needed = FILE_BROWSER_POOL_RESERVE;
    if (size + 2 > capacity)
    {
        needed += (capacity < 8 ? 8 : capacity) * 2 * sizeof(mui_list_item_t);
    }

    return mui_mem_biggest_free() >= needed;
}

static bool amiibo_scene_file_browser_at_home(app_amiibo_t *app)
{
    const char *folder = string_get_cstr(app->current_folder);
    size_t len = strlen(app->home_folder);
    return strncmp(folder, app->home_folder, len) == 0 &&
           (folder[len] == '\0' || (folder[len] == '/' && folder[len + 1] == '\0'));
}

static void amiibo_scene_file_browser_reload_folders(app_amiibo_t *app)
{
    vfs_driver_t *p_vfs_driver;
    vfs_dir_t dir;
    vfs_obj_t obj;

    mui_list_view_clear_items(app->p_list_view);
    if (amiibo_scene_file_browser_at_home(app))
    {
        mui_list_view_add_item(app->p_list_view, ICON_HOME, _T(MAIN_MENU), (void *) FOLDER_LIST_PARENT);
    }
    else
    {
        mui_list_view_add_item(app->p_list_view, ICON_BACK, "..", (void *) FOLDER_LIST_PARENT);
    }

    p_vfs_driver = vfs_get_driver(app->current_drive);

    int32_t res = p_vfs_driver->open_dir(string_get_cstr(app->current_folder), &dir);
    if (res == VFS_OK)
    {
        while ((res = p_vfs_driver->read_dir(&dir, &obj)) == VFS_OK)
        {
            // hide file or dir if flagged with hidden
            vfs_meta_t meta;
            memset(&meta, 0, sizeof(vfs_meta_t));
            vfs_meta_decode(obj.meta, sizeof(obj.meta), &meta);
            if (meta.has_flags && (meta.flags & VFS_OBJ_FLAG_HIDDEN))
            {
                continue;
            }
            if (!amiibo_scene_file_browser_has_room(app))
            {
                mui_list_view_add_item(app->p_list_view, ICON_ERROR, _T(TOO_MANY_FILES), (void *) FOLDER_LIST_NOTICE);
                break;
            }
            uint16_t icon = obj.type == VFS_TYPE_DIR ? ICON_FOLDER : ICON_FILE;
            mui_list_view_add_item(app->p_list_view, icon, obj.name, (void *) -1);
        }
        p_vfs_driver->close_dir(&dir);
    }
    else
    {
        mui_list_view_add_item(app->p_list_view, ICON_ERROR, getLangString(_L_OPEN_FOLDER_FAILED), (void *) -1);
    }

    mui_list_view_sort(app->p_list_view, amiibo_scene_file_browser_list_item_cmp);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, AMIIBO_VIEW_ID_LIST);
}

static void amiibo_scene_file_browser_go_up(app_amiibo_t *app)
{
    struct cwk_segment seg;
    const char *folder_cstr = string_get_cstr(app->current_folder);
    cwk_path_get_last_segment(folder_cstr, &seg);
    char child[64];
    uint32_t n = seg.size < sizeof(child) - 1 ? seg.size : sizeof(child) - 1;
    memcpy(child, seg.begin, n);
    child[n] = 0;
    string_left(app->current_folder, seg.begin - folder_cstr);
    if (string_size(app->current_folder) > 1 && string_end_with_str_p(app->current_folder, "/"))
    {
        string_left(app->current_folder, string_size(app->current_folder) - 1);
    }
    if (string_size(app->current_folder) == 0)
    {
        string_cat_str(app->current_folder, "/");
    }
    amiibo_scene_file_browser_reload_folders(app);
    for (uint32_t i = 0; i < mui_list_view_item_size(app->p_list_view); i++)
    {
        mui_list_item_t *it = mui_list_item_array_get(app->p_list_view->items, i);
        if (it->icon == ICON_FOLDER && string_cmp_str(it->text, child) == 0)
        {
            mui_list_view_set_focus(app->p_list_view, i);
            break;
        }
    }
}

static void amiibo_scene_file_browser_on_selected(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                                  mui_list_item_t *p_item)
{
    app_amiibo_t *app = p_list_view->user_data;
    uint32_t idx = (uint32_t) p_item->user_data;

    if (idx == FOLDER_LIST_NOTICE)
    {
        return;
    }

    string_set(app->current_file, p_item->text);

    if (event == MUI_LIST_VIEW_EVENT_SELECTED)
    {
        if (idx == FOLDER_LIST_PARENT)
        {
            if (amiibo_scene_file_browser_at_home(app))
            {
                mini_app_launcher_kill(mini_app_launcher(), MINI_APP_ID_AMIIBO);
            }
            else
            {
                amiibo_scene_file_browser_go_up(app);
            }
        }
        else
        {
            if (p_item->icon == ICON_FOLDER)
            {
                if (!string_end_with_str_p(app->current_folder, "/"))
                {
                    string_cat_str(app->current_folder, "/");
                }
                string_cat(app->current_folder, p_item->text);
                amiibo_scene_file_browser_reload_folders(app);
            }
            else if (p_item->icon == ICON_HOME)
            {
                mini_app_launcher_kill(mini_app_launcher(), MINI_APP_ID_AMIIBO);
            }
            else if (p_item->icon != ICON_ERROR)
            {
                app->reload_amiibo_files = true;
                mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, AMIIBO_SCENE_AMIIBO_DETAIL);
            }
        }
    }
    else
    {
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, AMIIBO_SCENE_FILE_BROWSER_MENU);
    }
}

static int amiibo_scene_file_browser_on_back(void *ctx)
{
    app_amiibo_t *app = ctx;
    if (amiibo_scene_file_browser_at_home(app))
    {
        return 0;
    }
    amiibo_scene_file_browser_go_up(app);
    return 1;
}

void amiibo_scene_file_browser_on_enter(void *user_data)
{
    app_amiibo_t *app = user_data;
    NRF_LOG_INFO("%X", app);
    mui_list_view_set_selected_cb(app->p_list_view, amiibo_scene_file_browser_on_selected);
    mui_scene_dispatcher_set_back_handler(amiibo_scene_file_browser_on_back, app);
    mui_list_view_set_user_data(app->p_list_view, app);

    amiibo_scene_file_browser_reload_folders(app);

    mui_list_view_set_focus(app->p_list_view, app->current_focus_index);

    NRF_LOG_INFO("%X", app);
}

void amiibo_scene_file_browser_on_exit(void *user_data)
{
    app_amiibo_t *app = user_data;
    mui_scene_dispatcher_set_back_handler(NULL, NULL);
    mui_list_view_set_selected_cb(app->p_list_view, NULL);
    mui_list_view_set_user_data(app->p_list_view, NULL);
    mui_list_view_clear_items(app->p_list_view);
}
