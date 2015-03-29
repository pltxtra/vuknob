/*
 * VuKNOB
 * (C) 2015 by Anton Persson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <iostream>

#include "connector.hh"
#include "svg_loader.hh"
#include "machine.hh"
#include "machine_sequencer.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 *  Class Connector::ConnectionGraphic
 *
 ***************************/

std::map<Connector::ConnectionGraphic::cg_machine_cpl,
	 std::shared_ptr<Connector::ConnectionGraphic>,
	 Connector::ConnectionGraphic::cg_pair_compare> Connector::ConnectionGraphic::connection_graphics;


Connector::ConnectionGraphic::ConnectionGraphic(Connector *_context,
						const std::string &element_id,
						std::shared_ptr<MachineGraphic> _source,
						std::shared_ptr<MachineGraphic> _destination,
						const std::string &output,
						const std::string &input) : KammoGUI::SVGCanvas::ElementReference(_context, element_id), source(_source), destination(_destination) {
	output2input_names.insert({output, input});

	_source->get_position(x1, y1);
	_destination->get_position(x2, y2);

	recalculate_position();

	selectButton = find_child_by_class("selectButton");
	selectButton.set_event_handler(
		[this, _context](KammoGUI::SVGCanvas::SVGDocument *,
		       KammoGUI::SVGCanvas::ElementReference *,
		       const KammoGUI::SVGCanvas::MotionEvent &event) {
			if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
				auto s_m = source.lock();
				auto d_m = destination.lock();
				if(s_m && d_m) {
					_context->show_connection_list(center_x, center_y,
								       s_m->get_ri_machine(),
								       d_m->get_ri_machine(),
								       output2input_names);
				}
			}
		}
		);
	SATAN_DEBUG("   created new ConnectionGraphic. %p\n", this);
}

Connector::ConnectionGraphic::~ConnectionGraphic() {
	drop_element();
}

void Connector::ConnectionGraphic::recalculate_position() {
	double dx = x2 - x1;
	double dy = y2 - y1;

	double len = sqrt(dx * dx + dy * dy);

	// if hidden, or if len == 0.0 we just hide this graphic, don't need to calculate anything
	if(hidden || len == 0.0) {
		set_display("none");
	} else {
		set_display("inline");

		// set the lenght of the connection line
		find_child_by_class("connectionLine").set_line_coords(250.0 - len / 2.0, 250.0, 250.0 + len / 2.0, 250.0);

		// calculate additional coordinate data
		center_x = x1 + dx / 2.0;
		center_y = y1 + dy / 2.0;

		// calculte rotation angle
		double angle = atan2(dy, dx);

		// rotate, and translate to center
		KammoGUI::SVGCanvas::SVGMatrix transform_m;
		transform_m.init_identity();
		transform_m.translate(-250.0, -250.0);
		transform_m.rotate(angle);
		transform_m.translate(center_x, center_y);

		set_transform(transform_m);
	}	
}

void Connector::ConnectionGraphic::recalculate_end_point(std::shared_ptr<MachineGraphic> end_point) {
	if(end_point == (source.lock())) {
		end_point->get_position(x1, y1);
	} else {
		end_point->get_position(x2, y2);
	}
	recalculate_position();
}

auto Connector::ConnectionGraphic::attach(Connector *context,
					  std::shared_ptr<MachineGraphic> source_machine,
					  std::shared_ptr<MachineGraphic> destination_machine,
					  const std::string &output,
					  const std::string &input) -> std::shared_ptr<ConnectionGraphic> {
	auto found = connection_graphics.find({source_machine, destination_machine});

	// if a matching connection graphic object already exists, then we insert the new connection into that
	if(found != connection_graphics.end()) {
		bool non_existing = true;
		for(auto output2input : (*found).second->output2input_names) {
			if(output2input.first == output && output2input.second == input) {
				non_existing = false;
				break;
			}
		}
		// ignore duplicates
		if(non_existing) {
			(*found).second->output2input_names.insert({output, input});
		}
		return (*found).second;
	}

	// if we get here, there was no previously created ConnectionGraphic object - we must create one our selves
	
	std::stringstream id_stream;
	id_stream << "connection_" << (void *)source_machine.get() << "_" << (void *)destination_machine.get();
	
	SATAN_DEBUG("Create ConnectionGraphic - Connector context: %p\n", context);
	
	KammoGUI::SVGCanvas::ElementReference connection_template = KammoGUI::SVGCanvas::ElementReference(context, "connectionTemplate");
	KammoGUI::SVGCanvas::ElementReference connection_layer = KammoGUI::SVGCanvas::ElementReference(context, "connectionContainer");
	
	(void) connection_layer.add_element_clone(id_stream.str(), connection_template);
	
	auto retval = std::make_shared<ConnectionGraphic>(context, id_stream.str(), source_machine, destination_machine, output, input);

	connection_graphics[{source_machine, destination_machine}] = retval;
		
	return retval;
}

void Connector::ConnectionGraphic::debug_print() {
	
}

void Connector::ConnectionGraphic::detach(std::shared_ptr<MachineGraphic> source_machine,
					  std::shared_ptr<MachineGraphic> destination_machine,
					  const std::string &output,
					  const std::string &input) {
	auto found_gfx = connection_graphics.find({source_machine, destination_machine});

	for(auto cong : connection_graphics) {
		auto s = cong.first.first.lock();
		auto d = cong.first.second.lock();
		if(s) SATAN_DEBUG("    s exists: %p\n", s.get());
		if(d) SATAN_DEBUG("    d exists: %p\n", d.get());
		SATAN_DEBUG("    source: %p\n", source_machine.get());
		SATAN_DEBUG("    destin: %p\n", destination_machine.get());
	}
	
	// try to find existing graphics object
	if(found_gfx != connection_graphics.end()) {
		SATAN_DEBUG("  ConnectionGraphic::detach() - connection found!\n");
		auto found_con = (*found_gfx).second->output2input_names.find({output,input});

		// then if we in the graphics object find a connection matching the output/input
		// we will erase that
		if(found_con != (*found_gfx).second->output2input_names.end()) {
			(*found_gfx).second->output2input_names.erase(found_con);
		}

		// If there are no more connections in this ConnectionGraphic - erase the complete object too
		if((*found_gfx).second->output2input_names.size() == 0) {
			connection_graphics.erase(found_gfx);
		}
	} else {
		SATAN_DEBUG("  ConnectionGraphic::detach() - connection NOT found!\n");
	}

	if(connection_graphics.size() == 0) SATAN_DEBUG("ConnectionGraphic::detach() --------- !!!! NO MORE CONNECTIONS.\n");
}

void Connector::ConnectionGraphic::hide_all() {
	for(auto g : connection_graphics) {
		g.second->debug_print();
		g.second->set_display("none");
		g.second->hidden = true;
	}
}

void Connector::ConnectionGraphic::show_all() {
	for(auto g : connection_graphics) {
		g.second->set_display("inline");
		g.second->hidden = false;
	}
}

void Connector::ConnectionGraphic::machine_unregistered(std::shared_ptr<MachineGraphic> machine) {
	// delete all ConnectionGraphic objects that reference the machine.
	auto iterator = connection_graphics.begin();
	for(; iterator != connection_graphics.end(); iterator++) {
		auto src = (*iterator).second->source.lock();
		auto dst = (*iterator).second->source.lock();
		if(src == machine || dst == machine) {
			connection_graphics.erase(iterator);
		}
	}

	if(connection_graphics.size() == 0) SATAN_DEBUG("ConnectionGraphic::machine_unregistered() --------- !!!! NO MORE CONNECTIONS.\n");
}

/***************************
 *
 *  Class Connector::MachineGraphic::Transition
 *
 ***************************/

Connector::MachineGraphic::Transition::Transition(
	Connector::MachineGraphic *_context,
	std::function<void(Connector::MachineGraphic *context, float progress)> _callback) :
	KammoGUI::Animation(TRANSITION_TIME), ctx(_context), callback(_callback) {
}

void Connector::MachineGraphic::Transition::new_frame(float progress) {
	callback(ctx, progress);
}

void Connector::MachineGraphic::Transition::on_touch_event() { /* ignored */ }

/***************************
 *
 *  Class Connector::MachineGraphic
 *
 ***************************/

// we have to scale the positional data of a machine by this scaling factor to make they appear nicely
#define MACHINE_POS_SCALING 2000.0

std::map<std::shared_ptr<RemoteInterface::RIMachine>, std::weak_ptr<Connector::MachineGraphic> > Connector::MachineGraphic::mch2grph;
std::pair<std::weak_ptr<Connector::MachineGraphic>, std::string>* Connector::MachineGraphic::current_output = NULL;

Connector::MachineGraphic::IOSocket::IOSocket(KammoGUI::SVGCanvas::ElementReference original,
					      KammoGUI::SVGCanvas::ElementReference _socket_gfx,
					      MachineGraphic *_owner, const std::string &_name) :
	KammoGUI::SVGCanvas::ElementReference(&original), socket_gfx(_socket_gfx), owner(_owner), name(_name)
{}

void Connector::MachineGraphic::debug_print() {
	SATAN_DEBUG("Graphic: %s -> (%f, %f)\n",
		    machine->get_name().c_str(), pos_x, pos_y);
}

Connector::MachineGraphic::MachineGraphic(Connector *_context, const std::string &svg_id,
					  std::shared_ptr<RemoteInterface::RIMachine> _machine) :
	KammoGUI::SVGCanvas::ElementReference(_context, svg_id), tilt_x(0.0), detailed_mode(false), machine(_machine), context(_context), selected_animation(NULL) {

	pos_x = machine->get_x_position() * MACHINE_POS_SCALING;
	pos_y = machine->get_y_position() * MACHINE_POS_SCALING;

	name_copy = _machine->get_name();

	find_child_by_class("machineName").set_text_content(machine->get_name());
	auto selind = find_child_by_class("selectedIndicator"); selind.set_display("none");
	selind.get_transform(selection_indicator_base_t);

	socket_container = find_child_by_class("socketContainer");

	set_event_handler(on_event);

	{ // create sockets
		KammoGUI::SVGCanvas::ElementReference input_template = KammoGUI::SVGCanvas::ElementReference(context, "inputIndicatorTemplate");
		KammoGUI::SVGCanvas::ElementReference output_template = KammoGUI::SVGCanvas::ElementReference(context, "outputIndicatorTemplate");

		create_sockets(inputs, machine->get_input_names(), input_template, on_input_socket_event);
		create_sockets(outputs, machine->get_output_names(), output_template, on_output_socket_event);
	}
	{ // move the graphic into position
		
		get_transform(base_t);
		base_t.translate(-250.0, -250.0);

		refresh_position(0.0, 0.0);
	}
	
	refresh_appearance(1.0);
}

Connector::MachineGraphic::~MachineGraphic() {
	// make sure we delete the input and output ElementReference objects
	// otherwise the elements will be left dangling when we delete the element. (memleak)
	for(auto inp : inputs) delete inp;
	for(auto oup : outputs) delete oup;
	inputs.clear();
	outputs.clear();

	if(selected_animation) {
		deselect();
	}	
	
	drop_element();

	auto found = mch2grph.find(machine);
	if(found != mch2grph.end()) {
		mch2grph.erase(found);
	}
}

void Connector::MachineGraphic::on_move() {
	KammoGUI::run_on_GUI_thread(
		[this]() {
			pos_x = machine->get_x_position() * MACHINE_POS_SCALING;
			pos_y = machine->get_y_position() * MACHINE_POS_SCALING;
			refresh_position(0.0, 0.0);
		}
		);
}

void Connector::MachineGraphic::on_attach(std::shared_ptr<RemoteInterface::RIMachine> src_machine,
					  const std::string src_output,
					  const std::string dst_input) {
	if(src_machine->get_machine_type() == "MachineSequencer") return; // we can skip this case right away

	SATAN_DEBUG("---->     (%d) MachineGraphic() -- attached [%s] @ %s ---> [%s] @ %s\n",
		    gettid(),
		    src_machine->get_name().c_str(), src_output.c_str(),
		    machine->get_name().c_str(), dst_input.c_str());
	
	auto thiz = shared_from_this();
	KammoGUI::run_on_GUI_thread(
		[this, thiz,src_machine, src_output, dst_input]() {
			SATAN_DEBUG("(%d) MachineGraphic() -- attached [%s] @ %s ---> [%s] @ %s\n",
				    gettid(),
				    src_machine->get_name().c_str(), src_output.c_str(),
				    machine->get_name().c_str(), dst_input.c_str());
			
			auto weak_src_gfx = mch2grph.find(src_machine);
			
			// only attach if we have a MachineGraphic object for the source as well (in some cases we don't, like for MachineSequencer objects.)
			if(weak_src_gfx != mch2grph.end()) {
				if(auto src_gfx = (*weak_src_gfx).second.lock()) {
					auto congfx = ConnectionGraphic::attach(context, src_gfx, shared_from_this(), src_output, dst_input);
					
					{
						// insert here
						auto found_here = connections.find(congfx);
						if(found_here == connections.end()) {
							// we don't have this on file, insert it
							connections.insert(congfx);
						}
					}
					{
						// insert there
						auto found_there = src_gfx->connections.find(congfx);
						if(found_there == src_gfx->connections.end()) {
							// we don't have this on file, insert it
							src_gfx->connections.insert(congfx);
						}
					}
				} else {
					// the object pointed to by the weak_src_gfx pointer is gone - remove this entry
					mch2grph.erase(weak_src_gfx);
					SATAN_DEBUG("     **** MachineGraphic for %s has been removed - can't create ConnectionGraphic.\n",
						    src_machine->get_name().c_str());
				}
			} else {
				SATAN_DEBUG("     **** %s has no source graphic - can't create ConnectionGraphic.\n",
					    src_machine->get_name().c_str());
			}
			SATAN_DEBUG("   end MachineGraphic::attached.\n");
			SATAN_DEBUG("   \n");
		}
		);
}

void Connector::MachineGraphic::on_detach(std::shared_ptr<RemoteInterface::RIMachine> src_machine,
					  const std::string src_output,
					  const std::string dst_input) {
	auto thiz = shared_from_this();
	KammoGUI::run_on_GUI_thread(
		[this, thiz,src_machine, src_output, dst_input]() {
			SATAN_DEBUG("MachineGraphic() -- detached [%s] @ %s ---> [%s] @ %s\n",
				    src_machine->get_name().c_str(), src_output.c_str(),
				    machine->get_name().c_str(), dst_input.c_str());
			
			auto weak_src_gfx = thiz->mch2grph.find(src_machine);
			
			// only detach if we have a MachineGraphic object for the source as well (in some cases we don't, like for MachineSequencer objects.)
			if(weak_src_gfx != mch2grph.end()) {
				if(auto src_gfx = (*weak_src_gfx).second.lock()) {
					ConnectionGraphic::detach(src_gfx, shared_from_this(), src_output, dst_input);
				} else {
					// the object pointed to by the weak_src_gfx pointer is gone - remove this entry
					mch2grph.erase(weak_src_gfx);
				}
			}
		}
		);
}

bool Connector::MachineGraphic::matches_ri_machine(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	return (ri_machine == machine) ? true : false;
}

std::shared_ptr<RemoteInterface::RIMachine> Connector::MachineGraphic::get_ri_machine() {
	return machine;
}

void Connector::MachineGraphic::hide_sockets(std::vector<IOSocket *> &vctr) {
	for(auto socket : vctr) {
		socket->socket_gfx.set_display("none");
	}
}

void Connector::MachineGraphic::show_sockets(std::vector<IOSocket *> &vctr) {
	for(auto socket : vctr) {
		socket->socket_gfx.set_display("inline");
	}
}

void Connector::MachineGraphic::create_sockets(std::vector<IOSocket *> &vctr, std::vector<std::string> names,
					       KammoGUI::SVGCanvas::ElementReference &template_socket,
					       void (*this_on_event)(KammoGUI::SVGCanvas::SVGDocument *source,
								     KammoGUI::SVGCanvas::ElementReference *e_ref,
								     const KammoGUI::SVGCanvas::MotionEvent &event)) {
	double angle_step = 180.0 / 5.0; // divide one side into 5 segments (an input or output takes one segment on one side.)

	double angle = (-angle_step) * (((double)names.size() - 1.0) / 2.0);

	for(auto name : names) {
		std::stringstream id_stream;
		id_stream << "socket_" << ((void *)&vctr) << "_" << name;

		try {
			KammoGUI::SVGCanvas::ElementReference socket =
				socket_container.add_element_clone(id_stream.str(), template_socket);

			socket.find_child_by_class("nameTemplate").set_text_content(name);
			
			KammoGUI::SVGCanvas::SVGMatrix rotate_t;
			socket.get_transform(rotate_t);
						
			rotate_t.translate(-250.0, -250.0);
			rotate_t.rotate(angle * M_PI / 180.0);
			rotate_t.translate(250.0, 250.0);
			socket.set_transform(rotate_t);
			
			socket.set_display("inline");

			vctr.push_back(new IOSocket(socket.find_child_by_class("selectButton"), socket, this, name));
			vctr[vctr.size() - 1]->set_event_handler(this_on_event);
			
			angle += angle_step;
		} catch(KammoGUI::SVGCanvas::OperationFailedException &e) {
			SATAN_DEBUG("   OPERATION FAILED\n");
			throw;
		} catch(...) {
			SATAN_DEBUG("   UNKNOWN EXCEPTPION\n");
			throw;
		}

	}
}

void Connector::MachineGraphic::select(const std::string &output_socket_name) {
	current_output = new std::pair<std::weak_ptr<MachineGraphic>, std::string>(shared_from_this(), output_socket_name);

	selected_animation = new SelectionAnimation(this);
	context->start_animation(selected_animation);	
	
	find_child_by_class("selectedIndicator").set_display("inline");

	leave_detailed_mode();
	context->zoom_restore();
}

void Connector::MachineGraphic::deselect() {
	if(current_output) {
		delete current_output;
		current_output = NULL;
	}

	if(selected_animation) {
		selected_animation->stop();
		selected_animation = NULL;
	}
	find_child_by_class("selectedIndicator").set_display("none");
}

void Connector::MachineGraphic::refresh_appearance(double progress) {
	if(!detailed_mode) progress = 1.0 - progress; // inverted value

	{
		double scale = 0.5 + progress * 0.5;
		
		KammoGUI::SVGCanvas::SVGMatrix zoom_t;
		zoom_t.translate(-250.0, -250.0);
		zoom_t.scale(scale, scale);
		zoom_t.translate(250.0, 250.0);
		
		socket_container.set_transform(zoom_t);
	}

	{
		bool op_view = current_output ? false : true;;
	
		tilt_x = 125.0 * progress * (op_view ? -1.0 : 1.0);
		hide_sockets(op_view ? inputs : outputs);
		show_sockets(op_view ? outputs : inputs);

		refresh_position(0.0, 0.0);
	}
	
	socket_container.set_display(progress == 0.0 ? "none" : "inline");
}

void Connector::MachineGraphic::refresh_position(double temp_x, double temp_y) {
	appear_at_x = pos_x + temp_x + tilt_x;
	appear_at_y = pos_y + temp_y;
	
	KammoGUI::SVGCanvas::SVGMatrix move_t = base_t;
	move_t.translate(appear_at_x, appear_at_y);
	set_transform(move_t);

	for(auto weak_con : connections) {
		if(auto con = weak_con.lock()) {
			con->recalculate_end_point(shared_from_this());
		}
	}
}

void Connector::MachineGraphic::transition_progressed(MachineGraphic *ctx, float progress) {
	ctx->refresh_appearance((double)progress);
}

void Connector::MachineGraphic::on_event(KammoGUI::SVGCanvas::SVGDocument *source,
					 KammoGUI::SVGCanvas::ElementReference *e_ref,
					 const KammoGUI::SVGCanvas::MotionEvent &event) {
	MachineGraphic *ctx = (MachineGraphic *)e_ref;

	double now_x = event.get_x();
	double now_y = event.get_y();
	
	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		ctx->is_a_tap = false;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		// check if the user is moving the finger too far, indicating abort action
		// if so - disable is_a_tap
		if(ctx->is_a_tap) {
			if(fabs(now_x - ctx->first_selection_x) > 20)
				ctx->is_a_tap = false;
			if(fabs(now_y - ctx->first_selection_y) > 20)
				ctx->is_a_tap = false;
		} else if(!ctx->detailed_mode) {
			double scaling = ctx->context->get_scaling();
			double move_x = (now_x - ctx->first_selection_x) / scaling;
			double move_y = (now_y - ctx->first_selection_y) / scaling;

			ctx->refresh_position(move_x, move_y);
		}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		ctx->is_a_tap = true;
		ctx->first_selection_x = now_x;
		ctx->first_selection_y = now_y;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		if(ctx->is_a_tap) {
			if(ctx->selected_animation) {
				ctx->deselect();
			} else {
				ctx->detailed_mode = !(ctx->detailed_mode);
				
				Transition *transition = new (std::nothrow) Transition(ctx, transition_progressed);
				if(transition) {
					source->start_animation(transition);
				} else {
					ctx->refresh_appearance(1.0); // skip to final state directly
				}
				
				if(ctx->detailed_mode)
					ctx->context->zoom_in_at(ctx->shared_from_this(), ctx->pos_x, ctx->pos_y);
				else
					ctx->context->zoom_restore();
			}
		} else if(!ctx->detailed_mode) {
			double scaling = ctx->context->get_scaling();
			ctx->pos_x += (now_x - ctx->first_selection_x) / scaling;
			ctx->pos_y += (now_y - ctx->first_selection_y) / scaling;

			ctx->machine->set_position(ctx->pos_x / MACHINE_POS_SCALING, ctx->pos_y / MACHINE_POS_SCALING);
			
			ctx->refresh_position(0.0, 0.0);
		}
		break;
	}
}

void Connector::MachineGraphic::on_output_socket_event(KammoGUI::SVGCanvas::SVGDocument *source,
						       KammoGUI::SVGCanvas::ElementReference *e_ref,
						       const KammoGUI::SVGCanvas::MotionEvent &event) {
	if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
		IOSocket *io_sock = (IOSocket *)e_ref;
		
		if(current_output) {
			auto crnt = current_output->first.lock();
			crnt->deselect();
		}
		
		auto ownr = io_sock->owner->shared_from_this();
		ownr->select(io_sock->name);	
	}
}

void Connector::MachineGraphic::on_input_socket_event(KammoGUI::SVGCanvas::SVGDocument *source,
						      KammoGUI::SVGCanvas::ElementReference *e_ref,
						      const KammoGUI::SVGCanvas::MotionEvent &event) {
	if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
		IOSocket *io_sock = (IOSocket *)e_ref;
		
		if(current_output) {
			auto crnt = current_output->first.lock();

			auto s = crnt->get_ri_machine();
			auto d = io_sock->owner->get_ri_machine();

			d->attach_input(s, current_output->second, io_sock->name);
			
			crnt->deselect();
		}

		io_sock->owner->leave_detailed_mode();
		io_sock->owner->context->zoom_restore();
	}
}

std::shared_ptr<Connector::MachineGraphic> Connector::MachineGraphic::create(Connector *context,
									     std::shared_ptr<RemoteInterface::RIMachine> machine) {
	std::stringstream id_stream;
	id_stream << "graphic_" << (void *)machine.get();

	KammoGUI::SVGCanvas::ElementReference graphic_template = KammoGUI::SVGCanvas::ElementReference(context, "machineGraphicTemplate");
	KammoGUI::SVGCanvas::ElementReference connector_layer = KammoGUI::SVGCanvas::ElementReference(context, "connectorContainer");

	(void) connector_layer.add_element_clone(id_stream.str(), graphic_template);
	
	auto retval = std::make_shared<MachineGraphic>(context, id_stream.str(), machine);
	
	machine->set_state_change_listener(retval);

	mch2grph[machine] = retval;
	
	return retval;
}

void Connector::MachineGraphic::hide() {
	set_display("none");
}

void Connector::MachineGraphic::show() {
	set_display("inline");
}

void Connector::MachineGraphic::leave_detailed_mode() {
	detailed_mode = false;
	
	Transition *transition = new (std::nothrow) Transition(this, transition_progressed);
	if(transition) {
		context->start_animation(transition);
	} else {
		refresh_appearance(1.0); // skip to final state directly
	}
}

void Connector::MachineGraphic::get_position(double &_x, double &_y) {
	_x = appear_at_x;
	_y = appear_at_y;
}

void Connector::MachineGraphic::animate_selection() {
	if(current_output) {
		if(auto mgfx = current_output->first.lock()) {
			if(mgfx->selected_animation) {
				KammoGUI::SVGCanvas::SVGMatrix transform_m = mgfx->selection_indicator_base_t;
				
				// rotate around center
				transform_m.translate(-(250.0),
						      -(250.0)
					);
				transform_m.rotate(mgfx->selected_animation->selection_animation_progress * 2.0 * M_PI);
				transform_m.translate((250.0),
						      (250.0)
					);
				
				mgfx->find_child_by_class("selectedIndicator").set_transform(transform_m);
			}
		}
	}
}

/***************************
 *
 *  Class Connector::ZoomTransition
 *
 ***************************/

Connector::ZoomTransition::ZoomTransition(
	Connector *_context,
	std::function<void(Connector *context, double zoom, double x, double y)> _callback,
	double _zoom_start, double _zoom_stop,
	double _x_start, double _y_start,
	double _x_stop, double _y_stop) :

	KammoGUI::Animation(TRANSITION_TIME), ctx(_context), callback(_callback),
	zoom_start(_zoom_start), zoom_stop(_zoom_stop),
	x_start(_x_start), y_start(_y_start),
	x_stop(_x_stop), y_stop(_y_stop)

{}

void Connector::ZoomTransition::new_frame(float progress) {
	double T = (double)progress;
	double x = (x_stop - x_start) * T + x_start;
	double y = (y_stop - y_start) * T + y_start;
	double zoom = (zoom_stop - zoom_start) * T + zoom_start;
	callback(ctx, zoom, x, y);
}

void Connector::ZoomTransition::on_touch_event() { /* ignored */ }

/***************************
 *
 *  Class Connector
 *
 ***************************/

void Connector::on_render() {
	KammoGUI::SVGCanvas::SVGMatrix transform_m;

	// scale and translate
	transform_m.init_identity();
	transform_m.translate(-(position_x),
			      -(position_y)
			     );
	double d_scale = scaling * pan_zoom_scale;
	transform_m.scale(d_scale, d_scale);
	transform_m.translate(base_translate_x - (pan_zoom_offset_x * pan_zoom_scale - pan_zoom_offset_x),
			      base_translate_y - (pan_zoom_offset_y * pan_zoom_scale - pan_zoom_offset_y)
		);
	
	// fetch the root and transform it	
	KammoGUI::SVGCanvas::ElementReference(this, "connectionContainer").set_transform(transform_m);
	KammoGUI::SVGCanvas::ElementReference(this, "connectorContainer").set_transform(transform_m);

	MachineGraphic::animate_selection();
}

void Connector::on_resize() {
	int canvas_w, canvas_h;
	float canvas_w_inches, canvas_h_inches;
	KammoGUI::SVGCanvas::SVGRect document_size;
	
	// get data
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);
	get_canvas_size(canvas_w, canvas_h);
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	{
		center_x = (double)canvas_w / 2.0;
		center_y = (double)canvas_h / 2.0;
	}
	
	{ // calculate transform for the connectorContainer part of the document
		double tmp;

		// calculate the width of the canvas in "fingers" 
		tmp = canvas_w_inches / INCHES_PER_FINGER;
		double canvas_width_fingers = tmp;

		// calculate the size of a finger in pixels
		tmp = canvas_w / (canvas_width_fingers);
		double finger_width = tmp;
				
		// calculate initial scaling factor
		old_scaling = scaling = (2.0 * finger_width) / (double)document_size.width;

		// calculate zoom scaling factor
		zoom_scaling = (5.0 * finger_width) / (double)document_size.width;

		// calucate base translation
		base_translate_x = canvas_w / 2.0;
		base_translate_y = canvas_h / 2.0;
	}

	{ // let the panAndZoomSurface fill the entire view
		pan_and_zoom.set_rect_coords(0.0, 0.0, canvas_w, canvas_h);
	}
}

void Connector::zoom_callback(Connector *ctx, double zoom, double x, double y) {
	ctx->scaling = zoom;
	ctx->position_x = x;
	ctx->position_y = y;
}

bool Connector::on_scale(KammoGUI::ScaleGestureDetector *detector) {
	pan_zoom_scale *= (double)detector->get_scale_factor();

	return true;
}

bool Connector::on_scale_begin(KammoGUI::ScaleGestureDetector *detector) {
	double fc_x = (double)detector->get_focus_x();
	double fc_y = (double)detector->get_focus_y();

	// calculate offset and scale to document coordinates
	pan_zoom_offset_x = fc_x - center_x;
	pan_zoom_offset_y = fc_y - center_y;
	pan_zoom_scale = 1.0;
	
	return true;
}

void Connector::on_scale_end(KammoGUI::ScaleGestureDetector *detector) {
	scaling *= pan_zoom_scale;
	position_x += (pan_zoom_scale * pan_zoom_offset_x - pan_zoom_offset_x) / scaling;
	position_y += (pan_zoom_scale * pan_zoom_offset_y - pan_zoom_offset_y) / scaling;

	pan_zoom_scale = 1.0;
	pan_zoom_offset_x = pan_zoom_offset_y = 0.0;
}

void Connector::on_pan_and_zoom_event(KammoGUI::SVGCanvas::SVGDocument *source,
				      KammoGUI::SVGCanvas::ElementReference *e_ref,
				      const KammoGUI::SVGCanvas::MotionEvent &event) {
	Connector *ctx = (Connector *)source;

	static bool ignore_scroll = false;
	bool scroll_event = ctx->sgd.on_touch_event(event);

	if(auto selgraph = ctx->selected_graphic.lock()) {
		ignore_scroll = true;
	}	
	
	if((!(scroll_event)) && (!ignore_scroll)) {
		double now_x = event.get_x();
		double now_y = event.get_y();
		
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			ctx->is_a_tap = false;
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			// check if the user is moving the finger too far, indicating abort action
			// if so - disable is_a_tap
			if(ctx->is_a_tap) {
				if(fabs(now_x - ctx->first_selection_x) > 20)
					ctx->is_a_tap = false;
				if(fabs(now_y - ctx->first_selection_y) > 20)
					ctx->is_a_tap = false;
			} else {
				ctx->position_x -= (now_x - ctx->last_selection_x) / ctx->scaling;
				ctx->position_y -= (now_y - ctx->last_selection_y) / ctx->scaling;
				ctx->last_selection_x = now_x;
				ctx->last_selection_y = now_y;
			}
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			ctx->is_a_tap = true;
			ctx->first_selection_x = now_x;
			ctx->first_selection_y = now_y;
			ctx->last_selection_x = now_x;
			ctx->last_selection_y = now_y;
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			break;
		}

	} else {
		ignore_scroll = true;
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			// when the last finger is lifted, break the ignore_scroll lock
			ignore_scroll = false;
			SATAN_DEBUG(" reset ignore_scroll!\n");

			// reset zoom if in selection mode
			if(auto selgraph = ctx->selected_graphic.lock()) {
				selgraph->leave_detailed_mode();
				ctx->zoom_restore();
			}
			break;
		}
	}
}

Connector::Connector(KammoGUI::SVGCanvas *cnvs) : SVGDocument(std::string(SVGLoader::get_svg_directory() + "/connector.svg"), cnvs),
						  plus_button(cnvs, std::string(SVGLoader::get_svg_directory() + "/plusButton.svg"), CornerButton::bottom_right),
						  trash_button(cnvs, std::string(SVGLoader::get_svg_directory() + "/trashButton.svg"), CornerButton::top_right),
						  settings_button(cnvs, std::string(SVGLoader::get_svg_directory() + "/settingsButton.svg"), CornerButton::top_left),
						  list_view(cnvs), pan_and_zoom(this, "panAndZoomSurface"), position_x(0.0), position_y(0.0), pan_zoom_offset_x(0.0), pan_zoom_offset_y(0.0), pan_zoom_scale(1.0), sgd(this) {

	connection_list = std::make_shared<ConnectionList>(cnvs);
	
	// hide template graphics
	KammoGUI::SVGCanvas::ElementReference(this, "machineGraphicTemplateContainer").set_display("none");

	pan_and_zoom.set_event_handler(on_pan_and_zoom_event);

	plus_button.set_select_callback(
		[this]() {
			list_view.clear();

			for(auto handle2hint : RemoteInterface::HandleList::get_handles_and_hints()) {
				list_view.add_row(handle2hint.first);
			}
	
			list_view.select_from_list("Create new machine", this,
						  [this](void *context, bool row_selected, int row_index, const std::string &text) {
							  if(row_selected) {
								  SATAN_DEBUG("plusbutton selected %s\n", text.c_str());
								  RemoteInterface::HandleList::create_instance(
									  text,
									  position_x / MACHINE_POS_SCALING, position_y / MACHINE_POS_SCALING);
							  }
						  }
				);
		}
		);

	trash_button.set_select_callback(
		[this]() {
			if(auto selgraph = selected_graphic.lock()) {
				selgraph->get_ri_machine()->delete_machine();
				zoom_restore();
			}
		}
		);	
	trash_button.hide();

	settings_button.set_select_callback(
		[this]() {
			if(auto selgraph = selected_graphic.lock()) {
				selgraph->leave_detailed_mode();
				zoom_restore();

				auto name = strdup(selgraph->get_ri_machine()->get_name().c_str());
				if(name) {
					// show the ControlsContainer
					static KammoGUI::UserEvent *ue = NULL;
					KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showControlsContainer");
					if(ue != NULL) {
						std::map<std::string, void *> args;
						args["machine_to_control"] = name;
						KammoGUI::EventHandler::trigger_user_event(ue, args);
					} else {
						free(name);
					}
				}
			}
		}
		);	
	settings_button.hide();
}

Connector::~Connector() {}

double Connector::get_scaling() {return scaling;}


void Connector::show_connection_list(double x_pos, double y_pos,
				     std::shared_ptr<RemoteInterface::RIMachine> src,
				     std::shared_ptr<RemoteInterface::RIMachine> dst,
				     std::set<std::pair<std::string, std::string> > output2input_names) {

	{ // bring out the connection list
		connection_list->clear();
		for(auto o2i : output2input_names) {
			std::string output = o2i.first;
			std::string input = o2i.second;
			connection_list->add(o2i.first, o2i.second,
					     [src, dst, output, input](){
						     SATAN_DEBUG(" will call remote interface detach.\n");
						     dst->detach_input(src, output, input);
					     }
				);
		}
		connection_list->show();
	}
}

void Connector::zoom_in_at(std::shared_ptr<MachineGraphic> new_selection, double x_pos, double y_pos) {
	selected_graphic = new_selection;
	
	old_scaling = scaling;
	ZoomTransition *transition = new (std::nothrow) ZoomTransition(this, zoom_callback, scaling,
								       zoom_scaling,
								       position_x, position_y,
								       x_pos, y_pos);
	if(transition) {
		start_animation(transition);
	} else {
		zoom_callback(this, zoom_scaling, x_pos, y_pos); // skip to final state directly
	}

	for(auto graphic : graphics) {
		if(graphic != new_selection)
			graphic->hide();
	}

	ConnectionGraphic::hide_all();
	
	plus_button.hide();
	trash_button.show();
	settings_button.show();
}

void Connector::zoom_restore() {
	selected_graphic.reset();
	
	ZoomTransition *transition = new (std::nothrow) ZoomTransition(this, zoom_callback, scaling,
								       old_scaling,
								       position_x, position_y,
								       position_x, position_y);
	if(transition) {
		start_animation(transition);
	} else {
		zoom_callback(this, old_scaling, position_x, position_y); // skip to final state directly
	}

	for(auto graphic : graphics) {
		graphic->debug_print();
		graphic->show();
	}

	ConnectionGraphic::show_all();

	plus_button.show();
	trash_button.hide();
	settings_button.hide();
}

void Connector::ri_machine_registered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	if(ri_machine->get_machine_type() == "MachineSequencer") return; // we don't want to show the MachineSequencers in the connector view..

	SATAN_DEBUG("---->     --- (%d) will create MachineGraphic for %s\n", gettid(), ri_machine->get_name().c_str());
	  
	KammoGUI::run_on_GUI_thread(
		[this, ri_machine]() {
			graphics.push_back(MachineGraphic::create(this, ri_machine));
			get_parent()->redraw();
			SATAN_DEBUG("  --- (%d) CREATED MachineGraphic for %s\n", gettid(), ri_machine->get_name().c_str());
			SATAN_DEBUG("  \n");
		}
		);
}

void Connector::ri_machine_unregistered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	if(ri_machine->get_machine_type() == "MachineSequencer") return; // we don't know any MachineSequencer machines..

	KammoGUI::run_on_GUI_thread(
		[this, ri_machine]() {
			auto iterator = graphics.begin();
			for(; iterator != graphics.end(); iterator++) {
				if((*iterator)->matches_ri_machine(ri_machine)) {
					if(selected_graphic.lock() == (*iterator)) {
						zoom_restore();
					}
					
					ConnectionGraphic::machine_unregistered(*iterator);

					iterator = graphics.erase(iterator);
					break;
				}
			}
		}
		);
}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(ConnectorHandler,"connector");

virtual void on_init(KammoGUI::Widget *wid) {
	KammoGUI::SVGCanvas *cnvs = (KammoGUI::SVGCanvas *)wid;		
	cnvs->set_bg_color(1.0, 1.0, 1.0);

	static std::shared_ptr<Connector> connector_ui = std::make_shared<Connector>(cnvs);
	RemoteInterface::Client::register_ri_machine_set_listener(connector_ui);
}

KammoEventHandler_Instance(ConnectorHandler);
