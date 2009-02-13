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
};

static gboolean okc_send_im_fom(OkCupidOutgoingMessage *msg);
static gboolean okc_get_new_messages(OkCupidAccount *oca);

static void got_new_messages(OkCupidAccount *oca, gchar *data,
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

static gboolean okc_get_new_messages(OkCupidAccount *oca)
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

static void okc_send_im_cb(FacebookAccount *fba, gchar *data, gsize data_len, gpointer user_data)
{
	OkCupidOutgoingMessage *msg = user_data;

	/* NULL data crashes on Windows */
	if (data == NULL)
		data = "(null)";
	
	purple_debug_misc("okcupid", "sent im response: %s\n", data);
}

static gboolean fb_send_im_fom(OkCupidOutgoingMessage *msg)
{
	gchar *encoded_message;
	gchar *encoded_recipient;
	gchar *postdata;

	encoded_message = g_strdup(purple_url_encode(msg->message));
	encoded_recipient = g_strdup(purple_url_encode(msg->who));
	postdate = g_strdup_printf("send=1&attempt=%d&rid=%d&recipient=%s&body=%s&rand=%18f",
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
	FacebookOutgoingMessage *msg;

	msg = g_new0(FacebookOutgoingMessage, 1);
	msg->fba = pc->proto_data;

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

static void got_form_id_page(FacebookAccount *fba, gchar *data, gsize data_len, gpointer userdata)
{
	const gchar *start_text = "id=\"post_form_id\" name=\"post_form_id\" value=\"";
	gchar *post_form_id;
	gchar *channel_number;
	gchar *tmp = NULL;
	
	/* NULL data crashes on Windows */
	if (data == NULL)
		data = "(null)";
	
	tmp = g_strstr_len(data, data_len, start_text);
	if (tmp == NULL)
	{
		purple_debug_error("facebook", "couldn't find post_form_id\n");
		purple_debug_info("facebook", "page content: %s\n", data);
		/* Maybe they changed their HTML slightly? */
		purple_connection_error_reason(fba->pc,
				PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
				_("Error getting info from Facebook."));
		return;
	}
	tmp += strlen(start_text);
	post_form_id = g_strndup(tmp, strchr(tmp, '"') - tmp);

	g_free(fba->post_form_id);
	fba->post_form_id = post_form_id;

	/* dodgy as search for channel server number */
	if (!fba->channel_number)
	{
		start_text = "\", \"channel";
		tmp = g_strstr_len(data, data_len, start_text);
		if (tmp == NULL)
		{
			/* Some proxies strip whitepsace */
			start_text = "\",\"channel";
			tmp = g_strstr_len(data, data_len, start_text);
			if (tmp == NULL)
			{
				/* TODO: Is it better to pick a random channel number or to disconnect? */
				/* MARKCONFLICT (r283,r286) */
				channel_number = g_strdup(purple_account_get_string(fba->account, "last_channel_number", ""));
				if (channel_number[0] == '\0')
				{
					purple_debug_error("facebook", "couldn't find channel\n");
					purple_debug_misc("facebook", "page content: %s\n", data);
					purple_connection_error_reason(fba->pc,
							PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
							_("Chat service currently unavailable."));
					return;
				}
			}
		}

		if (tmp != NULL)
		{
			tmp += strlen(start_text);
			channel_number = g_strndup(tmp, strchr(tmp, '"') - tmp);
		}

		purple_account_set_string(fba->account, "last_channel_number", channel_number);

		g_free(fba->channel_number);
		fba->channel_number = channel_number;
	}

	tmp = g_strdup_printf("visibility=true&post_form_id=%s", post_form_id);
	fb_post_or_get(fba, FB_METHOD_POST, "apps.facebook.com", "/ajax/chat/settings.php", tmp, NULL, NULL, FALSE);
	g_free(tmp);

	/*
	 * Now that we have a channel number we can start looping and
	 * waiting for messages
	 */
	fb_get_new_messages(fba);
}

gboolean fb_get_post_form_id(FacebookAccount *fba)
{
	fb_post_or_get(fba, FB_METHOD_GET, NULL, "/home.php", NULL, got_form_id_page, NULL, FALSE);
	return FALSE;
}
