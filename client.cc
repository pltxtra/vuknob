/*
 * vu|KNOB
 * Copyright (C) 2021 by Anton Persson
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

#include <stdio.h>
#include <stdlib.h>
#include <thread>

#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>

#include <kamoflage/gnuVGcanvas.hh>
#include <vuknob/client.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static int port_to_connect_to = 0;

static AvahiSimplePoll *simple_poll = NULL;
static void resolve_callback(
	AvahiServiceResolver *r,
	AVAHI_GCC_UNUSED AvahiIfIndex interface,
	AVAHI_GCC_UNUSED AvahiProtocol protocol,
	AvahiResolverEvent event,
	const char *name,
	const char *type,
	const char *domain,
	const char *host_name,
	const AvahiAddress *address,
	uint16_t port,
	AvahiStringList *txt,
	AvahiLookupResultFlags flags,
	AVAHI_GCC_UNUSED void* userdata) {
	assert(r);
	/* Called whenever a service has been resolved successfully or timed out */
	switch (event) {
        case AVAHI_RESOLVER_FAILURE:
		fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
		break;
        case AVAHI_RESOLVER_FOUND: {
		char a[AVAHI_ADDRESS_STR_MAX], *t;
		fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain);
		avahi_address_snprint(a, sizeof(a), address);
		t = avahi_string_list_to_string(txt);
		fprintf(stderr,
			"\t%s:%u (%s)\n"
			"\tTXT=%s\n"
			"\tcookie is %u\n"
			"\tis_local: %i\n"
			"\tour_own: %i\n"
			"\twide_area: %i\n"
			"\tmulticast: %i\n"
			"\tcached: %i\n",
			host_name, port, a,
			t,
			avahi_string_list_get_service_cookie(txt),
			!!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
			!!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
			!!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
			!!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
			!!(flags & AVAHI_LOOKUP_RESULT_CACHED));
		avahi_free(t);

		if(port_to_connect_to == 0) {
			port_to_connect_to = port;
			printf("\n\n\n\nconnect to port %d\n\n\n\n", port_to_connect_to);
			vuknob_connect_to_server_at_port(port_to_connect_to);
		}
        }
	}
	avahi_service_resolver_free(r);
}

static void browse_callback(
	AvahiServiceBrowser *b,
	AvahiIfIndex interface,
	AvahiProtocol protocol,
	AvahiBrowserEvent event,
	const char *name,
	const char *type,
	const char *domain,
	AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
	void* userdata) {
	AvahiClient *c = (AvahiClient *)userdata;
	assert(b);
	/* Called whenever a new services becomes available on the LAN or is removed from the LAN */
	switch (event) {
        case AVAHI_BROWSER_FAILURE:
		fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
		avahi_simple_poll_quit(simple_poll);
		return;
        case AVAHI_BROWSER_NEW:
		fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
		/* We ignore the returned resolver object. In the callback
		   function we free it. If the server is terminated before
		   the callback function is called the server will free
		   the resolver for us. */
		if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0,
						 resolve_callback, c)))
			fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));
		break;
        case AVAHI_BROWSER_REMOVE:
		fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
		break;
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
		fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
		break;
	}
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
	assert(c);
	/* Called whenever the client or server state changes */
	if (state == AVAHI_CLIENT_FAILURE) {
		fprintf(stderr, "Server connection failure: %s\n", avahi_strerror(avahi_client_errno(c)));
		avahi_simple_poll_quit(simple_poll);
	}
}

int discover_with_avahi() {
	AvahiClient *client = NULL;
	AvahiServiceBrowser *sb = NULL;
	int error;
	int ret = 1;
	/* Allocate main loop object */
	if (!(simple_poll = avahi_simple_poll_new())) {
		fprintf(stderr, "Failed to create simple poll object.\n");
		goto fail;
	}
	/* Allocate a new client */
	client = avahi_client_new(avahi_simple_poll_get(simple_poll), (AvahiClientFlags)0, client_callback, NULL, &error);
	/* Check wether creating the client object succeeded */
	if (!client) {
		fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
		goto fail;
	}
	/* Create the service browser */
	if (!(sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_vuknob._tcp", NULL, (AvahiLookupFlags)0,
					     browse_callback, client))) {
		fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
		goto fail;
	}
	/* Run the main loop */
	avahi_simple_poll_loop(simple_poll);
	ret = 0;
fail:
	/* Cleanup things */
	if (sb)
		avahi_service_browser_free(sb);
	if (client)
		avahi_client_free(client);
	if (simple_poll)
		avahi_simple_poll_free(simple_poll);
	return ret;
}

typedef KammoGUI::MotionEvent MotionEvent;

static KammoGUI::GnuVGCanvas *cnvs;
static bool mouse_button_pressed = false;

namespace LogoScreenHandler {
	extern void __caller__();
}

void do_init() {
	printf("ALPHARU\n");
	LogoScreenHandler::__caller__();
	cnvs = new KammoGUI::GnuVGCanvas("sequence_container");//logoScreen");
	cnvs->set_bg_color(1.0, 1.0, 1.0);
	printf("BETARNA\n");

	KammoGUI::start();
}

void new_loop(GLFWwindow* window, int fb_width, int fb_height) {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, fb_width, fb_height);
	while (!glfwWindowShouldClose(window)) {
		pop_gnuvgcanvas_on_ui_thread_queue();
		glfwPollEvents();
		cnvs->step();
		glfwSwapBuffers(window);
	}
}

void framebuffer_size_callback(GLFWwindow* window, int fb_width, int fb_height) {
	glViewport(0, 0, fb_width, fb_height);
	cnvs->resize(fb_width, fb_height, fb_width / 110.0f, fb_height / 160.0f);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		printf("left button pressed.\n");
		mouse_button_pressed = true;

		MotionEvent m_evt;
		m_evt.init(0, 0, MotionEvent::ACTION_DOWN, 1, 0, xpos, ypos);
		m_evt.init_pointer(0, 0, xpos, ypos, 1.0);
		cnvs->register_motion_event(m_evt);
	}
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		printf("left button released.\n");
		mouse_button_pressed = false;

		MotionEvent m_evt;
		m_evt.init(0, 0, MotionEvent::ACTION_UP, 1, 0, xpos, ypos);
		m_evt.init_pointer(0, 0, xpos, ypos, 1.0);
		cnvs->register_motion_event(m_evt);
	}
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	if(mouse_button_pressed) {
		MotionEvent m_evt;
		m_evt.init(0, 0, MotionEvent::ACTION_MOVE, 1, 0, xpos, ypos);
		m_evt.init_pointer(0, 0, xpos, ypos, 1.0);
		cnvs->register_motion_event(m_evt);
	}
}

int main(void) {
	GLuint WIDTH = 800;
	GLuint HEIGHT = 600;
	GLFWwindow* window;

	std::thread avahi_thread(
		[](){
			discover_with_avahi();
		}
		);

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_SAMPLES, 8);
	window = glfwCreateWindow(WIDTH, HEIGHT, __FILE__, NULL, NULL);
	glfwMakeContextCurrent(window);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);

	printf("GL_VERSION  : %s\n", glGetString(GL_VERSION) );
	printf("GL_RENDERER : %s\n", glGetString(GL_RENDERER) );

	int fb_width, fb_height;
	glfwGetFramebufferSize(window, &fb_width, &fb_height);
	do_init();
	framebuffer_size_callback(window, fb_width, fb_height);
	new_loop(window, fb_width, fb_height);

	glfwTerminate();
	avahi_thread.join();

	return EXIT_SUCCESS;
}
