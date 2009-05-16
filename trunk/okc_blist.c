/*
 * libfacebook
 *
 * libfacebook is the property of its developers.  See the COPYRIGHT file
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

#include "libokcupid.h"
#include "okc_connection.h"
#include "okc_blist.h"

void okc_got_online_buddies(OkCupidAccount *oca, gchar *data,
		gsize data_len, gpointer userdata)
{
	//[ { "screenname" : "Saturn2888", "userid" : "16335530578074790482", "open_connection" : 0, "im_ok" : 0 } , 
	//{ "screenname" : "eionrobb", "userid" : "339665194716288918", "open_connection" : 1, "im_ok" : 0 } ]
	JsonParser *parser;
	JsonNode *root;
	
	parser = json_parser_new();
	if(!json_parser_load_from_data(parser, data, data_len, NULL))
	{
		return;	
	}
	root = json_parser_get_root(parser);
	JsonArray *presences;
	presences = json_node_get_array(root);
	int i;
	for(i = 0; i < json_array_get_length(presences); i++)
	{
		JsonObject *presence;
		presence = json_node_get_object(json_array_get_element(presences, i));
		const gchar *buddy_name;
		buddy_name = json_node_get_string(json_object_get_member(presence, "screenname"));
		
		const char *status_id;
		if (json_node_get_int(json_object_get_member(presence, "open_connection")))
		{
			status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_ONLINE);
		} else {
			status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
		}
		
		purple_prpl_got_user_status(oca->account, buddy_name, status_id, NULL);
	}
	
	g_object_unref(parser);
}

gboolean okc_get_online_buddies(gpointer data)
{
	OkCupidAccount *oca = data;
	gchar *url;
	gchar *usernames = NULL;
	gchar *new_usernames;
	GSList *buddies;
	GSList *lbuddy;
	PurpleBuddy *buddy;
	
	buddies = purple_find_buddies(oca->account, NULL);
	for (lbuddy = buddies; lbuddy; lbuddy = g_slist_next(lbuddy))
	{
		buddy = lbuddy->data;
		if (usernames == NULL)
		{
			usernames = g_strdup(purple_url_encode(buddy->name));
		} else {
			new_usernames = g_strconcat(usernames, ",", purple_url_encode(buddy->name), NULL);
			g_free(usernames);
			usernames = new_usernames;
		}
	}
	g_slist_free(buddies);
	
	url = g_strdup_printf("/instantevents?is_online=1&rand=0.%u&usernames=%s", g_random_int(), usernames);
	
	okc_post_or_get(oca, OKC_METHOD_POST, NULL, url,
			NULL, okc_got_online_buddies, NULL, FALSE);
	
	g_free(url);
	g_free(usernames);

	return TRUE;
}

void okc_blist_wink_buddy(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	OkCupidAccount *oca;
	gchar *postdata;
	
	if(!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return;
	buddy = (PurpleBuddy *) node;
	if (!buddy || !buddy->account || !buddy->account->gc)
		return;
	oca = buddy->account->gc->proto_data;
	if (!oca)
		return;
	
	postdata = g_strdup_printf("woo=1&u=%s&ajax=1", buddy->name);
	
	okc_post_or_get(fba, OKC_METHOD_POST, NULL, "/profile", postdata, NULL, NULL, FALSE);
	
	g_free(postdata);
}

