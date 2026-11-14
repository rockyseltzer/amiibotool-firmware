#include "vfs_meta.h"

#include "df_buffer.h"

void vfs_meta_decode(uint8_t *p_meta, uint8_t size, vfs_meta_t *p_out)
{
    NEW_BUFFER_READ(buff, p_meta, size);
    uint8_t meta_size = buff_get_u8(&buff);
    if (meta_size == 0 || meta_size >= size)
    {
        return;
    }

    buff_set_limit(&buff, meta_size + 1);

    while (buff_get_remain_size(&buff) > 0)
    {
        uint8_t type = buff_get_u8(&buff);
        if (type == VFS_META_TYPE_NOTES)
        {
            if (buff_get_remain_size(&buff) < 1)
            {
                return;
            }
            uint8_t len = buff_get_u8(&buff);
            if (len > buff_get_remain_size(&buff))
            {
                return;
            }
            size_t kept = len < VFS_META_MAX_NOTES_SIZE - 1 ? len : VFS_META_MAX_NOTES_SIZE - 1;
            p_out->has_notes = true;
            buff_get_byte_array(&buff, p_out->notes, kept);
            p_out->notes[kept] = '\0';
            buff_get_byte_array(&buff, NULL, len - kept);
        }
        else if (type == VFS_META_TYPE_FLAGS)
        {
            if (buff_get_remain_size(&buff) < 1)
            {
                return;
            }
            p_out->has_flags = true;
            p_out->flags = buff_get_u8(&buff);
        }
        else if (type == VFS_META_TYPE_AMIIBO_ID)
        {
            if (buff_get_remain_size(&buff) < 8)
            {
                return;
            }
            p_out->has_amiibo_id = true;
            p_out->amiibo_head = buff_get_u32(&buff);
            p_out->amiibo_tail = buff_get_u32(&buff);
        }
    }
}
void vfs_meta_encode(uint8_t *p_meta, uint8_t size, vfs_meta_t *p_in)
{
    NEW_BUFFER(buff, p_meta, size);
    buff_put_u8(&buff, 0); // place holder
    if (p_in->has_notes)
    {
        buff_put_u8(&buff, VFS_META_TYPE_NOTES);
        buff_put_u8(&buff, strlen(p_in->notes));
        buff_put_byte_array(&buff, p_in->notes, strlen(p_in->notes));
    }

    if (p_in->has_flags)
    {
        buff_put_u8(&buff, VFS_META_TYPE_FLAGS);
        buff_put_u8(&buff, p_in->flags);
    }

    if (p_in->has_amiibo_id)
    {
        buff_put_u8(&buff, VFS_META_TYPE_AMIIBO_ID);
        buff_put_u32(&buff, p_in->amiibo_head);
        buff_put_u32(&buff, p_in->amiibo_tail);
    }

    p_meta[0] = buff_get_size(&buff) - 1;
}
