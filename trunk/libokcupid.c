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
#include "okc_messages.h"
#include "okc_blist.h"

/******************************************************************************/
/* Utility functions */
/******************************************************************************/

gchar *okc_convert_unicode(const gchar *input)
{
	gunichar unicode_char;
	gchar unicode_char_str[6];
	gint unicode_char_len;
	gchar *next_pos;
	gchar *input_string;
	gchar *output_string;

	if (input == NULL)
		return NULL;

	next_pos = input_string = g_strdup(input);

	while ((next_pos = strstr(next_pos, "\\u")))
	{
		/* grab the unicode */
		sscanf(next_pos, "\\u%4x", &unicode_char);
		/* turn it to a char* */
		unicode_char_len = g_unichar_to_utf8(unicode_char, unicode_char_str);
		/* shove it back into the string */
		g_memmove(next_pos, unicode_char_str, unicode_char_len);
		/* move all the data after the \u0000 along */
		g_stpcpy(next_pos + unicode_char_len, next_pos + 6);
	}

	output_string = g_strcompress(input_string);
	g_free(input_string);

	return output_string;
}

/* Like purple_strdup_withhtml, but escapes htmlentities too */
gchar *okc_strdup_withhtml(const gchar *src)
{
	gulong destsize, i, j;
	gchar *dest;

	g_return_val_if_fail(src != NULL, NULL);

	/* New length is (length of src) + (number of \n's * 3) + (number of &'s * 5) +
		(number of <'s * 4) + (number of >'s *4) + (number of "'s * 6) -
		(number of \r's) + 1 */
	destsize = 1;
	for (i = 0; src[i] != '\0'; i++)
	{
		if (src[i] == '\n' || src[i] == '<' || src[i] == '>')
			destsize += 4;
		else if (src[i] == '&')
			destsize += 5;
		else if (src[i] == '"')
			destsize += 6;
		else if (src[i] != '\r')
			destsize++;
	}

	dest = g_malloc(destsize);

	/* Copy stuff, ignoring \r's, because they are dumb */
	for (i = 0, j = 0; src[i] != '\0'; i++) {
		if (src[i] == '\n') {
			strcpy(&dest[j], "<BR>");
			j += 4;
		} else if (src[i] == '<') {
			strcpy(&dest[j], "&lt;");
			j += 4;
		} else if (src[i] == '>') {
			strcpy(&dest[j], "&gt;");
			j += 4;
		} else if (src[i] == '&') {
			strcpy(&dest[j], "&amp;");
			j += 5;
		} else if (src[i] == '"') {
			strcpy(&dest[j], "&quot;");
			j += 6;
		} else if (src[i] != '\r')
			dest[j++] = src[i];
	}

	dest[destsize-1] = '\0';

	return dest;
}

/******************************************************************************/
/* PRPL functions */
/******************************************************************************/

static const char *okc_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "okcupid";
}

static GList *okc_statuses(PurpleAccount *account)
{
	GList *types = NULL;
	PurpleStatusType *status;

	purple_debug_info("okcupid", "statuses\n");

	/* OkCupid people are either online, online with no IM, offline */
	
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	purple_debug_info("okcupid", "statuses return\n");

	return types;
}

static gboolean okc_get_messages_failsafe(OkCupidAccount *oca)
{
	if (oca->last_messages_download_time < (time(NULL) - (60*5))) {
		/* Messages haven't been downloaded in a while-
		 * something is probably wrong */
		purple_debug_warning("okcupid",
				"executing message check failsafe\n");
		okc_get_new_messages(oca);
	}

	return TRUE;
}

static void okc_login_cb(OkCupidAccount *oca, gchar *response, gsize len,
		gpointer userdata)
{
	purple_connection_update_progress(oca->pc, _("Authenticating"), 2, 3);

	/* ok, we're logged in now! */
	purple_connection_set_state(oca->pc, PURPLE_CONNECTED);

	/* This will kick off our long-poll message retrieval loop */
	okc_get_new_messages_now(oca);
	
	okc_get_online_buddies(oca);
	
	oca->perpetual_messages_timer = purple_timeout_add_seconds(15,
			(GSourceFunc)okc_get_messages_failsafe, oca);
	oca->buddy_presence_timer = purple_timeout_add_seconds(60,
			(GSourceFunc)okc_get_online_buddies, oca);
}

static void okc_login(PurpleAccount *account)
{
	OkCupidAccount *oca;
	gchar *postdata, *encoded_username, *encoded_password;

	purple_debug_info("okcupid", "login\n");

	/* Create account and initialize state */
	oca = g_new0(OkCupidAccount, 1);
	oca->account = account;
	oca->pc = purple_account_get_connection(account);
	oca->last_messages_download_time = time(NULL) - 60; /* 60 secs is a safe buffer */
	oca->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	oca->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);

	account->gc->proto_data = oca;

	purple_connection_set_state(oca->pc, PURPLE_CONNECTING);
	purple_connection_update_progress(oca->pc, _("Connecting"), 1, 3);

	encoded_username = g_strdup(purple_url_encode(
			purple_account_get_username(account)));
	encoded_password = g_strdup(purple_url_encode(
			purple_account_get_password(account)));

	postdata = g_strdup_printf("p=&username=%s&password=%s&forever=on&submit=Login",
			encoded_username, encoded_password);
	g_free(encoded_username);
	g_free(encoded_password);

	okc_post_or_get(oca, OKC_METHOD_POST, "www.okcupid.com",
			"/login", postdata, okc_login_cb, NULL, FALSE);
	g_free(postdata);
}

static void okc_close(PurpleConnection *pc)
{
	OkCupidAccount *oca;

	purple_debug_info("okcupid", "disconnecting account\n");

	oca = pc->proto_data;

	okc_post_or_get(oca, OKC_METHOD_POST, NULL, "/logout",
			"ajax=1", NULL, NULL, FALSE);
	
	if (oca->new_messages_check_timer)
		purple_timeout_remove(oca->new_messages_check_timer);
	if (oca->buddy_presence_timer)
		purple_timeout_remove(oca->buddy_presence_timer);
	if (oca->perpetual_messages_timer)
		purple_timeout_remove(oca->perpetual_messages_timer);

	purple_debug_info("okcupid", "destroying %d incomplete connections\n",
			g_slist_length(oca->conns));

	while (oca->conns != NULL)
		okc_connection_destroy(oca->conns->data);

	while (oca->dns_queries != NULL) {
		PurpleDnsQueryData *dns_query = oca->dns_queries->data;
		purple_debug_info("okcupid", "canceling dns query for %s\n",
					purple_dnsquery_get_host(dns_query));
		oca->dns_queries = g_slist_remove(oca->dns_queries, dns_query);
		purple_dnsquery_destroy(dns_query);
	}

	g_hash_table_destroy(oca->cookie_table);
	g_hash_table_destroy(oca->hostname_ip_cache);
	g_free(oca);
}

static void okc_buddy_free(PurpleBuddy *buddy)
{
	OkCupidBuddy *obuddy = buddy->proto_data;
	if (obuddy != NULL)
	{
		buddy->proto_data = NULL;

		g_free(obuddy->name);
		g_free(obuddy->thumb_url);
		g_free(obuddy);
	}
}

/******************************************************************************/
/* Plugin functions */
/******************************************************************************/

static gboolean plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static void plugin_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;
	
	option = purple_account_option_bool_new("Ignore me", "okcupid_fake_setting", TRUE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
}

static PurplePluginProtocolInfo prpl_info = {
	/* options */
	OPT_PROTO_UNIQUE_CHATNAME,

	NULL,                   /* user_splits */
	NULL,                   /* protocol_options */
	NO_BUDDY_ICONS    /* icon_spec */
	/*{"jpg", 0, 0, 50, 50, -1, PURPLE_ICON_SCALE_SEND}*/, /* icon_spec */
	okc_list_icon,          /* list_icon */
	NULL,                   /* list_emblems */
	NULL,                   /* status_text */
	NULL,                   /* tooltip_text */
	okc_statuses,           /* status_types */
	NULL,                   /* blist_node_menu */
	NULL,                   /* chat_info */
	NULL,                   /* chat_info_defaults */
	okc_login,              /* login */
	okc_close,              /* close */
	okc_send_im,            /* send_im */
	NULL,                   /* set_info */
	NULL,                   /* send_typing */
	okc_get_info,           /* get_info */
	NULL,                   /* set_status */
	NULL,                   /* set_idle */
	NULL,                   /* change_passwd */
	okc_add_buddy,          /* add_buddy */
	NULL,                   /* add_buddies */
	okc_remove_buddy,       /* remove_buddy */
	NULL,                   /* remove_buddies */
	NULL,                   /* add_permit */
	okc_block_buddy,        /* add_deny */
	NULL,                   /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,                   /* set_permit_deny */
	NULL,                   /* join_chat */
	NULL,                   /* reject chat invite */
	NULL,                   /* get_chat_name */
	NULL,                   /* chat_invite */
	NULL,                   /* chat_leave */
	NULL,                   /* chat_whisper */
	NULL,                   /* chat_send */
	NULL,                   /* keepalive */
	NULL,                   /* register_user */
	NULL,                   /* get_cb_info */
	NULL,                   /* get_cb_away */
	NULL,                   /* alias_buddy */
	NULL,                   /* group_buddy */
	NULL,                   /* rename_group */
	okc_buddy_free,         /* buddy_free */
	NULL,                   /* convo_closed */
	purple_normalize_nocase,/* normalize */
	NULL,                   /* set_buddy_icon */
	NULL,                   /* remove_group */
	NULL,                   /* get_cb_real_name */
	NULL,                   /* set_chat_topic */
	NULL,                   /* find_blist_chat */
	NULL,                   /* roomlist_get_list */
	NULL,                   /* roomlist_cancel */
	NULL,                   /* roomlist_expand_category */
	NULL,                   /* can_receive_file */
	NULL,                   /* send_file */
	NULL,                   /* new_xfer */
	NULL,                   /* offline_message */
	NULL,                   /* whiteboard_prpl_ops */
	NULL,                   /* send_raw */
	NULL,                   /* roomlist_room_serialize */
	NULL,                   /* unregister_user */
	NULL,                   /* send_attention */
	NULL,                   /* attention_types */
	sizeof(PurplePluginProtocolInfo), /* struct_size */
	NULL,                   /* get_account_text_table */
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	2, /* major_version */
	3, /* minor version */
	PURPLE_PLUGIN_PROTOCOL, /* type */
	NULL, /* ui_requirement */
	0, /* flags */
	NULL, /* dependencies */
	PURPLE_PRIORITY_DEFAULT, /* priority */
	"prpl-bigbrownchunx-okcupid", /* id */
	"OkCupid", /* name */
	OKCUPID_PLUGIN_VERSION, /* version */
	N_("OkCupid Protocol Plugin"), /* summary */
	N_("OkCupid Protocol Plugin"), /* description */
	"Eion Robb <eionrobb@gmail.com>", /* author */
	"", /* homepage */
	plugin_load, /* load */
	plugin_unload, /* unload */
	NULL, /* destroy */
	NULL, /* ui_info */
	&prpl_info, /* extra_info */
	NULL, /* prefs_info */
	NULL, /* actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(okcupid, plugin_init, info);
