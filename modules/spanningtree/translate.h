/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

namespace Translate
{
	/** Generate a list of mode parameters suitable for FMODE/MODE from a Modes::ChangeList::List
	 * @param modes List of mode changes
	 * @return List of mode parameters built from the input. Does not include the modes themselves,
	 * only the parameters.
	 */
	CommandBase::Params ModeChangeListToParams(const Modes::ChangeList::List& modes);

	//xxx
	CommandBase::Params ParamsToNetwork(const std::vector<TranslateType>& types, const CommandBase::Params& params, CommandBase* custom_translator);

	/** Translates a single parameter to the network form.
	 * @param type The translation type to use.
	 * @param item The parameter to translate.
	 * @param custom_translator A custom translator to translate the parameter with if the type is CUSTOM.
	 * @param index The index of the parameter to translate.
	 */
	std::string SingleParamToNetwork(TranslateType type, const std::string& item, CommandBase* custom_translator, size_t index);
}
