/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#ifndef __GROUP__
#define __GROUP__

#include <sys/types.h>
#include <grp.h>

#include <glib.h>
#include <gio/gio.h>

#include "types.h"

G_BEGIN_DECLS

#define TYPE_GROUP (group_get_type ())
#define GROUP(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), TYPE_GROUP, Group))
#define IS_GROUP(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), TYPE_GROUP))

GType          group_get_type                (void) G_GNUC_CONST;
Group *        group_new                     (Daemon         *daemon,
                                              gid_t           gid);

void           group_update_from_grent       (Group          *group,
                                              struct group   *grent,
                                              GHashTable     *users);

void           group_changed                 (Group          *group);

const gchar *  group_get_object_path         (Group          *group);
gid_t          group_get_gid                 (Group          *group);
const gchar *  group_get_group_name          (Group          *group);
gboolean       group_get_local_group         (Group          *group);
GStrv          group_get_users               (Group          *group);

G_END_DECLS

#endif
