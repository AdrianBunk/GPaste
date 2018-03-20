/*
 * This file is part of GPaste.
 *
 * Copyright (c) 2010-2018, Marc-Antoine Perennou <Marc-Antoine@Perennou.com>
 */
/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

imports.gi.versions.St = '1.0';

const Lang = imports.lang;
const Signals = imports.signals;

const St = imports.gi.St;

var GPastePageItem = new Lang.Class({
    Name: 'GPastePageItem',
    Implements: [Signals.WithSignals],

    _init: function(page) {
        this.actor = new St.Button({
            reactive: true,
            can_focus: false,
            track_hover: true,
            style_class: 'calendar-day-base'
        });

        this.actor.child = new St.Label({ text: '' + page });

        this._page = page;

        this.actor.connect('clicked', () => {
            this.emit('switch', this._page);
        });
    },

    setActive: function(active) {
      if (active) {
          this.actor.add_style_pseudo_class('active');
          this.actor.set_style("font-weight: bold;");
      } else {
          this.actor.remove_style_pseudo_class('active');
          this.actor.set_style(null);
      }
    },

    destroy: function() {
        this.actor.destroy();
    }
});
