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

#include "libokcupid.h"
#include "okc_connection.h"
#include "okc_blist.h"
#include "okc_messages.h"

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
	
	postdata = g_strdup_printf("woo=1&u=%s&ajax=1", purple_url_encode(buddy->name));
	
	okc_post_or_get(oca, OKC_METHOD_POST, NULL, "/profile", postdata, NULL, NULL, FALSE);
	
	g_free(postdata);
}

static gchar *okc_fix_json(const gchar *data)
{
	gchar *output;
	gchar *temp;

#define okc_replace(a,b) (purple_strreplace(a, "\n " b " : ", "\n \"" b "\" : "))

	temp = okc_replace(data, "screenname"); output = temp;
	temp = okc_replace(output, "uid"); g_free(output); output = temp;
	temp = okc_replace(output, "age"); g_free(output); output = temp;
	temp = okc_replace(output, "gender"); g_free(output); output = temp;
	temp = okc_replace(output, "sexpref"); g_free(output); output = temp;
	temp = okc_replace(output, "thumb"); g_free(output); output = temp;
	temp = okc_replace(output, "pics"); g_free(output); output = temp;
	temp = okc_replace(output, "location"); g_free(output); output = temp;
	temp = okc_replace(output, "relationshipstatus"); g_free(output); output = temp;
	temp = okc_replace(output, "match"); g_free(output); output = temp;
	temp = okc_replace(output, "enemy"); g_free(output); output = temp;
	temp = okc_replace(output, "friend"); g_free(output); output = temp;
	temp = okc_replace(output, "success"); g_free(output); output = temp;
	temp = okc_replace(output, "lastipaddress"); g_free(output); output = temp;
	
	return output;
}

void okc_got_info(OkCupidAccount *oca, gchar *data,
		gsize data_len, gpointer userdata)
{
	gchar *username = userdata;
	JsonParser *parser;
	JsonNode *root;
	PurpleNotifyUserInfo *user_info;
	gchar *value_tmp, *fixeddata;
	GError *error = NULL;
	JsonObject *info;
	
	if (!data || !data_len)
	{
		g_free(username);
		return;
	}
	
	/*{ screenname : "Saturn2888", uid : "16335530578074790482", age : "22", gender : "M", sexpref : "Straight", 
	thumb : "134x16/333x215/2/13750662203864942041.jpeg", 
	pics : [ "http://cdn.okcimg.com/php/load_okc_image.php/images/134x16/333x215/2/13750662203864942041.jpeg" , 
	"http://cdn.okcimg.com/php/load_okc_image.php/images/133x170/393x430/2/15211878639154382289.jpeg" , 
	"http://cdn.okcimg.com/php/load_okc_image.php/images/35x25/185x175/2/13821158136993628299.jpeg" , 
	"http://cdn.okcimg.com/php/load_okc_image.php/images/53x86/263x296/2/1445788482889546403.jpeg" , 
	"http://cdn.okcimg.com/php/load_okc_image.php/images/91x0/388x297/2/6083536188236847527.jpeg" ], 
	location : "Overland Park, Kansas", relationshipstatus : "SINGLE", match : "84", enemy : "7", 
	friend : "66", success : "true", lastipaddress : "" } */
	
	purple_debug_info("okcupid", "okc_got_info: %s\n", data);
	
	user_info = purple_notify_user_info_new();
	/* Insert link to profile at top */
	value_tmp = g_strdup_printf("<a href=\"http://www.okcupid.com/profile/%s\">%s</a>",
								username, _("View web profile"));
	purple_notify_user_info_add_pair(user_info, NULL, value_tmp);
	purple_notify_user_info_add_section_break(user_info);
	g_free(value_tmp);
	
	parser = json_parser_new();
	if(!json_parser_load_from_data(parser, data, data_len, &error))
	{
		//There was an error.  Try 'fixing' the data then loading it again
		fixeddata = okc_fix_json(data);
		if (!json_parser_load_from_data(parser, fixeddata, -1, &error))
		{
			g_free(fixeddata);
			purple_debug_error("okcupid", "got_info error: %s\n", error->message);
			g_error_free(error);
			g_free(username);
			
			purple_notify_userinfo(oca->pc, username, user_info, NULL, NULL);
			purple_notify_user_info_destroy(user_info);
			
			return;	
		}
		g_free(fixeddata);
	}
	root = json_parser_get_root(parser);
	info = json_node_get_object(root);
	
	purple_notify_user_info_add_pair(user_info, _("Age"), json_node_get_string(json_object_get_member(info, "age")));
	purple_notify_user_info_add_pair(user_info, _("Gender"), json_node_get_string(json_object_get_member(info, "gender")));
	purple_notify_user_info_add_pair(user_info, _("Sexual Preference"), json_node_get_string(json_object_get_member(info, "sexpref")));
	purple_notify_user_info_add_pair(user_info, _("Relationship Status"), json_node_get_string(json_object_get_member(info, "relationshipstatus")));
	purple_notify_user_info_add_pair(user_info, _("Location"), json_node_get_string(json_object_get_member(info, "location")));
	purple_notify_user_info_add_pair(user_info, _("Match"), json_node_get_string(json_object_get_member(info, "match")));
	purple_notify_user_info_add_pair(user_info, _("Enemy"), json_node_get_string(json_object_get_member(info, "enemy")));
	purple_notify_user_info_add_pair(user_info, _("Friend"), json_node_get_string(json_object_get_member(info, "friend")));
	
	const gchar *buddy_icon = json_node_get_string(json_object_get_member(info, "thumb"));
	PurpleBuddy *buddy = purple_find_buddy(oca->account, username);
	OkCupidBuddy *obuddy = buddy->proto_data;
	if (obuddy == NULL)
	{
		gchar *buddy_icon_url;
		
		obuddy = g_new0(OkCupidBuddy, 1);
		obuddy->buddy = buddy;
		obuddy->oca = oca;
		
		// load the old buddy icon url from the icon 'checksum'
		buddy_icon_url = (char *)purple_buddy_icons_get_checksum_for_user(buddy);
		if (buddy_icon_url != NULL)
			obuddy->thumb_url = g_strdup(buddy_icon_url);
		
		buddy->proto_data = obuddy;				
	}	
	if (!obuddy->thumb_url || !g_str_equal(obuddy->thumb_url, buddy_icon))
	{
		g_free(obuddy->thumb_url);
		obuddy->thumb_url = g_strdup(buddy_icon);		
		gchar *buddy_icon_url = g_strdup_printf("/php/load_okc_image.php/images/%s", buddy_icon);
		okc_post_or_get(oca, OKC_METHOD_GET, "cdn.okcimg.com", buddy_icon_url, NULL, okc_buddy_icon_cb, g_strdup(username), FALSE);
		g_free(buddy_icon_url);
	}
	
	purple_notify_userinfo(oca->pc, username, user_info, NULL, NULL);
	purple_notify_user_info_destroy(user_info);
	
	g_object_unref(parser);
	g_free(username);
}

void okc_get_info(PurpleConnection *pc, const gchar *uid)
{
	gchar *profile_url;

	profile_url = g_strdup_printf("/rjs/userinfo?u=%s&template=userinfo%%2Fajax.html", purple_url_encode(uid));

	okc_post_or_get(pc->proto_data, OKC_METHOD_GET, NULL, profile_url, NULL, okc_got_info, g_strdup(uid), FALSE);

	g_free(profile_url);
}

void okc_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	gchar *postdata;
	
	postdata = g_strdup_printf("addbuddy=1&u=%s&ajax=1", purple_url_encode(buddy->name));
	
	okc_post_or_get(pc->proto_data, OKC_METHOD_POST, NULL, "/profile", postdata, NULL, NULL, FALSE);
	
	g_free(postdata);
}

void okc_remove_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	gchar *postdata;
	
	postdata = g_strdup_printf("removebuddy=1&u=%s&ajax=1", purple_url_encode(buddy->name));
	
	okc_post_or_get(pc->proto_data, OKC_METHOD_POST, NULL, "/profile", postdata, NULL, NULL, FALSE);
	
	g_free(postdata);
}

void okc_block_buddy(PurpleConnection *pc, const char *name)
{
	gchar *block_url;
	
	block_url = g_strdup_printf("/instantevents?im_block=1&target_screenname=%s", purple_url_encode(name));
	
	okc_post_or_get(pc->proto_data, OKC_METHOD_GET, NULL, block_url, NULL, NULL, NULL, FALSE);
	
	g_free(block_url);
}


GList *okc_blist_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		act = purple_menu_action_new(_("_Wink"),
										PURPLE_CALLBACK(okc_blist_wink_buddy),
										NULL, NULL);
		m = g_list_append(m, act);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node))
	{
		
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node))
	{
		
	}
	return m;
}
