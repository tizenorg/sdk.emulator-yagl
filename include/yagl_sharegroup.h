#ifndef _YAGL_SHAREGROUP_H_
#define _YAGL_SHAREGROUP_H_

#include "yagl_export.h"
#include "yagl_types.h"
#include "yagl_ref.h"
#include "yagl_namespace.h"

#define YAGL_NUM_NAMESPACES 6

#define YAGL_NUM_TEXTURE_TARGETS 4

struct yagl_sharegroup
{
    struct yagl_ref ref;

    struct yagl_namespace namespaces[YAGL_NUM_NAMESPACES];

    struct yagl_object *texture_zero[YAGL_NUM_TEXTURE_TARGETS];
};

YAGL_API struct yagl_sharegroup *yagl_sharegroup_create(void);

/*
 * Passing NULL won't hurt, this is for convenience.
 */
YAGL_API void yagl_sharegroup_acquire(struct yagl_sharegroup *sg);

/*
 * Passing NULL won't hurt, this is for convenience.
 */
YAGL_API void yagl_sharegroup_release(struct yagl_sharegroup *sg);

/*
 * Adds an object to share group and sets its local name,
 * this acquires 'obj', so the
 * caller should release 'obj' if he doesn't want to use it and wants
 * it to belong to this share group alone.
 */
YAGL_API void yagl_sharegroup_add(struct yagl_sharegroup *sg,
                                  int ns,
                                  struct yagl_object *obj);

/*
 * Same as the above, but adds an object with local_name.
 * If an object with such local name already exists then 'obj' will be
 * released and the existing object will be acquired and returned.
 * Otherwise, 'obj' is acquired and returned.
 * Typical use-case for this function is:
 *
 * yagl_object *obj;
 * obj = yagl_sharegroup_acquire_object(sg, ns, local_name);
 * if (!obj) {
 *     obj = yagl_xxx_create(...);
 *     obj = yagl_sharegroup_add_named(sg, ns, local_name, obj);
 * }
 * // use 'obj'.
 * yagl_object_release(obj);
 */
YAGL_API struct yagl_object *yagl_sharegroup_add_named(struct yagl_sharegroup *sg,
                                                       int ns,
                                                       yagl_object_name local_name,
                                                       struct yagl_object *obj);

/*
 * Removes an object from the share group, this also releases the
 * object.
 */
YAGL_API void yagl_sharegroup_remove(struct yagl_sharegroup *sg,
                                     int ns,
                                     yagl_object_name local_name);

/*
 * Acquires an object by its local name. Be sure to release the object when
 * you're done.
 */
YAGL_API struct yagl_object *yagl_sharegroup_acquire_object(struct yagl_sharegroup *sg,
                                                            int ns,
                                                            yagl_object_name local_name);

#endif
