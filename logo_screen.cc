/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
 *
 * About SATAN:
 *   Originally developed as a small subproject in
 *   a course about applied signal processing.
 * Original Developers:
 *   Anton Persson
 *   Johan Thim
 *
 * http://www.733kru.org/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <iostream>
using namespace std;

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <math.h>

#include <jngldrum/jinformer.hh>

#include "machine.hh"
#include "satan_project_entry.hh"
#include "logo_screen.hh"
#include "svg_loader.hh"
#include "remote_interface.hh"

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include "common.hh"

/***************************
 *
 *  Class LogoScreen::ThumpAnimation
 *
 ***************************/
LogoScreen::ThumpAnimation *LogoScreen::ThumpAnimation::active = NULL;

LogoScreen::ThumpAnimation::~ThumpAnimation() {
	active = NULL;
}

LogoScreen::ThumpAnimation::ThumpAnimation(double *_thump_offset, float duration) :
	KammoGUI::Animation(duration),
	thump_offset(_thump_offset) {}

LogoScreen::ThumpAnimation *LogoScreen::ThumpAnimation::start_thump(double *_thump_offset, float duration) {
	if(active) return NULL;

	active = new ThumpAnimation(_thump_offset, duration);

	return active;
}

void LogoScreen::ThumpAnimation::new_frame(float progress) {
	(*thump_offset) = sin((1.0 - progress) * 2.0 * M_PI);
}

void LogoScreen::ThumpAnimation::on_touch_event() { /* ignore touch */ }

/***************************
 *
 *  Class LogoScreen
 *
 ***************************/

void LogoScreen::on_resize() {
	KammoGUI::SVGCanvas::SVGRect document_size;
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);

	int pixel_w, pixel_h;
	get_canvas_size(pixel_w, pixel_h);

	float canvas_w_inches, canvas_h_inches;
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	float w_fingers = canvas_w_inches / INCHES_PER_FINGER;
	float h_fingers = canvas_h_inches / INCHES_PER_FINGER;

	// calculate pixel size of 12 by 15 fingers
	double finger_width = (12.0 * pixel_w ) / w_fingers;
	double finger_height = (15.0 * pixel_h) / h_fingers;

	// if larger than the canvas pixel size, then limit it
	finger_width = finger_width < pixel_w ? finger_width : pixel_w;
	finger_height = finger_height < pixel_h ? finger_height : pixel_h;

	// calculate scaling factor
	double scaling_w = (finger_width) / (double)document_size.width;
	double scaling_h = (finger_height) / (double)document_size.height;

	double scaling = scaling_w < scaling_h ? scaling_w : scaling_h;

	// calculate translation
	double translate_x = (pixel_w - document_size.width * scaling) / 2.0;
	double translate_y = (pixel_h - document_size.height * scaling) / 2.0;

	// initiate transform_m
	transform_m.init_identity();
	transform_m.translate(translate_x, translate_y);
	transform_m.scale(scaling, scaling);
}

void LogoScreen::on_render() {
	{ /* process thump */
		KammoGUI::SVGCanvas::SVGMatrix logo_base_t;
		KammoGUI::SVGCanvas::SVGMatrix logo_thump_t;

		logo_thump_t.translate(-250.0, -200.0);
		logo_thump_t.scale(1.0 + 0.1 * thump_offset,
				   1.0 + 0.1 * thump_offset
			);
		logo_thump_t.translate(250.0, 200.0);

		if(!logo_base_got) {
			knobBody_element->get_transform(knob_base_t);
			logo_base_got = true;
		}

		logo_base_t = knob_base_t;
		logo_base_t.multiply(logo_thump_t);
		knobBody_element->set_transform(logo_base_t);
	}

	KammoGUI::SVGCanvas::ElementReference root_element(this);
	root_element.set_transform(transform_m);
}

void remote_interface_disconnected() {
	SATAN_DEBUG("RemoteInterface connection lost.\n");
}

void failure_response(const std::string &response) {
	jInformer::inform(response);
}

void LogoScreen::element_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
				  KammoGUI::SVGCanvas::ElementReference *e_ref,
				  const KammoGUI::SVGCanvas::MotionEvent &event) {
	LogoScreen *ctx = (LogoScreen *)source;

	if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
		if(e_ref == ctx->knobBody_element) {
			ThumpAnimation *thump = ThumpAnimation::start_thump(&(ctx->thump_offset), 0.2);
			if(thump)
				ctx->start_animation(thump);
		} else if(e_ref == ctx->google_element) {
			// show google+ community
			KammoGUI::external_uri("https://plus.google.com/u/0/communities/117584425255812411008");
		} else if(e_ref == ctx->network_element) {
			ctx->server_list.clear();
			ctx->server_list.add_row("localhost");
#ifdef ANDROID
			auto list_content = AndroidJavaInterface::list_services();
			for(auto srv : list_content) {
				SATAN_DEBUG("   SRVC ---> %s\n", srv.first.c_str());
				ctx->server_list.add_row(srv.first);
			}
#endif
			ctx->server_list.select_from_list("Select vuKNOB server", NULL,
							  [ctx, list_content](void *context, bool selected, int row_number, const std::string &row) {
								  if(selected) {
									  auto srv = list_content.find(row);
									  if(srv != list_content.end()) {
										  ctx->selected_server = srv->second.first;
										  ctx->selected_port = srv->second.second;
									  } else {
										  ctx->selected_server = "localhost";
										  ctx->selected_port = RemoteInterface::Server::start_server(); // get the port number (the server is probably already started anyways..)
									  }
									  SATAN_DEBUG("Selected server ip/port: %s : %d",
										      ctx->selected_server.c_str(), ctx->selected_port);
								  }
							  }
				);
		} else if(e_ref == ctx->start_element) {
			// clear everything and prepare for start
			Machine::prepare_baseline();
			SatanProjectEntry::clear_satan_project();

			// if the selected port is equal to -1, default to the local host & port
			if(ctx->selected_port == -1) {
				ctx->selected_port = RemoteInterface::Server::start_server();
				ctx->selected_server = "localhost";
			}

			// connect to the selected server
			RemoteInterface::Client::start_client(ctx->selected_server, ctx->selected_port,
							      remote_interface_disconnected, failure_response);

			// just show main UI
			{
				static KammoGUI::UserEvent *ue = NULL;
				KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showMainUIContainer");
				if(ue != NULL)
					KammoGUI::EventHandler::trigger_user_event(ue);
			}

			// if we should start with the JAM view, skip to that directly
			if(ctx->start_with_jam_view) {
				static KammoGUI::UserEvent *uej = NULL;
				KammoGUI::get_widget((KammoGUI::Widget **)&uej, "showLivePad2");
				if(uej != NULL)
					KammoGUI::EventHandler::trigger_user_event(uej);
			}
		}
	}
}

#ifdef ANDROID
extern std::string __ANDROID_installation_id;

static void yes(void *ignored) {
	char bfr[1024];
	snprintf(bfr, 1024, "http://vuknob.com/devlink_redirect_counter.php?myid=%s", __ANDROID_installation_id.c_str());
	KammoGUI::external_uri(bfr);
}

static void no(void *ignored) {
	// do not do anything
	SATAN_DEBUG("NO   unique ID: %s\n", __ANDROID_installation_id.c_str());
}

static void ask_question(void *ignored) {
	KammoGUI::ask_yes_no("Can code?", "VuKNOB is looking for developers - want to know more?",
			     yes, NULL,
			     no, NULL);
}

#endif

LogoScreen::LogoScreen(bool hide_network_element, KammoGUI::SVGCanvas *cnvs, std::string fname) : SVGDocument(fname, cnvs), logo_base_got(false), server_list(cnvs) {
	start_with_jam_view = !hide_network_element;

	google_element = new KammoGUI::SVGCanvas::ElementReference(this, "google");
	google_element->set_event_handler(element_on_event);
	start_element = new KammoGUI::SVGCanvas::ElementReference(this, "start");
	start_element->set_event_handler(element_on_event);

	network_element = new KammoGUI::SVGCanvas::ElementReference(this, "network");
	network_element->set_event_handler(element_on_event);
	SATAN_DEBUG("LogoScreen::LogoScreen() - hide_network_element = %s\n", hide_network_element ? "true" : "false");
	if(hide_network_element) network_element->set_display("none");

	knobBody_element = new KammoGUI::SVGCanvas::ElementReference(this, "knobBody");
	knobBody_element->set_event_handler(element_on_event);

#ifdef ANDROID
//	KammoGUI::run_on_GUI_thread(ask_question, NULL);
#endif

}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(LogoScreenHandler,"logoScreen:logoScreenOld");

virtual void on_init(KammoGUI::Widget *wid) {
	KammoGUI::SVGCanvas *cnvs = (KammoGUI::SVGCanvas *)wid;
	cnvs->set_bg_color(1.0, 0.631373, 0.137254);

	SATAN_DEBUG("init LogoScreen - id: %s\n", wid->get_id().c_str());

	if(wid->get_id() == "logoScreen") {
		(void)new LogoScreen(false, cnvs, std::string(SVGLoader::get_svg_directory() + "/logoScreen.svg"));
	} else if(wid->get_id() == "logoScreenOld") {
		(void)new LogoScreen(true, cnvs, std::string(SVGLoader::get_svg_directory() + "/logoScreen.svg"));
	}

	// start up local VuKNOB server here
	int port_number = RemoteInterface::Server::start_server();
#ifdef ANDROID
	AndroidJavaInterface::announce_service(port_number);
	AndroidJavaInterface::discover_services();
#endif

}

KammoEventHandler_Instance(LogoScreenHandler);
