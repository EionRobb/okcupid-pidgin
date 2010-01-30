/*
 * libokcupid
 *
 * libokcupid is the property of its developers.  See the COPYRIGHT file
 * for more details.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OKCUPID_BLIST_H
#define OKCUPID_BLIST_H

#include "libokcupid.h"

void okc_blist_wink_buddy(PurpleBlistNode *node, gpointer data);
void okc_get_info(PurpleConnection *pc, const gchar *uid);
void okc_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group);
void okc_remove_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group);
void okc_block_buddy(PurpleConnection *, const char *name);
GList *okc_blist_node_menu(PurpleBlistNode *node);

#endif /* OKCUPID_BLIST_H */
