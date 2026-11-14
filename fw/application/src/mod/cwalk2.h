#ifndef CWALK2_H
#define CWALK2_H

#include <stdbool.h>
#include <stddef.h>

bool cwalk_append_segment(char *out_path, const char *path, const char *segment);
size_t cwalk_get_segment_size(const char *path);
bool cwalk_same_prefix_segment(const char *base, const char *target);
bool cwalk_dir_prefix_match(char *out_path, const char *path);

#endif /* CWALK2_H */
