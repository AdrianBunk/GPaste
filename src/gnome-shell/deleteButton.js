/*
 * This file is part of GPaste.
 *
 * Copyright (c) 2010-2019, Marc-Antoine Perennou <Marc-Antoine@Perennou.com>
 */

const { Clutter, GObject, St } = imports.gi;

var GPasteDeleteButton = GObject.registerClass(
class GPasteDeleteButton extends St.Button {
    _init(client, uuid) {
        super._init();

        this.child = new St.Icon({
            icon_name: 'edit-delete-symbolic',
            style_class: 'popup-menu-icon'
        });

        this._client = client;
        this.setUuid(uuid);

        this.connect('clicked', this._onClick.bind(this));
    }

    setUuid(uuid) {
        this._uuid = uuid;
    }

    _onClick() {
        this._client.delete(this._uuid, null);
        return Clutter.EVENT_STOP;
    }
});
