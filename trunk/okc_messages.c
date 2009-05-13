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

void got_new_messages(OkCupidAccount *oca, gchar *data,
		gsize data_len, gpointer userdata)
{
	PurpleConnection *pc = userdata;
	PurpleBuddy *buddy;

	/* NULL data will crash on Windows */
	if (data == NULL)
		data = "(null)";

	purple_debug_misc("okcupid", "got new messages: %s\n", data);

	/* Process incomming messages here */
	/* { "server_gmt" : 1234171728, "server_seqid" : 28322813, "people" : [ { "screenname" : "Saturn2888", "location" : "Overland Park, Kansas, United States", "distance" : 8118, "distance_units" : "miles", "thumb" : "134x16/333x215/2/13750662203864942041.jpeg", "match" : 86, "friend" : 66, "enemy" : 6, "age" : 22, "gender" : "M", "orientation" : "S", "open_connection" : "1", "im_ok" : "1" } ], "events" : [ { "type" : "im", "from" : "Saturn2888", "server_gmt" : 1232701652, "server_seqid" : 9791021, "contents" : "test test" } , { "type" : "im", "to" : "Saturn2888", "server_gmt" : 1232701674, "server_seqid" : 9791217, "contents" : "chatty chatty" } , { "type" : "im", "from" : "Saturn2888", "server_gmt" : 1232701680, "server_seqid" : 9791280, "contents" : "nope" } , { "type" : "im", "to" : "Saturn2888", "server_gmt" : 1234171728, "server_seqid" : 28322813, "contents" : "hi?" } ] } */

	/* Continue looping, waiting for more messages */
	okc_get_new_messages(oca);
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

	purple_debug_info("facebook", "getting new messages\n");

	fetch_url = g_strdup_printf("/instantevents?rand=%18f", g_random_double());

	okc_post_or_get(oca, OKC_METHOD_GET, NULL, fetch_url, NULL, got_new_messages, oca->pc, TRUE);
	oca->last_messages_download_time = now;

	g_free(fetch_url);

	return FALSE;
}

void okc_send_im_cb(OkCupidAccount *oca, gchar *data, gsize data_len, gpointer user_data)
{
	OkCupidOutgoingMessage *msg = user_data;

	/* NULL data crashes on Windows */
	if (data == NULL)
		data = "(null)";
	
	purple_debug_misc("okcupid", "sent im response: %s\n", data);
}

gboolean okc_send_im_fom(OkCupidOutgoingMessage *msg)
{
	gchar *encoded_message;
	gchar *encoded_recipient;
	gchar *postdata;

	encoded_message = g_strdup(purple_url_encode(msg->message));
	encoded_recipient = g_strdup(purple_url_encode(msg->who));
	postdata = g_strdup_printf("send=1&attempt=%d&rid=%d&recipient=%s&body=%s&rand=%18f",
			msg->retry_count + 1,
			msg->rid,
			encoded_recipient,
			encoded_message,
			g_random_double());
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
