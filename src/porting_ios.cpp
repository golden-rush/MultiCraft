/*
MultiCraft

Copyright (C) 2014-2019 Maksim Gamarnik [MoNTE48] MoNTE48@mail.ua
Copyright (C) 2016-2019 sfan5

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3.0 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <string>

#include "porting.h"
#include "config.h"

static void *uiviewcontroller;

namespace porting {
	void initializePathsiOS() {
		char buf[128];

		ioswrap_paths(PATH_DOCUMENTS, buf, sizeof(buf));
		path_user = std::string(buf);
		ioswrap_paths(PATH_LIBRARY_SUPPORT, buf, sizeof(buf));
		path_share = std::string(buf);
		path_locale = std::string(buf) + "/locale";
		ioswrap_paths(PATH_LIBRARY_CACHE, buf, sizeof(buf));
		path_cache = std::string(buf);
	}

	void copyAssets() {
		ioswrap_assets();
	}

	float getDisplayDensity() {
		return 1;
	}

	v2u32 getDisplaySize() {
		static bool firstrun = true;
		static v2u32 retval;

		if (firstrun) {
			unsigned int values[2];
			ioswrap_size(values);
			retval.X = values[0];
			retval.Y = values[1];
			firstrun = false;
		}

		return retval;
	}

	void setViewController(void *v) {
		uiviewcontroller = v;
	}

	void showInputDialog(const std::string &acceptButton, const std::string &hint,
						 const std::string &current, int editType) {
		ioswrap_show_dialog(uiviewcontroller, acceptButton.c_str(), hint.c_str(), current.c_str(), editType);
	}

	int getInputDialogState() {
		return ioswrap_get_dialog(NULL);
	}

	std::string getInputDialogValue() {
		const char *str;
		ioswrap_get_dialog(&str);
		return std::string(str);
	}

	void notifyAbortLoading() {
		ioswrap_asset_refresh();
	}

	void notifyServerConnect(bool is_multiplayer) {
#ifdef ADS
		ads_allow(!is_multiplayer);
#endif
	}

	void notifyExitGame() {
#ifdef ADS
		ads_allow(true);
#endif
	}
}

extern int real_main(int argc, char *argv[]);

void irrlicht_main() {
	static const char *args[] = {
			PROJECT_NAME,
	};
	real_main(1, (char **) args);
}
