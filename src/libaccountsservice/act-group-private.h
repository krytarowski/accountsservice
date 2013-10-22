/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
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
 * Private interfaces to the ActGroup object
 */

#ifndef __ACT_GROUP_PRIVATE_H_
#define __ACT_GROUP_PRIVATE_H_

#include <pwd.h>

#include "act-user-manager.h"
#include "act-group.h"

G_BEGIN_DECLS

void           _act_group_update_from_object_path  (ActUserManager *manager,
                                                    ActGroup       *group,
                                                    const char     *object_path);
void           _act_group_update_as_nonexistent    (ActGroup    *group);
void           _act_group_load_from_group          (ActGroup    *group,
                                                    ActGroup    *group_to_copy);

G_END_DECLS

#endif /* !__ACT_GROUP_PRIVATE_H_ */
