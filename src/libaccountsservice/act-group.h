/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

/*
 * Facade object for group data, owned by ActUserManager
 */

#ifndef __ACT_GROUP_H__
#define __ACT_GROUP_H__

#include <sys/types.h>
#include <glib.h>
#include <glib-object.h>

#include "act-types.h"

G_BEGIN_DECLS

#define ACT_TYPE_GROUP (act_group_get_type ())
#define ACT_GROUP(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), ACT_TYPE_GROUP, ActGroup))
#define ACT_IS_GROUP(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), ACT_TYPE_GROUP))

typedef struct _ActGroupClass ActGroupClass;

GType          act_group_get_type                  (void) G_GNUC_CONST;
ActUserManager *act_group_get_manager              (ActGroup *group);

const char    *act_group_get_object_path           (ActGroup    *group);

gboolean       act_group_is_loaded                 (ActGroup    *group);
gboolean       act_group_is_nonexistent            (ActGroup    *group);

gid_t          act_group_get_gid                   (ActGroup    *group);
const char    *act_group_get_group_name            (ActGroup    *group);
ActUser      **act_group_get_users                 (ActGroup    *group);

void           act_group_set_group_name            (ActGroup    *group,
                                                    const gchar *new_group_name);
void           act_group_add_user                  (ActGroup    *group,
                                                    ActUser     *user);
void           act_group_remove_user               (ActGroup    *group,
                                                    ActUser     *user);

G_END_DECLS

#endif /* __ACT_GROUP_H__ */
