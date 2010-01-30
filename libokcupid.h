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

#ifndef LIBOKCUPID_H
#define LIBOKCUPID_H

#define OKCUPID_PLUGIN_VERSION "1.0"

#include <glib.h>

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <unistd.h>

#include <json-glib/json-glib.h>

#ifndef G_GNUC_NULL_TERMINATED
#	if __GNUC__ >= 4
#		define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#	else
#		define G_GNUC_NULL_TERMINATED
#	endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef _WIN32
#	include "win32dep.h"
#	define dlopen(a,b) LoadLibrary(a)
#	define RTLD_LAZY
#	define dlsym(a,b) GetProcAddress(a,b)
#	define dlclose(a) FreeLibrary(a)
#else
#	include <arpa/inet.h>
#	include <dlfcn.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "accountopt.h"
#include "connection.h"
#include "debug.h"
#include "dnsquery.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "sslconn.h"
#include "version.h"

typedef struct _OkCupidAccount OkCupidAccount;
typedef struct _OkCupidBuddy OkCupidBuddy;

typedef void (*OkCupidProxyCallbackFunc)(OkCupidAccount *oca, gchar *data, gsize data_len, gpointer user_data);

struct _OkCupidAccount {
	PurpleAccount *account;
	PurpleConnection *pc;
	GHashTable *hostname_ip_cache;
	GSList *conns; /**< A list of all active FacebookConnections */
	GSList *dns_queries;
	GHashTable *cookie_table;
	time_t last_messages_download_time;
	guint new_messages_check_timer;
	guint perpetual_messages_timer;
	guint buddy_presence_timer;
	guint server_seqid;
	guint server_gmt;
	guint last_message_count;
};

struct _OkCupidBuddy {
	OkCupidAccount *oca;
	PurpleBuddy *buddy;
	gchar *thumb_url;
};

gchar *okc_strdup_withhtml(const gchar *src);
gchar *okc_convert_unicode(const gchar *input);

#endif /* LIBOKCUPID_H */
