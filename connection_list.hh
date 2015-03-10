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

#ifndef CONNECTION_LIST_CLASS
#define CONNECTION_LIST_CLASS

#include <kamogui.hh>
#include <memory>
#include <functional>

class ConnectionList : public KammoGUI::SVGCanvas::SVGDocument{
private:
	KammoGUI::SVGCanvas::SVGMatrix base_transform_t;
	KammoGUI::SVGCanvas::SVGRect document_size;
	double element_vertical_offset, zoom_factor;
	
	class ConnectionGraphic : public KammoGUI::SVGCanvas::ElementReference {
	private:
		KammoGUI::SVGCanvas::ElementReference delete_button;
		
	public:
		ConnectionGraphic(ConnectionList *context, const std::string &id,
				  std::function<void()> on_delete,
				  const std::string &src, const std::string &dst);
		~ConnectionGraphic();
		
		static std::shared_ptr<ConnectionGraphic> create(ConnectionList *context, int id,
								 std::function<void()> on_delete,
								 const std::string &src, const std::string &dst);
	};

	class ZoomTransition : public KammoGUI::Animation {
	private:
		std::function<void(double zoom)> callback;

		double zoom_start, zoom_stop;
		
	public:
		ZoomTransition(std::function<void(double zoom)> callback,
			       double zoom_start, double zoom_stop);
		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};
	
	std::vector<std::shared_ptr<ConnectionGraphic> > graphics;
	KammoGUI::SVGCanvas::ElementReference backdrop;

public:
	
	ConnectionList(KammoGUI::SVGCanvas *cnv);
	~ConnectionList();

	virtual void on_render() override;
	virtual void on_resize() override;

	void clear();
	void add(const std::string &src, const std::string &dst, std::function<void()> on_delete);
	
	void show();
	void hide();

};

#endif

