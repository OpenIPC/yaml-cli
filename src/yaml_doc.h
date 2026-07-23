#ifndef YAML_DOC_H
#define YAML_DOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <yaml.h>

/* Operations on a loaded YAML document, addressed by a leading-dot dotted
 * path such as ".video0.codec". Keys are matched as whole components, so keys
 * containing a literal '.' are not addressable (as in the original tool). */

/* Print the scalar at `path` followed by a newline. Returns 1 if a scalar was
 * found and printed, 0 otherwise (missing path, or the path names a block). */
int yaml_doc_get(yaml_document_t *doc, const char *path);

/* Set `path` to the scalar `value`, creating intermediate mappings and the
 * final key as needed. Returns 1 on success, 0 on failure. */
int yaml_doc_set(yaml_document_t *doc, const char *path, const char *value);

/* Delete the key at `path`. Any parent mapping left empty by the deletion is
 * pruned as well, recursively, up to (but not including) the document root.
 * Returns 1 if a key was deleted, 0 if `path` was not found. */
int yaml_doc_del(yaml_document_t *doc, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef YAML_DOC_H */
