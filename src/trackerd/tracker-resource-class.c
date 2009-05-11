/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-data-update.h>

#include "tracker-dbus.h"
#include "tracker-events.h"
#include "tracker-resource-class.h"
#include "tracker-marshal.h"

#define TRACKER_RESOURCE_CLASS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_RESOURCE_CLASS, TrackerResourceClassPrivate))

typedef struct {
	gchar *rdf_class;
	GPtrArray *adds, *ups, *dels;
} TrackerResourceClassPrivate;

enum {
	SUBJECTS_ADDED,
	SUBJECTS_REMOVED,
	SUBJECTS_CHANGED,
	LAST_SIGNAL
};

static void tracker_resource_class_finalize (GObject *object);

G_DEFINE_TYPE(TrackerResourceClass, tracker_resource_class, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = {0};

static void
tracker_resource_class_class_init (TrackerResourceClassClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_resource_class_finalize;

	signals[SUBJECTS_ADDED] =
		g_signal_new ("subjects-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRV);

	signals[SUBJECTS_REMOVED] =
		g_signal_new ("subjects-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRV);

	signals[SUBJECTS_CHANGED] =
		g_signal_new ("subjects-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRV);

	g_type_class_add_private (object_class, sizeof (TrackerResourceClassPrivate));
}




static void
tracker_resource_class_init (TrackerResourceClass *object)
{
}

static void
emit_strings (TrackerResourceClass *object, gint signal_, GPtrArray *array)
{
	GStrv strings_to_emit;
	guint i;

	if (array->len > 0) {
		strings_to_emit = (GStrv) g_malloc0  (sizeof (gchar *) * (array->len + 1));

		for (i = 0; i < array->len; i++) {
			strings_to_emit[i] = array->pdata [i];
		}

		g_signal_emit (object, signal_, 0, strings_to_emit);

		/* Normal free, not a GStrv free, we free the strings later */
		g_free (strings_to_emit);
	}
}

static void
free_array (GPtrArray *array)
{
	guint i;
	for (i = 0; i < array->len; i++) {
		g_free (array->pdata [i]);
	}
	g_ptr_array_free (array, TRUE);
}

void
tracker_resource_class_emit_events (TrackerResourceClass  *object)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	if (priv->adds) {
		emit_strings (object, signals[SUBJECTS_ADDED], priv->adds);
		free_array (priv->adds);
		priv->adds = NULL;
	}

	if (priv->ups) {
		emit_strings (object, signals[SUBJECTS_CHANGED], priv->ups);
		free_array (priv->ups);
		priv->ups = NULL;
	}

	if (priv->dels) {
		emit_strings (object, signals[SUBJECTS_REMOVED], priv->dels);
		free_array (priv->dels);
		priv->dels = NULL;
	}
}


static void
tracker_resource_class_finalize (GObject *object)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	/* Emit pending events */
	tracker_resource_class_emit_events ((TrackerResourceClass *) object);

	g_free (priv->rdf_class);

	G_OBJECT_CLASS (tracker_resource_class_parent_class)->finalize (object);
}

TrackerResourceClass *
tracker_resource_class_new (const gchar *rdf_class)
{
	TrackerResourceClass	     *object;
	TrackerResourceClassPrivate *priv;

	object = g_object_new (TRACKER_TYPE_RESOURCE_CLASS, NULL);

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	priv->rdf_class = g_strdup (rdf_class);

	return object;
}


const gchar *
tracker_resource_class_get_rdf_class (TrackerResourceClass  *object)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	return priv->rdf_class;
}

void 
tracker_resource_class_add_event (TrackerResourceClass  *object,
				  const gchar           *uri,
				  TrackerDBusEventsType type)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	switch (type) {
		case TRACKER_DBUS_EVENTS_TYPE_ADD:
		if (!priv->adds)
			priv->adds = g_ptr_array_new ();
		g_ptr_array_add (priv->adds, g_strdup (uri));
		break;
		case TRACKER_DBUS_EVENTS_TYPE_UPDATE:
		if (!priv->ups)
			priv->ups = g_ptr_array_new ();
		g_ptr_array_add (priv->ups, g_strdup (uri));
		break;
		case TRACKER_DBUS_EVENTS_TYPE_DELETE:
		if (!priv->dels)
			priv->dels = g_ptr_array_new ();
		g_ptr_array_add (priv->dels, g_strdup (uri));
		break;
		default:
		break;
	}
}
