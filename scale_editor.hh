/*
 * VuKNOB
 * Copyright (C) 2015 by Anton Persson
 *
 * http://www.vuknob.com/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef SCALE_EDITOR
#define SCALE_EDITOR

#include <kamogui.hh>
#include <functional>
#include <list>

class ScaleEditor : public KammoGUI::SVGCanvas::SVGDocument {
private:
	KammoGUI::SVGCanvas::SVGMatrix transform_t, shade_transform_t;

	KammoGUI::SVGCanvas::ElementReference shade_layer;
	KammoGUI::SVGCanvas::ElementReference no_event;

	class Key : public KammoGUI::SVGCanvas::ElementReference {
	public:
		Key(ScaleEditor *parent, const std::string &id,
		    int index, std::function<void(int)> callback);
	};

	class Setting : public KammoGUI::SVGCanvas::ElementReference {
	private:
		KammoGUI::SVGCanvas::ElementReference play_button;
		KammoGUI::SVGCanvas::ElementReference setting_text;
	public:
		Setting(ScaleEditor *parent,
			const std::string &id_base,
			std::function<void(Setting*)> callback);

		void change_setting(int key_index);
	};

	std::list<Key*> keys;
	std::list<Setting*> settings;

	Setting* active_setting = 0;

public:
	ScaleEditor(KammoGUI::SVGCanvas *cnv);

	virtual void on_resize() override;
	virtual void on_render() override;

	void hide();
	void show();
};

#endif
