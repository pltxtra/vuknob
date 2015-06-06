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

#ifndef CONTROLLER_HANDLER_CLASS
#define CONTROLLER_HANDLER_CLASS

#include "remote_interface.hh"

class ControllerHandler : public RemoteInterface::RIMachine::RIMachineSetListener {
private:
	std::map<std::string, std::shared_ptr<RemoteInterface::RIMachine> > machines;
public:
	virtual void ri_machine_registered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) override;
	virtual void ri_machine_unregistered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) override;

	std::shared_ptr<RemoteInterface::RIMachine> get_machine_by_name(const std::string &name);

	static void disable_sample_editor_shortcut();
};

#endif
