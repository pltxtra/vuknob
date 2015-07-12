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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <sstream>
#include <kamogui.hh>
#include <functional>

#include "scale_editor.hh"
#include "svg_loader.hh"

#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include "common.hh"

ScaleEditor::Key::Key(ScaleEditor *parent, const std::string &id,
		      int index, std::function<void(int)> callback)
	: ElementReference(parent, id) {
	set_event_handler(
		[this, id, index, callback](KammoGUI::SVGCanvas::SVGDocument *source,
			   KammoGUI::SVGCanvas::ElementReference *e_ref,
			   const KammoGUI::SVGCanvas::MotionEvent &event) {
			SATAN_DEBUG("Key pressed: %d (%s)\n", index, id.c_str());
			callback(index);
		}
		);
}

ScaleEditor::Setting::Setting(ScaleEditor *parent, const std::string &id,
			      std::function<void(Setting*)> callback)
	: ElementReference(parent, id + "set") {
	play_button = ElementReference(parent, id + "play");
	setting_text = play_button.find_child_by_class("key_text");

	set_event_handler(
		[this, id, callback](KammoGUI::SVGCanvas::SVGDocument *source,
				     KammoGUI::SVGCanvas::ElementReference *e_ref,
				     const KammoGUI::SVGCanvas::MotionEvent &event) {
			SATAN_DEBUG("Setting pressed: %s\n", id.c_str());
			callback(this);
		}
		);

	play_button.set_event_handler(
		[this, id](KammoGUI::SVGCanvas::SVGDocument *source,
			   KammoGUI::SVGCanvas::ElementReference *e_ref,
			   const KammoGUI::SVGCanvas::MotionEvent &event) {
			SATAN_DEBUG("Play pressed: %s\n", id.c_str());
		}
		);
}

void ScaleEditor::Setting::change_setting(int key_index) {
	SATAN_DEBUG("change_setting(%d)\n", key_index);
}

ScaleEditor::ScaleEditor(KammoGUI::SVGCanvas *cnv)
	: SVGDocument(std::string(SVGLoader::get_svg_directory() + "/ScaleEditor.svg"), cnv) {
	auto on_cancel_event = [this](KammoGUI::SVGCanvas::SVGDocument *source,
				   KammoGUI::SVGCanvas::ElementReference *e_ref,
				   const KammoGUI::SVGCanvas::MotionEvent &event) {
		SATAN_DEBUG("cancel ScaleEditor.\n");
		hide();
	};

	shade_layer = KammoGUI::SVGCanvas::ElementReference(this, "shadeLayer");
	shade_layer.set_event_handler(on_cancel_event);
	no_event = KammoGUI::SVGCanvas::ElementReference(this, "noEvent");
	no_event.set_event_handler([](KammoGUI::SVGCanvas::SVGDocument *source,
				      KammoGUI::SVGCanvas::ElementReference *e_ref,
				      const KammoGUI::SVGCanvas::MotionEvent &event) {
					   SATAN_DEBUG("no event area pressed.\n");
				   }
		);

	auto select_setting = [this](Setting* new_setting) {
		active_setting = new_setting;
		SATAN_DEBUG("New setting: %p\n", new_setting);
	};

	auto change_setting = [this](int index) {
		if(active_setting) {
			active_setting->change_setting(index);
		}
	};

	keys.push_back(new Key(this, "c_1",  0, change_setting));
	keys.push_back(new Key(this, "cs1",  1, change_setting));
	keys.push_back(new Key(this, "d_1",  2, change_setting));
	keys.push_back(new Key(this, "ds1",  3, change_setting));
	keys.push_back(new Key(this, "e_1",  4, change_setting));
	keys.push_back(new Key(this, "f_1",  5, change_setting));
	keys.push_back(new Key(this, "fs1",  6, change_setting));
	keys.push_back(new Key(this, "g_1",  7, change_setting));
	keys.push_back(new Key(this, "gs1",  8, change_setting));
	keys.push_back(new Key(this, "a_1",  9, change_setting));
	keys.push_back(new Key(this, "as1", 10, change_setting));
	keys.push_back(new Key(this, "b_1", 11, change_setting));

	keys.push_back(new Key(this, "c_2", 12, change_setting));
	keys.push_back(new Key(this, "cs2", 13, change_setting));
	keys.push_back(new Key(this, "d_2", 14, change_setting));
	keys.push_back(new Key(this, "ds2", 15, change_setting));
	keys.push_back(new Key(this, "e_2", 16, change_setting));
	keys.push_back(new Key(this, "f_2", 17, change_setting));
	keys.push_back(new Key(this, "fs2", 18, change_setting));
	keys.push_back(new Key(this, "g_2", 19, change_setting));
	keys.push_back(new Key(this, "gs2", 20, change_setting));
	keys.push_back(new Key(this, "a_2", 21, change_setting));
	keys.push_back(new Key(this, "as2", 22, change_setting));
	keys.push_back(new Key(this, "b_2", 23, change_setting));

	settings.push_back(new Setting(this, "s1_", select_setting));
	settings.push_back(new Setting(this, "s2_", select_setting));
	settings.push_back(new Setting(this, "s3_", select_setting));
	settings.push_back(new Setting(this, "s4_", select_setting));
	settings.push_back(new Setting(this, "s5_", select_setting));
	settings.push_back(new Setting(this, "s6_", select_setting));
	settings.push_back(new Setting(this, "s7_", select_setting));

	hide();
}

void ScaleEditor::show() {
	KammoGUI::SVGCanvas::ElementReference root_element = KammoGUI::SVGCanvas::ElementReference(this);
	root_element.set_display("inline");
	active_setting = 0;
}

void ScaleEditor::hide() {
	KammoGUI::SVGCanvas::ElementReference root_element = KammoGUI::SVGCanvas::ElementReference(this);
	root_element.set_display("none");
}

void ScaleEditor::on_resize() {
	int canvas_w, canvas_h;
	float canvas_w_inches, canvas_h_inches;
	KammoGUI::SVGCanvas::SVGRect document_size;

	// get data
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);
	get_canvas_size(canvas_w, canvas_h);
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	{ // calculate transform for the shadeLayer
		// calculate scaling factor
		double scale_w = canvas_w / (double)document_size.width;
		double scale_h = canvas_h / (double)document_size.height;

		// initiate transform_t
		shade_transform_t.init_identity();
		shade_transform_t.scale(scale_w, scale_h);
	}

	{ // calculate transform for the main part of the document
		double scaling = FingerScaler::fit_to_fingers(canvas_w_inches, canvas_h_inches,
							      canvas_w, canvas_h,
							      7, 6,
							      document_size.width, document_size.height);

		// calculate translation
		double translate_x = (canvas_w - document_size.width * scaling) / 2.0;
		double translate_y = (canvas_h - document_size.height * scaling) / 2.0;

		// initiate transform_t
		transform_t.init_identity();
		transform_t.scale(scaling, scaling);
		transform_t.translate(translate_x, translate_y);
	}
}

void ScaleEditor::on_render() {
	{
		// Translate shade_layer, and scale it properly to fit the defined viewbox
		shade_layer.set_transform(shade_transform_t);

		// Translate ScaleEditor, and scale it properly to fit the defined viewbox
		KammoGUI::SVGCanvas::ElementReference main_layer(this, "mainLayer");
		main_layer.set_transform(transform_t);
	}
}
