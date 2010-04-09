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
#include "okc_messages.h"
#include "okc_connection.h"

typedef struct _OkCupidOutgoingMessage OkCupidOutgoingMessage;

struct _OkCupidOutgoingMessage {
	OkCupidAccount *oca;
	gchar *who;
	time_t time;
	gchar *message;
	gint msg_id;
	guint retry_count;
	gint rid;
};

gboolean okc_send_im_fom(OkCupidOutgoingMessage *msg);
void okc_check_inbox_cb(OkCupidAccount *oca, gchar *data, gsize data_len, gpointer user_data);

void okc_buddy_icon_cb(OkCupidAccount *oca, gchar *data, gsize data_len,
		gpointer user_data)
{
	gchar *buddyname;
	PurpleBuddy *buddy;
	OkCupidBuddy *obuddy;
	gpointer buddy_icon_data;

	buddyname = user_data;

	purple_debug_info("okcupid",
			"buddy icon for buddy %s %" G_GSIZE_FORMAT "\n",
			buddyname, data_len);

	buddy = purple_find_buddy(oca->account, buddyname);
	g_free(buddyname);
	if (buddy == NULL || buddy->proto_data == NULL)
		return;
	obuddy = buddy->proto_data;

	buddy_icon_data = g_memdup(data, data_len);

	purple_buddy_icons_set_for_user(oca->account, buddy->name,
			buddy_icon_data, data_len, obuddy->thumb_url);
}

void got_new_messages(OkCupidAccount *oca, gchar *data,
		gsize data_len, gpointer userdata)
{
	PurpleConnection *pc = userdata;

	/* NULL data will crash on Windows */
	if (data == NULL)
		data = "(null)";

	purple_debug_misc("okcupid", "got new messages: %s\n", data);

	/* Process incomming messages here */

	gchar *start_of_json = strchr(data, '{');
	gchar *end_of_json = strrchr(data, '}');
	
	if (!start_of_json || !end_of_json || start_of_json >= end_of_json)
	{
		okc_get_new_messages(oca);
		return;
	}
	
	gchar *json_string = g_strndup(start_of_json, end_of_json-start_of_json+1);
	
	JsonParser *parser;
	JsonNode *root;
	
	parser = json_parser_new();
	if(!json_parser_load_from_data(parser, json_string, -1, NULL))
	{
		g_free(json_string);
		okc_get_new_messages(oca);
		return;	
	}
	g_free(json_string);
	root = json_parser_get_root(parser);
	JsonObject *objnode;
	objnode = json_node_get_object(root);
	
	JsonArray *events = NULL;
	JsonArray *people = NULL;
	int unread_message_count = 0;
	
	if(json_object_has_member(objnode, "people"))
		people = json_node_get_array(json_object_get_member(objnode, "people"));
	if(json_object_has_member(objnode, "events"))
		events = json_node_get_array(json_object_get_member(objnode, "events"));
	if(json_object_has_member(objnode, "num_unread"))
		unread_message_count = json_node_get_int(json_object_get_member(objnode, "num_unread"));
	
	// Look through the buddy list for people to add first
	if (people != NULL)
	{
		GList *people_list = json_array_get_elements(people);
		GList *current;
		for (current = people_list; current; current = g_list_next(current))
		{
			JsonNode *currentNode = current->data;
			JsonObject *person = json_node_get_object(currentNode);
			
			if (!json_node_get_int(json_object_get_member(person, "is_buddy")) && 
				!purple_account_get_bool("show_stalkers", TRUE))
			{
				continue;
			}

			const gchar *buddy_name = json_node_get_string(json_object_get_member(person, "screenname"));
			const gchar *buddy_icon = json_node_get_string(json_object_get_member(person, "thumbnail"));
			gint is_online = json_node_get_int(json_object_get_member(person, "im_ok"));
			if (!json_object_has_member(person, "im_ok"))
				is_online = 1;
			
			PurpleBuddy *pbuddy = purple_find_buddy(oca->account, buddy_name);
			if (!pbuddy)
			{
				//Not everyone we talk to will be on our buddylist
				pbuddy = purple_buddy_new(oca->account, buddy_name, NULL);
				purple_blist_add_buddy(pbuddy, NULL, NULL, NULL);
			}
			if (pbuddy != NULL)
			{
				if (!json_node_get_int(json_object_get_member(person, "is_buddy")))
				{
					purple_blist_node_set_flags(&(pbuddy->node), PURPLE_BLIST_NODE_FLAG_NO_SAVE);
				} else {
					purple_blist_node_set_flags(&(pbuddy->node), 0);
				}
				if (is_online && !PURPLE_BUDDY_IS_ONLINE(pbuddy))
				{
					purple_prpl_got_user_status(oca->account, buddy_name, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);
				} else if (!is_online && PURPLE_BUDDY_IS_ONLINE(pbuddy))
				{
					purple_prpl_got_user_status(oca->account, buddy_name, purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE), NULL);
				}
				OkCupidBuddy *obuddy = pbuddy->proto_data;
				if (obuddy == NULL)
				{
					gchar *buddy_icon_url;
					
					obuddy = g_new0(OkCupidBuddy, 1);
					obuddy->buddy = pbuddy;
					obuddy->oca = oca;
					
					// load the old buddy icon url from the icon 'checksum'
					buddy_icon_url = (char *)purple_buddy_icons_get_checksum_for_user(pbuddy);
					if (buddy_icon_url != NULL)
						obuddy->thumb_url = g_strdup(buddy_icon_url);
					
					pbuddy->proto_data = obuddy;				
				}			
				if (!obuddy->thumb_url || !g_str_equal(obuddy->thumb_url, buddy_icon))
				{
					g_free(obuddy->thumb_url);
					obuddy->thumb_url = g_strdup(buddy_icon);
					if (g_str_has_prefix(buddy_icon, "http://cdn.okcimg.com/"))
						buddy_icon = &buddy_icon[21];
					okc_post_or_get(oca, OKC_METHOD_GET, "cdn.okcimg.com", buddy_icon, NULL, okc_buddy_icon_cb, g_strdup(buddy_name), FALSE);
				}
			}
		}
		g_list_free(people_list);
	}
	
	//loop through events looking for messages
	if (events != NULL)
	{
		GList *event_list = json_array_get_elements(events);
		GList *current;
		for (current = event_list; current; current = g_list_next(current))
		{
			JsonNode *currentNode = current->data;
			JsonObject *event = json_node_get_object(currentNode);
			const gchar *event_type;
			
			event_type = json_node_get_string(json_object_get_member(event, "type"));
			if (g_str_equal(event_type, "im"))
			{
				//instant message
				const gchar *message = json_node_get_string(json_object_get_member(event, "contents"));
				gchar *message_html = okc_strdup_withhtml(message);
				
				const gchar *who = NULL;
				PurpleMessageFlags flags;
				if (json_object_has_member(event, "to"))
				{
					who = json_node_get_string(json_object_get_member(event, "to"));
					flags = PURPLE_MESSAGE_SEND;
				} else if (json_object_has_member(event, "from"))
				{
					who = json_node_get_string(json_object_get_member(event, "from"));
					flags = PURPLE_MESSAGE_RECV;
				}
				if (who && (flags != PURPLE_MESSAGE_SEND || purple_account_get_bool(oca->account, "show_sent_messages", FALSE)))
					serv_got_im (pc, who, message_html, flags, time(NULL));

				g_free(message_html);
			} else if (g_str_equal(event_type, "orbit_user_signoff"))
			{
				//buddy signed off
				const gchar *buddy_name = json_node_get_string(json_object_get_member(event, "from"));
				PurpleBuddy *pbuddy = purple_find_buddy(oca->account, buddy_name);
				
				if (pbuddy && PURPLE_BUDDY_IS_ONLINE(pbuddy))
				{
					purple_prpl_got_user_status(oca->account, buddy_name, purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE), NULL);
				}
			} else if (g_str_equal(event_type, "stalk") && purple_account_get_bool("show_stalkers", TRUE))
			{
				//someone looked at the profile page (ie 'stalked' the user)
				const gchar *buddy_name = json_node_get_string(json_object_get_member(event, "from"));
				PurpleBuddy *pbuddy = purple_find_buddy(oca->account, buddy_name);

				//TODO: Update buddy message to say 'stalked' and/or open a conversation window with the stalker and/or put in a notification into pidgin's notification area
			}
		}
		g_list_free(event_list);
	}
	
	if (unread_message_count != oca->last_message_count)
	{
		oca->last_message_count = unread_message_count;
		if (unread_message_count > 0)
		{
			okc_post_or_get(oca, OKC_METHOD_GET, NULL, "/mailbox?folderid=1&low=1", NULL, okc_check_inbox_cb, NULL, FALSE);	
		}
	}
	
	if (json_object_has_member(objnode, "server_seqid"))
		oca->server_seqid = json_node_get_int(json_object_get_member(objnode, "server_seqid"));
	if (json_object_has_member(objnode, "server_gmt"))
		oca->server_gmt = json_node_get_int(json_object_get_member(objnode, "server_gmt"));
	
	g_object_unref(parser);
	
	/* Continue looping, waiting for more messages */
	okc_get_new_messages(oca);
}

void okc_get_new_messages_now(OkCupidAccount *oca)
{
	gchar *fetch_url;
	purple_debug_info("okcupid", "getting new messages now\n");

	fetch_url = g_strdup_printf("/instantevents?rand=0.%u&server_seqid=%u&server_gmt=%u&"
					"load_thumbnails=1&buddylist=1&"
					"show_offline=1&num_unread=1&im_status=1", 
					g_random_int(), oca->server_seqid, oca->server_gmt);

	okc_post_or_get(oca, OKC_METHOD_GET, NULL, fetch_url, NULL, got_new_messages, oca->pc, FALSE);

	g_free(fetch_url);
}

gboolean okc_get_new_messages(OkCupidAccount *oca)
{
	time_t now;
	gchar *fetch_url;

	oca->new_messages_check_timer = 0;

	now = time(NULL);
	if (oca->last_messages_download_time > now - 3) {
		/*
		 * Wait a bit before fetching more messages, to make sure we
		 * never hammer their servers.
		 *
		 * TODO: This could be smarter.  Like, allow 3 requests per
		 *       10 seconds or something.
		 */
		oca->new_messages_check_timer = purple_timeout_add_seconds(
				3 - (now - oca->last_messages_download_time),
				(GSourceFunc)okc_get_new_messages, oca);
		return FALSE;
	}

	purple_debug_info("okcupid", "getting new messages\n");

	fetch_url = g_strdup_printf("/instantevents?rand=0.%u&server_seqid=%u&server_gmt=%u&"
					"load_thumbnails=1&do_event_poll=1&buddylist=1&"
					"show_offline=1&num_unread=1&im_status=1&"
					"do_post_read=1", 
					g_random_int(), oca->server_seqid, oca->server_gmt);

	okc_post_or_get(oca, OKC_METHOD_GET, NULL, fetch_url, NULL, got_new_messages, oca->pc, TRUE);
	oca->last_messages_download_time = now;

	g_free(fetch_url);

	return FALSE;
}

void okc_msg_destroy(OkCupidOutgoingMessage *msg)
{
	if (msg == NULL)
		return;
	
	g_free(msg->who);
	g_free(msg->message);
	g_free(msg);
}

void okc_send_im_cb(OkCupidAccount *oca, gchar *data, gsize data_len, gpointer user_data)
{
	OkCupidOutgoingMessage *msg = user_data;

	if (data == NULL || data_len == 0)
	{
		//No response, resend message
		okc_send_im_fom(msg);
		return;
	}
	
	purple_debug_misc("okcupid", "sent im response: %s\n", data);
	
	/*
	Possible responses:

             {
               "message_sent" : 0,
               "reason" : "recip_not_online"
             }
             {
               "message_sent" : 0,
               "reason" : "im_self"
             }

	*/
	JsonParser *parser;
	JsonNode *root;
	
	parser = json_parser_new();
	if(!json_parser_load_from_data(parser, data, data_len, NULL))
	{
		okc_msg_destroy(msg);
		return;	
	}
	root = json_parser_get_root(parser);
	JsonObject *response;
	response = json_node_get_object(root);
	
	gint message_sent = json_node_get_int(json_object_get_member(response, "status"));
	
	if (message_sent < 100)
	{
		//Save the message we sent
		purple_debug_info("okcupid", "putting message into hashtable: '%s'\n", msg->message);
		
		okc_msg_destroy(msg);
		g_object_unref(parser);
		return;
	}
	
	const gchar *reason = json_node_get_string(json_object_get_member(response, "status_str"));
	if (reason != NULL)
	{
		if (g_str_equal(reason, "recip_not_online"))
		{
			serv_got_im(oca->pc, msg->who, _("Recipient not online"), PURPLE_MESSAGE_ERROR, time(NULL));
		} else if (g_str_equal(reason, "im_self"))
		{
			serv_got_im(oca->pc, msg->who, _("You cannot send an IM to yourself"), PURPLE_MESSAGE_ERROR, time(NULL));
		} else if (g_str_equal(reason, "im_not_ok"))
		{
			serv_got_im(oca->pc, msg->who, _("Recipient is 'missing'"), PURPLE_MESSAGE_ERROR, time(NULL));
		} else if (g_str_equal(reason, "recip_im_off"))
		{
			serv_got_im(oca->pc, msg->who, _("Recipient turned IM off"), PURPLE_MESSAGE_ERROR, time(NULL));		
		}
	}
	
	okc_msg_destroy(msg);
	g_object_unref(parser);
}

gboolean okc_send_im_fom(OkCupidOutgoingMessage *msg)
{
	gchar *encoded_message;
	gchar *encoded_recipient;
	gchar *postdata;

	encoded_message = g_strdup(purple_url_encode(msg->message));
	encoded_recipient = g_strdup(purple_url_encode(msg->who));
	postdata = g_strdup_printf("send=1&attempt=%d&rid=%d&recipient=%s&topic=false&body=%s",
			msg->retry_count + 1,
			msg->rid,
			encoded_recipient,
			encoded_message);
	g_free(encoded_message);
	g_free(encoded_recipient);

	okc_post_or_get(msg->oca, OKC_METHOD_POST, NULL, "/instantevents", postdata, okc_send_im_cb, msg, FALSE);
	g_free(postdata);

	return FALSE;
}

int okc_send_im(PurpleConnection *pc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	OkCupidOutgoingMessage *msg;

	msg = g_new0(OkCupidOutgoingMessage, 1);
	msg->oca = pc->proto_data;

	/* convert html to plaintext, removing trailing spaces */
	msg->message = purple_markup_strip_html(message);
	if (strlen(msg->message) > 999)
	{
		g_free(msg->message);
		g_free(msg);
		return -E2BIG;
	}
	
	msg->rid = g_random_int_range(0, 2000000000); /* just fits inside a 32bit int */
	msg->who = g_strdup(who);
	msg->time = time(NULL);
	msg->retry_count = 0;

	okc_send_im_fom(msg);
	
	if (purple_account_get_bool(pc->account, "show_sent_messages", FALSE))
	{
		return 0;
	}

	return strlen(message);
}

void okc_check_inbox_cb(OkCupidAccount *oca, gchar *data, gsize data_len, gpointer user_data)
{
	JsonParser *parser;
	JsonNode *root;
	JsonObject *mailbox;
	JsonArray *messages;
	
	purple_debug_misc("okcupid", "check_inbox_cb\n%s", (data?data:"(null)"));
	
	parser = json_parser_new();
	if(!json_parser_load_from_data(parser, data, data_len, NULL))
	{
		purple_debug_warning("okcupid", "Could not parse mailbox data\n");
		return;	
	}
	root = json_parser_get_root(parser);
	mailbox = json_node_get_object(root);
	if (json_object_has_member(mailbox, "messages"))
	{
		messages = json_node_get_array(json_object_get_member(mailbox, "messages"));
		GList *message_list = json_array_get_elements(messages);
		GList *current;
		for (current = message_list; current; current = g_list_next(current))
		{
			JsonNode *currentNode = current->data;
			JsonObject *message = json_node_get_object(currentNode);
			gboolean is_new = (gboolean) json_node_get_int(json_object_get_member(message, "is_new"));
			if (!is_new)
			{
				continue;
			}
			const gchar *subject = json_node_get_string(json_object_get_member(message, "subject"));
			const gchar *from = json_node_get_string(json_object_get_member(message, "person"));
			const gchar *to = oca->account->username;
			const gchar *thread_id = json_node_get_string(json_object_get_member(message, "thread_id"));
			gchar *url = g_strdup_printf("http://www.okcupid.com/messages?readmsg=true&threadid=%s&folder=1", thread_id);
			purple_notify_email(oca->pc, subject, from, to, url, NULL, NULL);
			g_free(url);
		}
		g_list_free(message_list);
	}
	
	g_object_unref(parser);
}
