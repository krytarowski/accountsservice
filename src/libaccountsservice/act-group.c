/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
 * Copyright (C) 2007-2008 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2013 Red Hat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>

#include <stdio.h>
#include <float.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "act-user-manager-private.h"
#include "act-group-private.h"
#include "accounts-group-generated.h"

/**
 * SECTION:act-group
 * @title: ActGroup
 * @short_description: information about a group
 *
 * An ActGroup object represents a group on the system.
 */

/**
 * ActGroup:
 *
 * Represents a group on the system.
 *
 * Since: 0.6.36
 */

#define ACT_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ACT_TYPE_GROUP, ActGroupClass))
#define ACT_IS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ACT_TYPE_GROUP))
#define ACT_GROUP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), ACT_TYPE_GROUP, ActGroupClass))

#define ACCOUNTS_NAME            "org.freedesktop.Accounts"
#define ACCOUNTS_GROUP_INTERFACE "org.freedesktop.Accounts.Group"

enum {
        PROP_0,
        PROP_GID,
        PROP_GROUP_NAME,
        PROP_LOCAL_GROUP,
        PROP_USERS,              // XXX - don't know how to define this
        PROP_NONEXISTENT,
        PROP_IS_LOADED
};

enum {
        CHANGED,
        LAST_SIGNAL
};

struct _ActGroup {
        GObject         parent;

        ActUserManager  *manager;
        GDBusConnection *connection;
        AccountsGroup   *accounts_proxy;
        GDBusProxy      *object_proxy;
        GCancellable    *get_all_call;
        char            *object_path;

        gid_t           gid;
        char           *group_name;
        ActUser       **users;             // NULL terminated

        guint           gid_set : 1;
        guint           is_loaded : 1;
        guint           local_group : 1;
        guint           nonexistent : 1;
};

struct _ActGroupClass
{
        GObjectClass parent_class;
};

static void act_group_finalize     (GObject      *object);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ActGroup, act_group, G_TYPE_OBJECT)

static void
act_group_get_property (GObject    *object,
                        guint       param_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
        ActGroup *group;

        group = ACT_GROUP (object);

        switch (param_id) {
        case PROP_GID:
                g_value_set_int (value, group->gid);
                break;
        case PROP_GROUP_NAME:
                g_value_set_string (value, group->group_name);
                break;
        case PROP_LOCAL_GROUP:
                g_value_set_boolean (value, group->local_group);
                break;
        case PROP_IS_LOADED:
                g_value_set_boolean (value, group->is_loaded);
                break;
        case PROP_NONEXISTENT:
                g_value_set_boolean (value, group->nonexistent);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}


static void
act_group_class_init (ActGroupClass *class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (class);

        gobject_class->finalize = act_group_finalize;
        gobject_class->get_property = act_group_get_property;

        g_object_class_install_property (gobject_class,
                                         PROP_GID,
                                         g_param_spec_int ("gid",
                                                           "Group ID",
                                                           "The GID for this group.",
                                                           0, G_MAXINT, 0,
                                                           G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_GROUP_NAME,
                                         g_param_spec_string ("group-name",
                                                              "Group Name",
                                                              "The name for this group.",
                                                              NULL,
                                                              G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_NONEXISTENT,
                                         g_param_spec_boolean ("nonexistent",
                                                               "Doesn't exist",
                                                               "Determines whether or not the group object represents a valid group.",
                                                               FALSE,
                                                               G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_IS_LOADED,
                                         g_param_spec_boolean ("is-loaded",
                                                               "Is loaded",
                                                               "Determines whether or not the group object is loaded and ready to read from.",
                                                               FALSE,
                                                               G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_LOCAL_GROUP,
                                         g_param_spec_boolean ("local-group",
                                                               "Local Group",
                                                               "Local Group",
                                                               FALSE,
                                                               G_PARAM_READABLE));
        /**
         * ActGroup::changed:
         *
         * Emitted when the group changes in some way.
         */
        signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
act_group_init (ActGroup *group)
{
        GError *error = NULL;

        group->local_group = TRUE;
        group->group_name = NULL;

        group->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (group->connection == NULL) {
                g_warning ("Couldn't connect to system bus: %s", error->message);
                g_error_free (error);
        }
}

static void
free_user_array (ActUser **a)
{
        int i;
        if (a) {
                for (i = 0; a[i]; i++)
                        g_object_unref (a[i]);
                g_free (a);
        }
}

static void
act_group_finalize (GObject *object)
{
        ActGroup *group;

        group = ACT_GROUP (object);

        g_free (group->group_name);
        free_user_array (group->users);

        if (G_OBJECT_CLASS (act_group_parent_class)->finalize)
                (*G_OBJECT_CLASS (act_group_parent_class)->finalize) (object);
}

static void
set_is_loaded (ActGroup  *group,
               gboolean  is_loaded)
{
        if (group->is_loaded != is_loaded) {
                group->is_loaded = is_loaded;
                g_object_notify (G_OBJECT (group), "is-loaded");
        }
}

/**
 * _act_group_update_as_nonexistent:
 * @group: the group object to update.
 *
 * Set's the 'non-existent' property of @group to #TRUE
 * Can only be called before the group is loaded.
 *
 * Since: 0.6.36
 **/
void
_act_group_update_as_nonexistent (ActGroup *group)
{
        g_return_if_fail (ACT_IS_GROUP (group));
        g_return_if_fail (!act_group_is_loaded (group));
        g_return_if_fail (group->object_path == NULL);

        group->nonexistent = TRUE;
        g_object_notify (G_OBJECT (group), "nonexistent");

        set_is_loaded (group, TRUE);
}

/**
 * act_group_get_gid:
 * @group: the group object to examine.
 *
 * Retrieves the ID of @group.
 *
 * Returns: the group id.
 *
 * Since: 0.6.36
 **/

gid_t
act_group_get_gid (ActGroup *group)
{
        g_return_val_if_fail (ACT_IS_GROUP (group), -1);

        return group->gid;
}

/**
 * act_group_get_object_path:
 * @group: a #ActGroup
 *
 * Returns the group accounts service object path of @group,
 * or %NULL if @group doesn't have an object path associated
 * with it.
 *
 * Returns: (transfer none): the object path of the group
 *
 * Since: 0.6.36
 */
const char *
act_group_get_object_path (ActGroup *group)
{
        g_return_val_if_fail (ACT_IS_GROUP (group), NULL);

        return group->object_path;
}

/**
 * act_group_get_manager:
 * @group: a #ActGroup
 *
 * Returns the #ActUserManager that this #ActGroup object belongs to.
 *
 * Returns: (transfer none): the #ActUserManager
 *
 * Since: 0.6.36
 */
ActUserManager *
act_group_get_manager (ActGroup *group)
{
        return group->manager;
}

/**
 * act_group_get_group_name:
 * @group: the group object to examine.
 *
 * Retrieves the name of @group.
 *
 * Returns: (transfer none): a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 0.6.36
 **/

const char *
act_group_get_group_name (ActGroup *group)
{
        g_return_val_if_fail (ACT_IS_GROUP (group), NULL);

        return group->group_name;
}

/**
 * act_group_get_users:
 * @group: the group object to examine.
 *
 * Retrieves the members of @group that are users.
 *
 * Returns: (transfer none): a NULL-terminated array of pointers.
 *
 * Since: 0.6.36
 **/

ActUser **
act_group_get_users (ActGroup *group)
{
        g_return_val_if_fail (ACT_IS_GROUP (group), NULL);

        return group->users;
}

/**
 * act_group_is_nonexistent:
 * @group: the group object to examine.
 *
 * Retrieves whether the group is nonexistent or not.
 *
 * Returns: (transfer none): %TRUE if the group is nonexistent
 *
 * Since: 0.6.36
 **/
gboolean
act_group_is_nonexistent (ActGroup   *group)
{
        g_return_val_if_fail (ACT_IS_GROUP (group), FALSE);

        return group->nonexistent;
}

static void
collect_props (const gchar    *key,
               GVariant       *value,
               ActGroup       *group)
{
        gboolean handled = TRUE;

        if (strcmp (key, "Gid") == 0) {
                guint64 new_gid;

                new_gid = g_variant_get_uint64 (value);
                if (!group->gid_set || (guint64) group->gid != new_gid) {
                        group->gid = (uid_t) new_gid;
                        group->gid_set = TRUE;
                        g_object_notify (G_OBJECT (group), "gid");
                }
        } else if (strcmp (key, "GroupName") == 0) {
                const char *new_group_name;

                new_group_name = g_variant_get_string (value, NULL);
                if (g_strcmp0 (group->group_name, new_group_name) != 0) {
                        g_free (group->group_name);
                        group->group_name = g_strdup (new_group_name);
                        g_object_notify (G_OBJECT (group), "group-name");
                }
        } else if (strcmp (key, "LocalGroup") == 0) {
                gboolean new_local;

                new_local = g_variant_get_boolean (value);
                if (group->local_group != new_local) {
                        group->local_group = new_local;
                        g_object_notify (G_OBJECT (group), "local-group");
                }
        } else if (strcmp (key, "Users") == 0) {
                gboolean changed;
                GVariantIter iter;
                int i;
                const gchar *user_path;

                if (group->users == NULL)
                        changed = TRUE;
                else {
                        changed = FALSE;
                        i = 0;
                        g_variant_iter_init (&iter, value);
                        while (group->users[i] && g_variant_iter_next (&iter, "&o", &user_path)) {
                                if (g_strcmp0 (act_user_get_object_path (group->users[i]), user_path) != 0) {
                                        changed = TRUE;
                                        break;
                                }
                                i++;
                        }
                        if (group->users[i] != NULL || i != g_variant_n_children (value))
                                changed = TRUE;
                }

                if (changed) {
                        free_user_array (group->users);
                        group->users = g_new0 (ActUser*, g_variant_n_children (value)+1);
                        i = 0;
                        g_variant_iter_init (&iter, value);
                        while (g_variant_iter_next (&iter, "&o", &user_path)) {
                                group->users[i] = g_object_ref(_act_user_manager_get_user (group->manager,
                                                                                           user_path));
                                i++;
                        }
                        // g_object_notify (G_OBJECT (group), "users");
                }
        } else {
                handled = FALSE;
        }

        if (!handled) {
                g_debug ("unhandled property %s", key);
        }
}

static void
on_get_all_finished (GObject        *object,
                     GAsyncResult   *result,
                     gpointer data)
{
        GDBusProxy  *proxy = G_DBUS_PROXY (object);
        ActGroup     *group = data;
        GError      *error;
        GVariant    *res;
        GVariantIter *iter;
        gchar       *key;
        GVariant    *value;

        g_assert (G_IS_DBUS_PROXY (group->object_proxy));
        g_assert (group->object_proxy == proxy);

        error = NULL;
        res = g_dbus_proxy_call_finish (proxy, result, &error);

        if (! res) {
                g_debug ("Error calling GetAll() when retrieving properties for %s: %s",
                         group->object_path, error->message);
                g_error_free (error);

                if (!group->is_loaded) {
                        set_is_loaded (group, TRUE);
                }
                return;
        }

        g_object_unref (group->get_all_call);
        group->get_all_call = NULL;

        g_variant_get (res, "(a{sv})", &iter);
        while (g_variant_iter_next (iter, "{sv}", &key, &value)) {
                collect_props (key, value, group);
                g_free (key);
                g_variant_unref (value);
        }
        g_variant_iter_free (iter);
        g_variant_unref (res);

        if (!group->is_loaded) {
                set_is_loaded (group, TRUE);
        }

        g_signal_emit (group, signals[CHANGED], 0);
}

static void
update_info (ActGroup *group)
{
        g_assert (G_IS_DBUS_PROXY (group->object_proxy));

        if (group->get_all_call != NULL) {
                g_cancellable_cancel (group->get_all_call);
                g_object_unref (group->get_all_call);
        }

        group->get_all_call = g_cancellable_new ();
        g_dbus_proxy_call (group->object_proxy,
                           "GetAll",
                           g_variant_new ("(s)", ACCOUNTS_GROUP_INTERFACE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           group->get_all_call,
                           on_get_all_finished,
                           group);
}

static void
changed_handler (AccountsGroup *object,
                 gpointer   *data)
{
        ActGroup *group = ACT_GROUP (data);

        update_info (group);
}

/**
 * _act_group_update_from_object_path:
 * @group: the group object to update.
 * @object_path: the object path of the group to use.
 *
 * Updates the properties of @group from the accounts service via
 * the object path in @object_path.
 *
 * Since: 0.6.36
 **/
void
_act_group_update_from_object_path (ActUserManager *manager,
                                    ActGroup       *group,
                                    const char     *object_path)
{
        GError *error = NULL;

        g_return_if_fail (ACT_IS_USER_MANAGER (manager));
        g_return_if_fail (ACT_IS_GROUP (group));
        g_return_if_fail (object_path != NULL);
        g_return_if_fail (group->object_path == NULL);

        group->manager = manager;
        group->object_path = g_strdup (object_path);

        group->accounts_proxy = accounts_group_proxy_new_sync (group->connection,
                                                               G_DBUS_PROXY_FLAGS_NONE,
                                                               ACCOUNTS_NAME,
                                                               group->object_path,
                                                               NULL,
                                                               &error);
        if (!group->accounts_proxy) {
                g_warning ("Couldn't create accounts proxy: %s", error->message);
                g_error_free (error);
                return;
        }
        g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (group->accounts_proxy), INT_MAX);

        g_signal_connect (group->accounts_proxy, "changed", G_CALLBACK (changed_handler), group);

        group->object_proxy = g_dbus_proxy_new_sync (group->connection,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     0,
                                                     ACCOUNTS_NAME,
                                                     group->object_path,
                                                     "org.freedesktop.DBus.Properties",
                                                     NULL,
                                                     &error);
        if (!group->object_proxy) {
                g_warning ("Couldn't create accounts property proxy: %s", error->message);
                g_error_free (error);
                return;
        }

        update_info (group);
}

void
_act_group_load_from_group (ActGroup    *group,
                            ActGroup    *group_to_copy)
{
        if (!group_to_copy->is_loaded) {
                return;
        }

        /* loading groups may already have a gid, group name, or session list
         * from creation, so only update them if necessary
         */
        if (!group->gid_set) {
                group->gid = group_to_copy->gid;
                g_object_notify (G_OBJECT (group), "gid");
        }

        if (group->group_name == NULL) {
                group->group_name = g_strdup (group_to_copy->group_name);
                g_object_notify (G_OBJECT (group), "group-name");
        }

        set_is_loaded (group, TRUE);
}

/**
 * act_group_is_loaded:
 * @group: a #ActGroup
 *
 * Determines whether or not the group object is loaded and ready to read from.
 *
 * Returns: %TRUE or %FALSE
 *
 * Since: 0.6.36
 */
gboolean
act_group_is_loaded (ActGroup *group)
{
        return group->is_loaded;
}

/**
 * act_group_set_group_name:
 * @group: the group object to alter
 * @new_group_name: the new group name
 *
 * Changes the group name of @group to @new_group_name.
 *
 * Note this function is synchronous and ignores errors.
 *
 * Since: 0.6.36
 **/
void
act_group_set_group_name (ActGroup    *group,
                          const gchar *new_group_name)
{
        GError *error = NULL;

        g_return_if_fail (ACT_IS_GROUP (group));
        g_return_if_fail (ACCOUNTS_IS_GROUP (group->accounts_proxy));

        if (!accounts_group_call_set_group_name_sync (group->accounts_proxy,
                                                      new_group_name,
                                                      NULL,
                                                      &error)) {
                g_warning ("SetGroupName call failed: %s", error->message);
                g_error_free (error);
        }
}

/**
 * act_group_add_user:
 * @group: the group object to alter
 * @user: the user to add
 *
 * Makes @user a direct member of @group.  The @user object must
 * correspond to an existing user.
 *
 * Note this function is synchronous and ignores errors.
 *
 * Since: 0.6.36
 **/
void
act_group_add_user (ActGroup    *group,
                    ActUser     *user)
{
        GError *error = NULL;

        g_return_if_fail (ACT_IS_GROUP (group));
        g_return_if_fail (ACT_IS_USER (user));
        g_return_if_fail (act_user_get_object_path (user) != NULL);
        g_return_if_fail (ACCOUNTS_IS_GROUP (group->accounts_proxy));

        if (!accounts_group_call_add_user_sync (group->accounts_proxy,
                                                act_user_get_object_path (user),
                                                NULL,
                                                &error)) {
                g_warning ("AddUser call failed: %s", error->message);
                g_error_free (error);
        }
}

/**
 * act_group_remove_user:
 * @group: the group object to alter
 * @user: the user to remove
 *
 * Makes sure that @user is not a direct member of @group.  The @user
 * object must correspond to an existing user.
 *
 * Note this function is synchronous and ignores errors.
 *
 * Since: 0.6.36
 **/
void
act_group_remove_user (ActGroup    *group,
                       ActUser     *user)
{
        GError *error = NULL;

        g_return_if_fail (ACT_IS_GROUP (group));
        g_return_if_fail (ACT_IS_USER (user));
        g_return_if_fail (act_user_get_object_path (user) != NULL);
        g_return_if_fail (ACCOUNTS_IS_GROUP (group->accounts_proxy));

        if (!accounts_group_call_remove_user_sync (group->accounts_proxy,
                                                act_user_get_object_path (user),
                                                NULL,
                                                &error)) {
                g_warning ("RemoveUser call failed: %s", error->message);
                g_error_free (error);
        }
}
