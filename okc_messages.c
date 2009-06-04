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
	guint rid;
};

gboolean okc_send_im_fom(OkCupidOutgoingMessage *msg);

void buddy_icon_cb(OkCupidAccount *oca, gchar *data, gsize data_len,
		gpointer user_data)
{
	gchar *buddyname;
	PurpleBuddy *buddy;
	gpointer buddy_icon_data;

	buddyname = user_data;

	purple_debug_info("okcupid",
			"buddy icon for buddy %s %" G_GSIZE_FORMAT "\n",
			buddyname, data_len);

	buddy = purple_find_buddy(oca->account, buddyname);
	g_free(buddyname);
	if (buddy == NULL)
		return;

	buddy_icon_data = g_memdup(data, data_len);

	purple_buddy_icons_set_for_user(oca->account, buddy->name,
			buddy_icon_data, data_len, NULL);
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
	/* { "server_gmt" : 1234171728, "server_seqid" : 28322813, "people" : [ { "screenname" : "Saturn2888", "location" : "Overland Park, Kansas, United States", "distance" : 8118, "distance_units" : "miles", "thumb" : "134x16/333x215/2/13750662203864942041.jpeg", "match" : 86, "friend" : 66, "enemy" : 6, "age" : 22, "gender" : "M", "orientation" : "S", "open_connection" : "1", "im_ok" : "1" } ], "events" : [ { "type" : "im", "from" : "Saturn2888", "server_gmt" : 1232701652, "server_seqid" : 9791021, "contents" : "test test" } , { "type" : "im", "to" : "Saturn2888", "server_gmt" : 1232701674, "server_seqid" : 9791217, "contents" : "chatty chatty" } , { "type" : "im", "from" : "Saturn2888", "server_gmt" : 1232701680, "server_seqid" : 9791280, "contents" : "nope" } , { "type" : "im", "to" : "Saturn2888", "server_gmt" : 1234171728, "server_seqid" : 28322813, "contents" : "hi?" } ] } */
	/*  var response =  {"im_off" : 0, "events" : [{"server_gmt" : 1234171728, "server_seqid" : 28322813, "from" : "eionrobb", "contents" : "hi?", "type" : "im"}, {"server_gmt" : 1234172578, "server_seqid" : 28328267, "to" : "eionrobb", "contents" : "worked", "type" : "im"}, {"server_gmt" : 1242216197, "server_seqid" : 23732214, "from" : "eionrobb", "contents" : "test", "type" : "im"}], "num_unread" : 0, "server_gmt" : 1242216215, "people" : [{"thumb" : "0x0/0x0/2/15281378330166548237.jpeg", "open_connection" : 1, "gender" : "M", "age" : 25, "match" : 85, "screenname" : "eionrobb", "location" : "Christchurch, New Zealand", "im_ok" : 1, "orientation" : "S", "friend" : 66, "enemy" : 7, "distance" : 8118}], "server_seqid" : 23732214}
	                    parent.InstantEvents.openConnection_cb(response, ""); */
	/* {"im_off" : 0, "events" : [{"server_gmt" : 1242541572, "server_seqid" : 30129765, "from" : "Saturn2888", "contents" : "I'm getting double messages", "type" : "im"}], "server_seqid" : 30129765, "people" : [{"thumb" : "134x16/333x215/2/13750662203864942041.jpeg", "open_connection" : 1, "gender" : "M", "age" : 22, "match" : 85, "screenname" : "Saturn2888", "location" : "Overland Park, Kansas", "im_ok" : 1, "orientation" : "S", "friend" : 66, "enemy" : 7, "distance" : 8118}], "online_buddies" : [{"im_ok" : 1, "screenname" : "Saturn2888", "is_online" : 1, "userid" : 16335530578074790482}], "server_gmt" : 1242541572, "num_unread" : 2} */
	
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
	JsonArray *online_buddies = NULL;
	int unread_message_count = 0;
	
	if(json_object_has_member(objnode, "events"))
		events = json_node_get_array(json_object_get_member(objnode, "events"));
	if(json_object_has_member(objnode, "people"))
		people = json_node_get_array(json_object_get_member(objnode, "people"));
	if(json_object_has_member(objnode, "num_unread"))
		unread_message_count = json_node_get_int(json_object_get_member(objnode, "num_unread"));
	if(json_object_has_member(objnode, "online_buddies"))
		online_buddies = json_node_get_array(json_object_get_member(objnode, "online_buddies"));
	
	//Look through online_buddies first to add them to our buddy list (if necessary)
	if (online_buddies != NULL)
	{
		GList *online_buddies_list = json_array_get_elements(online_buddies);
		GList *current;
		for (current = online_buddies_list; current; current = g_list_next(current))
		{
			JsonNode *currentNode = current->data;
			JsonObject *buddy = json_node_get_object(currentNode);
			
			//"online_buddies" : [{"im_ok" : 1, "screenname" : "Saturn2888", "is_online" : 1, "userid" : 16335530578074790482}]
			const gchar *buddy_name = json_node_get_string(json_object_get_member(buddy, "screenname"));
			gint is_online = json_node_get_int(json_object_get_member(buddy, "is_online"));
			PurpleBuddy *pbuddy = purple_find_buddy(oca->account, buddy_name);	
			
			if (!pbuddy)
			{
				pbuddy = purple_buddy_new(oca->account, buddy_name, NULL);
				purple_blist_add_buddy(pbuddy, NULL, NULL, NULL);
			}
			
			if (is_online && !PURPLE_BUDDY_IS_ONLINE(pbuddy))
			{
				purple_prpl_got_user_status(oca->account, buddy_name, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);
			} else if (!is_online && PURPLE_BUDDY_IS_ONLINE(pbuddy))
			{
				purple_prpl_got_user_status(oca->account, buddy_name, purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE), NULL);
			}
		}
		g_list_free(online_buddies_list);
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
				gchar *message = json_node_dup_string(json_object_get_member(event, "contents"));
				
				//sometimes the message can be embedded within a JSON object :(
				JsonParser *message_parser = json_parser_new();
				if(json_parser_load_from_data(message_parser, message, -1, NULL))
				{
					//yep, its JSON :( -- this is ripe for injections :(
					//{ "text" : "how good are you at bowling?  " , "topic" : false }
					JsonNode *message_root = json_parser_get_root(message_parser);
					JsonObject *message_object = json_node_get_object(message_root);
					if (json_object_has_member(message_object, "text"))
					{
						g_free(message);
						message = json_node_dup_string(json_object_get_member(message_object, "text"));
					}
					g_object_unref(message_parser);
				}
				
				gchar *message_stripped = purple_markup_strip_html(message);
				g_free(message);
				gchar *message_html = okc_strdup_withhtml(message_stripped);
				
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
				if (who && (flags != PURPLE_MESSAGE_SEND || !g_hash_table_remove(oca->sent_messages_hash, message_stripped)))
					serv_got_im (pc, who, message_html, flags, time(NULL));
				g_free(message_html);
				g_free(message_stripped);
			} else if (g_str_equal(event_type, "orbit_user_signoff"))
			{
				//buddy signed off
				const gchar *buddy_name = json_node_get_string(json_object_get_member(event, "from"));
				PurpleBuddy *pbuddy = purple_find_buddy(oca->account, buddy_name);
				
				if (pbuddy && PURPLE_BUDDY_IS_ONLINE(pbuddy))
				{
					purple_prpl_got_user_status(oca->account, buddy_name, purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE), NULL);
				}
			}
		}
		g_list_free(event_list);
	}
	
	if (people != NULL)
	{
		GList *people_list = json_array_get_elements(people);
		GList *current;
		for (current = people_list; current; current = g_list_next(current))
		{
			JsonNode *currentNode = current->data;
			JsonObject *person = json_node_get_object(currentNode);
			
			const gchar *buddy_name = json_node_get_string(json_object_get_member(person, "screenname"));
			const gchar *buddy_icon = json_node_get_string(json_object_get_member(person, "thumb"));
			
			gchar *tmp = g_strdup_printf("buddy_icon_%s_cache", buddy_name);
			if (!g_str_equal(purple_account_get_string(oca->account, tmp, ""), buddy_icon))
			{
				/* Save the buddy icon so that they don't all need to be reloaded at startup */
				purple_account_set_string(oca->account, tmp, buddy_icon);
				gchar *buddy_icon_url = g_strdup_printf("/php/load_okc_image.php/images/%s", buddy_icon);
				okc_post_or_get(oca, OKC_METHOD_GET, "cdn.okcimg.com", buddy_icon_url, NULL, buddy_icon_cb, g_strdup(buddy_name), FALSE);
				g_free(buddy_icon_url);
			}
			g_free(tmp);
		}
		g_list_free(people_list);
	}
	
	if (unread_message_count != oca->last_message_count)
	{
		oca->last_message_count = unread_message_count;
		gchar *url = g_strdup("http://www.okcupid.com/mailbox");
		purple_notify_emails(pc, unread_message_count, FALSE, NULL, NULL, &(oca->account->username), &(url), NULL, NULL);
		g_free(url);
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

	fetch_url = g_strdup_printf("/instantevents?refresh_toolbar=1&show_online=1&num_unread=1&im_status=1");

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

	fetch_url = g_strdup_printf("/instantevents?rand=0.%u&server_seqid=%u&server_gmt=%u&show_online=1&num_unread=1&im_status=1", g_random_int(), oca->server_seqid, oca->server_gmt);

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
	
	gint message_sent = json_node_get_int(json_object_get_member(response, "message_sent"));
	
	if (message_sent)
	{
		//Save the message we sent
		g_hash_table_insert(oca->sent_messages_hash, g_strdup(msg->message), NULL);
		
		okc_msg_destroy(msg);
		g_object_unref(parser);
		return;
	}
	
	const gchar *reason = json_node_get_string(json_object_get_member(response, "reason"));
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
	postdata = g_strdup_printf("send=1&attempt=%d&rid=%d&recipient=%s&topic=false&body=%s&rand=0.%d",
			msg->retry_count + 1,
			msg->rid,
			encoded_recipient,
			encoded_message,
			g_random_int());
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

	return strlen(message);
}
