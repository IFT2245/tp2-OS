#ifndef EPHEMERAL_H
#define EPHEMERAL_H

/**
 * @brief Creates a temporary container folder (e.g. /tmp/container_XXXXXX).
 * @return A malloc'ed string with the folder path or NULL on failure.
 */
char* ephemeral_create_container(void);

/**
 * @brief Removes the ephemeral container folder created by ephemeral_create_container().
 *        By default attempts rmdir; optionally can remove contents if EPHEMERAL_RM_RECURSIVE is defined.
 * @param path Path to remove. If NULL, does nothing.
 */
void ephemeral_remove_container(const char* path);

#endif
