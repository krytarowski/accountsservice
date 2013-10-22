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
 * Private interfaces to the ActUserManager object
 */

#ifndef __ACT_USER_MANAGER_PRIVATE_H_
#define __ACT_USER_MANAGER_PRIVATE_H_

#include <pwd.h>

#include "act-user-manager.h"

G_BEGIN_DECLS

ActUser  *_act_user_manager_get_user   (ActUserManager *manager,
                                        const char     *object_path);
ActGroup *_act_user_manager_get_group  (ActUserManager *manager,
                                        const char     *group_path);

G_END_DECLS

#endif /* !__ACT_GROUP_PRIVATE_H_ */
