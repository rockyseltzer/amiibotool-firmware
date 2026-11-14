#include "amiibo_helper.h"
#include "amiibo_scene.h"
#include "app_amiibo.h"
#include "i18n/language.h"
#include "mui_list_view.h"

#define ICON_INFO 0xe1c5

static void amiibo_scene_amiibo_info_on_selected(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                                 mui_list_item_t *p_item)
{
    app_amiibo_t *app = p_list_view->user_data;
    mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
}

static const char *amiibo_scene_amiibo_info_platform(uint32_t title_id_high)
{
    switch (title_id_high >> 16)
    {
        case 0x0004:
            return "3DS";
        case 0x0005:
            return "Wii U";
        case 0x0100:
            return "Switch";
        default:
            return "Other";
    }
}

static void amiibo_scene_amiibo_info_add_row(app_amiibo_t *app, const char *label, const char *value)
{
    mui_list_view_add_item_ext(app->p_list_view, ICON_INFO, label, value, NULL);
}

static void amiibo_scene_amiibo_info_add_date(app_amiibo_t *app, const char *label, uint16_t date)
{
    char buff[16];
    bool valid = amiibo_helper_format_date(date, buff, sizeof(buff));
    amiibo_scene_amiibo_info_add_row(app, label, valid ? buff : getLangString(_L_INFO_NOT_SET));
}

void amiibo_scene_amiibo_info_on_enter(void *user_data)
{
    app_amiibo_t *app = user_data;
    amiibo_info_t info;
    char buff[16];

    if (amiibo_helper_read_info(&app->ntag, &info) != NRF_SUCCESS)
    {
        mui_list_view_add_item(app->p_list_view, ICON_INFO, getLangString(_L_INFO_UNREADABLE), NULL);
    }
    else
    {
        const char *not_set = getLangString(_L_INFO_NOT_SET);

        amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_NICKNAME),
                                         info.nickname[0] ? info.nickname : not_set);
        amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_OWNER), info.owner[0] ? info.owner : not_set);
        amiibo_scene_amiibo_info_add_date(app, getLangString(_L_INFO_CREATED), info.created);
        amiibo_scene_amiibo_info_add_date(app, getLangString(_L_INFO_MODIFIED), info.modified);

        snprintf(buff, sizeof(buff), "%u", info.writes);
        amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_WRITES), buff);

        if (info.has_app_data)
        {
            amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_PLATFORM),
                                             amiibo_scene_amiibo_info_platform(info.title_id_high));
            snprintf(buff, sizeof(buff), "%08lx", (unsigned long) info.app_id);
            amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_APP_ID), buff);
        }
        else
        {
            amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_PLATFORM), not_set);
            amiibo_scene_amiibo_info_add_row(app, getLangString(_L_INFO_APP_ID), not_set);
        }
    }

    mui_list_view_set_selected_cb(app->p_list_view, amiibo_scene_amiibo_info_on_selected);
    mui_list_view_set_user_data(app->p_list_view, app);

    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, AMIIBO_VIEW_ID_LIST);
}

void amiibo_scene_amiibo_info_on_exit(void *user_data)
{
    app_amiibo_t *app = user_data;
    mui_list_view_set_selected_cb(app->p_list_view, NULL);
    mui_list_view_set_user_data(app->p_list_view, NULL);
    mui_list_view_clear_items(app->p_list_view);
}
