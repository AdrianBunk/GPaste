/*
 *      This file is part of GPaste.
 *
 *      Copyright 2011-2015 Marc-Antoine Perennou <Marc-Antoine@Perennou.com>
 *
 *      GPaste is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      GPaste is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with GPaste.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gpaste-clipboards-manager.h>
#include <gpaste-image-item.h>
#include <gpaste-uris-item.h>

struct _GPasteClipboardsManager
{
    GObject parent_instance;
};

typedef struct
{
    GPasteClipboard *clipboard;
    gulong           signal;
} _Clipboard;

typedef struct
{
    GSList         *clipboards;
    GPasteHistory  *history;
    GPasteSettings *settings;

    gulong          selected_signal;
} GPasteClipboardsManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GPasteClipboardsManager, g_paste_clipboards_manager, G_TYPE_OBJECT)

/**
 * g_paste_clipboards_manager_add_clipboard:
 * @self: a #GPasteClipboardsManager instance
 * @clipboard: (transfer none): the GPasteClipboard to add
 *
 * Add a #GPasteClipboard to the #GPasteClipboardsManager
 *
 * Returns:
 */
G_PASTE_VISIBLE void
g_paste_clipboards_manager_add_clipboard (GPasteClipboardsManager *self,
                                          GPasteClipboard         *clipboard)
{
    g_return_if_fail (G_PASTE_IS_CLIPBOARDS_MANAGER (self));
    g_return_if_fail (G_PASTE_IS_CLIPBOARD (clipboard));

    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (self);
    _Clipboard *clip = g_new0 (_Clipboard, 1);

    clip->clipboard = g_object_ref (clipboard);

    priv->clipboards = g_slist_prepend (priv->clipboards, clip);
    g_paste_clipboard_bootstrap (clipboard, priv->history);
}

static void
g_paste_clipboards_manager_sync_ready (GtkClipboard *clipboard G_GNUC_UNUSED,
                                       const gchar  *text,
                                       gpointer user_data)
{
    if (text)
        gtk_clipboard_set_text (user_data, text, -1);
}

/**
 * g_paste_clipboards_manager_sync_from_to:
 * @self: a #GPasteClipboardsManager instance
 * @from: the Source clipboard type
 * @to: the destination clipboard type
 *
 * Sync a clipboard into another
 *
 * Returns:
 */
G_PASTE_VISIBLE void
g_paste_clipboards_manager_sync_from_to (GPasteClipboardsManager *self,
                                         GdkAtom                  from,
                                         GdkAtom                  to)
{
    g_return_if_fail (G_PASTE_IS_CLIPBOARDS_MANAGER (self));

    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (self);
    GtkClipboard *_from = NULL, *_to = NULL;
    g_autofree gchar *_from_name = gdk_atom_name (from);
    g_autofree gchar *_to_name = gdk_atom_name (to);

    g_debug ("clipboards-manager: sync from %s to %s", _from_name, _to_name);

    for (GSList *clipboard = priv->clipboards; clipboard; clipboard = g_slist_next (clipboard))
    {
        _Clipboard *_clip = clipboard->data;
        GPasteClipboard *clip = _clip->clipboard;
        GdkAtom cur = g_paste_clipboard_get_target (clip);

        if (cur == from)
            _from = g_paste_clipboard_get_real (clip);
        else if (cur == to)
            _to = g_paste_clipboard_get_real (clip);
    }

    if (_from && _to)
    {
        gtk_clipboard_request_text (_from,
                                    g_paste_clipboards_manager_sync_ready,
                                    _to);
    }
}

static void
g_paste_clipboards_manager_notify_finish (GPasteClipboardsManagerPrivate *priv,
                                          GPasteClipboard                *clipboard,
                                          GPasteItem                     *item,
                                          const gchar                    *synchronized_text,
                                          gboolean                        something_in_clipboard)
{
    GPasteHistory *history = priv->history;

    g_debug ("clipboards-manager: notify finish");

    if (item)
        g_paste_history_add (history, item);

    if (!something_in_clipboard)
        g_paste_clipboard_ensure_not_empty (clipboard, history);

    if (synchronized_text)
    {
        for (GSList *_clipboard = priv->clipboards; _clipboard; _clipboard = g_slist_next (_clipboard))
        {
            _Clipboard *_clip = _clipboard->data;
            GPasteClipboard *clip = _clip->clipboard;

            if (clipboard == clip)
                continue;

            const gchar *text = g_paste_clipboard_get_text (clip);

            if (!text || g_strcmp0 (text, synchronized_text))
                g_paste_clipboard_select_text (clip, synchronized_text);
        }
    }
}

typedef struct {
    GPasteClipboardsManagerPrivate *priv;
    GPasteClipboard                *clip;
    gboolean                        track;
    gboolean                        uris_available;
} GPasteClipboardsManagerCallbackData;

static void
g_paste_clipboards_manager_text_ready (GPasteClipboard *clipboard,
                                       const gchar     *text,
                                       gpointer         user_data)
{
    g_autofree GPasteClipboardsManagerCallbackData *data = user_data;
    GPasteClipboardsManagerPrivate *priv = data->priv;
    GPasteItem *item = NULL;
    const gchar *synchronized_text = NULL;

    g_debug ("clipboards-manager: text ready");

    /* Did we already have some contents, or did we get some now? */
    gboolean something_in_clipboard = !!g_paste_clipboard_get_text (clipboard);

    /* If our contents got updated */
    if (text)
    {
        if (data->track)
        {
            if (data->uris_available)
                item = G_PASTE_ITEM (g_paste_uris_item_new (text));
            else
                item = G_PASTE_ITEM (g_paste_text_item_new (text));
        }

        if (g_paste_settings_get_synchronize_clipboards (priv->settings))
            synchronized_text = text;
    }

    g_paste_clipboards_manager_notify_finish (priv, clipboard, item, synchronized_text, something_in_clipboard);
}

static void
g_paste_clipboards_manager_image_ready (GPasteClipboard *clipboard,
                                        GdkPixbuf       *image,
                                        gpointer         user_data)
{
    g_autofree GPasteClipboardsManagerCallbackData *data = user_data;
    GPasteClipboardsManagerPrivate *priv = data->priv;
    GPasteItem *item = NULL;

    g_debug ("clipboards-manager: image ready");

    /* Did we already have some contents, or did we get some now? */
    gboolean something_in_clipboard = !!g_paste_clipboard_get_image_checksum (clipboard);

    /* If our contents got updated */
    if (image && data->track)
        item = G_PASTE_ITEM (g_paste_image_item_new (image));

    g_paste_clipboards_manager_notify_finish (priv, clipboard, item, NULL, something_in_clipboard);
}

static void
g_paste_clipboards_manager_targets_ready (GtkClipboard     *clipboard G_GNUC_UNUSED,
                                          GtkSelectionData *targets,
                                          gpointer          user_data)
{
    g_autofree GPasteClipboardsManagerCallbackData *data = user_data;

    g_debug ("clipboards-manager: targets ready");

    if (gtk_selection_data_get_length (targets) >= 0)
    {
        data->uris_available = gtk_selection_data_targets_include_uri (targets);

        if (data->uris_available || gtk_selection_data_targets_include_text (targets))
        {
            /* Update our cache from the real Clipboard */
            g_paste_clipboard_set_text (data->clip,
                                        g_paste_clipboards_manager_text_ready,
                                        data);
            data = NULL;
        }
        else if (g_paste_settings_get_images_support (data->priv->settings) && gtk_selection_data_targets_include_image (targets, FALSE))
        {
            /* Update our cache from the real Clipboard */
            g_paste_clipboard_set_image (data->clip,
                                         g_paste_clipboards_manager_image_ready,
                                         data);
            data = NULL;
        }
    }
    else
    {
        g_debug ("clipboards-manager: no target ready");

        g_paste_clipboard_clear (data->clip);
        g_paste_clipboard_ensure_not_empty (data->clip, data->priv->history);
    }
}

static void
g_paste_clipboards_manager_notify (GPasteClipboard *clipboard,
                                   GdkEvent        *event G_GNUC_UNUSED,
                                   gpointer         user_data)
{
    GPasteClipboardsManagerPrivate *priv = user_data;

    g_debug ("clipboards-manager: notify");

    GPasteSettings *settings = priv->settings;
    GdkAtom atom = g_paste_clipboard_get_target (clipboard);
    gboolean track = ((atom != GDK_SELECTION_PRIMARY || g_paste_settings_get_primary_to_history (settings)) &&
                      g_paste_settings_get_track_changes (settings));

    GPasteClipboardsManagerCallbackData *data = g_new (GPasteClipboardsManagerCallbackData, 1);

    data->priv = priv;
    data->clip = clipboard;
    data->track = track;

    gtk_clipboard_request_contents (g_paste_clipboard_get_real (clipboard),
                                    gdk_atom_intern_static_string ("TARGETS"),
                                    g_paste_clipboards_manager_targets_ready,
                                    data);
}

/**
 * g_paste_clipboards_manager_activate:
 * @self: a #GPasteClipboardsManager instance
 *
 * Activate the #GPasteClipboardsManager
 *
 * Returns:
 */
G_PASTE_VISIBLE void
g_paste_clipboards_manager_activate (GPasteClipboardsManager *self)
{
    g_return_if_fail (G_PASTE_IS_CLIPBOARDS_MANAGER (self));

    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (self);

    for (GSList *clipboard = priv->clipboards; clipboard; clipboard = g_slist_next (clipboard))
    {
        _Clipboard *clip = clipboard->data;

        clip->signal = g_signal_connect (clip->clipboard,
                                         "owner-change",
                                         G_CALLBACK (g_paste_clipboards_manager_notify),
                                         priv);
    }
}

/**
 * g_paste_clipboards_manager_select:
 * @self: a #GPasteClipboardsManager instance
 * @item: the #GPasteItem to select
 *
 * Select a new #GPasteItem
 *
 * Returns: %FALSE if the item was invalid, %TRUE otherwise
 */
G_PASTE_VISIBLE gboolean
g_paste_clipboards_manager_select (GPasteClipboardsManager *self,
                                   const GPasteItem        *item)
{
    g_return_val_if_fail (G_PASTE_IS_CLIPBOARDS_MANAGER (self), FALSE);
    g_return_val_if_fail (G_PASTE_IS_ITEM (item), FALSE);

    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (self);

    g_debug ("clipboards-manager: select");

    for (GSList *clipboard = priv->clipboards; clipboard; clipboard = g_slist_next (clipboard))
    {
        _Clipboard *clip = clipboard->data;

        if (!g_paste_clipboard_select_item (clip->clipboard, item))
        {
            g_debug ("clipboards-manager: item was invalid, deleting it");
            return FALSE;
        }
    }

    return TRUE;
}

static void
on_item_selected (GPasteClipboardsManager *self,
                  GPasteItem              *item,
                  GPasteHistory           *history G_GNUC_UNUSED)
{
    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (G_PASTE_CLIPBOARDS_MANAGER (self));

    if (!g_paste_clipboards_manager_select (self, item))
        g_paste_history_remove (priv->history, 0);
}

static void
_clipboard_free (gpointer data)
{
    _Clipboard *clip = data;

    g_signal_handler_disconnect (clip->clipboard, clip->signal);
    g_object_unref (clip->clipboard);
    g_free (clip);
}

static void
g_paste_clipboards_manager_dispose (GObject *object)
{
    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (G_PASTE_CLIPBOARDS_MANAGER (object));
    GPasteSettings *settings = priv->settings;

    if (settings)
    {
        g_signal_handler_disconnect (settings, priv->selected_signal);
        g_clear_object (&priv->settings);
        g_clear_object (&priv->history);
    }

    if (priv->clipboards)
    {
        g_slist_free_full (priv->clipboards, _clipboard_free);
        priv->clipboards = NULL;
    }

    G_OBJECT_CLASS (g_paste_clipboards_manager_parent_class)->dispose (object);
}

static void
g_paste_clipboards_manager_class_init (GPasteClipboardsManagerClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = g_paste_clipboards_manager_dispose;
}

static void
g_paste_clipboards_manager_init (GPasteClipboardsManager *self)
{
    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (self);

    priv->clipboards = NULL;
}

/**
 * g_paste_clipboards_manager_new:
 * @history: (transfer none): a #GPasteHistory instance
 * @settings: (transfer none): a #GPasteSettings instance
 *
 * Create a new instance of #GPasteClipboardsManager
 *
 * Returns: a newly allocated #GPasteClipboardsManager
 *          free it with g_object_unref
 */
G_PASTE_VISIBLE GPasteClipboardsManager *
g_paste_clipboards_manager_new (GPasteHistory  *history,
                                GPasteSettings *settings)
{
    g_return_val_if_fail (G_PASTE_IS_HISTORY (history), NULL);
    g_return_val_if_fail (G_PASTE_IS_SETTINGS (settings), NULL);

    GPasteClipboardsManager *self = g_object_new (G_PASTE_TYPE_CLIPBOARDS_MANAGER, NULL);
    GPasteClipboardsManagerPrivate *priv = g_paste_clipboards_manager_get_instance_private (self);

    priv->history = g_object_ref (history);
    priv->settings = g_object_ref (settings);

    priv->selected_signal = g_signal_connect_swapped (history,
                                                      "selected",
                                                      G_CALLBACK (on_item_selected),
                                                      self);

    return self;
}
