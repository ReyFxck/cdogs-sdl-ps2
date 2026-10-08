/* PS2SDK's libc declares these POSIX helpers but does not implement them.
 * tinydir uses writable copies. Preserve mounted device roots (host:, mass:/).
 */
#include <stddef.h>
#include <string.h>

static size_t PathRoot(const char *path)
{
    const char *colon = strchr(path, ':');
    const char *slash = strchr(path, '/');
    if (colon && (!slash || colon < slash))
        return (size_t)(colon - path) + 1 + (colon[1] == '/');
    return path[0] == '/' ? 1 : 0;
}
static size_t TrimEnd(char *path)
{
    size_t length = strlen(path), root = PathRoot(path);
    while (length > root && path[length - 1] == '/') path[--length] = '\0';
    return length;
}
char *basename(char *path)
{
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t length = TrimEnd(path), root = PathRoot(path);
    if (length == root) return root == 1 ? path : dot;
    char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path + root;
}
char *dirname(char *path)
{
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t length = TrimEnd(path), root = PathRoot(path);
    if (length == root) return path;
    char *slash = strrchr(path, '/');
    if (!slash)
    {
        if (!root) return dot;
        path[root] = '\0';
        return path;
    }
    size_t end = (size_t)(slash - path);
    while (end > root && path[end - 1] == '/') --end;
    if (end < root) end = root;
    path[end] = '\0';
    return *path ? path : dot;
}
