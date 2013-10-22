/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
  *
  * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
  * Copyright (C) 2007-2008 William Jon McCann <mccann@jhu.edu>
  * Copyright (C) 2009-2010 Red Hat, Inc.
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

#define _BSD_SOURCE

#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <grp.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <polkit/polkit.h>

#include "daemon.h"
#include "group.h"
#include "accounts-group-generated.h"
#include "util.h"

enum {
        PROP_0,
        PROP_GID,
        PROP_GROUP_NAME,
        PROP_LOCAL_GROUP,
        PROP_USERS,
};

struct Group {
        AccountsGroupSkeleton parent;

        gchar *object_path;

        Daemon       *daemon;

        gid_t         gid;
        gchar        *group_name;
        gboolean      local_group;
        GStrv         users;
};

typedef struct GroupClass
{
        AccountsGroupSkeletonClass parent_class;
} GroupClass;

static void group_accounts_group_iface_init (AccountsGroupIface *iface);

G_DEFINE_TYPE_WITH_CODE (Group, group, ACCOUNTS_TYPE_GROUP_SKELETON, G_IMPLEMENT_INTERFACE (ACCOUNTS_TYPE_GROUP, group_accounts_group_iface_init));

static GStrv
find_user_paths (Group *group, GHashTable *users, gchar **members)
{
        int i, j, n;
        GStrv paths;

        n = 0;
        for (i = 0; members[i]; i++) {
                if (g_hash_table_lookup (users, members[i]))
                        n++;
        }

        paths = g_new0 (gchar *, n+1);
        for (i = 0, j = 0; members[i] && j < n; i++) {
                User *user = g_hash_table_lookup (users, members[i]);
                if (user)
                        paths[j++] = g_strdup (user_get_object_path (user));
        }

        strv_sort (paths);

        return paths;
}

void
group_update_from_grent (Group        *group,
                         struct group *grent,
                         GHashTable   *users)
{
        gboolean changed = FALSE;
        GStrv new_members;

        g_object_freeze_notify (G_OBJECT (group));

        if (grent->gr_gid != group->gid) {
                group->gid = grent->gr_gid;
                changed = TRUE;
                g_object_notify (G_OBJECT (group), "gid");
        }

        if (g_strcmp0 (group->group_name, grent->gr_name) != 0) {
                g_free (group->group_name);
                group->group_name = g_strdup (grent->gr_name);
                changed = TRUE;
                g_object_notify (G_OBJECT (group), "group-name");
        }

        new_members = find_user_paths (group, users, grent->gr_mem);
        if (!strv_equal (group->users, new_members)) {
                g_strfreev (group->users);
                group->users = new_members;
                changed = TRUE;
                g_object_notify (G_OBJECT (group), "users");
        } else
                g_strfreev (new_members);

        g_object_thaw_notify (G_OBJECT (group));

        if (changed)
                accounts_group_emit_changed (ACCOUNTS_GROUP (group));
}

void
group_changed (Group *group)
{
        accounts_group_emit_changed (ACCOUNTS_GROUP (group));
}

static gchar *
compute_object_path (Group *group)
{
        gchar *object_path;

        object_path = g_strdup_printf ("/org/freedesktop/Accounts/Group%ld",
                                       (long) group->gid);
        return object_path;
}

Group *
group_new (Daemon *daemon,
           gid_t   gid)
{
        Group *group;

        group = g_object_new (TYPE_GROUP, NULL);
        group->daemon = daemon;
        group->gid = gid;
        group->local_group = TRUE;

        group->object_path = compute_object_path (group);

        return group;
}

const gchar *
group_get_object_path (Group *group)
{
        return group->object_path;
}

gid_t
group_get_gid (Group *group)
{
        return group->gid;
}

const gchar *
group_get_group_name (Group *group)
{
        return group->group_name;
}

gboolean
group_get_local_group (Group *group)
{
        return group->local_group;
}

GStrv
group_get_users (Group *group)
{
        return group->users;
}

static void
throw_error (GDBusMethodInvocation *context,
             gint                   error_code,
             const gchar           *format,
             ...)
{
        va_list args;
        gchar *message;

        va_start (args, format);
        message = g_strdup_vprintf (format, args);
        va_end (args);

        g_dbus_method_invocation_return_error (context, ERROR, error_code, "%s", message);

        g_free (message);
}

static void
group_finalize (GObject *object)
{
        Group *group;

        group = GROUP (object);

        g_free (group->object_path);
        g_free (group->group_name);

        if (G_OBJECT_CLASS (group_parent_class)->finalize)
                (*G_OBJECT_CLASS (group_parent_class)->finalize) (object);
}

static void
group_set_property (GObject      *object,
                    guint         param_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
        Group *group = GROUP (object);

        switch (param_id) {
        case PROP_GID:
                group->gid = g_value_get_uint64 (value);
                break;
        case PROP_GROUP_NAME:
                group->group_name = g_value_dup_string (value);
                break;
        case PROP_LOCAL_GROUP:
                group->local_group = g_value_get_boolean (value);
                break;
        case PROP_USERS:
                group->users = g_value_dup_boxed (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
group_get_property (GObject    *object,
                    guint       param_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
        Group *group = GROUP (object);

        switch (param_id) {
        case PROP_GID:
                g_value_set_uint64 (value, group_get_gid (group));
                break;
        case PROP_GROUP_NAME:
                g_value_set_string (value, group_get_group_name (group));
                break;
        case PROP_LOCAL_GROUP:
                g_value_set_boolean (value, group_get_local_group (group));
                break;
        case PROP_USERS:
                g_value_set_boxed (value, group_get_users (group));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
group_class_init (GroupClass *class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (class);

        gobject_class->get_property = group_get_property;
        gobject_class->set_property = group_set_property;
        gobject_class->finalize = group_finalize;

        accounts_group_override_properties (gobject_class, 1);
}

typedef struct {
        Group *group;
        gchar *new_group_name;
} SetGroupNameData;

static void
free_set_group_data (SetGroupNameData *data)
{
        g_object_unref (data->group);
        g_free (data->new_group_name);
        g_free (data);
}

static void
group_set_group_name_authorized_cb (Daemon                *daemon,
                                    User                  *dummy,
                                    GDBusMethodInvocation *context,
                                    gpointer               user_data)

{
        SetGroupNameData *data = user_data;
        GError *error;
        const gchar *argv[6];

        sys_log (context, "changing name of group '%s' to '%s'",
                 group_get_group_name (data->group),
                 data->new_group_name);

        argv[0] = "/usr/sbin/groupmod";
        argv[1] = "-n";
        argv[2] = data->new_group_name;
        argv[3] = "--";
        argv[4] = group_get_group_name (data->group);
        argv[5] = NULL;

        error = NULL;
        if (!spawn_with_login_uid (context, argv, &error)) {
                throw_error (context, ERROR_FAILED, "running '%s' failed: %s", argv[0], error->message);
                g_error_free (error);
                return;
        }

        accounts_group_complete_set_group_name (NULL, context);
}

static gboolean
group_set_group_name (AccountsGroup         *group,
                      GDBusMethodInvocation *context,
                      const gchar           *new_group_name)
{
        SetGroupNameData *data;

        data = g_new0 (SetGroupNameData, 1);
        data->group = g_object_ref (GROUP (group));
        data->new_group_name = g_strdup (new_group_name);

        daemon_local_check_auth (data->group->daemon,
                                 NULL,
                                 "org.freedesktop.accounts.user-administration",
                                 TRUE,
                                 group_set_group_name_authorized_cb,
                                 context,
                                 data,
                                 (GDestroyNotify)free_set_group_data);

        return TRUE;

}

typedef struct {
        Group *group;
        User *user;
        gboolean add;
} AddRemoveUserData;

static void
free_add_remove_user_data (AddRemoveUserData *data)
{
        g_object_unref (data->group);
        g_object_unref (data->user);
        g_free (data);
}

static void
group_add_remove_user_authorized_cb (Daemon                *daemon,
                                     User                  *dummy,
                                     GDBusMethodInvocation *context,
                                     gpointer               user_data)

{
        AddRemoveUserData *data = user_data;
        GError *error;
        const gchar *argv[6];

        sys_log (context, "%s user '%s' %s group '%s'",
                 data->add? "add" : "remove",
                 user_get_user_name (data->user),
                 data->add? "to" : "from",
                 group_get_group_name (data->group));

        argv[0] = "/usr/sbin/groupmems";
        argv[1] = "-g";
        argv[2] = group_get_group_name (data->group);
        argv[3] = data->add? "-a" : "-d";
        argv[4] = user_get_user_name (data->user);
        argv[5] = NULL;

        error = NULL;
        if (!spawn_with_login_uid (context, argv, &error)) {
                throw_error (context, ERROR_FAILED, "running '%s' failed: %s", argv[0], error->message);
                g_error_free (error);
                return;
        }

        if (data->add)
                accounts_group_complete_add_user (NULL, context);
        else
                accounts_group_complete_remove_user (NULL, context);
}

static gboolean
group_add_remove_user (AccountsGroup         *group,
                       GDBusMethodInvocation *context,
                       const gchar           *user_path,
                       gboolean               add)
{
        AddRemoveUserData *data;
        User *user = daemon_local_get_user (GROUP(group)->daemon, user_path);

        if (user == NULL) {
                throw_error (context, ERROR_FAILED, "object '%s' does not exist", user_path);
                return TRUE;
        }

        data = g_new0 (AddRemoveUserData, 1);
        data->group = g_object_ref (GROUP (group));
        data->user = g_object_ref (user);
        data->add = add;

        daemon_local_check_auth (data->group->daemon,
                                 NULL,
                                 "org.freedesktop.accounts.user-administration",
                                 TRUE,
                                 group_add_remove_user_authorized_cb,
                                 context,
                                 data,
                                 (GDestroyNotify)free_add_remove_user_data);

        return TRUE;
}

static gboolean
group_add_user (AccountsGroup         *group,
                GDBusMethodInvocation *context,
                const gchar           *user_path)
{
        return group_add_remove_user (group, context, user_path, TRUE);
}

static gboolean
group_remove_user (AccountsGroup         *group,
                GDBusMethodInvocation *context,
                const gchar           *user_path)
{
        return group_add_remove_user (group, context, user_path, FALSE);
}

static void
group_accounts_group_iface_init (AccountsGroupIface *iface)
{
        iface->handle_set_group_name = group_set_group_name;
        iface->handle_add_user = group_add_user;
        iface->handle_remove_user = group_remove_user;
}

static void
group_init (Group *group)
{
        group->object_path = NULL;
        group->group_name = NULL;
}
