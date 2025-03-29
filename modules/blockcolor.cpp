/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017, 2020-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/exemption.h"
#include "modules/extban.h"
#include "numerichelper.h"

class ModuleBlockColor final
	: public Module
{
private:
	ExtBan::Acting extban;
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelMode bc;

public:
	ModuleBlockColor()
		: Module(VF_VENDOR, "Adds channel mode c (blockcolor) which allows channels to block messages which contain IRC formatting codes.")
		, extban(this, "blockcolor", 'c')
		, exemptionprov(this)
		, bc(this, "blockcolor", 'c')
	{
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if ((target.type == MessageTarget::TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			auto* c = target.Get<Channel>();

			ModResult res = exemptionprov.Check(user, c, "blockcolor");
			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			bool modeset = c->IsModeSet(bc);
			if (!extban.GetStatus(user, c).check(!modeset))
			{
				std::string_view ctcpname; // Unused.
				std::string_view message;
				if (!details.IsCTCP(ctcpname, message))
					message = details.text;

				auto it = std::find_if(message.begin(), message.end(), [](const char& chr)
				{
					switch (chr)
					{
						case '\x02': // Bold
						case '\x03': // Color
						case '\x04': // Hex Color
						case '\x1D': // Italic
						case '\x11': // Monospace
						case '\x16': // Reverse
						case '\x1E': // Strikethrough
						case '\x1F': // Underline
						case '\x0F': // Reset
							return true;
					}
					return false;
				});

				if (it != message.end())
				{
					if (modeset)
						user->WriteNumeric(Numerics::CannotSendTo(c, "messages containing formatting characters", &bc));
					else
						user->WriteNumeric(Numerics::CannotSendTo(c, "messages containing formatting characters", extban));
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleBlockColor)
