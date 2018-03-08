/*
 * This file is part of GPaste.
 *
 * Copyright (c) 2010-2018, Marc-Antoine Perennou <Marc-Antoine@Perennou.com>
 */

#if !defined (__G_PASTE_H_INSIDE__) && !defined (G_PASTE_COMPILATION)
#error "Only <gpaste.h> can be included directly."
#endif

#ifndef __G_PASTE_KEYBINDING_H__
#define __G_PASTE_KEYBINDING_H__

#include <gpaste-settings.h>

G_BEGIN_DECLS

#define G_PASTE_TYPE_KEYBINDING (g_paste_keybinding_get_type ())

G_PASTE_DERIVABLE_TYPE (Keybinding, keybinding, KEYBINDING, GObject)

struct _GPasteKeybindingClass
{
    GObjectClass parent_class;
};

typedef const gchar *(*GPasteKeybindingGetter) (const GPasteSettings *settings);
typedef void         (*GPasteKeybindingFunc)   (GPasteKeybinding *self,
                                                gpointer          data);

const guint32  *g_paste_keybinding_get_keycodes    (const GPasteKeybinding *self);
GdkModifierType g_paste_keybinding_get_modifiers   (const GPasteKeybinding *self);
const gchar    *g_paste_keybinding_get_dconf_key   (const GPasteKeybinding *self);
const gchar    *g_paste_keybinding_get_accelerator (const GPasteKeybinding *self,
                                                    const GPasteSettings   *settings);

void            g_paste_keybinding_activate      (GPasteKeybinding *self,
                                                  GPasteSettings   *settings);
void            g_paste_keybinding_deactivate    (GPasteKeybinding *self);
gboolean        g_paste_keybinding_is_active     (GPasteKeybinding *self);
void            g_paste_keybinding_perform       (GPasteKeybinding *self);
void            g_paste_keybinding_notify        (GPasteKeybinding *self,
                                                  GdkModifierType   modifiers,
                                                  guint64           keycode);

GPasteKeybinding *g_paste_keybinding_new (GType                  type,
                                          const gchar           *dconf_key,
                                          GPasteKeybindingGetter getter,
                                          GPasteKeybindingFunc   callback,
                                          gpointer               user_data);

G_END_DECLS

#endif /*__G_PASTE_KEYBINDING_H__*/
