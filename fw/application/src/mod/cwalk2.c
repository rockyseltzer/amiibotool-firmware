#include "cwalk2.h"

#include <stdio.h>
#include <string.h>

#include "cwalk.h"

#include "vfs.h"

bool cwalk_append_segment(char *out_path, const char *path, const char *segment)
{
    size_t path_len = strlen(path);
    const char *separator = (path_len > 0 && path[path_len - 1] == '/') ? "" : "/";
    int written = snprintf(out_path, VFS_MAX_PATH_LEN, "%s%s%s", path, separator, segment);
    if (written < 0 || written >= VFS_MAX_PATH_LEN)
    {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

size_t cwalk_get_segment_size(const char *path)
{
    struct cwk_segment segment;
    size_t segment_size = 0;
    if (!cwk_path_get_first_segment(path, &segment))
    {
        return 0;
    }

    do
    {
        segment_size++;
    } while (cwk_path_get_next_segment(&segment));

    return segment_size;
}

bool cwalk_same_prefix_segment(const char *base, const char *target)
{
    return strncmp(base, target, strlen(base)) == 0;
}

bool cwalk_dir_prefix_match(char *out_path, const char *path)
{
    size_t path_len = strlen(path);
    const char *separator = (path_len > 0 && path[path_len - 1] == '/') ? "" : "/";
    int written = snprintf(out_path, VFS_MAX_PATH_LEN, "%s%s", path, separator);
    if (written < 0 || written >= VFS_MAX_PATH_LEN)
    {
        out_path[0] = '\0';
        return false;
    }
    return true;
}
