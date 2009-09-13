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

#ifndef OKCUPID_MESSAGES_H
#define OKCUPID_MESSAGES_H

#include "libokcupid.h"

int okc_send_im(PurpleConnection *pc, const gchar *who, const gchar *message,
		PurpleMessageFlags flags);
gboolean okc_get_new_messages(OkCupidAccount *oca);
void okc_get_new_messages_now(OkCupidAccount *oca);
void okc_buddy_icon_cb(OkCupidAccount *oca, gchar *data, gsize data_len,
		gpointer user_data);

#endif /* OKCUPID_MESSAGES_H */
