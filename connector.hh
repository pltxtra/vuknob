/*
 * VuKNOB
 * (C) 2014 by Anton Persson
 *
 * http://www.vuknob.com/
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

#ifndef CONNECTOR_CLASS
#define CONNECTOR_CLASS

#include <kamogui.hh>
#include <functional>
#include <kamogui_scale_detector.hh>
#include <kamogui_fling_detector.hh>

#include "remote_interface.hh"
#include "machine.hh"
#include "listview.hh"
#include "corner_button.hh"
#include "connection_list.hh"

class Connector : public KammoGUI::SVGCanvas::SVGDocument, public KammoGUI::ScaleGestureDetector::OnScaleGestureListener,
		  public RemoteInterface::RIMachine::RIMachineSetListener {
private:
	class MachineGraphic;

	class ConnectionGraphic : public KammoGUI::SVGCanvas::ElementReference, public std::enable_shared_from_this<ConnectionGraphic>  {
	private:
		typedef std::pair<std::weak_ptr<MachineGraphic>,
				  std::weak_ptr<MachineGraphic>
				  > cg_machine_cpl;
		struct cg_pair_compare {
			bool operator() (const cg_machine_cpl& x,
					 const cg_machine_cpl& y) const {
				auto x1 = x.first;
				auto x2 = x.second;
				auto y1 = y.first;
				auto y2 = y.second;

				if(!x1.owner_before(y1) && !y1.owner_before(x1)) {
					return x2.owner_before(y2);
				}

				return x1.owner_before(y1);
			}
		};

		// internal storage of all current ConnectionGraphic objects (map a pair containing Output machine/Input machine to the connection graphic object)
		static std::map<cg_machine_cpl,
				std::shared_ptr<ConnectionGraphic>,
				cg_pair_compare> connection_graphics;

		KammoGUI::SVGCanvas::ElementReference selectButton;
		std::weak_ptr<MachineGraphic> source, destination;

		std::set<std::pair<std::string, std::string> > output2input_names; // set of pairs of output name -> input name

		double x1, y1, x2, y2, center_x, center_y;
		bool hidden = false;

		void recalculate_position();

		void debug_print();
	public:
		ConnectionGraphic(Connector *context,
				  const std::string &element_id,
				  std::shared_ptr<MachineGraphic> source,
				  std::shared_ptr<MachineGraphic> destination,
				  const std::string &output,
				  const std::string &input);
		~ConnectionGraphic();

		void recalculate_end_point(std::shared_ptr<MachineGraphic> end_point);

		static std::shared_ptr<ConnectionGraphic> attach(Connector *context,
								 std::shared_ptr<MachineGraphic> source,
								 std::shared_ptr<MachineGraphic> destination,
								 const std::string &output,
								 const std::string &input);

		static void detach(std::shared_ptr<MachineGraphic> source,
				   std::shared_ptr<MachineGraphic> destination,
				   const std::string &output,
				   const std::string &input);

		static void hide_all();
		static void show_all();

		static void machine_unregistered(std::shared_ptr<MachineGraphic> machine);
	};

	class MachineGraphic : public KammoGUI::SVGCanvas::ElementReference, public RemoteInterface::RIMachine::RIMachineStateListener, public std::enable_shared_from_this<MachineGraphic>  {
	private:
		static std::map<std::shared_ptr<RemoteInterface::RIMachine>, std::weak_ptr<MachineGraphic> > mch2grph;
		static std::pair<std::weak_ptr<MachineGraphic>, std::string> *current_output;

		class Transition : public KammoGUI::Animation {
		private:
			MachineGraphic *ctx;
			std::function<void(MachineGraphic *context, float progress)> callback;

		public:
			Transition(MachineGraphic *context, std::function<void(MachineGraphic *context, float progress)> callback);
			virtual void new_frame(float progress) override;
			virtual void on_touch_event() override;
		};

		class SelectionAnimation : public KammoGUI::Animation {
		private:
			MachineGraphic *ctx;
			float last_time;
		public:
			float selection_animation_progress; // between 0.0<1.0

			SelectionAnimation(MachineGraphic *context) : Animation(0.0), ctx(context), last_time(0.0), selection_animation_progress(0.0) {}
			virtual void new_frame(float progress) override {
				auto df = progress - last_time;
				last_time = progress;

				selection_animation_progress += df * 0.1;
				if(selection_animation_progress > 1.0)
					selection_animation_progress -= 1.0;
			}
			virtual void on_touch_event() override { /* ignore */ }

		};

		class IOSocket : public KammoGUI::SVGCanvas::ElementReference {
		public:
			IOSocket(KammoGUI::SVGCanvas::ElementReference original,
				 KammoGUI::SVGCanvas::ElementReference socket_gfx,
				 MachineGraphic *_owner, const std::string &_name);
			ElementReference socket_gfx;
			MachineGraphic *owner;
			std::string name;
		};

		// positional data
		KammoGUI::SVGCanvas::SVGMatrix base_t;
		double pos_x, pos_y, tilt_x;
		double appear_at_x, appear_at_y;

		// event data
		double first_selection_x, first_selection_y;
		bool is_a_tap;
		bool detailed_mode;

		// Connection data
		std::set<std::weak_ptr<ConnectionGraphic>,
			 std::owner_less<std::weak_ptr<ConnectionGraphic> > > connections;

		// Socket data
		KammoGUI::SVGCanvas::ElementReference socket_container;

		std::vector<IOSocket *> inputs;
		std::vector<IOSocket *> outputs;

		std::shared_ptr<RemoteInterface::RIMachine>machine;
		Connector *context;

		SelectionAnimation *selected_animation; // if this is set, then this machine is selected
		KammoGUI::SVGCanvas::SVGMatrix selection_indicator_base_t;

		void hide_sockets(std::vector<IOSocket *> &vctr);
		void show_sockets(std::vector<IOSocket *> &vctr);
		void create_sockets(std::vector<IOSocket *> &vctr, std::vector<std::string> names,
				    KammoGUI::SVGCanvas::ElementReference &template_socket,
				    void (*this_on_event)(KammoGUI::SVGCanvas::SVGDocument *source,
							  KammoGUI::SVGCanvas::ElementReference *e_ref,
							  const KammoGUI::SVGCanvas::MotionEvent &event));

		void select(const std::string &output_socket_name);
		void deselect();

		void refresh_appearance(double progress);
		void refresh_position(double rel_x, double rel_y);

		static void transition_progressed(MachineGraphic *ctx, float progress);
		static void on_event(KammoGUI::SVGCanvas::SVGDocument *source,
				     KammoGUI::SVGCanvas::ElementReference *e_ref,
				     const KammoGUI::SVGCanvas::MotionEvent &event);
		static void on_output_socket_event(KammoGUI::SVGCanvas::SVGDocument *source,
						   KammoGUI::SVGCanvas::ElementReference *e_ref,
						   const KammoGUI::SVGCanvas::MotionEvent &event);
		static void on_input_socket_event(KammoGUI::SVGCanvas::SVGDocument *source,
						  KammoGUI::SVGCanvas::ElementReference *e_ref,
						  const KammoGUI::SVGCanvas::MotionEvent &event);

		std::string name_copy;

	public:
		MachineGraphic(Connector *context, const std::string &svg_id, std::shared_ptr<RemoteInterface::RIMachine> machine);
		~MachineGraphic();

		void debug_print();

		virtual void on_move() override;
		virtual void on_attach(std::shared_ptr<RemoteInterface::RIMachine> src_machine,
				       const std::string src_output,
				       const std::string dst_input) override;
		virtual void on_detach(std::shared_ptr<RemoteInterface::RIMachine> src_machine,
				       const std::string src_output,
				       const std::string dst_input) override;

		bool matches_ri_machine(std::shared_ptr<RemoteInterface::RIMachine> ri_machine);
		std::shared_ptr<RemoteInterface::RIMachine> get_ri_machine();

		static std::shared_ptr<MachineGraphic> create(Connector *context, std::shared_ptr<RemoteInterface::RIMachine> machine);

		void hide();
		void show();
		void leave_detailed_mode();
		void get_position(double &x, double &y);

		static void animate_selection();
	};

	class ZoomTransition : public KammoGUI::Animation {
	private:
		Connector *ctx;
		std::function<void(Connector *context, double zoom, double x, double y)> callback;

		double zoom_start, zoom_stop;
		double x_start, y_start;
		double x_stop,  y_stop;

	public:
		ZoomTransition(Connector *context,
			       std::function<void(Connector *context, double zoom, double x, double y)> callback,
			       double zoom_start, double zoom_stop,
			       double x_start, double y_start,
			       double x_stop, double y_stop);
		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};

	CornerButton plus_button, trash_button, settings_button;
	ListView list_view;

	KammoGUI::SVGCanvas::ElementReference pan_and_zoom;

	double zoom_scaling, old_scaling, scaling, base_translate_x, base_translate_y, position_x, position_y;
	double center_x, center_y; // offset, in pixels, of the center of the view from the top left corner.
	std::vector<std::shared_ptr<MachineGraphic> > graphics;
	std::weak_ptr<MachineGraphic> selected_graphic;


	double pan_zoom_offset_x, pan_zoom_offset_y, pan_zoom_scale;
	double first_selection_x, first_selection_y;
	double last_selection_x, last_selection_y;
	bool is_a_tap;

	// used by ZoomTransition
	static void zoom_callback(Connector *ctx, double zoom, double x, double y);

	// connection list data
	std::shared_ptr<ConnectionList> connection_list;

	// BEGIN scale detector
	KammoGUI::ScaleGestureDetector sgd;
	virtual bool on_scale(KammoGUI::ScaleGestureDetector *detector);
	virtual bool on_scale_begin(KammoGUI::ScaleGestureDetector *detector);
	virtual void on_scale_end(KammoGUI::ScaleGestureDetector *detector);
	// END scale detector

	static void on_pan_and_zoom_event(KammoGUI::SVGCanvas::SVGDocument *source,
					  KammoGUI::SVGCanvas::ElementReference *e_ref,
					  const KammoGUI::SVGCanvas::MotionEvent &event);

public:

	Connector(KammoGUI::SVGCanvas *cnv);
	~Connector();

	virtual void on_resize();
	virtual void on_render();

	double get_scaling();

	void show_connection_list(double x_pos, double y_pos,
				  std::shared_ptr<RemoteInterface::RIMachine> src,
				  std::shared_ptr<RemoteInterface::RIMachine> dst,
				  std::set<std::pair<std::string, std::string> > output2input_names);

	void zoom_in_at(std::shared_ptr<MachineGraphic> selected, double x_pos, double y_pos);
	void zoom_restore();

	virtual void ri_machine_registered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) override;
	virtual void ri_machine_unregistered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) override;

};

#endif
